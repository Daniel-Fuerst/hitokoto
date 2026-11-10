# SPDX-License-Identifier: GPL-3.0-or-later
# S032 (code review O2): the lint s032_workflow_lint.cmake finds each deviation. Every case writes
# a copy of version.yml with one change and expects the lint to fail and name the deviation; the
# unchanged copy must pass.
#   -DCASE=<name>         the case below
#   -DWORKFLOW=<file>     .github/workflows/version.yml
#   -DLINT=<file>         tests/version/s032_workflow_lint.cmake
#   -DWORK_DIR=<dir>      a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOW LINT WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(READ "${WORKFLOW}" original)

# Replaces <old> by <new> in the original exactly once (fails if <old> is not there once).
function(changed old new out)
    string(FIND "${original}" "${old}" first)
    string(FIND "${original}" "${old}" last REVERSE)
    if(first EQUAL -1 OR NOT first EQUAL last)
        message(FATAL_ERROR "the case needs '${old}' exactly once in ${WORKFLOW}")
    endif()
    string(REPLACE "${old}" "${new}" text "${original}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Runs the lint on <text>; expects success, or failure with <part> in the message.
function(expect_lint text part)
    file(WRITE "${WORK_DIR}/version.yml" "${text}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DWORKFLOW=${WORK_DIR}/version.yml -P "${LINT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(REGEX REPLACE "[ \t\r\n]+" " " log "${out}${err}")
    if("${part}" STREQUAL "")
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "the lint fails on the unchanged file:\n${out}${err}")
        endif()
    else()
        if(code EQUAL 0)
            message(FATAL_ERROR "the lint passes, expected a failure naming '${part}'")
        endif()
        string(FIND "${log}" "${part}" pos)
        if(pos EQUAL -1)
            message(FATAL_ERROR "the lint fails without naming '${part}':\n${out}${err}")
        endif()
    endif()
endfunction()

if(CASE STREQUAL "Unchanged")
    expect_lint("${original}" "")

elseif(CASE STREQUAL "CheckoutWithoutMain")
    changed("          ref: main\n          sparse-checkout:" "          sparse-checkout:" text)
    expect_lint("${text}" "checks out main")

elseif(CASE STREQUAL "CheckoutOfAnotherRef")
    changed("          ref: main\n          sparse-checkout:"
            "          ref: \${{ github.event.pull_request.head.sha }}\n          sparse-checkout:" text)
    expect_lint("${text}" "checks out main")

elseif(CASE STREQUAL "SecondCheckout")
    changed("      - name: Check the version label\n"
            "      - uses: actions/checkout@v7\n      - name: Check the version label\n" text)
    expect_lint("${text}" "one checkout")

elseif(CASE STREQUAL "StoredCredentials")
    changed("          persist-credentials: false\n      - name: Check the version label"
            "      - name: Check the version label" text)
    expect_lint("${text}" "persist-credentials: false")

elseif(CASE STREQUAL "NoScript")
    changed("-DMODE=labels" "-DMODE=next" text)
    expect_lint("${text}" "NextVersion.cmake")

elseif(CASE STREQUAL "PullRequestTarget")
    changed("on:\n  pull_request:\n" "on:\n  pull_request_target:\n" text)
    expect_lint("${text}" "pull_request_target")

elseif(CASE STREQUAL "NoLabelJob")
    changed("\n  label:\n" "\n  labels:\n" text)
    expect_lint("${text}" "job label")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
