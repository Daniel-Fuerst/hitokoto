# SPDX-License-Identifier: GPL-3.0-or-later
# S042, decision 5: the script of the job "CI result" in ci.yml, taken from the file and run with
# bash on the results GitHub would hand it. Needs bash (not run on Windows).
#   -DCASE=<name>            the case below
#   -DWORKFLOWS_DIR=<dir>    .github/workflows
#   -DBASH=<file>            bash
#   -DWORK_DIR=<dir>         a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOWS_DIR BASH WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/workflow_lines.cmake")

# The run: | block of the job ci-result, its ten-space indentation removed.
read_workflow(ci.yml ci)
jobs("${ci}" job)
if(NOT "ci-result" IN_LIST job_ids)
    message(FATAL_ERROR "ci.yml has no job 'ci-result' (S042, decision 5)")
endif()
set(in_run FALSE)
set(script "")
foreach(line IN LISTS job_ci-result)
    if(line MATCHES "^        run: \\|$")
        set(in_run TRUE)
    elseif(in_run AND line MATCHES "^          (.*)$")
        string(APPEND script "${CMAKE_MATCH_1}\n")
    elseif(in_run AND line STREQUAL "")
        string(APPEND script "\n")
    elseif(in_run)
        break()
    endif()
endforeach()
string(REPLACE "@SC@" ";" script "${script}")
string(REPLACE "@LB@" "[" script "${script}")
string(REPLACE "@RB@" "]" script "${script}")
if("${script}" STREQUAL "")
    message(FATAL_ERROR "the job ci-result has no run: | block")
endif()
file(WRITE "${WORK_DIR}/ci-result.sh" "${script}")

# Runs the script with the result of Changes, its output and the results of all needed jobs.
function(expect_result changes docs_only results expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "CHANGES=${changes}" "DOCS_ONLY=${docs_only}"
                            "RESULTS=${results}" "${BASH}" "${WORK_DIR}/ci-result.sh"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if("${expected}" STREQUAL "pass" AND NOT code EQUAL 0)
        message(FATAL_ERROR "CI result fails for '${changes}', '${docs_only}', '${results}':\n${out}${err}")
    elseif("${expected}" STREQUAL "fail" AND code EQUAL 0)
        message(FATAL_ERROR "CI result passes for '${changes}', '${docs_only}', '${results}':\n${out}${err}")
    endif()
endfunction()

# Eight jobs: changes and the seven others, in any order.
set(all_passed "success success success success success success success success")
set(docs_skipped "success skipped skipped skipped skipped skipped skipped skipped")

if("${CASE}" STREQUAL "EveryJobPassed")
    expect_result(success false "${all_passed}" pass)

elseif("${CASE}" STREQUAL "OnlyDocumentationSkippedTheRest")
    expect_result(success true "${docs_skipped}" pass)

elseif("${CASE}" STREQUAL "AFailedOrCancelledJobFails")
    expect_result(success false "success success failure success success success success success" fail)
    expect_result(success false "success success success cancelled success success success success" fail)
    expect_result(success true "success skipped skipped failure skipped skipped skipped skipped" fail)

elseif("${CASE}" STREQUAL "AWrongVersionDocFails")
    # Changes failed: every other job is skipped.
    expect_result(failure "" "failure skipped skipped skipped skipped skipped skipped skipped" fail)
    expect_result(cancelled "" "cancelled skipped skipped skipped skipped skipped skipped skipped" fail)

elseif("${CASE}" STREQUAL "ASkippedJobInAFullRunFails")
    expect_result(success false "success success success skipped success success success success" fail)

elseif("${CASE}" STREQUAL "ARunJobInADocumentationRunFails")
    expect_result(success true "success skipped success skipped skipped skipped skipped skipped" fail)

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
