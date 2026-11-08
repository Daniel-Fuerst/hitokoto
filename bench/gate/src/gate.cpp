// SPDX-License-Identifier: GPL-3.0-or-later
// S029: stubs for the red tests; the decisions follow in the implementation.
#include "hitokoto/bench_gate/gate.hpp"

namespace hitokoto::bench_gate {

std::string runnerClass(std::string_view /*cpuinfo*/, int /*cpus*/, std::string_view /*l2*/, std::string_view /*l3*/) {
    return {};
}

bool isUnknownClass(std::string_view /*runnerClass*/) {
    return false;
}

bool isReportOnly(std::string_view /*key*/) {
    return false;
}

const std::vector<Budget>& hardBudgets() {
    static const std::vector<Budget> budgets;
    return budgets;
}

std::vector<Budget> hardBudgetMisses(const Values& /*values*/) {
    return {};
}

bool isCompleteResult(const Values& /*values*/) {
    return false;
}

std::vector<std::string> missingContextFields(const std::map<std::string, std::string, std::less<>>& /*context*/) {
    return {};
}

bool isEstablished(std::string_view /*runnerClass*/, const std::vector<Sample>& /*samples*/) {
    return false;
}

Values classBaseline(const std::vector<Sample>& /*samples*/) {
    return {};
}

std::optional<Values> anchor(const std::vector<Sample>& /*samples*/, const std::vector<std::string>& /*markerTimes*/) {
    return std::nullopt;
}

std::vector<KeyResult> compareWithBaseline(const Values& /*baseline*/, const Values& /*now*/) {
    return {};
}

std::vector<KeyResult> compareRounds(const std::array<Values, 2>& /*base*/,
                                     const std::array<Values, 2>& /*pullRequest*/) {
    return {};
}

std::vector<KeyResult> drift(const Values& /*anchorValues*/, const Values& /*baseline*/) {
    return {};
}

bool changesToolchain(const std::vector<std::string>& /*changedPaths*/) {
    return false;
}

bool changesHarness(const std::vector<std::string>& /*changedPaths*/) {
    return false;
}

Decision decide(const Inputs& /*inputs*/) {
    return {};
}

}  // namespace hitokoto::bench_gate
