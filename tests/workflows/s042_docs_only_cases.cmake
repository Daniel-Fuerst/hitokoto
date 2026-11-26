# SPDX-License-Identifier: GPL-3.0-or-later
# S042: cases for scripts/ci/DocsOnly.cmake, the decision of the job "Changes" in ci.yml, run with
# cmake -P on fixed inputs. The paths under docs/ below are changed files of a pull request, not
# files these cases read; their lines carry the marker for the lint of S042.
#   -DCASE=<name>          the case below
#   -DSCRIPT=<file>        scripts/ci/DocsOnly.cmake
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPT WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/expect.cmake")

set(theming "docs/theming.md")  # S042-mention: the list of these cases, not a file read
set(story "docs/stories/040-table-selection.md")  # S042-mention: a changed path
set(review "docs/reviews/040-table-selection-spec.md")  # S042-mention: a changed path

# Runs DocsOnly.cmake with LABELS, CHANGED and LIST (lists, one item per line in their files).
# Sets <prefix>_CODE, <prefix>_OUTPUT (the file's content, stripped) and <prefix>_LOG.
function(run_docs_only prefix)
    cmake_parse_arguments(PARSE_ARGV 1 a "" "" "LABELS;CHANGED;LIST")
    write_lines("${WORK_DIR}/labels.txt" ${a_LABELS})
    write_lines("${WORK_DIR}/changed.txt" ${a_CHANGED})
    write_lines("${WORK_DIR}/list.txt" ${a_LIST})
    file(REMOVE "${WORK_DIR}/output.txt")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DLABELS_FILE=${WORK_DIR}/labels.txt
                            -DCHANGED_FILE=${WORK_DIR}/changed.txt -DLIST_FILE=${WORK_DIR}/list.txt
                            -DOUTPUT=${WORK_DIR}/output.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(output "")
    if(EXISTS "${WORK_DIR}/output.txt")
        file(READ "${WORK_DIR}/output.txt" output)
        string(STRIP "${output}" output)
    endif()
    set(${prefix}_CODE "${code}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${output}" PARENT_SCOPE)
    set(${prefix}_LOG "${out}\n${err}" PARENT_SCOPE)
endfunction()

if("${CASE}" STREQUAL "OnlyDocumentationSkipsTheBuild")
    # Scenario: a documentation pull request is merged without the build.
    run_docs_only(r LABELS version:doc CHANGED ${story} ${review} LIST ${theming})
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "true" "documentation only")

elseif("${CASE}" STREQUAL "WithoutTheLabelEveryJobRuns")
    # Scenario: without the label nothing changes, also when only documentation changed.
    run_docs_only(r LABELS version:none CHANGED ${story} LIST ${theming})
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "false" "documentation only without version:doc")
    run_docs_only(r LABELS bench-accepted CHANGED src/ui/table_view.cpp LIST ${theming})
    expect_eq("${r_CODE}" "0" "exit code without any version label\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "false" "documentation only without any version label")

elseif("${CASE}" STREQUAL "AWrongLabelNamesEachFile")
    # Scenario: a wrong version:doc is red and names each file that does not count.
    run_docs_only(r LABELS version:doc
        CHANGED ${story} src/ui/table_view.cpp ${theming} README.md LIST ${theming})
    expect_eq("${r_CODE}" "1" "exit code")
    expect_eq("${r_OUTPUT}" "" "no decision is written")
    foreach(path IN ITEMS src/ui/table_view.cpp ${theming} README.md)
        expect_contains("${r_LOG}" "  ${path}" "the message names ${path}")
    endforeach()
    expect_not_contains("${r_LOG}" "${story}" "the message names only files that do not count")

elseif("${CASE}" STREQUAL "AFileMovedIntoDocsCountsWithItsOldPath")
    # Scenario: a file moved from src/ into docs/ (git diff --no-renames lists both paths).
    run_docs_only(r LABELS version:doc
        CHANGED src/core/notes.cpp docs/notes.cpp LIST ${theming})  # S042-mention: changed paths
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "  src/core/notes.cpp" "the message names the old path")

elseif("${CASE}" STREQUAL "OtherFilesOutsideDocsNeverCount")
    # Decision 2: CLAUDE.md, LICENSE, the list itself, paths that only start like docs/ and a docs
    # directory below the top do not count.
    set(elsewhere "src/docs/notes.md")  # S042-mention: a changed path
    set(others CLAUDE.md LICENSE docs.md docsearch/x.md tests/docs-read-by-tests.txt ${elsewhere})
    run_docs_only(r LABELS version:doc CHANGED ${others} LIST ${theming})
    expect_eq("${r_CODE}" "1" "exit code")
    foreach(path IN LISTS others)
        expect_contains("${r_LOG}" "  ${path}" "the message names ${path}")
    endforeach()

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    file(WRITE "${WORK_DIR}/labels.txt" "version:doc\r\n")
    file(WRITE "${WORK_DIR}/changed.txt" "${story}\r\n")
    file(WRITE "${WORK_DIR}/list.txt" "${theming}\r\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DLABELS_FILE=${WORK_DIR}/labels.txt
                            -DCHANGED_FILE=${WORK_DIR}/changed.txt -DLIST_FILE=${WORK_DIR}/list.txt
                            -DOUTPUT=${WORK_DIR}/output.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_eq("${code}" "0" "exit code\n${out}${err}")
    file(READ "${WORK_DIR}/output.txt" output)
    string(STRIP "${output}" output)
    expect_eq("${output}" "true" "documentation only")

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
