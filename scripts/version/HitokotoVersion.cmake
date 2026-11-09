# SPDX-License-Identifier: GPL-3.0-or-later
# S030 (ADR 0010): the version of a build, from the version tag (vMAJOR.MINOR.PATCH) the tag job
# puts on every merge into main.
#
# Included by the top-level CMakeLists.txt before project(): sets HITOKOTO_VERSION_FULL (for
# QApplication::applicationVersion and CPack) and HITOKOTO_VERSION_CORE (MAJOR.MINOR.PATCH, for
# project() and the macOS bundle).
#   - On a tagged commit: 0.4.0; three commits later: 0.4.0+3.gabc1234; with uncommitted changes
#     ".dirty" appended (0.4.0+dirty on the tagged commit), as SemVer build metadata.
#   - -DHITOKOTO_VERSION=<version> wins over git (source archives, package builds).
#   - Without git, without .git in the source directory or without a reachable version tag (a
#     shallow checkout): 0.0.0+unknown and a warning.
# The version is that of the last configure; a build that does not reconfigure keeps it.
# Included in script mode (cmake -P), it only defines the functions (tests/version, S030_*).

# MAJOR.MINOR.PATCH and the build metadata from the output of the git describe call below.
# Sets both to "" if the output is not of that form.
function(hitokoto_version_from_describe describe full_var core_var)
    set(full "")
    set(core "")
    if(describe MATCHES "^v((0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*))(-([0-9]+)-g([0-9a-f]+))?(-dirty)?$")
        set(core "${CMAKE_MATCH_1}")
        set(meta "")
        if(NOT "${CMAKE_MATCH_5}" STREQUAL "")
            set(meta "${CMAKE_MATCH_6}.g${CMAKE_MATCH_7}")
        endif()
        if(NOT "${CMAKE_MATCH_8}" STREQUAL "")
            if("${meta}" STREQUAL "")
                set(meta "dirty")
            else()
                string(APPEND meta ".dirty")
            endif()
        endif()
        set(full "${core}")
        if(NOT "${meta}" STREQUAL "")
            string(APPEND full "+${meta}")
        endif()
    endif()
    set(${full_var} "${full}" PARENT_SCOPE)
    set(${core_var} "${core}" PARENT_SCOPE)
endfunction()

# A version given by hand: MAJOR.MINOR.PATCH, optionally followed by -pre-release and +build
# metadata (SemVer). Sets both to "" if the value is not of that form.
function(hitokoto_version_from_option value full_var core_var)
    set(full "")
    set(core "")
    set(id "[0-9A-Za-z-]+")
    if(value MATCHES "^((0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*))(-${id}(\\.${id})*)?(\\+${id}(\\.${id})*)?$")
        set(full "${value}")
        set(core "${CMAKE_MATCH_1}")
    endif()
    set(${full_var} "${full}" PARENT_SCOPE)
    set(${core_var} "${core}" PARENT_SCOPE)
endfunction()

# hitokoto_determine_version(SOURCE_DIR <dir> [OPTION <version>] [NO_GIT]
#                            FULL <var> CORE <var> WARNING <var>)
# The version of the source tree in <dir>. OPTION is the value of -DHITOKOTO_VERSION (wins if not
# empty; an invalid value is an error). NO_GIT acts as if git were not installed (tests). WARNING
# is set to the reason for 0.0.0+unknown, otherwise to "".
function(hitokoto_determine_version)
    cmake_parse_arguments(PARSE_ARGV 0 a "NO_GIT" "SOURCE_DIR;OPTION;FULL;CORE;WARNING" "")
    set(warning "")
    if(NOT "${a_OPTION}" STREQUAL "")
        hitokoto_version_from_option("${a_OPTION}" full core)
        if("${full}" STREQUAL "")
            message(FATAL_ERROR "HITOKOTO_VERSION='${a_OPTION}' is not a version of the form "
                                "MAJOR.MINOR.PATCH (optionally with -pre-release and +build metadata)")
        endif()
    else()
        set(full "")
        set(git "")
        if(NOT a_NO_GIT)
            find_package(Git QUIET)
            if(GIT_FOUND)
                set(git "${GIT_EXECUTABLE}")
            endif()
        endif()
        if("${git}" STREQUAL "")
            set(warning "git not found")
        elseif(NOT EXISTS "${a_SOURCE_DIR}/.git")
            # Not a git checkout (a source archive), even if it lies inside another repository.
            set(warning "${a_SOURCE_DIR} is not a git checkout")
        else()
            # Only tags of the exact form vMAJOR.MINOR.PATCH: the glob of --match lets through
            # tags such as v0.3.0-tulip, which --exclude drops (anything but digits and dots).
            execute_process(
                COMMAND "${git}" -C "${a_SOURCE_DIR}" describe --tags --dirty --abbrev=7
                        --match "v[0-9]*.[0-9]*.[0-9]*" --exclude "v*[!0-9.]*"
                RESULT_VARIABLE code OUTPUT_VARIABLE describe ERROR_VARIABLE describe_error
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
            if(code EQUAL 0)
                hitokoto_version_from_describe("${describe}" full core)
                if("${full}" STREQUAL "")
                    set(warning "git describe gave '${describe}', which is not a version")
                endif()
            else()
                string(CONCAT warning "no version tag vMAJOR.MINOR.PATCH is reachable (a shallow "
                                     "checkout without tags?)")
            endif()
        endif()
        if("${full}" STREQUAL "")
            set(full "0.0.0+unknown")
            set(core "0.0.0")
            string(APPEND warning ". The version is ${full}. Builds without git take the version "
                                  "from -DHITOKOTO_VERSION=<MAJOR.MINOR.PATCH>.")
        endif()
    endif()
    set(${a_FULL} "${full}" PARENT_SCOPE)
    set(${a_CORE} "${core}" PARENT_SCOPE)
    set(${a_WARNING} "${warning}" PARENT_SCOPE)
endfunction()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(HITOKOTO_VERSION "" CACHE STRING
        "Version of a build without git (MAJOR.MINOR.PATCH); empty: from the version tag (git describe)")
    cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _hitokoto_scripts_dir)
    cmake_path(GET _hitokoto_scripts_dir PARENT_PATH _hitokoto_source_dir)
    hitokoto_determine_version(SOURCE_DIR "${_hitokoto_source_dir}" OPTION "${HITOKOTO_VERSION}"
        FULL HITOKOTO_VERSION_FULL CORE HITOKOTO_VERSION_CORE WARNING _hitokoto_version_warning)
    if(NOT "${_hitokoto_version_warning}" STREQUAL "")
        message(WARNING "hitokoto version: ${_hitokoto_version_warning}")
    endif()
    unset(_hitokoto_version_warning)
    unset(_hitokoto_scripts_dir)
    unset(_hitokoto_source_dir)
    message(STATUS "hitokoto version ${HITOKOTO_VERSION_FULL}")
endif()
