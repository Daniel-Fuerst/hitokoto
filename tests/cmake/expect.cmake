# SPDX-License-Identifier: GPL-3.0-or-later
# Helpers of the test cases written in CMake (cmake -P): the packaging cases of S034, S036 and
# S037, the version cases of S030 and the workflow lints. Each case file includes this file.

function(expect_eq actual expected what)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: '${actual}', expected '${expected}'")
    endif()
endfunction()

# CMake wraps long messages, so runs of white space count as one space.
function(expect_contains text part what)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_text "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_part "${part}")
    string(FIND "${flat_text}" "${flat_part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' not in\n${text}")
    endif()
endfunction()

function(expect_not_contains text part what)
    string(FIND "${text}" "${part}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' found in\n${text}")
    endif()
endfunction()

# Writes the items of a list one per line (an empty list gives an empty file).
function(write_lines file)
    set(text "")
    foreach(item IN LISTS ARGN)
        string(APPEND text "${item}\n")
    endforeach()
    file(WRITE "${file}" "${text}")
endfunction()
