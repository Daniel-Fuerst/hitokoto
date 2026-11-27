# SPDX-License-Identifier: GPL-3.0-or-later
include(GoogleTest)
include(cmake/DocsReadByTests.cmake)

# S042: test::docsFile, the only way for tests to reach a file under docs/.
hitokoto_docs_read_by_tests(${CMAKE_CURRENT_BINARY_DIR}/generated)

add_executable(hitokoto_core_tests
    core/test_s001_open_decode.cpp
    core/test_s001_read_file.cpp
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
    core/test_s038_subtle_menu_bar.cpp
    core/test_s039_settings_menu.cpp
    core/test_s043_smooth_scrolling_toggle.cpp
    core/test_s044_math_source.cpp
    core/test_s045_math_layout.cpp
    core/test_s047_display_math.cpp
    core/test_s048_settings_section_headings.cpp
    core/test_s040_table_selection.cpp
    core/test_s041_table_copy_paste.cpp
    core/test_s050_empty_bullet_after_text.cpp
    core/test_s051_task_checkboxes.cpp
    core/test_s004_save.cpp
    core/test_s005_theme.cpp
    core/test_s007_large_files.cpp
)
target_link_libraries(hitokoto_core_tests PRIVATE hitokoto::core GTest::gtest_main)
target_compile_definitions(hitokoto_core_tests PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_include_directories(hitokoto_core_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
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

# S042, decision 3: tests, benchmarks and fuzzers read files under docs/ only through the helper
# generated from tests/docs-read-by-tests.txt; the second test shows that the lint finds each kind
# of path, the third that the helper refuses a document not on the list.
set(s042_lint ${CMAKE_CURRENT_SOURCE_DIR}/lint/docs_paths_only_through_the_list.cmake)
set(s042_scan "${CMAKE_CURRENT_SOURCE_DIR}|${PROJECT_SOURCE_DIR}/bench|${PROJECT_SOURCE_DIR}/fuzz")
# Left out: the lint's samples, the lint itself and the helper's generator.
set(s042_exclude
    "${CMAKE_CURRENT_SOURCE_DIR}/lint/data|${s042_lint}|${CMAKE_CURRENT_SOURCE_DIR}/cmake/DocsReadByTests.cmake")
add_test(NAME S042_TestsReadDocsOnlyThroughTheList
    COMMAND ${CMAKE_COMMAND} "-DSCAN_DIRS=${s042_scan}" "-DEXCLUDE=${s042_exclude}" -P ${s042_lint})
add_test(NAME S042_DocsPathLintFindsEachKind
    COMMAND ${CMAKE_COMMAND} -DSCAN_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/lint/data/s042_docs_paths
            -P ${s042_lint})
set_tests_properties(S042_DocsPathLintFindsEachKind PROPERTIES
                     PASS_REGULAR_EXPRESSION "docs-paths-lint: 6 findings")
# Built only by the test, never by the build or clang-tidy: the file must not compile.
add_library(s042_unlisted_doc OBJECT EXCLUDE_FROM_ALL compile_fail/s042_unlisted_doc.cpp)
target_compile_features(s042_unlisted_doc PRIVATE cxx_std_20)
target_include_directories(s042_unlisted_doc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
set_target_properties(s042_unlisted_doc PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
add_test(NAME S042_TheHelperRefusesAnUnlistedDocument
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target s042_unlisted_doc --config $<CONFIG>)
set_tests_properties(S042_TheHelperRefusesAnUnlistedDocument PROPERTIES
                     PASS_REGULAR_EXPRESSION "this document is not on tests/docs-read-by-tests.txt")

# S029: the benchmark gate.
add_subdirectory(bench)

# S030: versions.
add_subdirectory(version)

# S034: the AUR package.
add_subdirectory(packaging)

# S035: a version only after a green CI on main (the wiring of the workflows).
add_subdirectory(workflows)

if(HITOKOTO_BUILD_UI)
    add_subdirectory(ui)
endif()
