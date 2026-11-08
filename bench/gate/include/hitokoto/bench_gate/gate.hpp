// SPDX-License-Identifier: GPL-3.0-or-later
// S029 / ADR 0009: the decisions of the Linux benchmark gate, standard library only. A runner class
// from /proc/cpuinfo and the cache sizes; class baselines from samples of runs of main; the class
// stage against the baseline, the main stage against the base commit measured on the same runner;
// an anchor per class against slow drift; the label for accepted slowdowns. Files, JSON and the
// workflow stay outside (hitokoto-bench-gate, ci.yml).
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::bench_gate {

/// Results of one run of hitokoto-bench: key -> value (ms or count), as in e2e-bench.json.
using Values = std::map<std::string, double, std::less<>>;

inline constexpr int kMinSamples = 3;       ///< samples before a class is established (and for an anchor)
inline constexpr int kWindow = 5;           ///< newest samples per key in a class baseline
inline constexpr double kTolerance = 0.25;  ///< allowed slowdown in both stages and for drift
inline constexpr double kMinDeltaMs = 2.0;  ///< smaller differences are timer and scheduler noise

/// The runner class: linux-<model>-<n>cpu-l2-<l2>-l3-<l3>. `cpuinfo` is the text of /proc/cpuinfo,
/// `cpus` the CPUs the job may use (nproc), `l2`/`l3` the sizes from /sys/.../cache/index2 and index3
/// as given there (e.g. "512K"). Without a non-empty model name the model part is "unknown".
std::string runnerClass(std::string_view cpuinfo, int cpus, std::string_view l2, std::string_view l3);

/// True for a class whose model part is "unknown"; such a class never becomes established.
bool isUnknownClass(std::string_view runnerClass);

/// Keys that are compared and reported but never fail a comparison (maxima, counts, settle times).
bool isReportOnly(std::string_view key);

struct Budget {
    std::string key;
    double limit = 0;
    std::string text;
};

/// The hard budgets of the brief that hold on every machine (saving, key events above 50 ms).
const std::vector<Budget>& hardBudgets();

/// Hard budgets the values miss (a key that is missing does not count as a miss).
std::vector<Budget> hardBudgetMisses(const Values& values);

/// True if the values hold every key of the hard budgets: the run finished and wrote its results.
bool isCompleteResult(const Values& values);

/// Context fields a sample must carry: class, model_name, cpus, commit, run_id, attempt, event,
/// start_time. Returns the missing or empty ones.
std::vector<std::string> missingContextFields(const std::map<std::string, std::string, std::less<>>& context);

/// One sample of a class: a run of main. `startTime` is run_started_at in UTC (ISO 8601, so that
/// text order is time order). An unreadable file is a sample with `readable` false.
struct Sample {
    std::string file;
    std::string startTime;
    Values values;
    bool readable = true;
};

/// True if the class has at least kMinSamples readable samples and is not an unknown class.
bool isEstablished(std::string_view runnerClass, const std::vector<Sample>& samples);

/// Per key the median over the newest kWindow readable samples that contain it; keys found in
/// fewer than kMinSamples samples have no baseline.
Values classBaseline(const std::vector<Sample>& samples);

/// Per key the median over the oldest kMinSamples readable samples newer than the newest marker
/// (start times of acceptance markers); none while fewer than kMinSamples such samples exist.
std::optional<Values> anchor(const std::vector<Sample>& samples, const std::vector<std::string>& markerTimes);

enum class KeyStatus {
    Ok,          ///< within tolerance
    Slow,        ///< slower than tolerance: fails
    Info,        ///< slower than tolerance, but a report-only key
    NoBaseline,  ///< the key has no class baseline (yet)
    OneSide,     ///< main stage: the key exists on one side only
};

struct KeyResult {
    std::string key;
    KeyStatus status = KeyStatus::Ok;
    double was = 0;  ///< baseline, mean of the base rounds, or anchor
    double now = 0;  ///< this run, mean of the pull request's rounds, or class baseline
};

/// Class stage: every key of `now`, compared with the class baseline.
std::vector<KeyResult> compareWithBaseline(const Values& baseline, const Values& now);

/// Main stage: two rounds per side. A key is Slow only if the mean of the pull request's rounds is
/// more than kTolerance and kMinDeltaMs above the base's mean and its faster round is slower than
/// the base's slower round.
std::vector<KeyResult> compareRounds(const std::array<Values, 2>& base, const std::array<Values, 2>& pullRequest);

/// Drift: the class baseline against its anchor, with the tolerance rule of the class stage.
std::vector<KeyResult> drift(const Values& anchorValues, const Values& baseline);

/// True if a changed path is part of the toolchain (ci.yml, CMakePresets.json, cmake/).
bool changesToolchain(const std::vector<std::string>& changedPaths);

/// True if a changed path is under bench/ (the two sides use different harnesses).
bool changesHarness(const std::vector<std::string>& changedPaths);

enum class RunKind { PullRequest, Main };

enum class Outcome {
    Green = 0,
    Fails = 1,
    NeedMainStage = 3,  ///< the class stage cannot decide: build the base and measure the rounds
};

enum class Stage { None, Class, Main };

struct Inputs {
    RunKind kind = RunKind::PullRequest;
    std::string runnerClass;
    Values results;                    ///< the run of this job (step 1)
    std::vector<Sample> samples;       ///< the samples of this class on bench-data
    std::vector<std::string> markers;  ///< start times of the acceptance markers
    std::optional<std::array<Values, 2>> baseRounds;
    std::optional<std::array<Values, 2>> pullRequestRounds;
    std::vector<std::string> changedPaths;  ///< paths the pull request changes against its base
    bool accepted = false;                  ///< the pull request carries bench-accepted
};

struct Decision {
    Outcome outcome = Outcome::Fails;
    Stage stage = Stage::None;             ///< the stage that decided
    std::vector<KeyResult> classKeys;      ///< class stage, if it ran
    std::vector<KeyResult> mainKeys;       ///< main stage, if it ran
    std::vector<KeyResult> driftKeys;      ///< keys that drifted from the anchor
    std::vector<Budget> hardBudgetMisses;  ///< of this run and of the pull request's rounds
    bool acceptedByLabel = false;          ///< comparison failures that the label accepted
    std::vector<std::string> notices;      ///< e.g. toolchain or harness changed
    std::vector<std::string> warnings;     ///< e.g. an unreadable sample, by file name
};

/// The decision of the gate for one run (S029, rules "Pull request run" and "Runs of main").
Decision decide(const Inputs& inputs);

}  // namespace hitokoto::bench_gate
