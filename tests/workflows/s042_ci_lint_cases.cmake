# SPDX-License-Identifier: GPL-3.0-or-later
# S042: the lints of s042_ci_cases.cmake find each deviation. Every case writes a copy of ci.yml
# with one change and expects the named lint to fail with the given text; the unchanged copy passes
# them all.
#   -DCASE=<name>         the case below
#   -DWORKFLOW=<file>     .github/workflows/ci.yml
#   -DLINT=<file>         tests/workflows/s042_ci_cases.cmake
#   -DLIST_FILE=<file>    tests/docs-read-by-tests.txt
#   -DWORK_DIR=<dir>      a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOW LINT LIST_FILE WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(READ "${WORKFLOW}" original)

set(lints ThePullRequestTriggerHasNoLabelEvents ChangesReadsTheLabelsAtRunTime
          EveryOtherJobWaitsForChanges TheCiResultNeedsEveryJob ThePushFilterFollowsTheList)

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

# Runs the lint <lint> on <text>; expects success, or failure with <part> in the message.
function(expect_lint text lint part)
    file(WRITE "${WORK_DIR}/ci.yml" "${text}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DCASE=${lint} -DWORKFLOWS_DIR=${WORK_DIR}
                            -DLIST_FILE=${LIST_FILE} -P "${LINT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(REGEX REPLACE "[ \t\r\n]+" " " log "${out}${err}")
    if("${part}" STREQUAL "")
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "the lint ${lint} fails on the unchanged file:\n${out}${err}")
        endif()
    else()
        if(code EQUAL 0)
            message(FATAL_ERROR "the lint ${lint} passes, expected a failure naming '${part}'")
        endif()
        string(FIND "${log}" "${part}" pos)
        if(pos EQUAL -1)
            message(FATAL_ERROR "the lint ${lint} fails without naming '${part}':\n${out}${err}")
        endif()
    endif()
endfunction()

if(CASE STREQUAL "Unchanged")
    foreach(lint IN LISTS lints)
        expect_lint("${original}" ${lint} "")
    endforeach()

elseif(CASE STREQUAL "LabelEvents")
    changed("\n  pull_request:\n" "\n  pull_request:\n    types: [opened, synchronize, labeled]\n" text)
    expect_lint("${text}" ThePullRequestTriggerHasNoLabelEvents "types or label events")

elseif(CASE STREQUAL "LabelsFromTheEvent")
    changed("gh api \"repos/$REPO/pulls/$NUMBER\" --jq '.labels[].name'"
            "echo '\${{ toJSON(github.event.pull_request.labels.*.name) }}' | jq -r '.[]'" text)
    expect_lint("${text}" ChangesReadsTheLabelsAtRunTime "through the API")

elseif(CASE STREQUAL "ChangesWithWriteRights")
    changed("      contents: read\n      pull-requests: read\n    outputs:"
            "      contents: read\n      pull-requests: write\n    outputs:" text)
    expect_lint("${text}" ChangesReadsTheLabelsAtRunTime "other rights")

elseif(CASE STREQUAL "AJobWithoutChanges")
    changed("    name: Lint (clang-tidy)\n    needs: changes\n" "    name: Lint (clang-tidy)\n" text)
    expect_lint("${text}" EveryOtherJobWaitsForChanges "the job tidy does not need the job changes")

elseif(CASE STREQUAL "AJobWithoutTheCondition")
    changed("    needs: [changes, build-test]\n    if: needs.changes.outputs.docs-only != 'true'\n"
            "    needs: [changes, build-test]\n" text)
    expect_lint("${text}" EveryOtherJobWaitsForChanges "condition of the job bench")

elseif(CASE STREQUAL "CiResultWithoutAJob")
    changed("    needs: [changes, format, core-sanitizers, ui-sanitizers, fuzz, build-test, tidy, bench]\n    if: always()"
            "    needs: [changes, format, core-sanitizers, fuzz, build-test, tidy, bench]\n    if: always()" text)
    expect_lint("${text}" TheCiResultNeedsEveryJob "needs of the job ci-result")

elseif(CASE STREQUAL "CiResultNotAlways")
    changed("    if: always()\n    runs-on: ubuntu-24.04\n    permissions: {}"
            "    runs-on: ubuntu-24.04\n    permissions: {}" text)
    expect_lint("${text}" TheCiResultNeedsEveryJob "condition of the job ci-result")

elseif(CASE STREQUAL "PushFilterWithoutTheList")
    changed("      - \"!docs/**\"\n      - \"docs/theming.md\"\n" "      - \"!docs/**\"\n" text)  # S042-mention
    expect_lint("${text}" ThePushFilterFollowsTheList "paths of the push trigger")

elseif(CASE STREQUAL "PushFilterInAnotherOrder")
    changed("      - \"**\"\n      - \"!docs/**\"\n" "      - \"!docs/**\"\n      - \"**\"\n" text)  # S042-mention
    expect_lint("${text}" ThePushFilterFollowsTheList "paths of the push trigger")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
