# SPDX-License-Identifier: GPL-3.0-or-later
# UI tests and benchmarks simulate keys only through tests/ui/key_input.hpp, never with
# QTest::key* directly: clang-tidy's static analyzer reports a false positive inside Qt's key
# simulation (clang-analyzer-cplusplus.NewDelete), and the helpers hide that code from the analyzer
# in one place. Fails if any other file calls QTest::key* directly.
if(NOT SCAN_DIRS OR NOT ALLOWED_FILE)
    message(FATAL_ERROR "SCAN_DIRS and ALLOWED_FILE must be set")
endif()

string(REPLACE "|" ";" dirs "${SCAN_DIRS}")
set(sources "")
foreach(dir IN LISTS dirs)
    file(GLOB_RECURSE found "${dir}/*.cpp" "${dir}/*.hpp")
    list(APPEND sources ${found})
endforeach()

get_filename_component(allowed "${ALLOWED_FILE}" REALPATH)
set(violations "")
foreach(src IN LISTS sources)
    get_filename_component(real "${src}" REALPATH)
    if(real STREQUAL allowed)
        continue()
    endif()
    file(STRINGS "${src}" lines)
    set(lineno 0)
    foreach(line IN LISTS lines)
        math(EXPR lineno "${lineno} + 1")
        if(line MATCHES "QTest::key(Click|Clicks|Press|Release|Event|Sequence)[ (]")
            string(APPEND violations "\n  ${src}:${lineno}: ${line}")
        endif()
    endforeach()
endforeach()

if(violations)
    message(FATAL_ERROR "Use test::keyClick/keyClicks from tests/ui/key_input.hpp instead of QTest::key*:${violations}")
endif()
message(STATUS "No direct QTest::key* calls in ${SCAN_DIRS}")
