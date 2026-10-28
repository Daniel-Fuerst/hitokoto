# SPDX-License-Identifier: GPL-3.0-or-later
# Windows: Qt's DLLs and a qt.conf beside an executable in the build tree, so that it starts
# without Qt's bin folder on PATH (Visual Studio's Test Explorer, a double click, a debugger).
# qt.conf points at the Qt installation, where Qt finds its plugins (platform, image formats).
# Elsewhere the runtime path (RPATH) does this already. Installation is not affected.
function(hitokoto_qt_runtime_beside target)
    if(NOT WIN32)
        return()
    endif()
    set(qt_conf "${CMAKE_BINARY_DIR}/qt-runtime/qt.conf")
    file(CONFIGURE OUTPUT "${qt_conf}" CONTENT "[Paths]\nPrefix = ${QT6_INSTALL_PREFIX}\n")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${qt_conf}" $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        VERBATIM)
endfunction()
