# SPDX-License-Identifier: GPL-3.0-or-later
# S030 (ADR 0010): version tags from pull request labels. Run with cmake -P; used by
# .github/workflows/version.yml and tested by tests/version (S030_*).
#
#   cmake -DMODE=labels -DLABELS_FILE=<f> -P NextVersion.cmake
#       Succeeds if the labels hold exactly one version label (the check "Version label").
#   cmake -DMODE=base -DTAGS_FILE=<f> -DOUTPUT=<f> -P NextVersion.cmake
#       Writes the highest version tag of the list.
#   cmake [-DMODE=next] -DTAGS_FILE=<f> -DLABELS_FILE=<f> -DOUTPUT=<f> -P NextVersion.cmake
#       Writes the next version tag after the highest one for the one version label, or "none".
#   cmake -DMODE=plan -DTAGS_FILE=<f> -DCANDIDATES_FILE=<f> -DOUTPUT=<f> -P NextVersion.cmake
#       Writes the tags to set, one line "<commit> <tag> <pull request>" each, in order. Fails at
#       the first merge without exactly one version label, after writing the tags before it.
#
# Files hold one item per line; a trailing carriage return is ignored.
#   TAGS_FILE: the tags reachable from the merge (git tag --merged). Only tags of the exact form
#     vMAJOR.MINOR.PATCH count; the highest by numeric comparison is the base.
#   LABELS_FILE: the names of a pull request's labels.
#   CANDIDATES_FILE: the commits on main's first-parent line after the base tag, oldest first, as
#     "commit <sha>", followed by "merge" if the commit has two parents, "pr <number>" if a pull
#     request with base main has the commit as its merge commit, and one "label <name>" line per
#     label of that pull request. A commit with one parent and no pull request was pushed directly
#     and gets no version; a merge without a pull request stops the plan (code review O1).
cmake_minimum_required(VERSION 3.25)

set(version_labels version:major version:minor version:patch version:none)
list(JOIN version_labels ", " version_labels_text)

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

function(write_output text)
    if(NOT DEFINED OUTPUT OR "${OUTPUT}" STREQUAL "")
        message(FATAL_ERROR "OUTPUT not set")
    endif()
    file(WRITE "${OUTPUT}" "${text}")
endfunction()

# The one version label among the labels, or a message why there is none.
function(version_label labels out_label out_error)
    set(found "")
    foreach(label IN LISTS labels)
        if(label IN_LIST version_labels)
            list(APPEND found "${label}")
        endif()
    endforeach()
    list(LENGTH found count)
    if(count EQUAL 1)
        set(${out_label} "${found}" PARENT_SCOPE)
        set(${out_error} "" PARENT_SCOPE)
        return()
    endif()
    if(count EQUAL 0)
        set(has "none of them")
    else()
        list(JOIN found ", " has)
        set(has "${count}: ${has}")
    endif()
    set(${out_label} "" PARENT_SCOPE)
    set(${out_error} "needs exactly one of the labels ${version_labels_text}; it has ${has}" PARENT_SCOPE)
endfunction()

# The highest tag of the form vMAJOR.MINOR.PATCH (numbers without leading zeros), without the "v".
function(base_version tags out)
    set(best "")
    foreach(tag IN LISTS tags)
        if(tag MATCHES "^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
            string(SUBSTRING "${tag}" 1 -1 v)
            if("${best}" STREQUAL "" OR v VERSION_GREATER best)
                set(best "${v}")
            endif()
        endif()
    endforeach()
    if("${best}" STREQUAL "")
        message(FATAL_ERROR "no version tag (vMAJOR.MINOR.PATCH) is reachable; a version is never "
                            "computed from scratch (fetch the tags: fetch-depth 0)")
    endif()
    set(${out} "${best}" PARENT_SCOPE)
endfunction()

# The version after <version> for a version label; "none" for version:none.
function(next_version version label out)
    string(REPLACE "." ";" parts "${version}")
    list(GET parts 0 major)
    list(GET parts 1 minor)
    list(GET parts 2 patch)
    if("${label}" STREQUAL "version:major")
        math(EXPR major "${major} + 1")
        set(minor 0)
        set(patch 0)
    elseif("${label}" STREQUAL "version:minor")
        math(EXPR minor "${minor} + 1")
        set(patch 0)
    elseif("${label}" STREQUAL "version:patch")
        math(EXPR patch "${patch} + 1")
    else()
        set(${out} "none" PARENT_SCOPE)
        return()
    endif()
    set(${out} "${major}.${minor}.${patch}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED MODE OR "${MODE}" STREQUAL "")
    set(MODE next)
endif()

if("${MODE}" STREQUAL "labels")
    read_lines("${LABELS_FILE}" labels)
    version_label("${labels}" label error)
    if(NOT "${error}" STREQUAL "")
        message(FATAL_ERROR "A pull request into main ${error}. What each level means: ADR 0010.")
    endif()
    message(STATUS "Version label: ${label}")

elseif("${MODE}" STREQUAL "base")
    read_lines("${TAGS_FILE}" tags)
    base_version("${tags}" base)
    write_output("v${base}\n")

elseif("${MODE}" STREQUAL "next")
    read_lines("${TAGS_FILE}" tags)
    read_lines("${LABELS_FILE}" labels)
    base_version("${tags}" base)
    version_label("${labels}" label error)
    if(NOT "${error}" STREQUAL "")
        message(FATAL_ERROR "The pull request ${error}.")
    endif()
    next_version("${base}" "${label}" next)
    if("${next}" STREQUAL "none")
        write_output("none\n")
    else()
        write_output("v${next}\n")
    endif()

elseif("${MODE}" STREQUAL "plan")
    read_lines("${TAGS_FILE}" tags)
    read_lines("${CANDIDATES_FILE}" lines)
    base_version("${tags}" version)
    message(STATUS "Last version: v${version}")

    # One candidate at a time: "commit" starts the next one, the previous one is decided first.
    set(plan "")
    set(stop "")
    set(sha "")
    macro(decide_candidate)
        if(NOT "${sha}" STREQUAL "" AND "${stop}" STREQUAL "")
            if("${pr}" STREQUAL "" AND is_merge)
                string(CONCAT stop "The merge commit ${sha} is not confirmed as the merge of a pull "
                    "request into main (no pull request with base main has it as its merge commit). "
                    "Re-run this job; if it stays red, the commit was merged outside a pull request.")
            elseif("${pr}" STREQUAL "")
                message(STATUS "${sha}: pushed directly (no merge of a pull request), no version")
            else()
                version_label("${labels}" label error)
                if(NOT "${error}" STREQUAL "")
                    string(CONCAT stop "The merge ${sha} of pull request #${pr}: the pull request "
                        "${error}. Set its label and re-run this job; later merges are tagged after it.")
                else()
                    next_version("${version}" "${label}" next)
                    if("${next}" STREQUAL "none")
                        message(STATUS "${sha}: pull request #${pr} is ${label}, no version")
                    else()
                        set(version "${next}")
                        message(STATUS "${sha}: pull request #${pr} is ${label}, v${version}")
                        string(APPEND plan "${sha} v${version} ${pr}\n")
                    endif()
                endif()
            endif()
        endif()
    endmacro()
    foreach(line IN LISTS lines)
        if(line MATCHES "^commit ([0-9a-f]+)$")
            set(next_sha "${CMAKE_MATCH_1}")
            decide_candidate()
            set(sha "${next_sha}")
            set(is_merge FALSE)
            set(pr "")
            set(labels "")
        elseif("${sha}" STREQUAL "")
            message(FATAL_ERROR "${CANDIDATES_FILE} must start with a line 'commit <sha>': '${line}'")
        elseif("${line}" STREQUAL "merge")
            set(is_merge TRUE)
        elseif(line MATCHES "^pr ([0-9]+)$")
            set(pr "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^label (.+)$")
            list(APPEND labels "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "unexpected line in ${CANDIDATES_FILE}: '${line}'")
        endif()
    endforeach()
    decide_candidate()
    write_output("${plan}")
    if(NOT "${stop}" STREQUAL "")
        message(FATAL_ERROR "${stop}")
    endif()

else()
    message(FATAL_ERROR "unknown MODE '${MODE}' (labels, base, next or plan)")
endif()
