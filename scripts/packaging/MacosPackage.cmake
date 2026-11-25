# SPDX-License-Identifier: GPL-3.0-or-later
# S049: the decision logic of the macOS disk image (DMG) in .github/workflows/packaging.yml. Run
# with cmake -P; tested by tests/packaging (S049_*).
#
#   cmake -DMODE=plan -DRECIPE_FILE=<f> -DPLAN_FILE=<f> -DRELEASES_FILE=<f> -DOUTPUT=<f>
#         -P MacosPackage.cmake
#       Which versions get a disk image. PLAN_FILE is the output of the Debian plan
#       (scripts/packaging/DebianPackage.cmake, MODE=plan): its line "build" lists the versions
#       the release needs (in a dry run the highest version). RECIPE_FILE is
#       packaging/macos/recipe.txt; RELEASES_FILE the published releases, one per line: the tag,
#       then the names of its assets. Writes, one per line:
#         build=<JSON array>        the versions of "build" not skipped for macOS
#         skipped=<X.Y.Z ...>       versions of "build" skipped for macOS (released without DMG)
#         skipped_reasons=<text>    "X.Y.Z: <reason>" of those, separated by "; "
#         without_dmg=<X.Y.Z ...>   published releases without the DMG, not skipped (never changed)
#         qt=, qt_arch=, target=    the Qt of the recipe and the oldest macOS the DMG runs on
#   cmake -DMODE=files -DEXPECTED_FILE=<f> -DACTUAL_FILE=<f> -P MacosPackage.cmake
#       Compares the files of hitokoto.app (paths relative to the bundle, one per line; a framework
#       counts as one entry, its directory) with the list of the recipe (comments with "#"),
#       ignoring order. Prints the whole list of the bundle and fails with the missing and the
#       unexpected files if they differ.
#   cmake -DMODE=minos -DMACOS_TARGET=<X.Y> -DMINOS_FILE=<f> -P MacosPackage.cmake
#       MINOS_FILE holds "<minimum macOS> <file>" per Mach-O file of the bundle (otool -l,
#       LC_BUILD_VERSION). Fails if a file needs a newer macOS than MACOS_TARGET, the oldest
#       macOS the recipe promises (brief: macOS 13 or newer).
#   cmake -DMODE=links -DLINKS_FILE=<f> -P MacosPackage.cmake
#       LINKS_FILE holds "<file> <library or rpath>" per line (otool -L and LC_RPATH of every
#       Mach-O file of the bundle, paths relative to the bundle). Fails for any reference that is
#       neither inside the bundle (@executable_path, @loader_path, @rpath) nor part of macOS
#       (/usr/lib, /System/Library): the disk image must not need the Qt of the build machine.
#   cmake -DMODE=notice -DTEMPLATE=<f> -DQT_VERSION=<X.Y.Z> -DOUTPUT=<f> -P MacosPackage.cmake
#       The third-party notice from its template, with @QT_VERSION@ and @QT_SERIES@ filled in.
#
# A trailing carriage return is ignored in every input.
cmake_minimum_required(VERSION 3.25)

set(version_regex "(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)")

function(require)
    foreach(v IN LISTS ARGN)
        if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
            message(FATAL_ERROR "${v} not set")
        endif()
    endforeach()
endfunction()

# The lines of a file, carriage returns removed, stripped, empty lines and comments dropped.
# Semicolons would split CMake lists; they are replaced by commas (only reasons can hold them).
function(read_lines file out)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} does not exist")
    endif()
    file(READ "${file}" text)
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "\n" ";" raw "${text}")
    set(lines "")
    foreach(line IN LISTS raw)
        string(STRIP "${line}" line)
        if(NOT "${line}" STREQUAL "" AND NOT line MATCHES "^#")
            list(APPEND lines "${line}")
        endif()
    endforeach()
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# packaging/macos/recipe.txt: recipe_qt, recipe_qt_arch, recipe_target, recipe_skipped,
# recipe_reason_<version>.
function(read_recipe file)
    read_lines("${file}" lines)
    set(qt "")
    set(qt_arch "")
    set(target "")
    set(skipped "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^qt ([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(qt "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^qt-arch ([a-z0-9_]+)$")
            set(qt_arch "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^macos ([1-9][0-9]*\\.[0-9]+)$")
            set(target "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^skip ([^ ]+)( +(.*))?$")
            set(version "${CMAKE_MATCH_1}")
            set(reason "${CMAKE_MATCH_3}")
            if(NOT version MATCHES "^${version_regex}$")
                message(FATAL_ERROR "${file}: '${line}': ${version} is not MAJOR.MINOR.PATCH")
            endif()
            if("${reason}" STREQUAL "")
                message(FATAL_ERROR "${file}: '${line}': a skip without a reason")
            endif()
            list(APPEND skipped "${version}")
            set(recipe_reason_${version} "${reason}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${file}: unknown line '${line}' (qt X.Y.Z, qt-arch <arch>, "
                                "macos X.Y, skip X.Y.Z <reason>)")
        endif()
    endforeach()
    foreach(key IN ITEMS qt qt-arch macos)
        if(key STREQUAL "macos")
            set(var target)
        else()
            string(REPLACE "-" "_" var "${key}")
        endif()
        if("${${var}}" STREQUAL "")
            message(FATAL_ERROR "${file}: no line '${key} …'")
        endif()
    endforeach()
    set(recipe_qt "${qt}" PARENT_SCOPE)
    set(recipe_qt_arch "${qt_arch}" PARENT_SCOPE)
    set(recipe_target "${target}" PARENT_SCOPE)
    set(recipe_skipped "${skipped}" PARENT_SCOPE)
endfunction()

function(dmg_file version out)
    set(${out} "hitokoto-${version}-arm64.dmg" PARENT_SCOPE)
endfunction()

function(json_array out)
    set(items "")
    foreach(item IN LISTS ARGN)
        list(APPEND items "\"${item}\"")
    endforeach()
    list(JOIN items "," joined)
    set(${out} "[${joined}]" PARENT_SCOPE)
endfunction()

function(plan)
    require(RECIPE_FILE PLAN_FILE RELEASES_FILE OUTPUT)
    read_recipe("${RECIPE_FILE}")

    # The versions the release needs, from the Debian plan's line build=["X.Y.Z",…].
    read_lines("${PLAN_FILE}" debian)
    set(build_line "")
    foreach(line IN LISTS debian)
        if(line MATCHES "^build=(.*)$")
            set(build_line "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT build_line MATCHES "^\\[.*\\]$")
        message(FATAL_ERROR "${PLAN_FILE}: no line build=[…]")
    endif()
    string(REGEX MATCHALL "\"[^\"]*\"" quoted "${build_line}")
    set(build "")
    set(skipped "")
    set(reasons "")
    foreach(item IN LISTS quoted)
        string(REPLACE "\"" "" version "${item}")
        if(NOT version MATCHES "^${version_regex}$")
            message(FATAL_ERROR "${PLAN_FILE}: '${version}' is not MAJOR.MINOR.PATCH")
        endif()
        if(version IN_LIST recipe_skipped)
            list(APPEND skipped "${version}")
            list(APPEND reasons "${version}: ${recipe_reason_${version}}")
            message(STATUS "${version} gets no disk image: ${recipe_reason_${version}}")
        else()
            list(APPEND build "${version}")
        endif()
    endforeach()

    # Published releases without the DMG: reported, never changed (S036 decision 2).
    read_lines("${RELEASES_FILE}" releases)
    set(without_dmg "")
    foreach(line IN LISTS releases)
        string(REGEX REPLACE " +" ";" fields "${line}")
        list(POP_FRONT fields tag)
        if(NOT tag MATCHES "^v${version_regex}$")
            continue()
        endif()
        string(SUBSTRING "${tag}" 1 -1 version)
        dmg_file("${version}" dmg)
        if(NOT dmg IN_LIST fields AND NOT version IN_LIST recipe_skipped)
            list(APPEND without_dmg "${version}")
            message(STATUS "the release v${version} has no ${dmg}; a published release is never "
                           "changed")
        endif()
    endforeach()
    list(SORT without_dmg COMPARE NATURAL)

    json_array(build_json ${build})
    list(JOIN skipped " " skipped_text)
    list(JOIN reasons "; " reasons_text)
    list(JOIN without_dmg " " without_text)
    file(WRITE "${OUTPUT}" "build=${build_json}\nskipped=${skipped_text}\n"
                           "skipped_reasons=${reasons_text}\nwithout_dmg=${without_text}\n"
                           "qt=${recipe_qt}\nqt_arch=${recipe_qt_arch}\ntarget=${recipe_target}\n")
endfunction()

# A list of paths relative to the bundle, sorted; "./" in front is dropped.
function(bundle_paths file out)
    read_lines("${file}" lines)
    set(paths "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^\\./" "" line "${line}")
        list(APPEND paths "${line}")
    endforeach()
    list(SORT paths)
    set(${out} "${paths}" PARENT_SCOPE)
endfunction()

function(files)
    require(EXPECTED_FILE ACTUAL_FILE)
    bundle_paths("${EXPECTED_FILE}" expected)
    bundle_paths("${ACTUAL_FILE}" actual)
    list(LENGTH actual count)
    list(JOIN actual "\n  " listing)
    message("files of hitokoto.app (${count}):\n  ${listing}")
    set(problems "")
    foreach(path IN LISTS expected)
        if(NOT path IN_LIST actual)
            string(APPEND problems "\n  missing: ${path}")
        endif()
    endforeach()
    foreach(path IN LISTS actual)
        if(NOT path IN_LIST expected)
            string(APPEND problems "\n  unexpected: ${path}")
        endif()
    endforeach()
    if(NOT "${problems}" STREQUAL "")
        message(FATAL_ERROR "the files of hitokoto.app differ from ${EXPECTED_FILE}:${problems}")
    endif()
endfunction()

function(minos)
    require(MACOS_TARGET MINOS_FILE)
    if(NOT MACOS_TARGET MATCHES "^[1-9][0-9]*\\.[0-9]+$")
        message(FATAL_ERROR "MACOS_TARGET '${MACOS_TARGET}' is not a macOS version X.Y")
    endif()
    read_lines("${MINOS_FILE}" lines)
    list(LENGTH lines count)
    if(count EQUAL 0)
        message(FATAL_ERROR "${MINOS_FILE} lists no Mach-O file")
    endif()
    set(problems "")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^([0-9]+(\\.[0-9]+)*) +(.+)$")
            message(FATAL_ERROR "${MINOS_FILE}: '${line}' is not '<minimum macOS> <file>'")
        endif()
        if(CMAKE_MATCH_1 VERSION_GREATER MACOS_TARGET)
            string(APPEND problems "\n  ${CMAKE_MATCH_3} needs macOS ${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT "${problems}" STREQUAL "")
        message(FATAL_ERROR "files of hitokoto.app need a newer macOS than ${MACOS_TARGET}, the oldest "
                            "the recipe promises:${problems}")
    endif()
    message(STATUS "every Mach-O file (${count}) runs on macOS ${MACOS_TARGET}")
endfunction()

function(links)
    require(LINKS_FILE)
    read_lines("${LINKS_FILE}" lines)
    set(problems "")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^([^ ]+) +(.+)$")
            message(FATAL_ERROR "${LINKS_FILE}: '${line}' is not '<file> <library>'")
        endif()
        set(binary "${CMAKE_MATCH_1}")
        set(library "${CMAKE_MATCH_2}")
        if(NOT library MATCHES "^(@executable_path/|@loader_path/|@rpath/|/usr/lib/|/System/Library/)")
            string(APPEND problems "\n  ${binary} -> ${library}")
        endif()
        # A path may climb out of the bundle through "..": no higher than the bundle, from the
        # folder of the file (@loader_path) or of the program, Contents/MacOS (@executable_path).
        if(library MATCHES "^@(executable|loader)_path/")
            string(REGEX MATCHALL "\\.\\./" ups "${library}")
            list(LENGTH ups climbs)
            if(library MATCHES "^@executable_path/")
                set(depth 2)
            else()
                string(REGEX MATCHALL "/" parts "${binary}")
                list(LENGTH parts depth)
            endif()
            if(climbs GREATER depth)
                string(APPEND problems "\n  ${binary} -> ${library} (outside the bundle)")
            endif()
        endif()
    endforeach()
    if(NOT "${problems}" STREQUAL "")
        message(FATAL_ERROR "hitokoto.app refers to libraries outside the bundle and macOS:${problems}")
    endif()
endfunction()

function(notice)
    require(TEMPLATE QT_VERSION OUTPUT)
    if(NOT QT_VERSION MATCHES "^([0-9]+\\.[0-9]+)\\.[0-9]+$")
        message(FATAL_ERROR "QT_VERSION '${QT_VERSION}' is not MAJOR.MINOR.PATCH")
    endif()
    set(series "${CMAKE_MATCH_1}")
    file(READ "${TEMPLATE}" text)
    string(REPLACE "@QT_VERSION@" "${QT_VERSION}" text "${text}")
    string(REPLACE "@QT_SERIES@" "${series}" text "${text}")
    if(text MATCHES "@[A-Z_]+@")
        message(FATAL_ERROR "${TEMPLATE}: placeholder left: ${CMAKE_MATCH_0}")
    endif()
    file(WRITE "${OUTPUT}" "${text}")
endfunction()

if("${MODE}" STREQUAL "plan")
    plan()
elseif("${MODE}" STREQUAL "files")
    files()
elseif("${MODE}" STREQUAL "minos")
    minos()
elseif("${MODE}" STREQUAL "links")
    links()
elseif("${MODE}" STREQUAL "notice")
    notice()
else()
    message(FATAL_ERROR "MODE must be plan, files, minos, links or notice (got '${MODE}')")
endif()
