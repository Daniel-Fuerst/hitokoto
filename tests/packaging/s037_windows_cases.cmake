# SPDX-License-Identifier: GPL-3.0-or-later
# S037: cases for scripts/packaging/WindowsPackage.cmake and for the recipe in packaging/windows,
# run with cmake -P on fixed inputs (no network, no Windows).
#   -DCASE=<name>          the case below
#   -DSCRIPT=<file>        scripts/packaging/WindowsPackage.cmake
#   -DRECIPE_DIR=<dir>     packaging/windows
#   -DSOURCE_DIR=<dir>     the source tree (LICENSE, packaging/debian, .github/workflows)
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPT RECIPE_DIR SOURCE_DIR WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(expect_eq actual expected what)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: '${actual}', expected '${expected}'")
    endif()
endfunction()

# CMake wraps long messages, so runs of white space count as one space.
function(expect_contains text part what)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_text "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_part "${part}")
    string(FIND "${flat_text}" "${flat_part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' not in\n${text}")
    endif()
endfunction()

function(expect_not_contains text part what)
    string(FIND "${text}" "${part}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' found in\n${text}")
    endif()
endfunction()

# Writes the items of a list one per line (an empty list gives an empty file).
function(write_lines file)
    set(text "")
    foreach(item IN LISTS ARGN)
        string(APPEND text "${item}\n")
    endforeach()
    file(WRITE "${file}" "${text}")
endfunction()

# The recipe's settings with the given extra lines (skips, accepted validation findings).
function(write_recipe)
    write_lines("${WORK_DIR}/recipe.txt" "# test" "qt 6.8.3" "qt-arch win64_msvc2022_64"
                "wix 7.0.1" ${ARGN})
endfunction()

# The output of the Debian plan (scripts/packaging/DebianPackage.cmake) the Windows plan reads.
function(write_debian_plan action versions build latest)
    write_lines("${WORK_DIR}/debian-plan.txt" "action=${action}" "versions=${versions}"
                "build=${build}" "latest=${latest}" "skipped=" "without_package=")
endfunction()

# Runs the plan; the result lines become variables plan_<key>, and plan_result the exit code.
function(run_plan)
    file(REMOVE "${WORK_DIR}/plan.txt")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=plan -DRECIPE_FILE=${WORK_DIR}/recipe.txt
            -DPLAN_FILE=${WORK_DIR}/debian-plan.txt -DRELEASES_FILE=${WORK_DIR}/releases.txt
            -DOUTPUT=${WORK_DIR}/plan.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(plan_result "${code}" PARENT_SCOPE)
    set(plan_messages "${out}${err}" PARENT_SCOPE)
    foreach(key IN ITEMS build skipped skipped_reasons without_msi qt qt_arch wix ice)
        set(plan_${key} "<unset>" PARENT_SCOPE)
    endforeach()
    if(code EQUAL 0 AND EXISTS "${WORK_DIR}/plan.txt")
        file(STRINGS "${WORK_DIR}/plan.txt" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^([a-z_]+)=(.*)$")
                set(plan_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
            endif()
        endforeach()
    endif()
endfunction()

function(expect_ok result messages what)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${what} failed (${result}):\n${messages}")
    endif()
endfunction()

function(expect_fails result messages part what)
    if(result EQUAL 0)
        message(FATAL_ERROR "${what} succeeded, expected it to fail with '${part}'")
    endif()
    expect_contains("${messages}" "${part}" "message of ${what}")
endfunction()

# Runs the check of the installed files against an expected list.
function(run_files expected_file actual_file)
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=files -DEXPECTED_FILE=${expected_file}
            -DACTUAL_FILE=${actual_file} -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(files_result "${code}" PARENT_SCOPE)
    set(files_messages "${out}${err}" PARENT_SCOPE)
endfunction()

if("${CASE}" STREQUAL "TheVersionsToReleaseGetAnMsi")
    # Decision 2: the Windows job builds exactly the versions the release needs.
    write_recipe()
    write_debian_plan(release "0.5.2 0.5.3" [=[["0.5.2","0.5.3"]]=] 0.5.3)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.2","0.5.3"]]=] "build")
    expect_eq("${plan_skipped}" "" "skipped")

elseif("${CASE}" STREQUAL "ADryRunBuildsTheHighestVersion")
    # Decision 11: the Debian plan of a dry run builds the highest version; so does Windows.
    write_recipe()
    write_debian_plan(current "" [=[["0.5.1"]]=] 0.5.1)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.1"]]=] "build")

elseif("${CASE}" STREQUAL "NothingToBuildBuildsNothing")
    # A version:none merge: every version has its release, nothing to build.
    write_recipe()
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" "[]" "build")

elseif("${CASE}" STREQUAL "SkippedVersionsAreLeftOutAndNamed")
    # Decision 2: a version skipped for Windows is released with the Debian package alone.
    write_recipe("skip 0.5.2 the MSI of 0.5.2 needs a fix in the tag")
    write_debian_plan(release "0.5.2 0.5.3" [=[["0.5.2","0.5.3"]]=] 0.5.3)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.3"]]=] "build")
    expect_eq("${plan_skipped}" "0.5.2" "skipped")
    expect_contains("${plan_skipped_reasons}" "0.5.2: the MSI of 0.5.2 needs a fix in the tag"
                    "reasons")

elseif("${CASE}" STREQUAL "AllVersionsSkippedBuildNothing")
    write_recipe("skip 0.5.1 reason")
    write_debian_plan(current "" [=[["0.5.1"]]=] 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" "[]" "build")
    expect_eq("${plan_skipped}" "0.5.1" "skipped")

elseif("${CASE}" STREQUAL "AReleaseWithoutTheMsiIsReported")
    # Decision 2: a published release without the MSI is reported and never changed; one skipped
    # for Windows is not reported as missing it, and tags not of the exact form are ignored.
    write_recipe("skip 0.5.2 reason")
    write_debian_plan(current "" "[]" 0.5.3)
    write_lines("${WORK_DIR}/releases.txt"
                "v0.3.0-tulip hitokoto-0.1.0.msi"
                "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS"
                "v0.5.2 hitokoto_0.5.2-1_amd64.deb SHA256SUMS"
                "v0.5.3 hitokoto_0.5.3-1_amd64.deb hitokoto-0.5.3-x64.msi SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_without_msi}" "0.5.1" "without_msi")

elseif("${CASE}" STREQUAL "VersionsBeyondWindowsInstallerFail")
    # Decision 6: Windows Installer reads major and minor up to 255, the patch up to 65535.
    write_recipe()
    write_lines("${WORK_DIR}/releases.txt")
    write_debian_plan(release "255.255.65535" [=[["255.255.65535"]]=] 255.255.65535)
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan at the limits")
    foreach(version IN ITEMS 256.0.0 0.256.0 0.0.65536)
        write_debian_plan(release "${version}" "[\"${version}\"]" ${version})
        run_plan()
        expect_fails("${plan_result}" "${plan_messages}"
                     "${version} cannot be the product version of an MSI" "the plan of ${version}")
    endforeach()
    # Listed as skipped, such a version is left out instead.
    write_recipe("skip 256.0.0 beyond Windows Installer")
    write_debian_plan(release "256.0.0" [=[["256.0.0"]]=] 256.0.0)
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan with 256.0.0 skipped")
    expect_eq("${plan_build}" "[]" "build")

elseif("${CASE}" STREQUAL "TheToolVersionsComeFromTheRecipe")
    # Decisions 3, 4 and 7, review O3: Qt, its arch, wix and the accepted validation findings.
    write_recipe("ice ICE61 the major upgrade allows the same version" "ice ICE91 per-user files")
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_qt}" "6.8.3" "qt")
    expect_eq("${plan_qt_arch}" "win64_msvc2022_64" "qt_arch")
    expect_eq("${plan_wix}" "7.0.1" "wix")
    expect_eq("${plan_ice}" "ICE61 ICE91" "ice")

elseif("${CASE}" STREQUAL "BadRecipesFail")
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    write_lines("${WORK_DIR}/recipe.txt" "qt-arch win64_msvc2022_64" "wix 7.0.1")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "no line 'qt" "a recipe without qt")
    write_lines("${WORK_DIR}/recipe.txt" "qt 6.8.3" "qt-arch win64_msvc2022_64")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "no line 'wix" "a recipe without wix")
    write_recipe("skip v0.5.2 a tag, not a version")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "not MAJOR.MINOR.PATCH" "a bad skip")
    write_recipe("skip 0.5.2")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "without a reason" "a skip without reason")
    write_recipe("ice 61 no ICE")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "not an ICE" "a bad ice line")
    write_recipe("ice ICE61")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "without a reason" "an ice line without reason")
    write_recipe("first 0.5.1")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "unknown line" "an unknown line")
    write_recipe()
    file(REMOVE "${WORK_DIR}/debian-plan.txt")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "debian-plan.txt" "a missing Debian plan")

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    file(WRITE "${WORK_DIR}/recipe.txt"
         "qt 6.8.3\r\nqt-arch win64_msvc2022_64\r\nwix 7.0.1\r\nskip 0.5.2 reason\r\n")
    file(WRITE "${WORK_DIR}/debian-plan.txt"
         "action=release\r\nversions=0.5.2 0.5.3\r\nbuild=[\"0.5.2\",\"0.5.3\"]\r\nlatest=0.5.3\r\n")
    file(WRITE "${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb\r\n")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.3"]]=] "build")
    expect_eq("${plan_wix}" "7.0.1" "wix")
    expect_eq("${plan_without_msi}" "0.5.1" "without_msi")

elseif("${CASE}" STREQUAL "TheFileListMatches")
    # Decision 5: the installed files, listed by the job in any order and with backslashes.
    write_lines("${WORK_DIR}/expected.txt" "# the files" "hitokoto.exe" "Qt6Core.dll"
                "platforms/qwindows.dll" "")
    write_lines("${WORK_DIR}/actual.txt" "platforms\\qwindows.dll" "Qt6Core.dll" "hitokoto.exe")
    run_files("${WORK_DIR}/expected.txt" "${WORK_DIR}/actual.txt")
    expect_ok("${files_result}" "${files_messages}" "the file check")
    # Windows' file names do not depend on case; neither does the check.
    write_lines("${WORK_DIR}/actual.txt" "PLATFORMS\\qwindows.dll" "qt6core.dll" "hitokoto.exe")
    run_files("${WORK_DIR}/expected.txt" "${WORK_DIR}/actual.txt")
    expect_ok("${files_result}" "${files_messages}" "the file check ignoring case")

elseif("${CASE}" STREQUAL "AMissingOrExtraFileFails")
    write_lines("${WORK_DIR}/expected.txt" "hitokoto.exe" "Qt6Core.dll" "platforms/qwindows.dll")
    write_lines("${WORK_DIR}/actual.txt" "hitokoto.exe" "Qt6Core.dll" "opengl32sw.dll")
    run_files("${WORK_DIR}/expected.txt" "${WORK_DIR}/actual.txt")
    expect_fails("${files_result}" "${files_messages}" "missing: platforms/qwindows.dll"
                 "the file check")
    expect_contains("${files_messages}" "unexpected: opengl32sw.dll" "the unexpected file")
    # The whole list is printed, so that a first dry run shows what windeployqt delivered.
    expect_contains("${files_messages}" "installed files (3):" "the list")

elseif("${CASE}" STREQUAL "TheLicenceBecomesRtf")
    # Decision 6 (ADR 0012): the licence dialog shows the GPL as RTF, made from LICENSE.
    file(WRITE "${WORK_DIR}/licence.txt" "GPL {text}\n  back\\slash\n\nend\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=rtf -DINPUT=${WORK_DIR}/licence.txt
            -DOUTPUT=${WORK_DIR}/licence.rtf -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_ok("${code}" "${out}${err}" "the RTF")
    file(READ "${WORK_DIR}/licence.rtf" rtf)
    expect_contains("${rtf}" "{\\rtf1\\ansi" "the RTF header")
    expect_contains("${rtf}" "GPL \\{text\\}\\par" "braces escaped")
    expect_contains("${rtf}" "  back\\\\slash\\par" "backslash escaped, spaces kept")
    expect_contains("${rtf}" "\\par\nend\\par" "an empty line")
    string(REGEX MATCH "}[\r\n]*$" closing "${rtf}")
    expect_eq("${closing}" "}\n" "the closing brace")
    # Only plain ASCII; anything else would need RTF's own escapes.
    file(WRITE "${WORK_DIR}/licence.txt" "Fürst\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=rtf -DINPUT=${WORK_DIR}/licence.txt
            -DOUTPUT=${WORK_DIR}/licence.rtf -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_fails("${code}" "${out}${err}" "not plain ASCII" "the RTF of a non-ASCII text")

elseif("${CASE}" STREQUAL "TheRealLicenceBecomesRtf")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=rtf -DINPUT=${SOURCE_DIR}/LICENSE
            -DOUTPUT=${WORK_DIR}/licence.rtf -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_ok("${code}" "${out}${err}" "the RTF of LICENSE")
    file(READ "${WORK_DIR}/licence.rtf" rtf)
    expect_contains("${rtf}" "GNU GENERAL PUBLIC LICENSE\\par" "the title")

elseif("${CASE}" STREQUAL "TheNoticeNamesTheQtVersion")
    # Decision 5: the third-party notice names the Qt version that is shipped.
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=notice
            -DTEMPLATE=${RECIPE_DIR}/third-party.txt.in -DQT_VERSION=6.8.3
            -DOUTPUT=${WORK_DIR}/THIRD-PARTY.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_ok("${code}" "${out}${err}" "the notice")
    file(READ "${WORK_DIR}/THIRD-PARTY.txt" notice)
    expect_contains("${notice}" "Qt 6.8.3" "the Qt version")
    expect_contains("${notice}" "GNU Lesser General Public License version 3" "Qt's licence")
    expect_contains("${notice}" "LGPL-3.0.txt" "the LGPL text")
    expect_contains("${notice}" "https://download.qt.io/archive/qt/6.8/6.8.3/" "Qt's source")
    expect_contains("${notice}" "Microsoft Visual C++" "the C++ runtime")
    expect_not_contains("${notice}" "@" "no placeholder left")

elseif("${CASE}" STREQUAL "TheRecipeBuildsWithTheCiQt")
    # Review O9: the recipe's Qt and the Qt of the Windows CI move together.
    file(STRINGS "${RECIPE_DIR}/recipe.txt" recipe REGEX "^qt ")
    string(REGEX REPLACE "^qt " "" recipe_qt "${recipe}")
    # The row of the build-test matrix (the benchmark matrix has a commented-out one).
    file(STRINGS "${SOURCE_DIR}/.github/workflows/ci.yml" rows REGEX "^ *- { name: Windows,")
    list(LENGTH rows count)
    expect_eq("${count}" "1" "rows for Windows in ci.yml")
    if(NOT rows MATCHES "qt: \"([0-9.]+)\", arch: ([a-z0-9_]+)")
        message(FATAL_ERROR "no qt and arch in the Windows row of ci.yml: ${rows}")
    endif()
    expect_eq("${recipe_qt}" "${CMAKE_MATCH_1}" "the recipe's Qt against ci.yml")
    file(STRINGS "${RECIPE_DIR}/recipe.txt" recipe_arch REGEX "^qt-arch ")
    expect_eq("${recipe_arch}" "qt-arch ${CMAKE_MATCH_2}" "the recipe's Qt arch against ci.yml")

elseif("${CASE}" STREQUAL "TheInstallerKeepsItsUpgradeCode")
    # Decision 6, ADR 0012: one upgrade code, in the recipe; the product code is new per build.
    file(READ "${RECIPE_DIR}/hitokoto.wxs" wxs)
    string(REGEX MATCHALL "UpgradeCode=\"[^\"]*\"" codes "${wxs}")
    list(LENGTH codes count)
    expect_eq("${count}" "1" "UpgradeCode attributes")
    # CMake's regular expressions have no {n}.
    string(REPEAT "[0-9A-F]" 4 hex4)
    string(REPEAT "${hex4}" 2 hex8)
    string(REPEAT "${hex4}" 3 hex12)
    if(NOT codes MATCHES "^UpgradeCode=\"${hex8}-${hex4}-${hex4}-${hex4}-${hex12}\"$")
        message(FATAL_ERROR "the upgrade code is not a fixed GUID in capitals: ${codes}")
    endif()
    expect_eq("${codes}" "UpgradeCode=\"68D14AF8-3060-418B-842B-8C36500B7803\""
              "the upgrade code (ADR 0012: changed only together with an Upgrade element for the old one)")
    expect_not_contains("${wxs}" "ProductCode=" "a fixed product code")
    expect_contains("${wxs}" "<MajorUpgrade" "the major upgrade")
    expect_contains("${wxs}" "DowngradeErrorMessage=" "an older version refused")

elseif("${CASE}" STREQUAL "TheInstallerOffersBothScopes")
    # Decision 6: per user or per machine, the dialog preselecting by the user's rights.
    file(READ "${RECIPE_DIR}/hitokoto.wxs" wxs)
    expect_contains("${wxs}" "Scope=\"perUserOrMachine\"" "the scope")
    expect_contains("${wxs}" "<Property Id=\"WixAppFolder\" Value=\"WixPerMachineFolder\""
                    "Everyone preselected")
    expect_contains("${wxs}" "Dialog=\"LicenseAgreementDlg\" Control=\"Next\" Event=\"NewDialog\" Value=\"InstallScopeDlg\""
                    "the scope dialog after the licence")
    expect_contains("${wxs}" "Condition=\"LicenseAccepted = &quot;1&quot; AND Privileged\""
                    "the scope dialog only for administrators")
    expect_contains("${wxs}" "Property=\"MSIINSTALLPERUSER\" Value=\"{}\"" "per machine chosen")
    expect_contains("${wxs}" "Property=\"MSIINSTALLPERUSER\" Value=\"1\"" "per user chosen")
    # Review (code) O1, Armin's test of the dialogs (2026-11-13): Windows deletes ALLUSERS=2 when
    # the UI starts with MSIINSTALLPERUSER=1; a choice of "Everyone" that sets only
    # MSIINSTALLPERUSER installed the files under Program Files, but the Start menu entry and the
    # Open with entry for the user alone. The dialog sets ALLUSERS with its choice.
    expect_contains("${wxs}" "Control=\"Next\" Property=\"ALLUSERS\" Value=\"1\"" "ALLUSERS for everyone")
    expect_contains("${wxs}" "Control=\"Next\" Property=\"ALLUSERS\" Value=\"{}\"" "ALLUSERS for the user")
    # The scope dialog offers no folder, so its texts do not promise one (hitokoto.wxl).
    file(READ "${RECIPE_DIR}/hitokoto.wxl" wxl)
    expect_contains("${wxl}" "Id=\"InstallScopeDlgPerMachineDescription\"" "the text for everyone")
    expect_not_contains("${wxl}" "folder" "a folder in the texts of the scope dialog")
    file(READ "${RECIPE_DIR}/build-package.ps1" build)
    expect_contains("${build}" "-loc (Join-Path $recipe 'hitokoto.wxl')" "the texts in the build")
    expect_contains("${wxs}" "<StandardDirectory Id=\"ProgramFiles64Folder\">" "the program folder")

elseif("${CASE}" STREQUAL "TheInstallerAsksForTheLicence")
    # Decision 6 (Armin, 2026-11-13): the GPL is accepted in the dialog or on the command line.
    file(READ "${RECIPE_DIR}/hitokoto.wxs" wxs)
    expect_contains("${wxs}"
        "<Launch Condition=\"Installed OR UILevel = 5 OR ACCEPT_LICENSE = &quot;yes&quot;\""
        "the launch condition")
    expect_contains("${wxs}" "<WixVariable Id=\"WixUILicenseRtf\"" "the licence text")
    expect_contains("${wxs}" "Condition=\"LicenseAccepted = &quot;1&quot;" "the checkbox")

elseif("${CASE}" STREQUAL "TheDebianReadmeIgnoresMissingFiles")
    # Review O5: SHA256SUMS lists both packages; a check of one download ignores the other.
    file(READ "${SOURCE_DIR}/packaging/debian/README.md" readme)
    expect_contains("${readme}" "sha256sum -c --ignore-missing SHA256SUMS" "the check")
    file(READ "${RECIPE_DIR}/README.md" readme)
    expect_contains("${readme}" "SHA256SUMS" "the Windows README's check")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
