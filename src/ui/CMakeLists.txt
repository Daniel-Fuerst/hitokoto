# SPDX-License-Identifier: GPL-3.0-or-later
# The thin UI layer: Qt Widgets. All logic it needs comes from hitokoto_core.
qt_add_library(hitokoto_ui STATIC
    application_icon.cpp application_icon.hpp
    block_layout.cpp block_layout.hpp
    block_styler.cpp block_styler.hpp
    document_session.cpp document_session.hpp
    editor_formats.cpp editor_formats.hpp
    editor_widget.cpp editor_widget.hpp
    main_window.cpp main_window.hpp
    qt_bridge.hpp
    table_controller.cpp table_controller.hpp
    table_painter.cpp table_painter.hpp
    theme_applier.cpp theme_applier.hpp
    theme_manager.cpp theme_manager.hpp
)
add_library(hitokoto::ui ALIAS hitokoto_ui)
target_include_directories(hitokoto_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hitokoto_ui PUBLIC hitokoto::core Qt6::Widgets Qt6::Svg)
qt_add_resources(hitokoto_ui hitokoto_icon
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/icons"
    FILES "${PROJECT_SOURCE_DIR}/icons/hitokoto.svg"
)
hitokoto_set_compile_options(hitokoto_ui)
