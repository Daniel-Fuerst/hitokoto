# SPDX-License-Identifier: GPL-3.0-or-later
# S037: the decision logic of the Windows installer (MSI) in .github/workflows/packaging.yml. Run
# with cmake -P; tested by tests/packaging (S037_*).
#
#   cmake -DMODE=plan -DRECIPE_FILE=<f> -DPLAN_FILE=<f> -DRELEASES_FILE=<f> -DOUTPUT=<f>
#         -P WindowsPackage.cmake
#       Which versions get an MSI. PLAN_FILE is the output of the Debian plan
#       (scripts/packaging/DebianPackage.cmake, MODE=plan): its line "build" lists the versions
#       the release needs (in a dry run the highest version). RECIPE_FILE is
#       packaging/windows/recipe.txt; RELEASES_FILE the published releases, one per line: the tag,
#       then the names of its assets. Writes, one per line:
#         build=<JSON array>        the versions of "build" not skipped for Windows
#         skipped=<X.Y.Z ...>       versions of "build" skipped for Windows (released without MSI)
#         skipped_reasons=<text>    "X.Y.Z: <reason>" of those, separated by "; "
#         without_msi=<X.Y.Z ...>   published releases without the MSI, not skipped (never changed)
#         qt=, qt_arch=, wix=       the tools of the recipe
#         ice=<ICEnn ...>           validation findings the recipe accepts
#       A version to build beyond the product version of Windows Installer (major and minor at
#       most 255, patch at most 65535) fails the plan, unless it is skipped.
#   cmake -DMODE=files -DEXPECTED_FILE=<f> -DACTUAL_FILE=<f> -P WindowsPackage.cmake
#       Compares the installed files (paths relative to the installation, one per line, "\" or "/")
#       with the list of the recipe (comments with "#"), ignoring case and order. Prints the whole
#       installed list and fails with the missing and the unexpected files if they differ.
#   cmake -DMODE=rtf -DINPUT=<f> -DOUTPUT=<f> -P WindowsPackage.cmake
#       The plain ASCII text INPUT (LICENSE) as RTF for the licence dialog, line by line.
#   cmake -DMODE=notice -DTEMPLATE=<f> -DQT_VERSION=<X.Y.Z> -DOUTPUT=<f> -P WindowsPackage.cmake
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

# packaging/windows/recipe.txt: recipe_qt, recipe_qt_arch, recipe_wix, recipe_skipped,
# recipe_reason_<version>, recipe_ice.
function(read_recipe file)
    read_lines("${file}" lines)
    set(qt "")
    set(qt_arch "")
    set(wix "")
    set(skipped "")
    set(ice "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^qt ([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(qt "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^qt-arch ([a-z0-9_]+)$")
            set(qt_arch "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^wix ([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(wix "${CMAKE_MATCH_1}")
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
        elseif(line MATCHES "^ice ([^ ]+)( +(.*))?$")
            set(id "${CMAKE_MATCH_1}")
            set(reason "${CMAKE_MATCH_3}")
            if(NOT id MATCHES "^ICE[0-9]+$")
                message(FATAL_ERROR "${file}: '${line}': ${id} is not an ICE (ICEnn)")
            endif()
            if("${reason}" STREQUAL "")
                message(FATAL_ERROR "${file}: '${line}': an accepted finding without a reason")
            endif()
            list(APPEND ice "${id}")
        else()
            message(FATAL_ERROR "${file}: unknown line '${line}' (qt X.Y.Z, qt-arch <arch>, "
                                "wix X.Y.Z, skip X.Y.Z <reason>, ice ICEnn <reason>)")
        endif()
    endforeach()
    foreach(key IN ITEMS qt qt-arch wix)
        string(REPLACE "-" "_" var "${key}")
        if("${${var}}" STREQUAL "")
            message(FATAL_ERROR "${file}: no line '${key} …'")
        endif()
    endforeach()
    set(recipe_qt "${qt}" PARENT_SCOPE)
    set(recipe_qt_arch "${qt_arch}" PARENT_SCOPE)
    set(recipe_wix "${wix}" PARENT_SCOPE)
    set(recipe_skipped "${skipped}" PARENT_SCOPE)
    set(recipe_ice "${ice}" PARENT_SCOPE)
endfunction()

function(msi_file version out)
    set(${out} "hitokoto-${version}-x64.msi" PARENT_SCOPE)
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
    set(wanted "")
    foreach(item IN LISTS quoted)
        string(REPLACE "\"" "" version "${item}")
        if(NOT version MATCHES "^${version_regex}$")
            message(FATAL_ERROR "${PLAN_FILE}: '${version}' is not MAJOR.MINOR.PATCH")
        endif()
        list(APPEND wanted "${version}")
    endforeach()

    set(build "")
    set(skipped "")
    set(reasons "")
    foreach(version IN LISTS wanted)
        if(version IN_LIST recipe_skipped)
            list(APPEND skipped "${version}")
            list(APPEND reasons "${version}: ${recipe_reason_${version}}")
            message(STATUS "${version} gets no MSI: ${recipe_reason_${version}}")
            continue()
        endif()
        # Windows Installer's ProductVersion: major.minor.build, at most 255.255.65535.
        string(REGEX MATCH "^${version_regex}$" ignored "${version}")
        if(CMAKE_MATCH_1 GREATER 255 OR CMAKE_MATCH_2 GREATER 255 OR CMAKE_MATCH_3 GREATER 65535)
            message(FATAL_ERROR "${version} cannot be the product version of an MSI (major and minor "
                                "at most 255, patch at most 65535); list it in the recipe as "
                                "'skip ${version} <reason>'")
        endif()
        list(APPEND build "${version}")
    endforeach()

    # Published releases without the MSI: reported, never changed (decision 2).
    read_lines("${RELEASES_FILE}" releases)
    set(without_msi "")
    foreach(line IN LISTS releases)
        string(REGEX REPLACE " +" ";" fields "${line}")
        list(POP_FRONT fields tag)
        if(NOT tag MATCHES "^v${version_regex}$")
            continue()
        endif()
        string(SUBSTRING "${tag}" 1 -1 version)
        msi_file("${version}" msi)
        if(NOT msi IN_LIST fields AND NOT version IN_LIST recipe_skipped)
            list(APPEND without_msi "${version}")
            message(STATUS "the release v${version} has no ${msi}; a published release is never "
                           "changed")
        endif()
    endforeach()
    list(SORT without_msi COMPARE NATURAL)

    json_array(build_json ${build})
    list(JOIN skipped " " skipped_text)
    list(JOIN reasons "; " reasons_text)
    list(JOIN without_msi " " without_text)
    list(JOIN recipe_ice " " ice_text)
    file(WRITE "${OUTPUT}" "build=${build_json}\nskipped=${skipped_text}\n"
                           "skipped_reasons=${reasons_text}\nwithout_msi=${without_text}\n"
                           "qt=${recipe_qt}\nqt_arch=${recipe_qt_arch}\nwix=${recipe_wix}\n"
                           "ice=${ice_text}\n")
endfunction()

# A list of paths, "\" as "/", lower case, sorted (Windows' file names do not depend on case).
function(normalized_paths file out shown)
    read_lines("${file}" lines)
    set(paths "")
    set(original "")
    foreach(line IN LISTS lines)
        string(REPLACE "\\" "/" line "${line}")
        string(REGEX REPLACE "^\\./" "" line "${line}")
        list(APPEND original "${line}")
        string(TOLOWER "${line}" lower)
        list(APPEND paths "${lower}")
    endforeach()
    list(SORT paths)
    list(SORT original CASE INSENSITIVE)
    set(${out} "${paths}" PARENT_SCOPE)
    set(${shown} "${original}" PARENT_SCOPE)
endfunction()

function(files)
    require(EXPECTED_FILE ACTUAL_FILE)
    normalized_paths("${EXPECTED_FILE}" expected expected_shown)
    normalized_paths("${ACTUAL_FILE}" actual actual_shown)
    list(LENGTH actual_shown count)
    list(JOIN actual_shown "\n  " listing)
    message("installed files (${count}):\n  ${listing}")
    set(problems "")
    foreach(path IN LISTS expected_shown)
        string(TOLOWER "${path}" lower)
        if(NOT lower IN_LIST actual)
            string(APPEND problems "\n  missing: ${path}")
        endif()
    endforeach()
    foreach(path IN LISTS actual_shown)
        string(TOLOWER "${path}" lower)
        if(NOT lower IN_LIST expected)
            string(APPEND problems "\n  unexpected: ${path}")
        endif()
    endforeach()
    if(NOT "${problems}" STREQUAL "")
        message(FATAL_ERROR "the installed files differ from ${EXPECTED_FILE}:${problems}")
    endif()
endfunction()

function(rtf)
    require(INPUT OUTPUT)
    file(READ "${INPUT}" text)
    if(text MATCHES "[^\t\n\r -~]")
        message(FATAL_ERROR "${INPUT} is not plain ASCII")
    endif()
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE "\\" "\\\\" text "${text}")
    string(REPLACE "{" "\\{" text "${text}")
    string(REPLACE "}" "\\}" text "${text}")
    string(REGEX REPLACE "\n$" "" text "${text}")
    string(REPLACE "\n" "\\par\n" text "${text}")
    file(WRITE "${OUTPUT}"
         "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern Consolas;}}\\f0\\fs16\n"
         "${text}\\par\n}\n")
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
elseif("${MODE}" STREQUAL "rtf")
    rtf()
elseif("${MODE}" STREQUAL "notice")
    notice()
else()
    message(FATAL_ERROR "MODE must be plan, files, rtf or notice (got '${MODE}')")
endif()
