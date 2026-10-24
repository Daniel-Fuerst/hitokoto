# SPDX-License-Identifier: GPL-3.0-or-later
# UI tests run headless (QT_QPA_PLATFORM=offscreen), one Qt Test class per story.
find_package(Qt6 REQUIRED COMPONENTS Test)

function(hitokoto_ui_test name)
    qt_add_executable(${name} ${name}.cpp ui_test_support.hpp)
    target_link_libraries(${name} PRIVATE hitokoto::ui Qt6::Test)
    hitokoto_set_compile_options(${name})
    if(NOT MSVC)
        # Qt Test macros (QTRY_*) expand to narrowing conversions in some Qt versions.
        target_compile_options(${name} PRIVATE -Wno-conversion -Wno-sign-conversion)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endfunction()

foreach(t IN ITEMS
        tst_s000_skeleton
        tst_s001_open
        tst_s002_headings
        tst_s003_bullets
        tst_s004_save
        tst_s005_theme
        tst_s007_large_files
        tst_s008_unsaved
        tst_s010_click)
    hitokoto_ui_test(${t})
endforeach()
