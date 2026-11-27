// SPDX-License-Identifier: GPL-3.0-or-later
// S051: task list items (`- [ ] milk`, `- [x] bread`) as lines: where the box is, what a click on
// it writes, what Backspace before the text removes and what Enter continues with.
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hitokoto::core {

struct TaskLineInfo {
    std::uint32_t markerChar = 0;    ///< column of '-', '*' or '+'
    std::uint32_t box = 0;           ///< column of '['
    std::uint32_t boxLength = 0;     ///< 3 for `[ ]`, `[x]`, `[X]`; 2 for `[]`
    std::uint32_t contentStart = 0;  ///< first character of the text (after the blanks behind `]`)
    bool checked = false;            ///< `[x]` or `[X]`
    bool isEmpty = false;            ///< nothing after the box and its blanks
    /// The box and the blanks after it, what Backspace before the text removes (the line stays a bullet).
    [[nodiscard]] Span boxWithBlanks() const noexcept { return {box, contentStart - box}; }
    friend bool operator==(const TaskLineInfo&, const TaskLineInfo&) = default;
};

/// If `line` is a task list item (a bullet item whose text starts with `[ ]`, `[x]`, `[X]` or `[]`
/// followed by a blank), where its parts are; else nullopt.
[[nodiscard]] std::optional<TaskLineInfo> parseTaskLine(std::u16string_view line);

/// A replacement of characters within one line.
struct LineEdit {
    Span span;
    std::u16string text;
    friend bool operator==(const LineEdit&, const LineEdit&) = default;
};

/// What a click on the box writes: `[ ]` and `[]` become `[x]`, `[x]` and `[X]` become `[ ]`.
[[nodiscard]] std::optional<LineEdit> toggledTask(std::u16string_view line);

/// The start of the next item when Enter continues a task item: its indentation, marker and blanks
/// with an unchecked box (`  * [x] done` -> `  * [ ] `); empty if `line` is no task item.
[[nodiscard]] std::u16string continuedTaskPrefix(std::u16string_view line);

}  // namespace hitokoto::core
