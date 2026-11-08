# SPDX-License-Identifier: GPL-3.0-or-later
# S029: runs hitokoto-bench-gate with the arguments in CASE_DIR/args.txt (one per line; @DIR@ is the
# case directory, @OUT@ a fresh output directory) and checks CASE_DIR/expected.txt: the first line is
# the exit code, every further line a text that the output or the report must contain.
file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")
file(STRINGS "${CASE_DIR}/args.txt" lines)
set(args "")
foreach(line IN LISTS lines)
    string(STRIP "${line}" line)  # a checkout with CRLF leaves \r at the end
    string(REPLACE "@DIR@" "${CASE_DIR}" line "${line}")
    string(REPLACE "@OUT@" "${OUT_DIR}" line "${line}")
    list(APPEND args "${line}")
endforeach()
execute_process(COMMAND "${GATE}" ${args}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(STRINGS "${CASE_DIR}/expected.txt" expected)
list(POP_FRONT expected expected_code)
string(STRIP "${expected_code}" expected_code)
set(report "")
if(EXISTS "${OUT_DIR}/report.md")
    file(READ "${OUT_DIR}/report.md" report)
endif()
set(all "${out}\n${err}\n${report}")
if(NOT code STREQUAL expected_code)
    message(FATAL_ERROR "exit code ${code}, expected ${expected_code}\n${all}")
endif()
foreach(text IN LISTS expected)
    string(STRIP "${text}" text)
    string(FIND "${all}" "${text}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "'${text}' not in the output or report\n${all}")
    endif()
endforeach()
