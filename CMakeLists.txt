# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

project(hitokoto
    VERSION 0.1.0
    DESCRIPTION "A native, single-file hybrid Markdown editor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HITOKOTO_BUILD_UI "Build the Qt user interface (off = core library and its tests only)" ON)
option(HITOKOTO_BUILD_TESTS "Build unit and UI tests" ON)
option(HITOKOTO_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(HITOKOTO_BUILD_FUZZERS "Build libFuzzer targets for the core (Clang only)" OFF)
option(HITOKOTO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(HITOKOTO_SANITIZE "" CACHE STRING "Comma-separated sanitizers, e.g. address,undefined")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HitokotoCompilerOptions)
include(HitokotoDependencies)

if(HITOKOTO_BUILD_FUZZERS)
    # Coverage instrumentation for everything the fuzzers link (the core).
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src/core)

if(HITOKOTO_BUILD_UI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
    qt_standard_project_setup()
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

if(HITOKOTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HITOKOTO_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

if(HITOKOTO_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
