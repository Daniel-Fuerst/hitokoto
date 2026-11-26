# SPDX-License-Identifier: GPL-3.0-or-later
# S042: the decision of the job "Changes" in ci.yml. Run with cmake -P; tested by tests/workflows
# (S042_*).
#
#   cmake -DLABELS_FILE=<f> -DCHANGED_FILE=<f> -DLIST_FILE=<f> -DOUTPUT=<f> -P DocsOnly.cmake
#
# Writes "true" to OUTPUT if the pull request carries version:doc and every changed file counts as
# documentation, "false" if it does not carry version:doc (every job runs). Fails, naming each file
# that does not count, if it carries version:doc and changes anything else; then the other jobs of
# ci.yml are skipped and "CI result" fails until the label is changed.
#
# Files hold one item per line; a trailing carriage return is ignored.
#   LABELS_FILE: the names of the pull request's labels, read at run time.
#   CHANGED_FILE: the changed files against the base (git diff --name-only --no-renames over three
#     dots), so a renamed file counts with its old and its new path.
#   LIST_FILE: tests/docs-read-by-tests.txt, the files under docs/ that tests read.
# Documentation (decision 2): every file under docs/ except those on the list. Nothing outside
# docs/ counts, also not README.md, CLAUDE.md or LICENSE.
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS LABELS_FILE CHANGED_FILE LIST_FILE OUTPUT)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()

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

read_lines("${LABELS_FILE}" labels)
if(NOT "version:doc" IN_LIST labels)
    file(WRITE "${OUTPUT}" "false\n")
    message(STATUS "Without the label version:doc every job runs.")
    return()
endif()

read_lines("${CHANGED_FILE}" changed)
read_lines("${LIST_FILE}" read_by_tests)
set(others "")
foreach(path IN LISTS changed)
    if(NOT path MATCHES "^docs/" OR path IN_LIST read_by_tests)
        string(APPEND others "\n  ${path}")
    endif()
endforeach()
if(NOT "${others}" STREQUAL "")
    message(FATAL_ERROR
        "The pull request carries version:doc, but these files are not documentation in the "
        "sense of S042 (files under docs/ that tests do not read, tests/docs-read-by-tests.txt):"
        "${others}\nChange the label to the one that fits (ADR 0010) and re-run all jobs; every "
        "job then runs.")
endif()
list(LENGTH changed count)
file(WRITE "${OUTPUT}" "true\n")
message(STATUS "version:doc and ${count} changed files, all documentation: the other jobs are "
               "skipped.")
