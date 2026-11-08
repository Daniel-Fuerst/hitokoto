// SPDX-License-Identifier: GPL-3.0-or-later
// S029 / ADR 0009: the decisions of the Linux benchmark gate.
#include "hitokoto/bench_gate/gate.hpp"

#include <algorithm>
#include <set>
#include <string_view>

namespace hitokoto::bench_gate {

namespace {

constexpr std::string_view kUnknownPrefix = "linux-unknown-";

// Compared and reported but never failing: maxima of few samples, counts of samples over 16 ms,
// settle times in idle slices, the time until the saved file is on disk and the p95 of 10 cold
// starts (bench/baselines/README.md). Hard budgets cover the absolute limits instead.
constexpr std::string_view kReportOnly[] = {"save_10mb_total_ms",
                                            "typing_small_max_ms",
                                            "typing_10mb_max_ms",
                                            "s018_table_typing_max_ms",
                                            "s021_codeblock_caret_max_ms",
                                            "s021_codeblock_enter_max_ms",
                                            "s021_codeblock_typing_max_ms",
                                            "typing_small_over_16ms",
                                            "typing_10mb_over_16ms",
                                            "s018_table_typing_over_16ms",
                                            "s021_codeblock_caret_over_16ms",
                                            "s021_codeblock_typing_over_16ms",
                                            "s023_break_typing_max_ms",
                                            "s023_break_typing_over_16ms",
                                            "s023_bullet_typing_max_ms",
                                            "s023_bullet_settled_ms",
                                            "s023_underline_typing_max_ms",
                                            "s023_underline_settled_ms",
                                            "s024_typing_max_ms",
                                            "s024_typing_over_16ms",
                                            "s024_enter_leave_max_ms",
                                            "s024_leave_5000_keys_max_ms",
                                            "s024_leave_5000_nested_max_ms",
                                            "s024_long_text_typing_max_ms",
                                            "s024_long_text_typing_over_16ms",
                                            "s024_leave_long_text_max_ms",
                                            "s028_field_typing_max_ms",
                                            "s028_field_typing_over_16ms",
                                            "s028_full_count_ms",
                                            "s028_step_max_ms",
                                            "s028_doc_typing_max_ms",
                                            "s028_doc_typing_over_16ms",
                                            "s028_typing_settled_ms",
                                            "s028_step_incomplete_max_ms",
                                            "s028_table_field_typing_max_ms",
                                            "s028_table_step_max_ms",
                                            "startup_p95_ms"};

constexpr std::string_view kContextFields[] = {"class",  "model_name", "cpus",  "commit",
                                               "run_id", "attempt",    "event", "start_time"};

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Lower case a-z and 0-9; every run of other characters becomes one '-', none at the ends.
std::string slug(std::string_view s) {
    std::string out;
    bool pendingHyphen = false;
    for (const char c : s) {
        const bool lower = c >= 'a' && c <= 'z';
        const bool upper = c >= 'A' && c <= 'Z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !upper && !digit) {
            pendingHyphen = !out.empty();
            continue;
        }
        if (pendingHyphen) {
            out += '-';
            pendingHyphen = false;
        }
        out += upper ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return out;
}

std::string firstModelName(std::string_view cpuinfo) {
    std::size_t pos = 0;
    while (pos < cpuinfo.size()) {
        const auto end = std::min(cpuinfo.find('\n', pos), cpuinfo.size());
        const std::string_view line = cpuinfo.substr(pos, end - pos);
        pos = end + 1;
        const auto colon = line.find(':');
        if (colon != std::string_view::npos && trim(line.substr(0, colon)) == "model name") {
            return std::string(trim(line.substr(colon + 1)));
        }
    }
    return {};
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 == 1 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

/// Readable samples, oldest first.
std::vector<const Sample*> readableByTime(const std::vector<Sample>& samples) {
    std::vector<const Sample*> r;
    for (const auto& s : samples) {
        if (s.readable) {
            r.push_back(&s);
        }
    }
    std::stable_sort(r.begin(), r.end(), [](const Sample* a, const Sample* b) { return a->startTime < b->startTime; });
    return r;
}

bool slower(std::string_view key, double was, double now) {
    return now > was * (1 + kTolerance) && now - was > minDeltaMs(key);
}

KeyStatus slowStatus(std::string_view key) {
    return isReportOnly(key) ? KeyStatus::Info : KeyStatus::Slow;
}

bool anySlow(const std::vector<KeyResult>& keys) {
    return std::any_of(keys.begin(), keys.end(), [](const KeyResult& k) { return k.status == KeyStatus::Slow; });
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.substr(0, prefix.size()) == prefix;
}

void addMisses(std::vector<Budget>& into, const Values& values) {
    for (auto& miss : hardBudgetMisses(values)) {
        const bool known = std::any_of(into.begin(), into.end(), [&](const Budget& b) { return b.key == miss.key; });
        if (!known) {
            into.push_back(std::move(miss));
        }
    }
}

}  // namespace

std::string runnerClass(std::string_view cpuinfo, int cpus, std::string_view l2, std::string_view l3) {
    std::string model = slug(firstModelName(cpuinfo));
    if (model.empty()) {
        model = "unknown";
    }
    return "linux-" + model + "-" + std::to_string(cpus) + "cpu-l2-" + slug(l2) + "-l3-" + slug(l3);
}

std::string modelName(std::string_view cpuinfo) {
    return firstModelName(cpuinfo);
}

bool isUnknownClass(std::string_view runnerClass) {
    return startsWith(runnerClass, kUnknownPrefix);
}

double minDeltaMs(std::string_view key) {
    const bool firstChunk = key.starts_with("open_") && key.ends_with("_first_chunk_ms");
    return firstChunk || key == "s023_open_10mb_first_chunk_ms" || key == "s024_open_10mb_properties_ms" ? kFrameDeltaMs
                                                                                                         : kMinDeltaMs;
}

bool isReportOnly(std::string_view key) {
    return std::find(std::begin(kReportOnly), std::end(kReportOnly), key) != std::end(kReportOnly);
}

const std::vector<Budget>& hardBudgets() {
    static const std::vector<Budget> budgets = {
        {"typing_small_max_ms", 50, "no key event above 50 ms (hard)"},
        {"typing_10mb_max_ms", 50, "no key event above 50 ms, 10 MB (hard)"},
        {"s018_table_typing_max_ms", 50, "no key event above 50 ms, table (hard)"},
        {"s021_codeblock_caret_max_ms", 50, "no caret move above 50 ms, code block (hard)"},
        {"s021_codeblock_enter_max_ms", 50, "entering a 10 MB code block <= 50 ms (hard)"},
        {"s021_codeblock_typing_max_ms", 50, "no key event above 50 ms, code block (hard)"},
        {"s023_break_typing_max_ms", 50, "no key event above 50 ms, break (hard)"},
        {"s023_bullet_typing_max_ms", 50, "no key event above 50 ms, \"- \" before a paragraph (hard)"},
        {"s023_underline_typing_max_ms", 50, "no key event above 50 ms, underline below a paragraph (hard)"},
        {"s024_typing_max_ms", 50, "no key event above 50 ms, below front matter (hard)"},
        {"s024_enter_leave_max_ms", 50, "entering and leaving front matter <= 50 ms (hard)"},
        {"s024_leave_5000_keys_max_ms", 50, "leaving 5,000 edited properties <= 50 ms (hard)"},
        {"s024_leave_5000_nested_max_ms", 50, "leaving a nested value of 5,000 lines <= 50 ms (hard)"},
        {"s024_long_text_typing_max_ms", 50, "no key event above 50 ms, below a long value (hard)"},
        {"s024_leave_long_text_max_ms", 50, "leaving a text value of 2,000 lines <= 50 ms (hard)"},
        {"s028_field_typing_max_ms", 50, "no key in the find bar above 50 ms, 10 MB (hard)"},
        {"s028_step_max_ms", 50, "no next match above 50 ms, 10 MB (hard)"},
        {"s028_step_incomplete_max_ms", 50, "no next match above 50 ms before the count is complete, 10 MB (hard)"},
        {"s028_table_field_typing_max_ms", 50,
         "no key in the find bar above 50 ms, 1000-row table, highlight all (hard)"},
        {"s028_table_step_max_ms", 50, "no next match above 50 ms, 1000-row table, highlight all (hard)"},
        {"s028_doc_typing_max_ms", 50, "no key event above 50 ms, 10 MB with find open (hard)"},
        {"save_10mb_ui_blocked_ms", 16, "saving never stalls the UI (hard)"},
        {"save_10mb_total_ms", 1000, "10 MB on disk within 1 s (hard)"},
    };
    return budgets;
}

std::vector<Budget> hardBudgetMisses(const Values& values) {
    std::vector<Budget> misses;
    for (const auto& b : hardBudgets()) {
        const auto it = values.find(b.key);
        if (it != values.end() && it->second > b.limit) {
            misses.push_back(b);
        }
    }
    return misses;
}

bool isCompleteResult(const Values& values) {
    const auto& budgets = hardBudgets();
    return std::all_of(budgets.begin(), budgets.end(), [&](const Budget& b) { return values.count(b.key) > 0; });
}

std::vector<std::string> missingContextFields(const std::map<std::string, std::string, std::less<>>& context) {
    std::vector<std::string> missing;
    for (const auto field : kContextFields) {
        const auto it = context.find(field);
        if (it == context.end() || trim(it->second).empty()) {
            missing.emplace_back(field);
        }
    }
    return missing;
}

bool isEstablished(std::string_view runnerClass, const std::vector<Sample>& samples) {
    return !isUnknownClass(runnerClass) && readableByTime(samples).size() >= static_cast<std::size_t>(kMinSamples);
}

Values classBaseline(const std::vector<Sample>& samples) {
    const auto byTime = readableByTime(samples);
    std::set<std::string, std::less<>> keys;
    for (const auto* s : byTime) {
        for (const auto& [key, value] : s->values) {
            keys.insert(key);
        }
    }
    Values baseline;
    for (const auto& key : keys) {
        std::vector<double> newest;
        for (auto it = byTime.rbegin(); it != byTime.rend() && newest.size() < static_cast<std::size_t>(kWindow);
             ++it) {
            const auto found = (*it)->values.find(key);
            if (found != (*it)->values.end()) {
                newest.push_back(found->second);
            }
        }
        if (newest.size() >= static_cast<std::size_t>(kMinSamples)) {
            baseline[key] = median(newest);
        }
    }
    return baseline;
}

std::optional<Values> anchor(const std::vector<Sample>& samples, const std::vector<std::string>& markerTimes) {
    std::string newestMarker;
    for (const auto& m : markerTimes) {
        newestMarker = std::max(newestMarker, m);
    }
    std::vector<const Sample*> after;
    for (const auto* s : readableByTime(samples)) {
        if (s->startTime > newestMarker) {
            after.push_back(s);
        }
    }
    if (after.size() < static_cast<std::size_t>(kMinSamples)) {
        return std::nullopt;
    }
    after.resize(static_cast<std::size_t>(kMinSamples));
    Values result;
    for (const auto& [key, value] : after.front()->values) {
        std::vector<double> values;
        for (const auto* s : after) {
            const auto found = s->values.find(key);
            if (found != s->values.end()) {
                values.push_back(found->second);
            }
        }
        if (values.size() == after.size()) {
            result[key] = median(values);
        }
    }
    return result;
}

std::vector<KeyResult> compareWithBaseline(const Values& baseline, const Values& now) {
    std::vector<KeyResult> keys;
    for (const auto& [key, value] : now) {
        const auto found = baseline.find(key);
        if (found == baseline.end()) {
            keys.push_back({key, KeyStatus::NoBaseline, 0, value});
            continue;
        }
        keys.push_back(
            {key, slower(key, found->second, value) ? slowStatus(key) : KeyStatus::Ok, found->second, value});
    }
    return keys;
}

std::vector<KeyResult> compareRounds(const std::array<Values, 2>& base, const std::array<Values, 2>& pullRequest) {
    std::set<std::string, std::less<>> keys;
    for (const auto* side : {&base, &pullRequest}) {
        for (const auto& round : *side) {
            for (const auto& [key, value] : round) {
                keys.insert(key);
            }
        }
    }
    const auto both = [](const std::array<Values, 2>& side, const std::string& key) {
        return side[0].count(key) > 0 && side[1].count(key) > 0;
    };
    std::vector<KeyResult> results;
    for (const auto& key : keys) {
        if (!both(base, key) || !both(pullRequest, key)) {
            results.push_back({key, KeyStatus::OneSide, 0, 0});
            continue;
        }
        const double b0 = base[0].at(key);
        const double b1 = base[1].at(key);
        const double p0 = pullRequest[0].at(key);
        const double p1 = pullRequest[1].at(key);
        const double was = (b0 + b1) / 2;
        const double now = (p0 + p1) / 2;
        const bool apart = std::min(p0, p1) > std::max(b0, b1);
        results.push_back({key, slower(key, was, now) && apart ? slowStatus(key) : KeyStatus::Ok, was, now});
    }
    return results;
}

std::vector<KeyResult> drift(const Values& anchorValues, const Values& baseline) {
    std::vector<KeyResult> keys;
    for (const auto& [key, was] : anchorValues) {
        const auto found = baseline.find(key);
        if (found != baseline.end()) {
            keys.push_back(
                {key, slower(key, was, found->second) ? slowStatus(key) : KeyStatus::Ok, was, found->second});
        }
    }
    return keys;
}

bool changesToolchain(const std::vector<std::string>& changedPaths) {
    return std::any_of(changedPaths.begin(), changedPaths.end(), [](const std::string& p) {
        return p == ".github/workflows/ci.yml" || p == "CMakePresets.json" || startsWith(p, "cmake/");
    });
}

bool changesHarness(const std::vector<std::string>& changedPaths) {
    return std::any_of(changedPaths.begin(), changedPaths.end(), [](const std::string& p) {
        return startsWith(p, "bench/") && !startsWith(p, "bench/gate/") && !startsWith(p, "bench/baselines/");
    });
}

Decision decide(const Inputs& in) {
    Decision d;
    addMisses(d.hardBudgetMisses, in.results);
    for (const auto& s : in.samples) {
        if (!s.readable) {
            d.warnings.push_back("unreadable sample skipped: " + s.file);
        }
    }
    const bool toolchain = changesToolchain(in.changedPaths);
    if (in.kind == RunKind::PullRequest) {
        if (toolchain) {
            d.notices.emplace_back(
                "the pull request changes the toolchain: both sides of the main stage are built with it");
        }
        if (changesHarness(in.changedPaths)) {
            d.notices.emplace_back("the pull request changes the harness: the two sides are measured differently");
        }
    }

    // Drift: the class baseline with this run as its newest sample, against the anchor.
    if (const auto anchorValues = anchor(in.samples, in.markers)) {
        auto withRun = in.samples;
        withRun.push_back({"(this run)", "9999-12-31T23:59:59Z", in.results, true});
        for (auto& k : drift(*anchorValues, classBaseline(withRun))) {
            if (k.status == KeyStatus::Slow) {
                d.driftKeys.push_back(std::move(k));
            }
        }
    }

    const bool established = isEstablished(in.runnerClass, in.samples);
    if (established) {
        d.classKeys = compareWithBaseline(classBaseline(in.samples), in.results);
        d.stage = Stage::Class;
    }
    const bool classSlow = anySlow(d.classKeys);

    if (in.kind == RunKind::Main) {
        d.outcome = d.hardBudgetMisses.empty() && d.driftKeys.empty() ? Outcome::Green : Outcome::Fails;
        return d;
    }
    if (established && !classSlow) {
        d.outcome = d.hardBudgetMisses.empty() ? Outcome::Green : Outcome::Fails;
        return d;
    }
    if (!d.hardBudgetMisses.empty()) {
        d.outcome = Outcome::Fails;
        return d;
    }
    if (!in.baseRounds || !in.pullRequestRounds) {
        d.outcome = Outcome::NeedMainStage;
        return d;
    }

    d.stage = Stage::Main;
    d.mainKeys = compareRounds(*in.baseRounds, *in.pullRequestRounds);
    for (const auto& round : *in.pullRequestRounds) {
        addMisses(d.hardBudgetMisses, round);
    }
    bool comparisonFails = anySlow(d.mainKeys) || (classSlow && toolchain);
    if (comparisonFails && in.accepted) {
        d.acceptedByLabel = true;
        comparisonFails = false;
    }
    d.outcome = comparisonFails || !d.hardBudgetMisses.empty() ? Outcome::Fails : Outcome::Green;
    return d;
}

}  // namespace hitokoto::bench_gate
