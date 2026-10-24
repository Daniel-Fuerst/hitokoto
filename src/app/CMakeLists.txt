# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(hitokoto WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(hitokoto PRIVATE hitokoto::ui)
target_compile_definitions(hitokoto PRIVATE HITOKOTO_VERSION="${PROJECT_VERSION}")
hitokoto_set_compile_options(hitokoto)
set_target_properties(hitokoto PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME hitokoto
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.hitokoto.editor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

install(TARGETS hitokoto BUNDLE DESTINATION . RUNTIME DESTINATION bin)
