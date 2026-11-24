# SPDX-License-Identifier: GPL-3.0-or-later
# The thin UI layer: Qt Widgets. All logic it needs comes from hitokoto_core.
qt_add_library(hitokoto_ui STATIC
    application_icon.cpp application_icon.hpp
    block_layout.cpp block_layout.hpp
    block_styler.cpp block_styler.hpp
    block_user_state.hpp
    document_session.cpp document_session.hpp
    editor_painters.cpp editor_painters.hpp
    editor_formats.cpp editor_formats.hpp
    editor_widget.cpp editor_widget.hpp
    view_anchor.cpp view_anchor.hpp
    find_bar.cpp find_bar.hpp
    find_controller.cpp find_controller.hpp
    floating_toolbar.cpp floating_toolbar.hpp
    list_editing.cpp list_editing.hpp
    main_window.cpp main_window.hpp
    math_block_embedder.cpp math_block_embedder.hpp
    math_font.cpp math_font.hpp
    math_render.cpp math_render.hpp
    menu_bar.cpp menu_bar.hpp
    options.hpp
    properties_embedder.cpp properties_embedder.hpp
    properties_view.cpp properties_view.hpp
    qt_bridge.hpp
    settings_menu.cpp settings_menu.hpp
    smooth_scroller.cpp smooth_scroller.hpp
    toggle_switch.cpp toggle_switch.hpp
    table_embedder.cpp table_embedder.hpp
    table_model.cpp table_model.hpp
    table_paste.cpp table_paste.hpp
    table_view.cpp table_view.hpp
    text_input.cpp text_input.hpp
    theme_applier.cpp theme_applier.hpp
    theme_manager.cpp theme_manager.hpp
)
add_library(hitokoto::ui ALIAS hitokoto_ui)
target_include_directories(hitokoto_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(hitokoto_ui PRIVATE "HITOKOTO_UI_VERSION=\"${HITOKOTO_VERSION_FULL}\"")
target_link_libraries(hitokoto_ui PUBLIC hitokoto::core Qt6::Widgets Qt6::Svg)
qt_add_resources(hitokoto_ui hitokoto_icon
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/icons"
    FILES "${PROJECT_SOURCE_DIR}/icons/hitokoto.svg"
)
# S045, ADR 0014: the math font and its licence, inside the program (fonts/stix-two-math/README.md).
qt_add_resources(hitokoto_ui hitokoto_math_font
    PREFIX "/fonts"
    BASE "${PROJECT_SOURCE_DIR}/fonts/stix-two-math"
    FILES "${PROJECT_SOURCE_DIR}/fonts/stix-two-math/STIXTwoMath-Regular.otf"
          "${PROJECT_SOURCE_DIR}/fonts/stix-two-math/OFL.txt"
)
hitokoto_set_compile_options(hitokoto_ui)
