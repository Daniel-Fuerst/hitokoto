// SPDX-License-Identifier: GPL-3.0-or-later
// Ordered list items as lines (S053): the prefix of an item, its indentation, and renumbering the
// list an item belongs to.
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

struct OrderedLineInfo {
    std::uint32_t column = 0;        ///< column of the first digit (a tab advances to the next multiple of 4)
    Span digits;                     ///< the number as written
    std::uint32_t number = 0;        ///< its value (at most 9 digits, CommonMark 5.2)
    char16_t delimiter = 0;          ///< '.' or ')'
    std::uint32_t prefixLength = 0;  ///< up to the content, blanks after the delimiter included
    bool isEmpty = false;            ///< nothing but blanks after the delimiter
    friend bool operator==(const OrderedLineInfo&, const OrderedLineInfo&) = default;
};

/// If `line` is an ordered list item, returns its prefix information; else nullopt.
[[nodiscard]] std::optional<OrderedLineInfo> parseOrderedLine(std::u16string_view line);

/// Columns that Tab and Shift+Tab move an item by: the digits, the delimiter and one blank, so that
/// an item indented once stands at the content column of a sibling written the same way.
[[nodiscard]] std::uint32_t orderedMarkerWidth(const OrderedLineInfo& info) noexcept;

/// `line` with `orderedMarkerWidth` blanks added before it (Tab).
[[nodiscard]] std::u16string indentOrderedLine(std::u16string_view line);

/// `line` with up to `orderedMarkerWidth` columns of leading blanks removed (Shift+Tab); the line as
/// it is if it has none.
[[nodiscard]] std::u16string outdentOrderedLine(std::u16string_view line);

/// The prefix Enter writes for the item after `info` on `line`: the same indentation, the next
/// number, the same delimiter and the same blanks after it (at least one).
[[nodiscard]] std::u16string nextOrderedPrefix(std::u16string_view line, const OrderedLineInfo& info);

/// Line `i` of a document, or nullopt past its end. Lets the renumbering walk a document without
/// copying it.
using LineAt = std::function<std::optional<std::u16string>(std::size_t)>;

/// One number to rewrite: replace `digits` on line `line` by `text`.
struct NumberEdit {
    std::size_t line = 0;
    Span digits;
    std::u16string text;
    friend bool operator==(const NumberEdit&, const NumberEdit&) = default;
};

/// The edits that number the list holding the item on line `index` without gaps: its first item
/// keeps its number (the start), each next item gets one more (S053, decision 4). A list is the run
/// of items at the item's column with its delimiter, and the blank or further indented lines
/// between them. With `restart`, the first item gets the number 1. Empty if `index` is no item.
[[nodiscard]] std::vector<NumberEdit> renumberOrderedList(const LineAt& lineAt, std::size_t index,
                                                          bool restart = false);

/// What happened to the item on line `index` before the lists around it are renumbered.
enum class ReindentKind : std::uint8_t {
    Indented,   ///< moved to a deeper column (Tab): it starts at 1 if it is the first of its new list
    Outdented,  ///< moved to a shallower column (Shift+Tab, Enter on an empty nested item): the items
                ///< that followed it at the old column start a nested list at 1
    Removed,    ///< its marker was removed (Enter on an empty top-level item)
};

/// The edits that renumber the list the item on line `index` is in now and the list it left at
/// `oldColumn` with `delimiter` (S053, decisions 4 and 5).
[[nodiscard]] std::vector<NumberEdit> renumberAfterReindent(const LineAt& lineAt, std::size_t index,
                                                            std::uint32_t oldColumn, char16_t delimiter,
                                                            ReindentKind kind);

}  // namespace hitokoto::core
