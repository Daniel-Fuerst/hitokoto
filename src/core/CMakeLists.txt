# SPDX-License-Identifier: GPL-3.0-or-later
# The core: C++ standard library only. It must never include or link Qt (checked below and
# by the core-only CI job, which builds without Qt installed).
add_library(hitokoto_core STATIC
    src/text_codec.cpp
    src/file_io.cpp
    src/platform.cpp
    src/element_registry.cpp
    src/block_rules.cpp
    src/height_index.cpp
    src/elements/heading.cpp
    src/elements/bullet.cpp
    src/elements/inline_code.cpp
    src/elements/emphasis.cpp
    src/elements/escape.cpp
    src/elements/fenced_code.cpp
    src/elements/table.cpp
    src/elements/front_matter.cpp
    src/reveal.cpp
    src/theme.cpp
)
add_library(hitokoto::core ALIAS hitokoto_core)
target_include_directories(hitokoto_core PUBLIC include)
target_compile_features(hitokoto_core PUBLIC cxx_std_20)
hitokoto_set_compile_options(hitokoto_core)

# Layering guard: fail the configure step if anyone links Qt into the core.
cmake_language(DEFER CALL hitokoto_check_core_has_no_qt)
function(hitokoto_check_core_has_no_qt)
    get_target_property(libs hitokoto_core LINK_LIBRARIES)
    if(libs)
        foreach(lib IN LISTS libs)
            if(lib MATCHES "Qt")
                message(FATAL_ERROR "hitokoto_core must not link Qt (found ${lib})")
            endif()
        endforeach()
    endif()
endfunction()
