# SPDX-License-Identifier: GPL-3.0-or-later
include(GoogleTest)

add_executable(hitokoto_core_tests
    core/test_s001_open_decode.cpp
    core/test_s002_headings.cpp
    core/test_s003_bullets.cpp
    core/test_s013_inline_emphasis.cpp
    core/test_s014_fenced_code_blocks.cpp
    core/test_s015_inline_code.cpp
    core/test_s018_tables.cpp
    core/test_s019_height_index.cpp
    core/test_s020_backslash_escapes.cpp
    core/test_s021_code_block_fences.cpp
    core/test_s022_inline_links.cpp
    core/test_s023_thematic_breaks.cpp
    core/test_s024_front_matter.cpp
    core/test_s026_floating_toolbar.cpp
    core/test_s028_find_in_file.cpp
    core/test_s031_paste_tables.cpp
    core/test_s033_soft_line_breaks.cpp
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

# S000: UI tests and benchmarks simulate keys only through tests/ui/key_input.hpp.
add_test(NAME S000_KeyInputOnlyThroughTestHelpers
    COMMAND ${CMAKE_COMMAND} "-DSCAN_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/ui|${PROJECT_SOURCE_DIR}/bench"
            -DALLOWED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/ui/key_input.hpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/lint/no_direct_qtest_keys.cmake)

# S031 (review O5): no personal data in the clipboard samples; the second test shows that the lint
# finds each kind of it.
add_test(NAME S031_SamplesCarryNoPersonalData
    COMMAND ${CMAKE_COMMAND} -DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/s031
            -P ${CMAKE_CURRENT_SOURCE_DIR}/lint/no_personal_data.cmake)
add_test(NAME S031_PersonalDataLintFindsEachKind
    COMMAND ${CMAKE_COMMAND} -DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/lint/data/s031_personal_data
            -P ${CMAKE_CURRENT_SOURCE_DIR}/lint/no_personal_data.cmake)
set_tests_properties(S031_PersonalDataLintFindsEachKind PROPERTIES
                     PASS_REGULAR_EXPRESSION "personal-data-lint: 5 findings")

# S029: the benchmark gate.
add_subdirectory(bench)

# S030: versions.
add_subdirectory(version)

# S034: the AUR package.
add_subdirectory(packaging)

if(HITOKOTO_BUILD_UI)
    add_subdirectory(ui)
endif()
