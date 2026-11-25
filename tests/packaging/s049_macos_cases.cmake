# SPDX-License-Identifier: GPL-3.0-or-later
# S049: cases for scripts/packaging/MacosPackage.cmake and for the recipe in packaging/macos, run
# with cmake -P on fixed inputs (no network, no macOS).
#   -DCASE=<name>          the case below
#   -DSCRIPT=<file>        scripts/packaging/MacosPackage.cmake
#   -DRECIPE_DIR=<dir>     packaging/macos
#   -DSOURCE_DIR=<dir>     the source tree (packaging/windows, .github/workflows, src/app)
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPT RECIPE_DIR SOURCE_DIR WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/expect.cmake")

# The recipe's settings with the given extra lines (skips).
function(write_recipe)
    write_lines("${WORK_DIR}/recipe.txt" "# test" "qt 6.12.0" "qt-arch clang_64" "macos 13.0"
                ${ARGN})
endfunction()

# The output of the Debian plan (scripts/packaging/DebianPackage.cmake) the macOS plan reads.
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
    foreach(key IN ITEMS build skipped skipped_reasons without_dmg qt qt_arch target)
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

# Runs a mode of the script with -D<arguments>; result in run_result, messages in run_messages.
function(run_mode mode)
    set(defines "")
    foreach(argument IN LISTS ARGN)
        list(APPEND defines "-D${argument}")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=${mode} ${defines} -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(run_result "${code}" PARENT_SCOPE)
    set(run_messages "${out}${err}" PARENT_SCOPE)
endfunction()

if("${CASE}" STREQUAL "TheVersionsToReleaseGetADiskImage")
    # Decision 2: the macOS job builds exactly the versions the release needs.
    write_recipe()
    write_debian_plan(release "0.5.2 0.5.3" [=[["0.5.2","0.5.3"]]=] 0.5.3)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.2","0.5.3"]]=] "build")
    expect_eq("${plan_skipped}" "" "skipped")

elseif("${CASE}" STREQUAL "ADryRunBuildsTheHighestVersion")
    # Decision 10: the Debian plan of a dry run builds the highest version; so does macOS.
    write_recipe()
    write_debian_plan(current "" [=[["0.5.1"]]=] 0.5.1)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.1"]]=] "build")

elseif("${CASE}" STREQUAL "NothingToBuildBuildsNothing")
    write_recipe()
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" "[]" "build")

elseif("${CASE}" STREQUAL "SkippedVersionsAreLeftOutAndNamed")
    # Decision 2: a version skipped for macOS is released without the DMG.
    write_recipe("skip 0.5.2 the DMG of 0.5.2 needs a fix in the tag")
    write_debian_plan(release "0.5.2 0.5.3" [=[["0.5.2","0.5.3"]]=] 0.5.3)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.3"]]=] "build")
    expect_eq("${plan_skipped}" "0.5.2" "skipped")
    expect_contains("${plan_skipped_reasons}" "0.5.2: the DMG of 0.5.2 needs a fix in the tag"
                    "reasons")

elseif("${CASE}" STREQUAL "AReleaseWithoutTheDiskImageIsReported")
    # Decision 2: a published release without the DMG is reported and never changed; one skipped
    # for macOS is not reported as missing it, and tags not of the exact form are ignored.
    write_recipe("skip 0.5.2 reason")
    write_debian_plan(current "" "[]" 0.5.3)
    write_lines("${WORK_DIR}/releases.txt"
                "v0.3.0-tulip hitokoto-0.1.0-arm64.dmg"
                "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS"
                "v0.5.2 hitokoto_0.5.2-1_amd64.deb SHA256SUMS"
                "v0.5.3 hitokoto_0.5.3-1_amd64.deb hitokoto-0.5.3-x64.msi hitokoto-0.5.3-arm64.dmg SHA256SUMS"
                "v0.5.4 hitokoto_0.5.4-1_amd64.deb hitokoto-0.5.4-x64.msi SHA256SUMS")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_without_dmg}" "0.5.1 0.5.4" "without_dmg")

elseif("${CASE}" STREQUAL "TheToolVersionsComeFromTheRecipe")
    # Decision 3: Qt, its arch and the oldest macOS.
    write_recipe()
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_qt}" "6.12.0" "qt")
    expect_eq("${plan_qt_arch}" "clang_64" "qt_arch")
    expect_eq("${plan_target}" "13.0" "target")

elseif("${CASE}" STREQUAL "BadRecipesFail")
    write_debian_plan(current "" "[]" 0.5.1)
    write_lines("${WORK_DIR}/releases.txt")
    write_lines("${WORK_DIR}/recipe.txt" "qt-arch clang_64" "macos 13.0")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "no line 'qt" "a recipe without qt")
    write_lines("${WORK_DIR}/recipe.txt" "qt 6.12.0" "qt-arch clang_64")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "no line 'macos" "a recipe without macos")
    write_recipe("skip v0.5.2 a tag, not a version")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "not MAJOR.MINOR.PATCH" "a bad skip")
    write_recipe("skip 0.5.2")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "without a reason" "a skip without reason")
    write_lines("${WORK_DIR}/recipe.txt" "qt 6.12.0" "qt-arch clang_64" "macos 13")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "unknown line" "a macOS version without minor")
    write_recipe("wix 7.0.1")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "unknown line" "an unknown line")
    write_recipe()
    file(REMOVE "${WORK_DIR}/debian-plan.txt")
    run_plan()
    expect_fails("${plan_result}" "${plan_messages}" "debian-plan.txt" "a missing Debian plan")

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    file(WRITE "${WORK_DIR}/recipe.txt"
         "qt 6.12.0\r\nqt-arch clang_64\r\nmacos 13.0\r\nskip 0.5.2 reason\r\n")
    file(WRITE "${WORK_DIR}/debian-plan.txt"
         "action=release\r\nversions=0.5.2 0.5.3\r\nbuild=[\"0.5.2\",\"0.5.3\"]\r\nlatest=0.5.3\r\n")
    file(WRITE "${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb\r\n")
    run_plan()
    expect_ok("${plan_result}" "${plan_messages}" "the plan")
    expect_eq("${plan_build}" [=[["0.5.3"]]=] "build")
    expect_eq("${plan_target}" "13.0" "target")
    expect_eq("${plan_without_dmg}" "0.5.1" "without_dmg")

elseif("${CASE}" STREQUAL "TheFileListMatches")
    # Decision 5: the files of the bundle, listed by the job in any order; a framework is one entry.
    write_lines("${WORK_DIR}/expected.txt" "# the files" "Contents/MacOS/hitokoto"
                "Contents/Frameworks/QtCore.framework" "Contents/PlugIns/platforms/libqcocoa.dylib"
                "")
    write_lines("${WORK_DIR}/actual.txt" "./Contents/PlugIns/platforms/libqcocoa.dylib"
                "Contents/Frameworks/QtCore.framework" "Contents/MacOS/hitokoto")
    run_mode(files EXPECTED_FILE=${WORK_DIR}/expected.txt ACTUAL_FILE=${WORK_DIR}/actual.txt)
    expect_ok("${run_result}" "${run_messages}" "the file check")

elseif("${CASE}" STREQUAL "AMissingOrExtraFileFails")
    write_lines("${WORK_DIR}/expected.txt" "Contents/MacOS/hitokoto"
                "Contents/PlugIns/platforms/libqcocoa.dylib")
    write_lines("${WORK_DIR}/actual.txt" "Contents/MacOS/hitokoto"
                "Contents/PlugIns/imageformats/libqjpeg.dylib")
    run_mode(files EXPECTED_FILE=${WORK_DIR}/expected.txt ACTUAL_FILE=${WORK_DIR}/actual.txt)
    expect_fails("${run_result}" "${run_messages}"
                 "missing: Contents/PlugIns/platforms/libqcocoa.dylib" "the file check")
    expect_contains("${run_messages}" "unexpected: Contents/PlugIns/imageformats/libqjpeg.dylib"
                    "the unexpected file")
    # The whole list is printed, so that a first dry run shows what macdeployqt delivered.
    expect_contains("${run_messages}" "files of hitokoto.app (2):" "the list")
    # macOS file names are compared as they are: a list must name them exactly.
    write_lines("${WORK_DIR}/actual.txt" "contents/macos/hitokoto"
                "Contents/PlugIns/platforms/libqcocoa.dylib")
    run_mode(files EXPECTED_FILE=${WORK_DIR}/expected.txt ACTUAL_FILE=${WORK_DIR}/actual.txt)
    expect_fails("${run_result}" "${run_messages}" "missing: Contents/MacOS/hitokoto"
                 "the file check with other case")

elseif("${CASE}" STREQUAL "AFileForANewerMacosFails")
    # Decision 4 (brief: macOS 13 or newer): no file of the bundle may need a newer macOS.
    write_lines("${WORK_DIR}/minos.txt" "13.0 Contents/MacOS/hitokoto"
                "12.0 Contents/Frameworks/QtCore.framework/Versions/A/QtCore"
                "13.0 Contents/PlugIns/platforms/libqcocoa.dylib")
    run_mode(minos MACOS_TARGET=13.0 MINOS_FILE=${WORK_DIR}/minos.txt)
    expect_ok("${run_result}" "${run_messages}" "the oldest macOS")
    write_lines("${WORK_DIR}/minos.txt" "13.0 Contents/MacOS/hitokoto"
                "14.0 Contents/Frameworks/QtCore.framework/Versions/A/QtCore"
                "13.0.1 Contents/PlugIns/platforms/libqcocoa.dylib")
    run_mode(minos MACOS_TARGET=13.0 MINOS_FILE=${WORK_DIR}/minos.txt)
    expect_fails("${run_result}" "${run_messages}"
                 "Contents/Frameworks/QtCore.framework/Versions/A/QtCore needs macOS 14.0"
                 "a framework for macOS 14")
    expect_contains("${run_messages}" "libqcocoa.dylib needs macOS 13.0.1" "a patch release too")
    write_lines("${WORK_DIR}/minos.txt")
    run_mode(minos MACOS_TARGET=13.0 MINOS_FILE=${WORK_DIR}/minos.txt)
    expect_fails("${run_result}" "${run_messages}" "lists no Mach-O file" "an empty list")
    write_lines("${WORK_DIR}/minos.txt" "13.0 Contents/MacOS/hitokoto")
    run_mode(minos MACOS_TARGET=13 MINOS_FILE=${WORK_DIR}/minos.txt)
    expect_fails("${run_result}" "${run_messages}" "is not a macOS version X.Y" "a bad target")

elseif("${CASE}" STREQUAL "ALibraryOutsideTheBundleFails")
    # Decision 5: the disk image needs nothing of the build machine (its Qt, Homebrew).
    write_lines("${WORK_DIR}/links.txt"
                "Contents/MacOS/hitokoto @rpath/QtWidgets.framework/Versions/A/QtWidgets"
                "Contents/MacOS/hitokoto @executable_path/../Frameworks"
                "Contents/MacOS/hitokoto /usr/lib/libc++.1.dylib"
                "Contents/MacOS/hitokoto /System/Library/Frameworks/AppKit.framework/Versions/C/AppKit"
                "Contents/PlugIns/platforms/libqcocoa.dylib @loader_path/../../Frameworks/QtGui.framework/Versions/A/QtGui")
    run_mode(links LINKS_FILE=${WORK_DIR}/links.txt)
    expect_ok("${run_result}" "${run_messages}" "the links into the bundle and macOS")
    write_lines("${WORK_DIR}/links.txt"
                "Contents/MacOS/hitokoto /Users/runner/work/Qt/6.12.0/macos/lib"
                "Contents/PlugIns/platforms/libqcocoa.dylib /opt/homebrew/lib/libfoo.dylib"
                "Contents/MacOS/hitokoto @executable_path/../../../Qt/lib"
                "Contents/PlugIns/styles/libqmacstyle.dylib libqmacstyle.dylib")
    run_mode(links LINKS_FILE=${WORK_DIR}/links.txt)
    expect_fails("${run_result}" "${run_messages}"
                 "Contents/MacOS/hitokoto -> /Users/runner/work/Qt/6.12.0/macos/lib"
                 "the runner's Qt")
    expect_contains("${run_messages}" "-> /opt/homebrew/lib/libfoo.dylib" "Homebrew")
    expect_contains("${run_messages}" "-> @executable_path/../../../Qt/lib (outside the bundle)"
                    "a path climbing out of the bundle")
    expect_contains("${run_messages}" "-> libqmacstyle.dylib" "a bare name")

elseif("${CASE}" STREQUAL "TheNoticeNamesTheQtVersion")
    # Decision 5: the third-party notice names the Qt version that is shipped.
    run_mode(notice TEMPLATE=${RECIPE_DIR}/third-party.txt.in QT_VERSION=6.12.0
             OUTPUT=${WORK_DIR}/THIRD-PARTY.txt)
    expect_ok("${run_result}" "${run_messages}" "the notice")
    file(READ "${WORK_DIR}/THIRD-PARTY.txt" notice)
    expect_contains("${notice}" "Qt 6.12.0" "the Qt version")
    expect_contains("${notice}" "GNU Lesser General Public License version 3" "Qt's licence")
    expect_contains("${notice}" "LGPL-3.0.txt" "the LGPL text")
    expect_contains("${notice}" "https://download.qt.io/archive/qt/6.12/6.12.0/" "Qt's source")
    expect_contains("${notice}" "STIX Two Math" "the math font")
    expect_not_contains("${notice}" "Microsoft" "no C++ runtime of Windows")
    expect_not_contains("${notice}" "@" "no placeholder left")

elseif("${CASE}" STREQUAL "TheRecipeBuildsWithTheCiQt")
    # Decision 4: the recipe's Qt is QT_VERSION of ci.yml and the Qt and arch of its macOS row.
    file(STRINGS "${SOURCE_DIR}/.github/workflows/ci.yml" env_line REGEX "^  QT_VERSION: ")
    if(NOT env_line MATCHES "^  QT_VERSION: \"([0-9]+\\.[0-9]+\\.[0-9]+)\"$")
        message(FATAL_ERROR "no QT_VERSION in ci.yml: '${env_line}'")
    endif()
    set(qt_version "${CMAKE_MATCH_1}")
    file(STRINGS "${SOURCE_DIR}/.github/workflows/ci.yml" rows REGEX "^ *- { name: macOS,")
    list(LENGTH rows count)
    expect_eq("${count}" "1" "rows for macOS in ci.yml")
    if(NOT rows MATCHES "qt: \"([0-9.]+)\", arch: ([a-z0-9_]+)")
        message(FATAL_ERROR "no qt and arch in the macOS row of ci.yml: ${rows}")
    endif()
    set(ci_arch "${CMAKE_MATCH_2}")
    expect_eq("${CMAKE_MATCH_1}" "${qt_version}" "the macOS row of ci.yml against QT_VERSION")
    file(STRINGS "${RECIPE_DIR}/recipe.txt" recipe_qt REGEX "^qt ")
    expect_eq("${recipe_qt}" "qt ${qt_version}" "the recipe's Qt against QT_VERSION")
    file(STRINGS "${RECIPE_DIR}/recipe.txt" recipe_arch REGEX "^qt-arch ")
    expect_eq("${recipe_arch}" "qt-arch ${ci_arch}" "the recipe's Qt arch against ci.yml")

elseif("${CASE}" STREQUAL "TheRecipeRunsOnMacos13")
    # Decision 4 (docs/stories/README.md decision 2: macOS 13 or newer). 13.3, not 13.0: the
    # floating-point std::to_chars of the tags' core (properties.cpp) is in Apple's libc++ only
    # from macOS 13.3 on; with 13.0 the first dry run on pull request 80 did not compile.
    file(STRINGS "${RECIPE_DIR}/recipe.txt" target REGEX "^macos ")
    expect_eq("${target}" "macos 13.3" "the oldest macOS of the recipe")
    file(READ "${RECIPE_DIR}/build-package.sh" build)
    expect_contains("${build}" "-DCMAKE_OSX_DEPLOYMENT_TARGET=\"$target\"" "the deployment target")
    expect_contains("${build}" "plutil -replace LSMinimumSystemVersion -string \"$target\""
                    "LSMinimumSystemVersion")
    expect_contains("${build}" "-DMODE=minos" "the check of every Mach-O file")

elseif("${CASE}" STREQUAL "TheDiskImageIsForAppleSiliconAlone")
    # Decision 4 (Daniel, 2026-11-25): arm64 only; Qt's universal files are thinned.
    file(READ "${RECIPE_DIR}/build-package.sh" build)
    expect_contains("${build}" "-DCMAKE_OSX_ARCHITECTURES=arm64" "the architecture of the build")
    expect_contains("${build}" "lipo \"$f\" -thin arm64" "the thinning of Qt's files")
    expect_contains("${build}" "out/hitokoto-$version-arm64.dmg" "the name of the DMG")
    expect_not_contains("${build}" "x86_64;" "a second architecture")

elseif("${CASE}" STREQUAL "TheBundleIsSignedAdHoc")
    # Decision 6 (Daniel, 2026-11-25): signed ad hoc, not notarised, no secret.
    file(READ "${RECIPE_DIR}/build-package.sh" build)
    expect_contains("${build}" "codesign --force --sign - --timestamp=none \"$app\"" "the signature")
    expect_contains("${build}" "codesign --verify --deep --strict" "its verification")
    expect_not_contains("${build}" "notarytool" "a notarisation")
    expect_not_contains("${build}" "Developer ID" "a Developer ID")
    file(READ "${RECIPE_DIR}/install-check.sh" check)
    expect_contains("${check}" "Signature=adhoc" "the check of the signature")
    file(READ "${RECIPE_DIR}/README.md" readme)
    expect_contains("${readme}" "com.apple.quarantine" "the README on Gatekeeper")

elseif("${CASE}" STREQUAL "TheDiskImageShipsNoImageFormatPlugins")
    # Decision 5, as for the MSI (ADR 0013): hitokoto decodes no image file; the icon engine stays.
    file(STRINGS "${RECIPE_DIR}/files.txt" files REGEX "^[^#]")
    foreach(f IN LISTS files)
        if(f MATCHES "^Contents/PlugIns/imageformats/")
            message(FATAL_ERROR "files.txt lists an image format plugin: ${f}")
        endif()
    endforeach()
    if(NOT "Contents/PlugIns/iconengines/libqsvgicon.dylib" IN_LIST files)
        message(FATAL_ERROR "files.txt does not list the icon engine libqsvgicon.dylib")
    endif()
    file(READ "${RECIPE_DIR}/build-package.sh" build)
    expect_contains("${build}" "rm -rf \"$app/Contents/PlugIns/imageformats\""
                    "the removal of the image format plugins")

elseif("${CASE}" STREQUAL "TheBundleCarriesTheLicences")
    # Decision 5: the licences inside the bundle, with the program wherever it is copied.
    file(STRINGS "${RECIPE_DIR}/files.txt" files REGEX "^[^#]")
    foreach(f IN ITEMS LICENSE.txt LGPL-3.0.txt THIRD-PARTY.txt)
        if(NOT "Contents/Resources/${f}" IN_LIST files)
            message(FATAL_ERROR "files.txt does not list Contents/Resources/${f}")
        endif()
    endforeach()
    if(NOT EXISTS "${SOURCE_DIR}/packaging/windows/LGPL-3.0.txt")
        message(FATAL_ERROR "the LGPL text the build copies is missing: packaging/windows/LGPL-3.0.txt")
    endif()
    file(READ "${RECIPE_DIR}/build-package.sh" build)
    expect_contains("${build}" "cp \"$recipe/../windows/LGPL-3.0.txt\"" "the LGPL text")

elseif("${CASE}" STREQUAL "TheBundleIdentityStays")
    # Decision 6: macOS keeps the settings and the permissions of an app by its identifier; it
    # stays what the program's CMakeLists.txt gave every build so far.
    file(READ "${SOURCE_DIR}/src/app/CMakeLists.txt" app)
    expect_contains("${app}" "MACOSX_BUNDLE_GUI_IDENTIFIER dev.hitokoto.editor" "the identifier")
    expect_contains("${app}" "MACOSX_BUNDLE_SHORT_VERSION_STRING \${HITOKOTO_VERSION_CORE}"
                    "the version in Info.plist")

elseif("${CASE}" STREQUAL "TheScriptsRunWithTheBashOfMacos")
    # macOS brings bash 3.2: no mapfile, no associative arrays, no ${x,,}.
    foreach(name IN ITEMS build-package.sh install-check.sh)
        file(READ "${RECIPE_DIR}/${name}" text)
        foreach(part IN ITEMS "mapfile" "readarray" "declare -A" ",,}" "^^}")
            expect_not_contains("${text}" "${part}" "${name}")
        endforeach()
        expect_contains("${text}" "set -euo pipefail" "${name}: strict mode")
    endforeach()

elseif("${CASE}" STREQUAL "TheReadmeChecksTheDownload")
    # SHA256SUMS lists every package of the release; a check of one download ignores the others.
    file(READ "${RECIPE_DIR}/README.md" readme)
    expect_contains("${readme}" "shasum -a 256 -c --ignore-missing SHA256SUMS" "the check")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
