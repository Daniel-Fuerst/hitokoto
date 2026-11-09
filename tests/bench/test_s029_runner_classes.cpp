// SPDX-License-Identifier: GPL-3.0-or-later
// S029: benchmark compared with main on the same runner (ADR 0009). The runner class from
// /proc/cpuinfo and the cache sizes (information); the comparison of the rounds; toolchain changes in
// their own pull request; the label; hard budgets of both sides; runs of main.
#include "hitokoto/bench_gate/gate.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace hitokoto::bench_gate;

namespace {

// Hard budgets of hitokoto-bench (bench/hitokoto_bench.cpp, kBudgets with hard = true).
const std::vector<std::string> kHardKeys = {
    "typing_small_max_ms",
    "typing_10mb_max_ms",
    "s018_table_typing_max_ms",
    "s021_codeblock_caret_max_ms",
    "s021_codeblock_enter_max_ms",
    "s021_codeblock_typing_max_ms",
    "s023_break_typing_max_ms",
    "s023_bullet_typing_max_ms",
    "s023_underline_typing_max_ms",
    "s024_typing_max_ms",
    "s024_enter_leave_max_ms",
    "s024_leave_5000_keys_max_ms",
    "s024_leave_5000_nested_max_ms",
    "s024_long_text_typing_max_ms",
    "s024_leave_long_text_max_ms",
    "s028_field_typing_max_ms",
    "s028_step_max_ms",
    "s028_step_incomplete_max_ms",
    "s028_table_field_typing_max_ms",
    "s028_table_step_max_ms",
    "s028_doc_typing_max_ms",
    "save_10mb_ui_blocked_ms",
    "save_10mb_total_ms",
};

const char* const kClass = "linux-amd-epyc-7763-64-core-processor-4cpu-l2-512k-l3-32768k";

/// A complete result: every hard-budget key well within its budget, plus `extra`.
Values result(const Values& extra = {}) {
    Values v;
    for (const auto& k : kHardKeys) {
        v[k] = 5.0;
    }
    for (const auto& [k, x] : extra) {
        v[k] = x;
    }
    return v;
}

const KeyResult* find(const std::vector<KeyResult>& keys, const std::string& key) {
    const auto it = std::find_if(keys.begin(), keys.end(), [&](const KeyResult& k) { return k.key == key; });
    return it == keys.end() ? nullptr : &*it;
}

bool contains(const std::vector<std::string>& texts, const std::string& part) {
    return std::any_of(texts.begin(), texts.end(),
                       [&](const std::string& t) { return t.find(part) != std::string::npos; });
}

const char* const kCpuinfoEpyc =
    "processor\t: 0\n"
    "vendor_id\t: AuthenticAMD\n"
    "cpu family\t: 25\n"
    "model name\t: AMD EPYC 7763 64-Core Processor\n"
    "cpu MHz\t\t: 3244.000\n"
    "\n"
    "processor\t: 1\n"
    "vendor_id\t: AuthenticAMD\n"
    "model name\t: AMD EPYC 7763 64-Core Processor\n"
    "\n";

}  // namespace

// --- Runner class -------------------------------------------------------------------------------

TEST(S029_RunnerClasses, ClassFromModelNameCpuCountAndCaches) {
    EXPECT_EQ(runnerClass(kCpuinfoEpyc, 4, "512K", "32768K"), kClass);
}

TEST(S029_RunnerClasses, ModelNameIsLowerCasedAndRunsOfOtherCharactersBecomeOneHyphen) {
    const char* const intel =
        "processor\t: 0\nmodel name\t: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\n\n"
        "processor\t: 1\nmodel name\t: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\n";
    EXPECT_EQ(runnerClass(intel, 4, "1280K", "49152K"),
              "linux-intel-r-xeon-r-platinum-8370c-cpu-2-80ghz-4cpu-l2-1280k-l3-49152k");
}

TEST(S029_RunnerClasses, TheFirstModelNameCounts) {
    const char* const mixed = "processor\t: 0\nmodel name\t: First CPU\n\nprocessor\t: 1\nmodel name\t: Second CPU\n";
    EXPECT_EQ(runnerClass(mixed, 2, "1M", "32M"), "linux-first-cpu-2cpu-l2-1m-l3-32m");
}

TEST(S029_RunnerClasses, MissingOrEmptyModelNameGivesAnUnknownClass) {
    const std::string none = runnerClass("processor\t: 0\nvendor_id\t: ARM\n", 4, "1024K", "32768K");
    EXPECT_EQ(none, "linux-unknown-4cpu-l2-1024k-l3-32768k");
    const std::string empty = runnerClass("processor\t: 0\nmodel name\t:   \n", 4, "1024K", "32768K");
    EXPECT_EQ(empty, "linux-unknown-4cpu-l2-1024k-l3-32768k");
}

TEST(S029_RunnerClasses, ModelNameForTheReport) {
    EXPECT_EQ(modelName(kCpuinfoEpyc), "AMD EPYC 7763 64-Core Processor");
    EXPECT_EQ(modelName("processor\t: 0\nmodel name\t:   \n"), "");
    EXPECT_EQ(modelName("processor\t: 0\n"), "");
}

// --- Comparison of the rounds ------------------------------------------------------------------

TEST(S029_RunnerClasses, AKeyFailsOnlyIfMeansDifferAndRoundsDoNotOverlap) {
    const std::array<Values, 2> base = {Values{{"open_1mb_loaded_ms", 100}, {"s024_open_10mb_properties_ms", 31}},
                                        Values{{"open_1mb_loaded_ms", 104}, {"s024_open_10mb_properties_ms", 56}}};
    const std::array<Values, 2> pr = {
        Values{{"open_1mb_loaded_ms", 130}, {"s024_open_10mb_properties_ms", 55}, {"s030_new_key_ms", 3}},
        Values{{"open_1mb_loaded_ms", 140}, {"s024_open_10mb_properties_ms", 61}, {"s030_new_key_ms", 3}}};
    const auto keys = compareRounds(base, pr);
    ASSERT_NE(find(keys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(keys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
    EXPECT_DOUBLE_EQ(find(keys, "open_1mb_loaded_ms")->was, 102.0);
    EXPECT_DOUBLE_EQ(find(keys, "open_1mb_loaded_ms")->now, 135.0);
    // Two levels on one runner: +33 % in the means, but 55 < 56, so the sides overlap.
    ASSERT_NE(find(keys, "s024_open_10mb_properties_ms"), nullptr);
    EXPECT_EQ(find(keys, "s024_open_10mb_properties_ms")->status, KeyStatus::Ok);
    ASSERT_NE(find(keys, "s030_new_key_ms"), nullptr);
    EXPECT_EQ(find(keys, "s030_new_key_ms")->status, KeyStatus::OneSide);
}

TEST(S029_RunnerClasses, FirstScreenKeysNeedAFrameOfDifference) {
    EXPECT_DOUBLE_EQ(minDeltaMs("open_10mb_first_chunk_ms"), kFrameDeltaMs);
    EXPECT_DOUBLE_EQ(minDeltaMs("open_1mb_first_chunk_ms"), kFrameDeltaMs);
    EXPECT_DOUBLE_EQ(minDeltaMs("s023_open_10mb_first_chunk_ms"), kFrameDeltaMs);
    EXPECT_DOUBLE_EQ(minDeltaMs("s024_open_10mb_properties_ms"), kFrameDeltaMs);
    EXPECT_DOUBLE_EQ(minDeltaMs("open_10mb_styled_ms"), kMinDeltaMs);
    EXPECT_DOUBLE_EQ(minDeltaMs("typing_small_p99_ms"), kMinDeltaMs);
}

TEST(S029_RunnerClasses, AFrameSlipOfTheFirstScreenDoesNotFailButARegressionDoes) {
    // PR #31 (2026-11-08, AMD EPYC 9V45): round medians 48.08 and 47.92 ms for the base, 64.57 and
    // 64.63 ms for the pull request, every round with single opens at both levels. No code change.
    const std::array<Values, 2> base = {Values{{"open_10mb_first_chunk_ms", 48.075046}},
                                        Values{{"open_10mb_first_chunk_ms", 47.922364}}};
    const std::array<Values, 2> slip = {Values{{"open_10mb_first_chunk_ms", 64.566111}},
                                        Values{{"open_10mb_first_chunk_ms", 64.633316}}};
    const auto keys = compareRounds(base, slip);
    ASSERT_NE(find(keys, "open_10mb_first_chunk_ms"), nullptr);
    EXPECT_EQ(find(keys, "open_10mb_first_chunk_ms")->status, KeyStatus::Ok);
    const std::array<Values, 2> slower = {Values{{"open_10mb_first_chunk_ms", 75.0}},
                                          Values{{"open_10mb_first_chunk_ms", 78.0}}};
    EXPECT_EQ(compareRounds(base, slower)[0].status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, ReportOnlyKeysAndHardBudgetsMatchHitokotoBench) {
    EXPECT_TRUE(isReportOnly("typing_small_max_ms"));
    EXPECT_TRUE(isReportOnly("save_10mb_total_ms"));
    EXPECT_TRUE(isReportOnly("s028_full_count_ms"));
    EXPECT_TRUE(isReportOnly("startup_p95_ms"));
    EXPECT_FALSE(isReportOnly("typing_small_p99_ms"));
    EXPECT_FALSE(isReportOnly("s024_open_10mb_properties_ms"));
    std::vector<std::string> keys;
    for (const auto& b : hardBudgets()) {
        keys.push_back(b.key);
    }
    std::vector<std::string> expected = kHardKeys;
    std::sort(keys.begin(), keys.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(keys, expected);
    const auto misses = hardBudgetMisses(result({{"save_10mb_ui_blocked_ms", 17}, {"typing_10mb_max_ms", 50}}));
    ASSERT_EQ(misses.size(), 1U);
    EXPECT_EQ(misses[0].key, "save_10mb_ui_blocked_ms");
    EXPECT_DOUBLE_EQ(misses[0].limit, 16.0);
}

TEST(S029_RunnerClasses, TheResultCarriesTheFourRoundsInMeasuringOrder) {
    // Review O1: base 1, pull request 1, pull request 2, base 2, besides the means.
    const std::array<Values, 2> base = {Values{{"open_1mb_loaded_ms", 100}}, Values{{"open_1mb_loaded_ms", 104}}};
    const std::array<Values, 2> pr = {Values{{"open_1mb_loaded_ms", 130}}, Values{{"open_1mb_loaded_ms", 140}}};
    const auto keys = compareRounds(base, pr);
    ASSERT_EQ(keys.size(), 1U);
    EXPECT_DOUBLE_EQ(keys[0].rounds[0], 100.0);
    EXPECT_DOUBLE_EQ(keys[0].rounds[1], 130.0);
    EXPECT_DOUBLE_EQ(keys[0].rounds[2], 140.0);
    EXPECT_DOUBLE_EQ(keys[0].rounds[3], 104.0);
}

TEST(S029_RunnerClasses, PathsOfTheToolchainAndPathsAllowedBesideIt) {
    EXPECT_TRUE(changesToolchainFiles({"CMakePresets.json"}));
    EXPECT_TRUE(changesToolchainFiles({"src/core/markdown.cpp", "cmake/HitokotoDependencies.cmake"}));
    EXPECT_FALSE(changesToolchainFiles({".github/workflows/ci.yml", "docs/cmake.md", "bench/hitokoto_bench.cpp"}));
    const auto beside = pathsBesideToolchain(
        {".github/workflows/ci.yml", "CMakePresets.json", "cmake/HitokotoDependencies.cmake", "docs/performance.md",
         "docs/adr/0011-qt-6-13.md", "README.md", "src/core/markdown.cpp", "tests/core/test_x.cpp"});
    EXPECT_EQ(beside, (std::vector<std::string>{"src/core/markdown.cpp", "tests/core/test_x.cpp"}));
    EXPECT_TRUE(changesHarness({"bench/hitokoto_bench.cpp"}));
    EXPECT_FALSE(changesHarness({"bench/gate/src/gate.cpp", "bench/baselines/README.md", "src/bench.cpp"}));
}

// --- Decisions for pull requests ------------------------------------------------------------------

namespace {

/// A pull request run whose rounds are as fast as the base's.
Inputs pullRequestRun() {
    Inputs in;
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 104}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 101}}), result({{"open_1mb_loaded_ms", 103}})};
    return in;
}

}  // namespace

TEST(S029_RunnerClasses, APullRequestAsFastAsMainIsGreen) {
    const Decision d = decide(pullRequestRun());
    EXPECT_EQ(d.outcome, Outcome::Green);
    ASSERT_NE(find(d.keys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.keys, "open_1mb_loaded_ms")->status, KeyStatus::Ok);
}

TEST(S029_RunnerClasses, ARealRegressionFails) {
    Inputs in = pullRequestRun();
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}}), result({{"open_1mb_loaded_ms", 140}})};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    ASSERT_NE(find(d.keys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.keys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, APullRequestRunWithoutRoundsFails) {
    Inputs in;
    in.baseRounds = {result(), result()};
    EXPECT_EQ(decide(in).outcome, Outcome::Fails);
}

TEST(S029_RunnerClasses, TheLabelAcceptsComparisonsButNotHardBudgets) {
    Inputs in = pullRequestRun();
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}}), result({{"open_1mb_loaded_ms", 140}})};
    in.accepted = true;
    const Decision accepted = decide(in);
    EXPECT_EQ(accepted.outcome, Outcome::Green);
    EXPECT_TRUE(accepted.acceptedByLabel);
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}, {"save_10mb_ui_blocked_ms", 20}}),
                            result({{"open_1mb_loaded_ms", 140}})};
    const Decision hard = decide(in);
    EXPECT_EQ(hard.outcome, Outcome::Fails);
    ASSERT_EQ(hard.hardBudgetMisses.size(), 1U);
    EXPECT_EQ(hard.hardBudgetMisses[0].key, "save_10mb_ui_blocked_ms");
}

TEST(S029_RunnerClasses, HardBudgetsOfThePullRequestFailThoseOfTheBaseAreInformation) {
    // Review O3: a miss of the base is reported with round, value and limit; the outcome stays.
    Inputs in = pullRequestRun();
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}),
                     result({{"open_1mb_loaded_ms", 104}, {"typing_small_max_ms", 222}})};
    const Decision base = decide(in);
    EXPECT_EQ(base.outcome, Outcome::Green);
    EXPECT_TRUE(base.hardBudgetMisses.empty());
    ASSERT_EQ(base.baseBudgetMisses.size(), 1U);
    EXPECT_EQ(base.baseBudgetMisses[0].budget.key, "typing_small_max_ms");
    EXPECT_EQ(base.baseBudgetMisses[0].round, 2);
    EXPECT_DOUBLE_EQ(base.baseBudgetMisses[0].value, 222.0);
    EXPECT_DOUBLE_EQ(base.baseBudgetMisses[0].budget.limit, 50.0);
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 101}}),
                            result({{"open_1mb_loaded_ms", 103}, {"typing_small_max_ms", 54.2}})};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    ASSERT_EQ(d.hardBudgetMisses.size(), 1U);
    EXPECT_EQ(d.hardBudgetMisses[0].key, "typing_small_max_ms");
}

TEST(S029_RunnerClasses, AToolchainChangeAloneIsCompared) {
    Inputs in = pullRequestRun();
    in.changedPaths = {".github/workflows/ci.yml", "docs/performance.md"};
    in.qtVersionChanged = true;
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(d.toolchainChange);
    EXPECT_TRUE(d.mixedPaths.empty());
    EXPECT_TRUE(contains(d.notices, "Qt version"));
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}}), result({{"open_1mb_loaded_ms", 140}})};
    EXPECT_EQ(decide(in).outcome, Outcome::Fails);  // a slower Qt
    in.accepted = true;
    EXPECT_EQ(decide(in).outcome, Outcome::Green);
}

TEST(S029_RunnerClasses, AToolchainChangeMixedWithCodeFailsEvenWithTheLabel) {
    Inputs in = pullRequestRun();
    in.changedPaths = {"CMakePresets.json", "src/core/markdown.cpp", "README.md"};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    EXPECT_TRUE(d.toolchainChange);
    EXPECT_EQ(d.mixedPaths, (std::vector<std::string>{"src/core/markdown.cpp"}));
    in.accepted = true;
    EXPECT_EQ(decide(in).outcome, Outcome::Fails);
    // Without a toolchain change, ci.yml and code may change together.
    Inputs plain = pullRequestRun();
    plain.changedPaths = {".github/workflows/ci.yml", "src/core/markdown.cpp"};
    EXPECT_EQ(decide(plain).outcome, Outcome::Green);
    EXPECT_FALSE(decide(plain).toolchainChange);
    // A Qt update with code fails as well.
    plain.qtVersionChanged = true;
    EXPECT_EQ(decide(plain).outcome, Outcome::Fails);
}

TEST(S029_RunnerClasses, ANewRunnerImageIsNamedAsNotVisible) {
    Inputs in = pullRequestRun();
    in.changedPaths = {".github/workflows/ci.yml"};
    in.imageChanged = true;
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(d.toolchainChange);
    EXPECT_TRUE(contains(d.notices, "runner image"));
    EXPECT_TRUE(contains(d.notices, "not visible"));
}

TEST(S029_RunnerClasses, AHarnessChangeIsNoticedButDoesNotFail) {
    Inputs in = pullRequestRun();
    in.changedPaths = {"bench/hitokoto_bench.cpp"};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(contains(d.notices, "harness"));
}

// --- Decisions for runs of main -------------------------------------------------------------------

TEST(S029_RunnerClasses, RunsOfMainCheckTheHardBudgetsOnly) {
    Inputs in;
    in.kind = RunKind::Main;
    in.results = result({{"open_1mb_loaded_ms", 500}});
    const Decision green = decide(in);
    EXPECT_EQ(green.outcome, Outcome::Green);
    EXPECT_TRUE(green.keys.empty());
    in.results["save_10mb_ui_blocked_ms"] = 17;
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    ASSERT_EQ(d.hardBudgetMisses.size(), 1U);
    EXPECT_EQ(d.hardBudgetMisses[0].key, "save_10mb_ui_blocked_ms");
}
