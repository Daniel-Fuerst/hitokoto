# SPDX-License-Identifier: GPL-3.0-or-later
# Test and benchmark dependencies. Runtime dependency is Qt only (found in the top-level file).
# Installed packages are preferred; otherwise a pinned release tag is fetched.
include(FetchContent)

if(HITOKOTO_BUILD_TESTS)
    find_package(GTest 1.14 CONFIG QUIET)
    if(NOT GTest_FOUND)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.18.0
            GIT_SHALLOW TRUE
            SYSTEM)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

if(HITOKOTO_BUILD_BENCHMARKS)
    find_package(benchmark 1.8 CONFIG QUIET)
    if(NOT benchmark_FOUND)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.10.0
            GIT_SHALLOW TRUE
            SYSTEM)
        FetchContent_MakeAvailable(benchmark)
    endif()
endif()
