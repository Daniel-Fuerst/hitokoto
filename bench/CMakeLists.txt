# SPDX-License-Identifier: GPL-3.0-or-later
# S006/S007 benchmarks. Build with -DHITOKOTO_BUILD_BENCHMARKS=ON and a Release build type.

add_executable(hitokoto_core_bench core_bench.cpp)
target_link_libraries(hitokoto_core_bench PRIVATE hitokoto::core benchmark::benchmark_main)
hitokoto_set_compile_options(hitokoto_core_bench)

if(HITOKOTO_BUILD_UI)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(hitokoto-bench hitokoto_bench.cpp)
    target_link_libraries(hitokoto-bench PRIVATE hitokoto::ui hitokoto::bench_gate Qt6::Test)
    # Simulated key input shared with the UI tests (tests/ui/key_input.hpp).
    target_include_directories(hitokoto-bench PRIVATE ${PROJECT_SOURCE_DIR}/tests/ui)
    target_compile_definitions(hitokoto-bench PRIVATE HITOKOTO_APP_PATH="$<TARGET_FILE:hitokoto>")
    # S031: the markup of the clipboard samples, for the generated tables (:/s031/<name>.html).
    qt_add_resources(hitokoto-bench "s031_samples" PREFIX "/s031"
                     BASE ${PROJECT_SOURCE_DIR}/tests/data/s031/samples
                     FILES ${PROJECT_SOURCE_DIR}/tests/data/s031/samples/outlook-t1-example.html
                           ${PROJECT_SOURCE_DIR}/tests/data/s031/samples/excel-t6-example.html)
    hitokoto_set_compile_options(hitokoto-bench)
    hitokoto_qt_runtime_beside(hitokoto-bench)
endif()
