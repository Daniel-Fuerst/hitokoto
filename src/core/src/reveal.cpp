// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/reveal.hpp"

#include <algorithm>

namespace hitokoto::core {

LineRange revealedLines(std::int64_t anchorLine, std::int64_t caretLine) noexcept {
    return {std::min(anchorLine, caretLine), std::max(anchorLine, caretLine)};
}

std::vector<LineRange> revealChanges(LineRange before, LineRange after) {
    // Symmetric difference of two intervals.
    std::vector<LineRange> parts;
    const auto addDifference = [&parts](LineRange a, LineRange b) {  // a minus b
        if (a.empty()) {
            return;
        }
        if (b.empty() || b.last < a.first || b.first > a.last) {
            parts.push_back(a);
            return;
        }
        if (a.first < b.first) {
            parts.push_back({a.first, b.first - 1});
        }
        if (a.last > b.last) {
            parts.push_back({b.last + 1, a.last});
        }
    };
    addDifference(before, after);
    addDifference(after, before);
    std::sort(parts.begin(), parts.end(), [](const LineRange& x, const LineRange& y) { return x.first < y.first; });
    // Merge touching ranges.
    std::vector<LineRange> merged;
    for (const auto& p : parts) {
        if (!merged.empty() && p.first <= merged.back().last + 1) {
            merged.back().last = std::max(merged.back().last, p.last);
        } else {
            merged.push_back(p);
        }
    }
    return merged;
}

}  // namespace hitokoto::core
