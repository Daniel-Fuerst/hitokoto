# SPDX-License-Identifier: GPL-3.0-or-later
# S042: lints over ci.yml (pull requests that change only documentation skip the build). Run with
# cmake -P; the file is read as text (no YAML parser). s042_ci_lint_cases.cmake shows each of them
# red on a broken copy.
#   -DCASE=<name>            the case below
#   -DWORKFLOWS_DIR=<dir>    .github/workflows (or the directory of a broken copy)
#   -DLIST_FILE=<file>       tests/docs-read-by-tests.txt
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOWS_DIR LIST_FILE)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/workflow_lines.cmake")

read_workflow(ci.yml ci)
jobs("${ci}" job)

# The jobs that skip when only documentation changed: all but "Changes", "CI result" and "Version".
set(gated ${job_ids})
list(REMOVE_ITEM gated changes ci-result version)

# The ids named by the needs of a job, as a list ("a" or "[a, b]").
function(job_needs job_lines out)
    job_value("${job_lines}" needs needs)
    if(needs MATCHES "^@LB@(.*)@RB@$")
        set(needs "${CMAKE_MATCH_1}")
    endif()
    string(REPLACE "," ";" needs "${needs}")
    list(TRANSFORM needs STRIP)
    set(${out} "${needs}" PARENT_SCOPE)
endfunction()

# The lines of a trigger under on: (four-space keys and deeper), without its key line.
function(trigger lines name out)
    section("${lines}" on body)
    set(in FALSE)
    set(result "")
    foreach(line IN LISTS body)
        if(line MATCHES "^  ([a-z_]+):")
            if("${CMAKE_MATCH_1}" STREQUAL "${name}")
                set(in TRUE)
                continue()
            endif()
            set(in FALSE)
        elseif(in)
            list(APPEND result "${line}")
        endif()
    endforeach()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

if("${CASE}" STREQUAL "ThePullRequestTriggerHasNoLabelEvents")
    # Decision 4: labeled and unlabeled fire for every label and would cancel the running CI.
    section("${ci}" on triggers)
    list(JOIN triggers "\n" text)
    if(NOT text MATCHES "(^|\n)  pull_request:")
        message(FATAL_ERROR "ci.yml does not run on pull requests")
    endif()
    trigger("${ci}" pull_request pr)
    list(JOIN pr "\n" pr_text)
    if(pr_text MATCHES "types" OR text MATCHES "labeled")
        message(FATAL_ERROR "the pull request trigger of ci.yml has types or label events:\n${text}")
    endif()

elseif("${CASE}" STREQUAL "ChangesReadsTheLabelsAtRunTime")
    # Decision 4: "Changes" reads the labels through the API when it runs, not from the event.
    if(NOT "changes" IN_LIST job_ids)
        message(FATAL_ERROR "ci.yml has no job 'changes' (S042, decision 4)")
    endif()
    job_value("${job_changes}" name name)
    expect_eq("${name}" "Changes" "name of the job changes")
    list(JOIN job_changes "\n" text)
    if(NOT text MATCHES "gh api \"repos/\\$REPO/pulls/\\$NUMBER\" --jq '\\.labels@LB@@RB@\\.name'")
        message(FATAL_ERROR "the job Changes does not read the labels through the API (gh api)")
    endif()
    if(text MATCHES "github\\.event\\.pull_request\\.labels")
        message(FATAL_ERROR "the job Changes reads the labels from the event")
    endif()
    if(NOT text MATCHES "-P scripts/ci/DocsOnly\\.cmake")
        message(FATAL_ERROR "the job Changes does not decide with scripts/ci/DocsOnly.cmake")
    endif()
    if(NOT text MATCHES "\n    permissions:\n      contents: read\n      pull-requests: read\n    [a-z]")
        message(FATAL_ERROR "the job Changes asks for other rights than contents: read and "
                            "pull-requests: read")
    endif()

elseif("${CASE}" STREQUAL "EveryOtherJobWaitsForChanges")
    # Decision 4: every job but "Changes", "CI result" and "Version" skips when only documentation
    # changed; a failed "Changes" (a wrong version:doc) skips them as well.
    if(NOT "changes" IN_LIST job_ids)
        message(FATAL_ERROR "ci.yml has no job 'changes' (S042, decision 4)")
    endif()
    foreach(id IN LISTS gated)
        job_needs("${job_${id}}" needs)
        if(NOT "changes" IN_LIST needs)
            message(FATAL_ERROR "the job ${id} does not need the job changes")
        endif()
        job_condition("${job_${id}}" condition)
        expect_eq("${condition}" "needs.changes.outputs.docs-only != 'true'" "condition of the job ${id}")
    endforeach()

elseif("${CASE}" STREQUAL "TheCiResultNeedsEveryJob")
    # Decision 5: "CI result" waits for every job of ci.yml but "Version" (which needs it) and
    # always runs.
    if(NOT "ci-result" IN_LIST job_ids)
        message(FATAL_ERROR "ci.yml has no job 'ci-result' (S042, decision 5)")
    endif()
    job_value("${job_ci-result}" name name)
    expect_eq("${name}" "CI result" "name of the job ci-result")
    job_needs("${job_ci-result}" needed)
    list(SORT needed)
    set(others ${job_ids})
    list(REMOVE_ITEM others ci-result version)
    list(SORT others)
    expect_eq("${needed}" "${others}" "needs of the job ci-result (every job but itself and version)")
    job_condition("${job_ci-result}" condition)
    expect_eq("${condition}" "always()" "condition of the job ci-result")
    job_value("${job_ci-result}" permissions permissions)
    expect_eq("${permissions}" "{}" "permissions of the job ci-result")

elseif("${CASE}" STREQUAL "ThePushFilterFollowsTheList")
    # Decision 6: a push to main that changes only documentation starts no run; the documents tests
    # read are taken in again.
    file(STRINGS "${LIST_FILE}" listed)
    set(expected "\"**\"" "\"!docs/**\"")  # S042-mention: the pattern of the push filter
    foreach(path IN LISTS listed)
        string(STRIP "${path}" path)
        if(NOT "${path}" STREQUAL "")
            list(APPEND expected "\"${path}\"")
        endif()
    endforeach()
    trigger("${ci}" push push)
    set(in_paths FALSE)
    set(patterns "")
    set(branches "")
    foreach(line IN LISTS push)
        if(line MATCHES "^    branches: (.*)$")
            set(branches "${CMAKE_MATCH_1}")
        endif()
        if(line MATCHES "^    paths:")
            set(in_paths TRUE)
        elseif(in_paths AND line MATCHES "^      - (.*)$")
            list(APPEND patterns "${CMAKE_MATCH_1}")
        elseif(in_paths AND NOT line MATCHES "^ *#")
            set(in_paths FALSE)
        endif()
    endforeach()
    expect_eq("${branches}" "@LB@main@RB@" "branches of the push trigger")
    expect_eq("${patterns}" "${expected}" "paths of the push trigger (every file, none under docs, the list)")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
