# SPDX-License-Identifier: GPL-3.0-or-later
# S032 (code review O2): guards .github/workflows/version.yml against an accidental change that
# would let the check "Version label" run a pull request's own copy of NextVersion.cmake again.
#   cmake -DWORKFLOW=<version.yml> -P s032_workflow_lint.cmake
# Fails, naming every deviation, unless the file has no pull_request_target and its job "label"
# checks out main once, without stored credentials, and runs NextVersion.cmake in the mode labels.
# A deliberate change of the versioning adapts this test with it (story S032, decision 2).
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED WORKFLOW OR NOT EXISTS "${WORKFLOW}")
    message(FATAL_ERROR "WORKFLOW not set or missing: '${WORKFLOW}'")
endif()
file(STRINGS "${WORKFLOW}" lines)

set(problems "")
macro(problem text)
    list(APPEND problems "${text}")
endmacro()

# The job "label": from the line "  label:" to the next job (a line indented by two spaces).
set(in_label FALSE)
set(found_label FALSE)
set(checkouts 0)
set(ref_main FALSE)
set(no_credentials FALSE)
set(runs_script FALSE)
foreach(line IN LISTS lines)
    string(REGEX REPLACE "\r$" "" line "${line}")
    if(line MATCHES "^[^#]*pull_request_target")  # comments may name it
        problem("the workflow uses pull_request_target")
    endif()
    if(line MATCHES "^  [^ #][^:]*:")
        if(line MATCHES "^  label: *$")
            set(in_label TRUE)
            set(found_label TRUE)
        else()
            set(in_label FALSE)
        endif()
        continue()
    endif()
    if(NOT in_label)
        continue()
    endif()
    if(line MATCHES "uses: *actions/checkout@")
        math(EXPR checkouts "${checkouts} + 1")
    elseif(line MATCHES "^ +ref: *")
        if(line MATCHES "^ +ref: *main *$")
            set(ref_main TRUE)
        endif()
    elseif(line MATCHES "^ +persist-credentials: *false *$")
        set(no_credentials TRUE)
    elseif(line MATCHES "-DMODE=labels" AND line MATCHES "-P +scripts/version/NextVersion\\.cmake")
        set(runs_script TRUE)
    endif()
endforeach()

if(NOT found_label)
    problem("the workflow has no job label")
else()
    if(NOT checkouts EQUAL 1)
        problem("the job label has ${checkouts} checkouts, expected one checkout")
    endif()
    if(NOT ref_main)
        problem("the job label no longer checks out main (ref: main)")
    endif()
    if(NOT no_credentials)
        problem("the job label checks out without persist-credentials: false")
    endif()
    if(NOT runs_script)
        problem("the job label does not run scripts/version/NextVersion.cmake in the mode labels")
    endif()
endif()

if(problems)
    list(REMOVE_DUPLICATES problems)
    list(JOIN problems "\n  " text)
    message(FATAL_ERROR "S032: ${WORKFLOW}:\n  ${text}\n"
        "The check Version label must run NextVersion.cmake from main (story S032). If the change "
        "is deliberate, adapt tests/version/s032_workflow_lint.cmake with it.")
endif()
message(STATUS "S032: the job label checks out main and runs NextVersion.cmake from it")
