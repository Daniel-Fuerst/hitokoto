# SPDX-License-Identifier: GPL-3.0-or-later
# S029 / ADR 0009: the benchmark gate. The library decides and uses the C++ standard library only
# (tested in core-sanitize without Qt); hitokoto-bench-gate reads files and JSON with Qt Core and
# writes the report for the workflow (exit 0 green, 1 fails, 3 run the main stage).

add_library(hitokoto_bench_gate STATIC src/gate.cpp)
add_library(hitokoto::bench_gate ALIAS hitokoto_bench_gate)
target_include_directories(hitokoto_bench_gate PUBLIC include)
hitokoto_set_compile_options(hitokoto_bench_gate)

if(HITOKOTO_BUILD_UI)
    add_executable(hitokoto-bench-gate src/hitokoto_bench_gate.cpp)
    target_link_libraries(hitokoto-bench-gate PRIVATE hitokoto::bench_gate Qt6::Core)
    hitokoto_set_compile_options(hitokoto-bench-gate)
    hitokoto_qt_runtime_beside(hitokoto-bench-gate)
endif()
