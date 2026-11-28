# SPDX-License-Identifier: GPL-3.0-or-later
# UI tests run headless (QT_QPA_PLATFORM=offscreen), one Qt Test class per story.
find_package(Qt6 REQUIRED COMPONENTS Test)

# The environment of every UI test. Under Windows the offscreen platform looks for fonts in Qt's
# lib/fonts, which Qt no longer ships, and lays text out without a real font; the tests then measure
# nothing like what a user sees (a test of S033 failed only there). They take the system's fonts.
set(hitokoto_ui_test_env "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{SystemRoot}/Fonts" hitokoto_system_fonts)
    list(APPEND hitokoto_ui_test_env "QT_QPA_FONTDIR=${hitokoto_system_fonts}")
endif()

# Time limits of the UI tests. Under the sanitizers (the CI's job ui-sanitizers) a test takes two to
# four times as long; the slowest, tst_s017, takes about 120 s there instead of 37 s. The limits
# inside the tests scale with test::kTimeScale (ui_test_support.hpp).
set(hitokoto_ui_test_timeout 300)
set(hitokoto_ui_test_timeout_scaled 120)
if(HITOKOTO_SANITIZE)
    set(hitokoto_ui_test_timeout 900)
    set(hitokoto_ui_test_timeout_scaled 360)
endif()

function(hitokoto_ui_test name)
    qt_add_executable(${name} ${name}.cpp ui_test_support.hpp key_input.hpp)
    target_link_libraries(${name} PRIVATE hitokoto::ui Qt6::Test)
    hitokoto_set_compile_options(${name})
    if(NOT MSVC)
        # Qt Test macros (QTRY_*) expand to narrowing conversions in some Qt versions.
        target_compile_options(${name} PRIVATE -Wno-conversion -Wno-sign-conversion)
    endif()
    if(HITOKOTO_SANITIZE)
        target_compile_definitions(${name} PRIVATE HITOKOTO_TESTS_UNDER_SANITIZERS)  # test::kTimeScale
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${hitokoto_ui_test_env}"
                         TIMEOUT ${hitokoto_ui_test_timeout})
    hitokoto_qt_runtime_beside(${name})
endfunction()

foreach(t IN ITEMS
        tst_s000_skeleton
        tst_s000_random_edits
        tst_s001_open
        tst_s002_headings
        tst_s003_bullets
        tst_s004_save
        tst_s005_theme
        tst_s007_large_files
        tst_s008_unsaved
        tst_s009_concealment
        tst_s010_click
        tst_s013_inline_emphasis
        tst_s014_fenced_code_blocks
        tst_s015_inline_code
        tst_s016_app_icon
        tst_s017_save_without_stall
        tst_s018_tables
        tst_s019_editor_surface
        tst_s020_backslash_escapes
        tst_s021_code_block_fences
        tst_s022_inline_links
        tst_s023_thematic_breaks
        tst_s024_front_matter
        tst_s026_floating_toolbar
        tst_s027_smooth_scrolling
        tst_s028_find_in_file
        tst_s031_paste_tables
        tst_s033_soft_line_breaks
        tst_s038_subtle_menu_bar
        tst_s039_settings_menu
        tst_s040_table_selection
        tst_s041_table_copy_paste
        tst_s043_smooth_scrolling_toggle
        tst_s044_math_source
        tst_s045_bundled_math_font
        tst_s046_typeset_inline_math
        tst_s047_typeset_display_math
        tst_s048_settings_section_headings
        tst_s052_task_checkboxes
        tst_s053_numbered_lists)
    hitokoto_ui_test(${t})
endforeach()

# S044: the LaTeX sample from tests/data.
target_compile_definitions(tst_s044_math_source PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(tst_s045_bundled_math_font PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(tst_s046_typeset_inline_math PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(tst_s047_typeset_display_math PRIVATE HITOKOTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# S031: the clipboard samples as resources (:/s031/<name>.html and .txt).
file(GLOB hitokoto_s031_samples ${PROJECT_SOURCE_DIR}/tests/data/s031/samples/*.html
     ${PROJECT_SOURCE_DIR}/tests/data/s031/samples/*.txt)
target_sources(tst_s031_paste_tables PRIVATE paste_samples.hpp)
qt_add_resources(tst_s031_paste_tables "s031_samples" PREFIX "/s031"
                 BASE ${PROJECT_SOURCE_DIR}/tests/data/s031/samples
                 FILES ${hitokoto_s031_samples})

# S023: the line of a thematic break is crisp at fractional scaling too.
add_test(NAME tst_s023_thematic_breaks_scaled COMMAND tst_s023_thematic_breaks aConcealedBreakIsCrispAtTheDeviceScale)
set_tests_properties(tst_s023_thematic_breaks_scaled PROPERTIES
                     ENVIRONMENT "${hitokoto_ui_test_env};QT_SCALE_FACTOR=1.5"
                     TIMEOUT ${hitokoto_ui_test_timeout_scaled})

if(WIN32)
    # Qt's DLLs must be found beside the test executable: Visual Studio's Test Explorer starts the
    # tests without Qt's bin folder on PATH; ctest's environment properties do not reach them there.
    file(TO_NATIVE_PATH "$ENV{SystemRoot}/System32" hitokoto_system32)
    add_test(NAME S000_UiTestsStartWithoutQtOnPath
             COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=set:${hitokoto_system32}"
                     $<TARGET_FILE:tst_s000_skeleton>)
    set_tests_properties(S000_UiTestsStartWithoutQtOnPath PROPERTIES
                         ENVIRONMENT "${hitokoto_ui_test_env}" TIMEOUT 60)
endif()
