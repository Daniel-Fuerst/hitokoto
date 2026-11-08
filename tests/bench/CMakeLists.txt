# SPDX-License-Identifier: GPL-3.0-or-later
# S029: the benchmark gate. Unit tests of the decisions (standard library only) and cases that run
# hitokoto-bench-gate on fixed files in data/<case>/ (args.txt, expected.txt).
add_executable(hitokoto_bench_gate_tests test_s029_runner_classes.cpp)
target_link_libraries(hitokoto_bench_gate_tests PRIVATE hitokoto::bench_gate GTest::gtest_main)
hitokoto_set_compile_options(hitokoto_bench_gate_tests)
gtest_discover_tests(hitokoto_bench_gate_tests DISCOVERY_MODE PRE_TEST)

if(TARGET hitokoto-bench-gate)
    foreach(c IN ITEMS
            established_green
            established_slow
            main_stage_green
            main_stage_regression
            too_few_samples
            unreadable_sample
            toolchain_change
            label_accepted
            hard_budget
            drift_main
            drift_after_marker)
        add_test(NAME S029_GateCase_${c}
            COMMAND ${CMAKE_COMMAND} -DGATE=$<TARGET_FILE:hitokoto-bench-gate>
                    -DCASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/${c}
                    -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gate-cases/${c}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_gate_case.cmake)
    endforeach()
endif()
