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
    if(package MATCHES "name: windows-")
        message(FATAL_ERROR "windows-package hands on an artifact the release job takes")
    endif()
    if(NOT package MATCHES "name: msi-build-\\$\\{\\{ matrix.version \\}\\}")
        message(FATAL_ERROR "windows-package does not hand on msi-build-<version>")
    endif()
    list(JOIN job_windows-check "\n" check)
    if(NOT check MATCHES "name: msi-build-\\$\\{\\{ matrix.version \\}\\}")
        message(FATAL_ERROR "windows-check does not take msi-build-<version>")
    endif()
    if(NOT check MATCHES "name: windows-\\$\\{\\{ matrix.version \\}\\}\n          path: [^\n]*hitokoto-\\$\\{\\{ matrix.version \\}\\}-x64.msi\n")
        message(FATAL_ERROR "windows-check does not hand on exactly the MSI of the version")
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
    if(NOT release MATCHES "\n          pattern: windows-\\*\n          merge-multiple: true\n          path: \\$\\{\\{ runner.temp \\}\\}/debian\n")
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

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
