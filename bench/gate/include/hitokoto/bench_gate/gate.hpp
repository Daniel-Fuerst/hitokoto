// SPDX-License-Identifier: GPL-3.0-or-later
// S029 / ADR 0009: the decisions of the Linux benchmark gate, standard library only. Every pull
// request run measures the base commit and the pull request in turn on the same runner (base, pull
// request, pull request, base) and compares the rounds; runs of main check the hard budgets. A
// toolchain change comes in its own pull request. The runner class from /proc/cpuinfo and the cache
// sizes is reported for information. Files, JSON and the workflow stay outside (hitokoto-bench-gate,
// ci.yml).
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

inline constexpr double kTolerance = 0.25;     ///< allowed slowdown of the mean
inline constexpr double kMinDeltaMs = 2.0;     ///< smaller differences are timer and scheduler noise
inline constexpr double kFrameDeltaMs = 20.0;  ///< first-screen keys jump by one frame (~17 ms) on one runner

/// The runner class: linux-<model>-<n>cpu-l2-<l2>-l3-<l3>. `cpuinfo` is the text of /proc/cpuinfo,
/// `cpus` the CPUs the job may use (nproc), `l2`/`l3` the sizes from /sys/.../cache/index2 and index3
/// as given there (e.g. "512K"). Without a non-empty model name the model part is "unknown".
std::string runnerClass(std::string_view cpuinfo, int cpus, std::string_view l2, std::string_view l3);

/// The first non-empty model name of /proc/cpuinfo as written there, for the report; empty if none.
std::string modelName(std::string_view cpuinfo);

/// The smallest difference that counts for a key: kFrameDeltaMs for the first screen of an opening
/// (open_*_first_chunk_ms, s023_open_10mb_first_chunk_ms, s024_open_10mb_properties_ms), whose
/// single opens lie on two levels one frame apart on one runner (PR #31); kMinDeltaMs for all others.
double minDeltaMs(std::string_view key);

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

enum class KeyStatus {
    Ok,       ///< within tolerance, or the two sides overlap
    Slow,     ///< slower than tolerance and no overlap: fails
    Info,     ///< slower than tolerance and no overlap, but a report-only key
    OneSide,  ///< the key exists on one side only
};

struct KeyResult {
    std::string key;
    KeyStatus status = KeyStatus::Ok;
    std::array<double, 4> rounds{};  ///< in measuring order: base 1, pull request 1, pull request 2, base 2
    double was = 0;                  ///< mean of the base's rounds
    double now = 0;                  ///< mean of the pull request's rounds
};

/// Two rounds per side. A key is Slow only if the mean of the pull request's rounds is more than
/// kTolerance and minDeltaMs(key) above the base's mean and its faster round is slower than the
/// base's slower round. `rounds` holds the four values in measuring order.
std::vector<KeyResult> compareRounds(const std::array<Values, 2>& base, const std::array<Values, 2>& pullRequest);

/// True if a changed path changes the toolchain in the repository (CMakePresets.json, cmake/). The
/// Qt version and the runner image of the bench job in ci.yml are compared by the workflow.
bool changesToolchainFiles(const std::vector<std::string>& changedPaths);

/// The changed paths that are not allowed beside a toolchain change: everything except files under
/// docs/ and files named *.md, and except the toolchain files themselves and ci.yml.
std::vector<std::string> pathsBesideToolchain(const std::vector<std::string>& changedPaths);

/// True if a changed path is under bench/ but not bench/gate/ or bench/baselines/ (the two sides
/// are measured by different harnesses).
bool changesHarness(const std::vector<std::string>& changedPaths);

enum class RunKind { PullRequest, Main };

enum class Outcome {
    Green = 0,
    Fails = 1,
};

/// A hard budget missed by one round of the base: information only.
struct RoundMiss {
    Budget budget;
    int round = 0;  ///< 1 or 2: the first or the second round of the base
    double value = 0;
};

struct Inputs {
    RunKind kind = RunKind::PullRequest;
    Values results;  ///< runs of main: the single run of this job
    std::optional<std::array<Values, 2>> baseRounds;
    std::optional<std::array<Values, 2>> pullRequestRounds;
    std::vector<std::string> changedPaths;  ///< paths the pull request changes against its base
    bool qtVersionChanged = false;          ///< the Qt version of the bench job differs from the base's
    bool imageChanged = false;              ///< the runner image (os) of the bench job differs from the base's
    bool accepted = false;                  ///< the pull request carries bench-accepted
};

struct Decision {
    Outcome outcome = Outcome::Fails;
    std::vector<KeyResult> keys;              ///< the comparison of the rounds (pull request runs)
    std::vector<Budget> hardBudgetMisses;     ///< of the pull request's rounds, or of the run of main
    std::vector<RoundMiss> baseBudgetMisses;  ///< of the base's rounds: information
    bool toolchainChange = false;             ///< Qt version, image, CMakePresets.json or cmake/
    std::vector<std::string> mixedPaths;      ///< paths beside a toolchain change: the job fails
    bool acceptedByLabel = false;             ///< comparison failures that the label accepted
    std::vector<std::string> notices;         ///< Qt version, image or harness changed
};

/// The decision of the gate for one run (S029, rules "Pull request run" and "Runs of main"). A pull
/// request run without both rounds fails.
Decision decide(const Inputs& inputs);

}  // namespace hitokoto::bench_gate
