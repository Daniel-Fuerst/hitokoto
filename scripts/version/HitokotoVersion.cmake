# SPDX-License-Identifier: GPL-3.0-or-later
# S030 (ADR 0010): the version of a build. Stub until the implementation.
function(hitokoto_version_from_describe describe full_var core_var)
    set(${full_var} "unimplemented" PARENT_SCOPE)
    set(${core_var} "unimplemented" PARENT_SCOPE)
endfunction()

function(hitokoto_version_from_option value full_var core_var)
    set(${full_var} "unimplemented" PARENT_SCOPE)
    set(${core_var} "unimplemented" PARENT_SCOPE)
endfunction()

function(hitokoto_determine_version)
    cmake_parse_arguments(PARSE_ARGV 0 a "NO_GIT" "SOURCE_DIR;OPTION;FULL;CORE;WARNING" "")
    set(${a_FULL} "unimplemented" PARENT_SCOPE)
    set(${a_CORE} "unimplemented" PARENT_SCOPE)
    set(${a_WARNING} "" PARENT_SCOPE)
endfunction()
