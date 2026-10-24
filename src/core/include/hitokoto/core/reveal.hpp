// SPDX-License-Identifier: GPL-3.0-or-later
// Which lines show raw syntax, and which lines must be re-rendered when that changes.
#pragma once

#include <cstdint>
#include <vector>

namespace hitokoto::core {

/// Inclusive range of line numbers. `first > last` means empty.
struct LineRange {
    std::int64_t first = 0;
    std::int64_t last = -1;
    [[nodiscard]] bool empty() const noexcept { return first > last; }
    [[nodiscard]] bool contains(std::int64_t line) const noexcept { return line >= first && line <= last; }
    friend bool operator==(const LineRange&, const LineRange&) = default;
};

/// Lines whose markers are revealed: every line touched by the selection, or the caret line.
/// `anchorLine` and `caretLine` are the lines of the selection ends (equal without selection).
LineRange revealedLines(std::int64_t anchorLine, std::int64_t caretLine) noexcept;

/// Lines that enter or leave the revealed range when it changes from `before` to `after`.
/// These, and only these, need re-rendering. Result ranges are disjoint and sorted.
std::vector<LineRange> revealChanges(LineRange before, LineRange after);

}  // namespace hitokoto::core
