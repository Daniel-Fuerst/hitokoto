# SPDX-License-Identifier: GPL-3.0-or-later
# Warnings, checks and sanitizers, applied per target through hitokoto_set_compile_options().
#
# Checks against memory errors that the toolchain does not turn on by itself, in every build type:
# - MSVC: /guard:cf (Control Flow Guard: an indirect call may only reach the start of a function)
#   and /sdl (more compile-time checks as errors, and runtime checks such as stricter /GS). ASLR,
#   DEP and /GS are MSVC's defaults.
# - libstdc++: _GLIBCXX_ASSERTIONS, its cheap precondition checks (operator[] within bounds, front()
#   and back() of a non-empty container, ...): a violated one aborts instead of reading or writing
#   out of bounds. It changes no layout or complexity; libc++ (macOS) ignores it.
# Stack protector, _FORTIFY_SOURCE, RELRO and BIND_NOW come from the toolchain or the distribution
# that packages hitokoto (Debian's dpkg-buildflags, Arch's makepkg; Ubuntu's GCC turns most of them
# on by itself), which also decide their levels; setting them here as well would clash with theirs.

function(hitokoto_set_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /sdl /guard:cf)
        target_link_options(${target} PRIVATE /guard:cf)
        if(HITOKOTO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
        target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS)
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
