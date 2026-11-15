# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

# S030 (ADR 0010): the version comes from the version tag (git describe), never from this file.
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/version/HitokotoVersion.cmake")

project(hitokoto
    VERSION ${HITOKOTO_VERSION_CORE}
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
include(HitokotoQtRuntime)

if(HITOKOTO_BUILD_FUZZERS)
    # Coverage instrumentation for everything the fuzzers link (the core).
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src/core)

if(HITOKOTO_BUILD_UI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg)
    qt_standard_project_setup()
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

# S029: the benchmark gate is built with the tests (its own tests, also without Qt) and with the
# benchmarks (the CI job uses hitokoto-bench-gate from the bench preset).
if(HITOKOTO_BUILD_TESTS OR HITOKOTO_BUILD_BENCHMARKS)
    add_subdirectory(bench/gate)
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

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${HITOKOTO_VERSION_FULL}")
set(CPACK_PACKAGE_CONTACT "daniel@fuerst.priv.at")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

# The system string in the names of the archives (TGZ).
set(CPACK_SYSTEM_NAME "linux-amd64")

# S036 (decision 12): CPack makes no .deb; the Debian package for trixie comes from packaging/debian
# and is released by .github/workflows/packaging.yml. TGZ stays for S012.
set(CPACK_GENERATOR "TGZ")

include(CPack)
