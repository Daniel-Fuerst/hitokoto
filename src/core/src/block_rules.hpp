// SPDX-License-Identifier: GPL-3.0-or-later
// Where CommonMark blocks start (S023, ADR 0006): the one thematic-break recogniser, setext
// underlines, the start of blocks hitokoto does not render yet, and whether the text after a
// container marker is paragraph text. Shared by the elements and the registry; internal to the core.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hitokoto::core::blocks {

[[nodiscard]] constexpr bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

/// Index of the first character that is no space or tab (`line.size()` for a blank line).
[[nodiscard]] std::size_t firstNonBlank(std::u16string_view line) noexcept;
/// Column after the spaces and tabs from `from` on, starting at `column`; a tab advances to the
/// next multiple of 4 (CommonMark 2.2).
[[nodiscard]] std::uint32_t columnAfterBlanks(std::u16string_view line, std::size_t from,
                                              std::uint32_t column) noexcept;
/// Width of the leading spaces and tabs in columns.
[[nodiscard]] std::uint32_t indentWidth(std::u16string_view line) noexcept;

/// CommonMark 4.1 from `start` on: three or more of the same `-`, `*` or `_`, only spaces and tabs
/// between and after them. The caller decides about indentation.
[[nodiscard]] bool isThematicBreakFrom(std::u16string_view line, std::size_t start) noexcept;
/// A thematic break at the top level: at most three columns of indentation.
[[nodiscard]] bool isThematicBreak(std::u16string_view line) noexcept;
/// CommonMark 4.3: 1 (`=`) or 2 (`-`) if `line` can underline paragraph text whose content column
/// is `column`: indented by `column` to `column + 3` columns, one or more `=` or `-` without blanks
/// between them, then only blanks. Else 0.
[[nodiscard]] std::uint8_t setextUnderline(std::u16string_view line, std::uint32_t column) noexcept;
/// An ATX heading opening at `start`: one to six `#`, then a blank or the end of the line.
[[nodiscard]] bool isAtxHeadingFrom(std::u16string_view line, std::size_t start) noexcept;
/// A code fence opening at `start`: three or more backticks or tildes.
[[nodiscard]] bool isFenceFrom(std::u16string_view line, std::size_t start) noexcept;

/// Content column of the paragraph text after a container marker that ends at `markerEnd` (index)
/// and `markerEndColumn` (column), or -1 if the rest of the line is no paragraph text: blank,
/// indented code (five or more columns of padding), a thematic break, an ATX heading or a fence.
[[nodiscard]] std::int64_t paragraphColumnAfterMarker(std::u16string_view line, std::size_t markerEnd,
                                                      std::uint32_t markerEndColumn) noexcept;

/// What the line before is, for deciding which blocks may start (CommonMark 5, GFM 4.10).
enum class After : std::uint8_t {
    Block,      ///< a blank line, another block or the start of the document: every block can start
    Paragraph,  ///< paragraph text the line can continue: only blocks that interrupt a paragraph
    Lazy,       ///< paragraph text the line can only continue lazily (less indented than the content
                ///< of the item it is in, or after an ordered item or block quote): blocks that
                ///< interrupt a paragraph, and ordered list items of any number
    TableRow,   ///< a table row (cmark-gfm)
};

/// A block that hitokoto does not render yet.
enum class OtherBlock : std::uint8_t {
    None,      ///< no such block starts here
    Opaque,    ///< its lines are no paragraph text (indented code, HTML, link reference definition,
               ///< an item or block quote without paragraph text)
    LazyText,  ///< an ordered list item or block quote with paragraph text: lines after it can
               ///< continue that text lazily, but never underline it
};

/// Does `line` start an ordered list item, block quote, indented code block, HTML block or link
/// reference definition, given what the line before is?
[[nodiscard]] OtherBlock otherBlockStart(std::u16string_view line, After after) noexcept;

}  // namespace hitokoto::core::blocks
