# SPDX-License-Identifier: GPL-3.0-or-later
# Warnings and sanitizers, applied per target through hitokoto_set_compile_options().

function(hitokoto_set_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        if(HITOKOTO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
        if(HITOKOTO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(HITOKOTO_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "HITOKOTO_SANITIZE is supported with GCC and Clang only")
    endif()
    add_compile_options(-fsanitize=${HITOKOTO_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${HITOKOTO_SANITIZE})
endif()
