# SPDX-License-Identifier: GPL-3.0-or-later
# libFuzzer targets for the core (brief, section 7): arbitrary input must never crash,
# corrupt text or produce inconsistent results. Clang only.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "HITOKOTO_BUILD_FUZZERS needs Clang")
endif()

foreach(name IN ITEMS fuzz_codec fuzz_render fuzz_theme)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE hitokoto::core)
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
