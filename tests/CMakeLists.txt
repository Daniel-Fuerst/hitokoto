# SPDX-License-Identifier: GPL-3.0-or-later
include(GoogleTest)

add_executable(hitokoto_core_tests
    core/test_s001_open_decode.cpp
    core/test_s002_headings.cpp
    core/test_s003_bullets.cpp
    core/test_s013_inline_emphasis.cpp
    core/test_s014_fenced_code_blocks.cpp
    core/test_s015_inline_code.cpp
    core/test_s004_save.cpp
    core/test_s005_theme.cpp
    core/test_s007_large_files.cpp
)
target_link_libraries(hitokoto_core_tests PRIVATE hitokoto::core GTest::gtest_main)
target_compile_definitions(hitokoto_core_tests PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
hitokoto_set_compile_options(hitokoto_core_tests)
gtest_discover_tests(hitokoto_core_tests DISCOVERY_MODE PRE_TEST)

# S005: no hard-coded styling values in UI code.
add_test(NAME S005_NoStylingLiteralsInUi
    COMMAND ${CMAKE_COMMAND} -DUI_DIR=${PROJECT_SOURCE_DIR}/src/ui -P ${CMAKE_CURRENT_SOURCE_DIR}/lint/no_styling_literals.cmake)

if(HITOKOTO_BUILD_UI)
    add_subdirectory(ui)
endif()
