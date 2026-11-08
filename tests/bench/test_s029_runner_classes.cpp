// SPDX-License-Identifier: GPL-3.0-or-later
// S029: benchmark baselines per runner class (ADR 0009). The runner class from /proc/cpuinfo and the
// cache sizes; established classes and their baselines; the class stage and the main stage; drift
// from the anchor; the label; hard budgets; what the writer checks before it records a sample.
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

Sample sample(const std::string& time, const Values& values) {
    return Sample{"linux/" + std::string(kClass) + "/" + time + ".json", time, values, true};
}

/// `n` samples with the given value of `open_1mb_loaded_ms`, at 2026-11-0<day>T0<i>:00:00Z.
std::vector<Sample> samples(int n, double value, int day = 1) {
    std::vector<Sample> s;
    s.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        s.push_back(sample("2026-11-0" + std::to_string(day) + "T0" + std::to_string(i) + ":00:00Z",
                           result({{"open_1mb_loaded_ms", value}})));
    }
    return s;
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
    EXPECT_TRUE(isUnknownClass(none));
    const std::string empty = runnerClass("processor\t: 0\nmodel name\t:   \n", 4, "1024K", "32768K");
    EXPECT_EQ(empty, "linux-unknown-4cpu-l2-1024k-l3-32768k");
    EXPECT_TRUE(isUnknownClass(empty));
    EXPECT_FALSE(isUnknownClass(kClass));
}

TEST(S029_RunnerClasses, ModelNameForTheReport) {
    EXPECT_EQ(modelName(kCpuinfoEpyc), "AMD EPYC 7763 64-Core Processor");
    EXPECT_EQ(modelName("processor\t: 0\nmodel name\t:   \n"), "");
    EXPECT_EQ(modelName("processor\t: 0\n"), "");
}

// --- Established class, baseline, anchor ---------------------------------------------------------

TEST(S029_RunnerClasses, AClassIsEstablishedAtThreeReadableSamples) {
    EXPECT_FALSE(isEstablished(kClass, samples(2, 100)));
    EXPECT_TRUE(isEstablished(kClass, samples(3, 100)));
    auto withUnreadable = samples(3, 100);
    withUnreadable[1].readable = false;
    withUnreadable[1].values.clear();
    EXPECT_FALSE(isEstablished(kClass, withUnreadable));
    EXPECT_FALSE(isEstablished("linux-unknown-4cpu-l2-1024k-l3-32768k", samples(5, 100)));
}

TEST(S029_RunnerClasses, BaselineIsTheMedianOfTheNewestFiveSamplesByStartTime) {
    std::vector<Sample> s;
    // Seven samples, not in time order: values 1..7 at hours 1..7; the newest five are 3..7.
    for (int hour : {5, 1, 7, 3, 2, 6, 4}) {
        s.push_back(sample("2026-11-01T0" + std::to_string(hour) + ":00:00Z",
                           result({{"open_1mb_loaded_ms", static_cast<double>(hour)}})));
    }
    const Values b = classBaseline(s);
    ASSERT_TRUE(b.count("open_1mb_loaded_ms"));
    EXPECT_DOUBLE_EQ(b.at("open_1mb_loaded_ms"), 5.0);
}

TEST(S029_RunnerClasses, AKeyNeedsThreeSamplesForABaseline) {
    auto s = samples(5, 100);
    s[3].values["s030_new_key_ms"] = 10;  // in two samples only
    s[4].values["s030_new_key_ms"] = 12;
    s[0].values["s031_old_key_ms"] = 20;  // in the three oldest only
    s[1].values["s031_old_key_ms"] = 22;
    s[2].values["s031_old_key_ms"] = 21;
    const Values b = classBaseline(s);
    EXPECT_FALSE(b.count("s030_new_key_ms"));
    ASSERT_TRUE(b.count("s031_old_key_ms"));
    EXPECT_DOUBLE_EQ(b.at("s031_old_key_ms"), 21.0);
}

TEST(S029_RunnerClasses, UnreadableSamplesAreSkipped) {
    auto s = samples(4, 100);
    s[3].readable = false;
    s[3].values = result({{"open_1mb_loaded_ms", 1000}});  // must not count even if values were left
    const Values b = classBaseline(s);
    ASSERT_TRUE(b.count("open_1mb_loaded_ms"));
    EXPECT_DOUBLE_EQ(b.at("open_1mb_loaded_ms"), 100.0);
}

TEST(S029_RunnerClasses, AnchorIsTheMedianOfTheOldestThreeSamples) {
    auto s = samples(3, 100);
    const auto later = samples(2, 200, 2);
    s.insert(s.end(), later.begin(), later.end());
    const auto a = anchor(s, {});
    ASSERT_TRUE(a.has_value());
    EXPECT_DOUBLE_EQ(a.value_or(Values{}).at("open_1mb_loaded_ms"), 100.0);
}

TEST(S029_RunnerClasses, AnchorFormsAnewAfterTheNewestMarker) {
    auto s = samples(3, 100, 1);
    const auto after = samples(2, 130, 3);
    s.insert(s.end(), after.begin(), after.end());
    const std::vector<std::string> markers = {"2026-10-30T00:00:00Z", "2026-11-02T00:00:00Z"};
    EXPECT_FALSE(anchor(s, markers).has_value());  // two samples after the newest marker
    s.push_back(sample("2026-11-04T00:00:00Z", result({{"open_1mb_loaded_ms", 140}})));
    const auto a = anchor(s, markers);
    ASSERT_TRUE(a.has_value());
    EXPECT_DOUBLE_EQ(a.value_or(Values{}).at("open_1mb_loaded_ms"), 130.0);
}

// --- Comparisons --------------------------------------------------------------------------------

TEST(S029_RunnerClasses, ClassStageComparesWithToleranceAndTheTwoMillisecondRule) {
    const Values baseline = {{"open_1mb_loaded_ms", 100}, {"typing_small_p99_ms", 4}, {"typing_small_max_ms", 2}};
    const Values now = {
        {"open_1mb_loaded_ms", 126}, {"typing_small_p99_ms", 5.9}, {"typing_small_max_ms", 30}, {"s030_new_key_ms", 7}};
    const auto keys = compareWithBaseline(baseline, now);
    ASSERT_NE(find(keys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(keys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
    EXPECT_DOUBLE_EQ(find(keys, "open_1mb_loaded_ms")->was, 100.0);
    EXPECT_DOUBLE_EQ(find(keys, "open_1mb_loaded_ms")->now, 126.0);
    ASSERT_NE(find(keys, "typing_small_p99_ms"), nullptr);
    EXPECT_EQ(find(keys, "typing_small_p99_ms")->status, KeyStatus::Ok);  // +47 %, but under 2 ms
    ASSERT_NE(find(keys, "typing_small_max_ms"), nullptr);
    EXPECT_EQ(find(keys, "typing_small_max_ms")->status, KeyStatus::Info);  // report-only
    ASSERT_NE(find(keys, "s030_new_key_ms"), nullptr);
    EXPECT_EQ(find(keys, "s030_new_key_ms")->status, KeyStatus::NoBaseline);
    EXPECT_EQ(compareWithBaseline(baseline, {{"open_1mb_loaded_ms", 125}})[0].status, KeyStatus::Ok);
}

TEST(S029_RunnerClasses, MainStageFailsAKeyOnlyIfMeansDifferAndRoundsDoNotOverlap) {
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

TEST(S029_RunnerClasses, DriftComparesTheBaselineWithItsAnchor) {
    const auto keys = drift({{"open_1mb_loaded_ms", 100}, {"open_10mb_styled_ms", 2600}},
                            {{"open_1mb_loaded_ms", 130}, {"open_10mb_styled_ms", 2700}});
    ASSERT_NE(find(keys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(keys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
    ASSERT_NE(find(keys, "open_10mb_styled_ms"), nullptr);
    EXPECT_EQ(find(keys, "open_10mb_styled_ms")->status, KeyStatus::Ok);
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

TEST(S029_RunnerClasses, PathsThatChangeTheToolchainOrTheHarness) {
    EXPECT_TRUE(changesToolchain({"src/ui/editor_widget.cpp", ".github/workflows/ci.yml"}));
    EXPECT_TRUE(changesToolchain({"CMakePresets.json"}));
    EXPECT_TRUE(changesToolchain({"cmake/HitokotoDependencies.cmake"}));
    EXPECT_FALSE(changesToolchain({"src/core/markdown.cpp", "bench/hitokoto_bench.cpp", "docs/cmake.md"}));
    EXPECT_TRUE(changesHarness({"bench/hitokoto_bench.cpp"}));
    EXPECT_FALSE(changesHarness({"bench-data/x.json", "src/bench.cpp"}));
}

// --- What the writer checks ----------------------------------------------------------------------

TEST(S029_RunnerClasses, OnlyCompleteResultsWithTheirContextAreRecorded) {
    EXPECT_TRUE(isCompleteResult(result()));
    Values partial = result();
    partial.erase("save_10mb_total_ms");
    EXPECT_FALSE(isCompleteResult(partial));
    const std::map<std::string, std::string, std::less<>> context = {
        {"class", kClass}, {"model_name", "AMD EPYC 7763 64-Core Processor"},
        {"cpus", "4"},     {"commit", "705c578"},
        {"run_id", "123"}, {"attempt", "1"},
        {"event", "push"}, {"start_time", "2026-11-08T10:00:00Z"}};
    EXPECT_TRUE(missingContextFields(context).empty());
    auto incomplete = context;
    incomplete.erase("commit");
    incomplete["event"] = "";
    const auto missing = missingContextFields(incomplete);
    EXPECT_EQ(missing, (std::vector<std::string>{"commit", "event"}));
}

// --- Decisions for pull requests ------------------------------------------------------------------

TEST(S029_RunnerClasses, EstablishedClassWithinToleranceIsGreenAfterTheClassStage) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100);
    in.results = result({{"open_1mb_loaded_ms", 110}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_EQ(d.stage, Stage::Class);
}

TEST(S029_RunnerClasses, EstablishedClassSlowerThanItsBaselineNeedsTheMainStage) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100);
    in.results = result({{"open_1mb_loaded_ms", 140}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::NeedMainStage);
    ASSERT_NE(find(d.classKeys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.classKeys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, SlowerThanTheClassBaselineButNotThanMainIsGreen) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100);
    in.results = result({{"open_1mb_loaded_ms", 140}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 138}}), result({{"open_1mb_loaded_ms", 142}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 139}}), result({{"open_1mb_loaded_ms", 141}})};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_EQ(d.stage, Stage::Main);
    ASSERT_NE(find(d.classKeys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.classKeys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, ARealRegressionFailsInTheMainStage) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(1, 100);
    in.results = result({{"open_1mb_loaded_ms", 140}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 104}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}}), result({{"open_1mb_loaded_ms", 140}})};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    EXPECT_EQ(d.stage, Stage::Main);
    ASSERT_NE(find(d.mainKeys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.mainKeys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, AClassWithTooFewSamplesGoesToTheMainStage) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(2, 100);
    in.results = result({{"open_1mb_loaded_ms", 100}});
    const Decision without = decide(in);
    EXPECT_EQ(without.outcome, Outcome::NeedMainStage);
    EXPECT_TRUE(without.classKeys.empty());
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 101}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 102}})};
    const Decision with = decide(in);
    EXPECT_EQ(with.outcome, Outcome::Green);
    EXPECT_EQ(with.stage, Stage::Main);
}

TEST(S029_RunnerClasses, AnUnreadableSampleIsNamedInAWarning) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100);
    Sample broken;
    broken.file = "linux/" + std::string(kClass) + "/broken.json";
    broken.startTime = "2026-11-01T09:00:00Z";
    broken.readable = false;
    in.samples.push_back(broken);
    in.results = result({{"open_1mb_loaded_ms", 100}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(contains(d.warnings, "broken.json"));
}

TEST(S029_RunnerClasses, TheLabelAcceptsComparisonsButNotHardBudgets) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(1, 100);
    in.results = result({{"open_1mb_loaded_ms", 140}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 104}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 130}}), result({{"open_1mb_loaded_ms", 140}})};
    in.accepted = true;
    const Decision accepted = decide(in);
    EXPECT_EQ(accepted.outcome, Outcome::Green);
    EXPECT_TRUE(accepted.acceptedByLabel);
    in.results["save_10mb_ui_blocked_ms"] = 20;
    const Decision hard = decide(in);
    EXPECT_EQ(hard.outcome, Outcome::Fails);
    ASSERT_EQ(hard.hardBudgetMisses.size(), 1U);
    EXPECT_EQ(hard.hardBudgetMisses[0].key, "save_10mb_ui_blocked_ms");
}

TEST(S029_RunnerClasses, HardBudgetsOfThePullRequestsRoundsFailThoseOfTheBaseDoNot) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(1, 100);
    in.results = result({{"open_1mb_loaded_ms", 100}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}, {"typing_small_max_ms", 222}}),
                     result({{"open_1mb_loaded_ms", 101}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 101}})};
    EXPECT_EQ(decide(in).outcome, Outcome::Green);
    in.pullRequestRounds->at(1)["typing_small_max_ms"] = 54.2;
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    ASSERT_EQ(d.hardBudgetMisses.size(), 1U);
    EXPECT_EQ(d.hardBudgetMisses[0].key, "typing_small_max_ms");
}

TEST(S029_RunnerClasses, AToolchainChangeKeepsAClassStageFailureUnlessTheLabelIsSet) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100);
    in.results = result({{"open_1mb_loaded_ms", 140}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 139}}), result({{"open_1mb_loaded_ms", 141}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 140}}), result({{"open_1mb_loaded_ms", 141}})};
    in.changedPaths = {".github/workflows/ci.yml"};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    EXPECT_TRUE(contains(d.notices, "toolchain"));
    in.accepted = true;
    EXPECT_EQ(decide(in).outcome, Outcome::Green);
}

TEST(S029_RunnerClasses, AHarnessChangeIsNoticedButDoesNotFail) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(1, 100);
    in.results = result({{"open_1mb_loaded_ms", 100}});
    in.baseRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 101}})};
    in.pullRequestRounds = {result({{"open_1mb_loaded_ms", 100}}), result({{"open_1mb_loaded_ms", 101}})};
    in.changedPaths = {"bench/hitokoto_bench.cpp"};
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(contains(d.notices, "harness"));
}

// --- Decisions for runs of main -------------------------------------------------------------------

TEST(S029_RunnerClasses, RunsOfMainReportTheClassStageWithoutFailingOnIt) {
    Inputs in;
    in.kind = RunKind::Main;
    in.runnerClass = kClass;
    in.samples = samples(5, 100);
    in.markers = {"2026-11-02T00:00:00Z"};  // no anchor yet: no drift check
    in.results = result({{"open_1mb_loaded_ms", 200}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_EQ(d.stage, Stage::Class);
    ASSERT_NE(find(d.classKeys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_EQ(find(d.classKeys, "open_1mb_loaded_ms")->status, KeyStatus::Slow);
}

TEST(S029_RunnerClasses, DriftFailsARunOfMain) {
    Inputs in;
    in.kind = RunKind::Main;
    in.runnerClass = kClass;
    in.samples = samples(3, 100, 1);
    const auto slower = samples(2, 130, 2);
    in.samples.insert(in.samples.end(), slower.begin(), slower.end());
    in.results = result({{"open_1mb_loaded_ms", 130}});  // with it, the newest five: 100 100 130 130 130
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Fails);
    ASSERT_NE(find(d.driftKeys, "open_1mb_loaded_ms"), nullptr);
    EXPECT_DOUBLE_EQ(find(d.driftKeys, "open_1mb_loaded_ms")->was, 100.0);
    EXPECT_DOUBLE_EQ(find(d.driftKeys, "open_1mb_loaded_ms")->now, 130.0);
}

TEST(S029_RunnerClasses, DriftIsInformationInAPullRequestRun) {
    Inputs in;
    in.runnerClass = kClass;
    in.samples = samples(3, 100, 1);
    const auto slower = samples(3, 130, 2);
    in.samples.insert(in.samples.end(), slower.begin(), slower.end());
    in.results = result({{"open_1mb_loaded_ms", 130}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_NE(find(d.driftKeys, "open_1mb_loaded_ms"), nullptr);
}

TEST(S029_RunnerClasses, AnAcceptedSlowdownEndsTheDrift) {
    Inputs in;
    in.kind = RunKind::Main;
    in.runnerClass = kClass;
    in.samples = samples(3, 100, 1);
    const auto slower = samples(3, 130, 3);
    in.samples.insert(in.samples.end(), slower.begin(), slower.end());
    in.markers = {"2026-11-02T00:00:00Z"};
    in.results = result({{"open_1mb_loaded_ms", 130}});
    const Decision d = decide(in);
    EXPECT_EQ(d.outcome, Outcome::Green);
    EXPECT_TRUE(d.driftKeys.empty());
}
