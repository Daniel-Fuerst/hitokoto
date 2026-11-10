# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(hitokoto WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(hitokoto PRIVATE hitokoto::ui)
# S030: the full version (0.4.0+3.gabc1234) for QApplication::applicationVersion.
configure_file(hitokoto_version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/hitokoto_version.hpp" @ONLY)
target_include_directories(hitokoto PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
hitokoto_set_compile_options(hitokoto)
hitokoto_qt_runtime_beside(hitokoto)

if(WIN32)
    configure_file(hitokoto.rc.in "${CMAKE_CURRENT_BINARY_DIR}/hitokoto.rc" @ONLY)
    target_sources(hitokoto PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/hitokoto.rc")
endif()

if(APPLE)
    set_source_files_properties("${PROJECT_SOURCE_DIR}/icons/hitokoto.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(hitokoto PRIVATE "${PROJECT_SOURCE_DIR}/icons/hitokoto.icns")
endif()

set_target_properties(hitokoto PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME hitokoto
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.hitokoto.editor
    MACOSX_BUNDLE_BUNDLE_VERSION ${HITOKOTO_VERSION_CORE}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${HITOKOTO_VERSION_CORE}
    MACOSX_BUNDLE_ICON_FILE hitokoto.icns)

install(TARGETS hitokoto BUNDLE DESTINATION . RUNTIME DESTINATION bin)
