# SPDX-License-Identifier: GPL-3.0-or-later
# S037: lints over packaging.yml for the Windows installer: the Windows jobs build and check the
# versions of the plan without writing, only a checked MSI reaches the release job, and the release
# job waits for both packages. Run with cmake -P; the file is read as text (no YAML parser).
#   -DCASE=<name>            the case below
#   -DWORKFLOWS_DIR=<dir>    .github/workflows
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOWS_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/workflow_lines.cmake")

set(windows_jobs windows-package windows-check)

read_workflow(packaging.yml packaging)
jobs("${packaging}" job)
foreach(id IN LISTS windows_jobs ITEMS debian-plan debian-release)
    if(NOT id IN_LIST job_ids)
        message(FATAL_ERROR "packaging.yml has no job '${id}' (jobs: ${job_ids})")
    endif()
endforeach()

if("${CASE}" STREQUAL "TheWindowsJobsOnlyRead")
    # Decision 9: the release job stays the only one that writes; the Windows jobs read.
    foreach(id IN LISTS windows_jobs)
        list(JOIN job_${id} "\n" text)
        if(NOT text MATCHES "\n    permissions:\n      contents: read\n    [a-z]")
            message(FATAL_ERROR "the job ${id} does not have exactly contents: read")
        endif()
        if(text MATCHES "secrets\\.")
            message(FATAL_ERROR "the job ${id} uses a secret")
        endif()
    endforeach()

elseif("${CASE}" STREQUAL "TheWindowsJobsRunOnTheCiImage")
    # Decisions 4 and 8: windows-2022, the image of the Windows CI.
    foreach(id IN LISTS windows_jobs)
        job_value("${job_${id}}" runs-on runs_on)
        expect_eq("${runs_on}" "windows-2022" "runs-on of ${id}")
    endforeach()

elseif("${CASE}" STREQUAL "OneWindowsVersionDoesNotStopTheOthers")
    # Decision 2: a matrix entry per version without fail-fast, from the plan.
    foreach(id IN LISTS windows_jobs)
        list(JOIN job_${id} "\n" text)
        if(NOT text MATCHES "\n      fail-fast: false\n")
            message(FATAL_ERROR "the matrix of ${id} can fail fast")
        endif()
        if(NOT text MATCHES "version: \\$\\{\\{ fromJSON\\(needs.debian-plan.outputs.windows_build\\) \\}\\}")
            message(FATAL_ERROR "the matrix of ${id} does not come from the Windows plan")
        endif()
    endforeach()
    job_value("${job_windows-check}" needs needs)
    if(NOT needs MATCHES "windows-package")
        message(FATAL_ERROR "windows-check does not need windows-package: '${needs}'")
    endif()

elseif("${CASE}" STREQUAL "OnlyACheckedInstallerIsReleased")
    # Decision 8: the build job hands its MSIs to the check job only; the check job hands on the
    # MSI of the version (never the 0.0.0 one) after every check passed.
    list(JOIN job_windows-package "\n" package)
    if(package MATCHES "name: windows-" OR package MATCHES "archive: false")
        message(FATAL_ERROR "windows-package hands on an artifact the release job takes")
    endif()
    if(NOT package MATCHES "name: msi-build-\\$\\{\\{ matrix.version \\}\\}")
        message(FATAL_ERROR "windows-package does not hand on msi-build-<version>")
    endif()
    list(JOIN job_windows-check "\n" check)
    if(NOT check MATCHES "name: msi-build-\\$\\{\\{ matrix.version \\}\\}")
        message(FATAL_ERROR "windows-check does not take msi-build-<version>")
    endif()
    # Armin (2026-11-13): the MSI itself, not a zip named windows-<version>; with archive: false
    # the artifact is named after the file (upload-artifact v7).
    if(NOT check MATCHES "\n          archive: false\n          path: [^\n]*/hitokoto-\\$\\{\\{ matrix.version \\}\\}-x64.msi\n")
        message(FATAL_ERROR "windows-check does not hand on exactly the MSI of the version, unzipped")
    endif()
    string(FIND "${check}" "uses: actions/upload-artifact" upload)
    string(FIND "${check}" "install-check.ps1" install)
    if(upload EQUAL -1 OR install EQUAL -1 OR upload LESS install)
        message(FATAL_ERROR "windows-check hands the MSI on before it is checked")
    endif()

elseif("${CASE}" STREQUAL "TheReleaseWaitsForBothPackages")
    # Decisions 2 and 9: the release job needs the Windows check, takes the checked MSIs into the
    # same flat directory as the Debian packages, and releases a version only with every package
    # it needs.
    job_value("${job_debian-release}" needs needs)
    foreach(other IN ITEMS debian-plan debian-package windows-check)
        if(NOT needs MATCHES "(^|[^a-z-])${other}([^a-z-]|$)")
            message(FATAL_ERROR "debian-release does not need ${other}: '${needs}'")
        endif()
    endforeach()
    list(JOIN job_debian-release "\n" release)
    if(NOT release MATCHES "\n          pattern: hitokoto-\\*-x64.msi\n          merge-multiple: true\n          path: \\$\\{\\{ runner.temp \\}\\}/debian\n")
        message(FATAL_ERROR "debian-release does not take the MSIs into the directory of the packages")
    endif()
    if(NOT release MATCHES "WINDOWS_SKIPPED: \\$\\{\\{ needs.debian-plan.outputs.windows_skipped \\}\\}")
        message(FATAL_ERROR "debian-release does not know the versions skipped for Windows")
    endif()
    if(NOT release MATCHES "msi=\"hitokoto-\\$\\{version\\}-x64.msi\"")
        message(FATAL_ERROR "debian-release does not look for hitokoto-<version>-x64.msi")
    endif()
    job_condition("${job_debian-release}" condition)
    if(condition MATCHES "needs.windows-")
        message(FATAL_ERROR "debian-release waits for every version of Windows: '${condition}'")
    endif()

elseif("${CASE}" STREQUAL "TheWindowsRecipeStartsADryRun")
    # Decision 11: a pull request that changes the Windows recipe runs the dry run.
    section("${packaging}" on on_lines)
    list(JOIN on_lines "\n" on_text)
    if(NOT on_text MATCHES "\n      - packaging/windows/\\*\\*\n")
        message(FATAL_ERROR "a pull request that changes packaging/windows starts no dry run")
    endif()

elseif("${CASE}" STREQUAL "TheEulaIsAcceptedBeforeWixRuns")
    # Decision 7, first dry run (pull request 45): WiX 7 refuses every command, also
    # "wix extension add", until its OSMF EULA is accepted (WIX7015). The job accepts it for the
    # major version of the recipe's wix before the first wix command.
    list(JOIN job_windows-package "\n" package)
    string(FIND "${package}" "eula accept" accept)
    string(FIND "${package}" "extension add" extension)
    if(accept EQUAL -1)
        message(FATAL_ERROR "windows-package does not accept WiX's EULA")
    endif()
    if(extension EQUAL -1 OR extension LESS accept)
        message(FATAL_ERROR "windows-package runs wix extension add before accepting the EULA")
    endif()
    # read_workflow stores square brackets as @LB@ and @RB@.
    string(FIND "${package}" "eula accept \"wix$($env:WIX.Split('.')@LB@0@RB@)\"" major)
    if(major EQUAL -1)
        message(FATAL_ERROR "windows-package does not accept the EULA of the recipe's major version")
    endif()

elseif("${CASE}" STREQUAL "AnEmptyIceListReachesTheBuild")
    # Second dry run (pull request 45): "pwsh build-package.ps1 … -Ice $ice" with no accepted ICE
    # passes -Ice without a value to the child process ("Missing an argument for parameter
    # 'Ice'"). The step runs the script in its own session with splatting, -Ice only if the recipe
    # lists any.
    list(JOIN job_windows-package "\n" package)
    if(package MATCHES "pwsh packaging/windows/build-package.ps1")
        message(FATAL_ERROR "windows-package runs build-package.ps1 in a child pwsh")
    endif()
    string(FIND "${package}" "& ./packaging/windows/build-package.ps1 @arguments" call)
    if(call EQUAL -1)
        message(FATAL_ERROR "windows-package does not call build-package.ps1 with splatted arguments")
    endif()
    string(FIND "${package}" "if ($ice.Count -gt 0) { $arguments.Ice = $ice }" optional)
    if(optional EQUAL -1)
        message(FATAL_ERROR "windows-package passes -Ice also without accepted ICE findings")
    endif()

elseif("${CASE}" STREQUAL "AFailedBuildDoesNotSkipTheOtherChecks")
    # Review (code) O4: windows-check needs every leg of windows-package; without !cancelled() the
    # default success() would skip the checks of every version when one build failed.
    job_condition("${job_windows-check}" condition)
    foreach(part IN ITEMS "!cancelled()" "needs.debian-plan.result == 'success'")
        string(FIND "${condition}" "${part}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "the condition of windows-check lacks ${part}: '${condition}'")
        endif()
    endforeach()

elseif("${CASE}" STREQUAL "AReleaseWithoutAnyMsiIsNotStopped")
    # Review (code) O3: when no Windows check passed, the download of windows-* finds nothing; a
    # version skipped for Windows is still released. The release step checks every file it needs.
    list(JOIN job_debian-release "\n" release)
    if(NOT release MATCHES "\n      - uses: actions/download-artifact@v8\n        if: [^\n]*\n        continue-on-error: true\n        with:\n          pattern: hitokoto-\\*-x64.msi\n")
        message(FATAL_ERROR "the download of windows-* in debian-release may stop the release")
    endif()

elseif("${CASE}" STREQUAL "TheHandOverOfTheMsiIsCheckedInADryRun")
    # The release job runs only on main; windows-handover downloads the checked MSIs exactly as the
    # release job does, also in a dry run, and checks that each one arrives as a file of its own
    # (the first unzipped artifacts of this workflow).
    if(NOT "windows-handover" IN_LIST job_ids)
        message(FATAL_ERROR "packaging.yml has no job windows-handover")
    endif()
    list(JOIN job_windows-handover "\n" handover)
    if(NOT handover MATCHES "\n    permissions: {}\n")
        message(FATAL_ERROR "windows-handover has permissions")
    endif()
    job_value("${job_windows-handover}" needs needs)
    if(NOT needs MATCHES "windows-check")
        message(FATAL_ERROR "windows-handover does not need windows-check: '${needs}'")
    endif()
    job_condition("${job_windows-handover}" condition)
    if(condition MATCHES "refs/heads/main" OR condition MATCHES "inputs.publish")
        message(FATAL_ERROR "windows-handover does not run in a dry run: '${condition}'")
    endif()
    if(NOT handover MATCHES "\n          pattern: hitokoto-\\*-x64.msi\n          merge-multiple: true\n          path: \\$\\{\\{ runner.temp \\}\\}/debian\n")
        message(FATAL_ERROR "windows-handover does not download like the release job")
    endif()
    if(NOT handover MATCHES "hitokoto-\\$\\{version\\}-x64.msi")
        message(FATAL_ERROR "windows-handover does not look for hitokoto-<version>-x64.msi")
    endif()

elseif("${CASE}" STREQUAL "WindowsInstallsAqtinstallFromAPinnedCommit")
    # Rework on 2026-11-16 (ADR 0013): no release of aqtinstall installs Qt 6.11 or newer for
    # Windows. Every call of jurplel/install-qt-action that can run on Windows installs aqtinstall
    # from one commit, named by its full SHA, with the version in a comment on the same line or the
    # line above; ci.yml and packaging.yml name the same commit.
    set(source "git\\+https://github\\.com/miurahr/aqtinstall\\.git@([0-9a-f]+)")
    set(version_comment "# aqtinstall [0-9]+\\.[0-9]+\\.[0-9]+")
    read_workflow(ci.yml ci)
    set(pinned "")
    set(previous "")
    foreach(line IN LISTS ci)
        if(line MATCHES "^  AQT_SOURCE_WINDOWS: ${source}( .*)?$")
            set(pinned "${CMAKE_MATCH_1}")
            if(NOT "${previous}${line}" MATCHES "${version_comment}")
                message(FATAL_ERROR "AQT_SOURCE_WINDOWS in ci.yml has no comment with the version")
            endif()
        endif()
        set(previous "${line}")
    endforeach()
    if("${pinned}" STREQUAL "")
        message(FATAL_ERROR "ci.yml has no env AQT_SOURCE_WINDOWS: git+https://github.com/miurahr/aqtinstall.git@<commit>")
    endif()
    string(LENGTH "${pinned}" length)
    expect_eq("${length}" "40" "the length of the commit of AQT_SOURCE_WINDOWS (a full SHA)")

    # The lines of each step that uses install-qt-action, from its uses: line to the step's end
    # (a line indented less than uses:), joined with \n.
    function(install_qt_steps lines out)
        set(steps "")
        set(step "")
        set(indent -1)
        foreach(line IN LISTS lines)
            if(indent GREATER_EQUAL 0)
                # A line of spaces only counts as blank. CMake before 4.1 refuses a regular
                # expression that matches an empty string, so the indentation is the group before
                # the first other character.
                set(depth ${indent})
                if(line MATCHES "^( *)[^ ]")
                    string(LENGTH "${CMAKE_MATCH_1}" depth)
                endif()
                if(depth LESS indent)
                    list(APPEND steps "${step}")
                    set(indent -1)
                else()
                    string(APPEND step "\n${line}")
                endif()
            endif()
            if(indent LESS 0 AND line MATCHES "^( *(- )?)uses: jurplel/install-qt-action@")
                string(LENGTH "${CMAKE_MATCH_1}" indent)
                set(step "${line}")
            endif()
        endforeach()
        if(indent GREATER_EQUAL 0)
            list(APPEND steps "${step}")
        endif()
        set(${out} "${steps}" PARENT_SCOPE)
    endfunction()

    # ci.yml: one step for all platforms; aqtinstall's commit only where the runner is Windows.
    install_qt_steps("${ci}" ci_steps)
    list(LENGTH ci_steps count)
    if(count EQUAL 0)
        message(FATAL_ERROR "ci.yml has no call of install-qt-action")
    endif()
    foreach(step IN LISTS ci_steps)
        if(NOT step MATCHES "\n +aqtsource: \\$\\{\\{ runner.os == 'Windows' && env.AQT_SOURCE_WINDOWS \\|\\| '' \\}\\}(\n|$)")
            message(FATAL_ERROR "a call of install-qt-action in ci.yml does not take AQT_SOURCE_WINDOWS on Windows:\n${step}")
        endif()
    endforeach()

    # packaging.yml: the Windows installer's job runs on Windows only and names the commit itself.
    install_qt_steps("${packaging}" packaging_steps)
    list(LENGTH packaging_steps count)
    if(count EQUAL 0)
        message(FATAL_ERROR "packaging.yml has no call of install-qt-action")
    endif()
    foreach(step IN LISTS packaging_steps)
        if(NOT step MATCHES "\n( +#[^\n]*\n)? +aqtsource: ${source}( #[^\n]*)?(\n|$)")
            message(FATAL_ERROR "a call of install-qt-action in packaging.yml does not install aqtinstall from a commit:\n${step}")
        endif()
        expect_eq("${CMAKE_MATCH_2}" "${pinned}" "the commit of aqtinstall in packaging.yml against ci.yml")
        if(NOT "${CMAKE_MATCH_1}${CMAKE_MATCH_3}" MATCHES "${version_comment}")
            message(FATAL_ERROR "the aqtsource of packaging.yml has no comment with the version")
        endif()
    endforeach()

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
