# SPDX-License-Identifier: GPL-3.0-or-later
# S034: the decision logic of .github/workflows/packaging.yml (the AUR package follows every
# version). Run with cmake -P; tested by tests/packaging (S034_*).
#
#   cmake -DMODE=plan -DTAGS_FILE=<f> -DSRCINFO_FILE=<f> -DOUTPUT=<f> -P AurPackage.cmake
#       Compares the highest version tag with the pkgver of the AUR package and writes
#         action=publish|current|ahead
#         version=<highest version tag without the "v">
#         aur_version=<pkgver of the AUR package>
#         aur_sha256=<first sha256sums entry of the AUR package (the archive), may be empty>
#       one per line. Fails without a version tag, and without a .SRCINFO that has a pkgver of the
#       form MAJOR.MINOR.PATCH: the package exists, and the workflow never creates one.
#   cmake -DMODE=pkgbuild -DPKGBUILD_FILE=<f> -DVERSION=<X.Y.Z> -DSHA256=<hex> -DOUTPUT=<f>
#         -P AurPackage.cmake
#       Writes the PKGBUILD with pkgver=<X.Y.Z>, pkgrel=1 and <hex> as the first sha256sums entry
#       (the source archive); everything else stays as it is. Fails if the PKGBUILD does not have
#       exactly one pkgver line, one pkgrel line and one sha256sums array.
#
# TAGS_FILE holds the tags reachable from the commit (git tag --merged), one per line. Only tags of
# the exact form vMAJOR.MINOR.PATCH count (as in scripts/version); the highest by numeric
# comparison is published. A trailing carriage return is ignored in every input.
cmake_minimum_required(VERSION 3.25)

set(version_regex "(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)")

function(require)
    foreach(v IN LISTS ARGN)
        if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
            message(FATAL_ERROR "${v} not set")
        endif()
    endforeach()
endfunction()

# The lines of a file without carriage returns and surrounding white space, empty lines dropped.
function(read_lines file out)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} does not exist")
    endif()
    file(STRINGS "${file}" raw)
    set(lines "")
    foreach(line IN LISTS raw)
        string(REGEX REPLACE "\r$" "" line "${line}")
        string(STRIP "${line}" line)
        if(NOT "${line}" STREQUAL "")
            list(APPEND lines "${line}")
        endif()
    endforeach()
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# The highest tag of the form vMAJOR.MINOR.PATCH, without the "v", or "".
function(highest_version tags out)
    set(best "")
    foreach(tag IN LISTS tags)
        if(tag MATCHES "^v${version_regex}$")
            string(SUBSTRING "${tag}" 1 -1 version)
            if("${best}" STREQUAL "" OR version VERSION_GREATER best)
                set(best "${version}")
            endif()
        endif()
    endforeach()
    set(${out} "${best}" PARENT_SCOPE)
endfunction()

function(plan)
    require(TAGS_FILE SRCINFO_FILE OUTPUT)
    read_lines("${TAGS_FILE}" tags)
    highest_version("${tags}" version)
    if("${version}" STREQUAL "")
        message(FATAL_ERROR "no version tag vMAJOR.MINOR.PATCH is reachable; nothing to publish")
    endif()

    # pkgver and the first sha256sums entry of the pkgbase section (makepkg --printsrcinfo).
    set(aur_version "")
    set(aur_sha256 "")
    if(EXISTS "${SRCINFO_FILE}")
        read_lines("${SRCINFO_FILE}" srcinfo)
        foreach(line IN LISTS srcinfo)
            if(line MATCHES "^pkgname = ")
                break()
            elseif(line MATCHES "^pkgver = (.*)$" AND "${aur_version}" STREQUAL "")
                set(aur_version "${CMAKE_MATCH_1}")
            elseif(line MATCHES "^sha256sums = (.*)$" AND "${aur_sha256}" STREQUAL "")
                set(aur_sha256 "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endif()
    if("${aur_version}" STREQUAL "")
        message(FATAL_ERROR "the AUR repository has no .SRCINFO with a pkgver; the package hitokoto "
                            "is expected to exist, and this workflow creates no package")
    endif()
    if(NOT aur_version MATCHES "^${version_regex}$")
        message(FATAL_ERROR "the AUR package has pkgver ${aur_version}, not of the form "
                            "MAJOR.MINOR.PATCH; compare it by hand")
    endif()

    if(version VERSION_GREATER aur_version)
        set(action publish)
        message(STATUS "publish ${version} (the AUR is at ${aur_version})")
    elseif(version VERSION_EQUAL aur_version)
        set(action current)
        message(STATUS "the AUR is already at ${aur_version}; nothing to publish")
    else()
        set(action ahead)
        message(STATUS "the AUR is ahead: ${aur_version}, the highest version tag is v${version}; "
                       "nothing to publish")
    endif()
    file(WRITE "${OUTPUT}" "action=${action}\nversion=${version}\naur_version=${aur_version}\n"
                           "aur_sha256=${aur_sha256}\n")
endfunction()

function(fill_pkgbuild)
    require(PKGBUILD_FILE VERSION SHA256 OUTPUT)
    file(REMOVE "${OUTPUT}")
    if(NOT VERSION MATCHES "^${version_regex}$")
        message(FATAL_ERROR "VERSION ${VERSION} is not of the form MAJOR.MINOR.PATCH")
    endif()
    if(NOT SHA256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "SHA256 ${SHA256} is not a lower-case hexadecimal SHA-256")
    endif()
    string(LENGTH "${SHA256}" length)
    if(NOT length EQUAL 64)
        message(FATAL_ERROR "SHA256 ${SHA256} is not a lower-case hexadecimal SHA-256")
    endif()
    if(NOT EXISTS "${PKGBUILD_FILE}")
        message(FATAL_ERROR "${PKGBUILD_FILE} does not exist")
    endif()
    file(READ "${PKGBUILD_FILE}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    set(text "\n${text}")

    foreach(key IN ITEMS pkgver pkgrel)
        string(REGEX MATCHALL "\n${key}=[^\n]*" found "${text}")
        list(LENGTH found count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "${PKGBUILD_FILE} needs exactly one line ${key}=…, it has ${count}")
        endif()
    endforeach()
    string(REGEX REPLACE "\npkgver=[^\n]*" "\npkgver=${VERSION}" text "${text}")
    string(REGEX REPLACE "\npkgrel=[^\n]*" "\npkgrel=1" text "${text}")

    string(REGEX MATCHALL "\nsha256sums=\\([^)]*\\)" found "${text}")
    list(LENGTH found count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${PKGBUILD_FILE} needs exactly one array sha256sums=(…), it has ${count}")
    endif()
    string(REGEX MATCH "\nsha256sums=\\([^)]*\\)" sums "${text}")
    string(REGEX MATCH "'[^']*'" first "${sums}")
    if("${first}" STREQUAL "")
        message(FATAL_ERROR "the array sha256sums in ${PKGBUILD_FILE} has no entry")
    endif()
    string(FIND "${sums}" "${first}" at)
    string(LENGTH "${first}" first_length)
    string(SUBSTRING "${sums}" 0 ${at} head)
    math(EXPR rest_at "${at} + ${first_length}")
    string(SUBSTRING "${sums}" ${rest_at} -1 tail)
    string(REPLACE "${sums}" "${head}'${SHA256}'${tail}" text "${text}")

    string(SUBSTRING "${text}" 1 -1 text)
    file(WRITE "${OUTPUT}" "${text}")
endfunction()

require(MODE)
if("${MODE}" STREQUAL "plan")
    plan()
elseif("${MODE}" STREQUAL "pkgbuild")
    fill_pkgbuild()
else()
    message(FATAL_ERROR "unknown MODE '${MODE}' (plan or pkgbuild)")
endif()
