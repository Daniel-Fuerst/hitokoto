# SPDX-License-Identifier: GPL-3.0-or-later
# S006/S007 benchmarks. Build with -DHITOKOTO_BUILD_BENCHMARKS=ON and a Release build type.

add_executable(hitokoto_core_bench core_bench.cpp)
target_link_libraries(hitokoto_core_bench PRIVATE hitokoto::core benchmark::benchmark_main)
hitokoto_set_compile_options(hitokoto_core_bench)

if(HITOKOTO_BUILD_UI)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(hitokoto-bench hitokoto_bench.cpp)
    target_link_libraries(hitokoto-bench PRIVATE hitokoto::ui Qt6::Test)
    target_compile_definitions(hitokoto-bench PRIVATE HITOKOTO_APP_PATH="$<TARGET_FILE:hitokoto>")
    hitokoto_set_compile_options(hitokoto-bench)
endif()
