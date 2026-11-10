// SPDX-License-Identifier: GPL-3.0-or-later
// S031 / ADR 0011: a table pasted from Outlook, Excel, Word or a web page becomes a GFM pipe table.
// The UI reads the clipboard (Qt's HTML import) into the model below; the core decides whether the
// paste converts, writes the Markdown and places it in the document.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// One cell of a pasted table: where it starts, what it spans, and its lines as the source gives
/// them (each line break and each paragraph boundary starts a new line; nothing trimmed yet).
struct PastedCell {
    std::uint32_t row = 0;
    std::uint32_t column = 0;
    std::uint32_t rowSpan = 1;
    std::uint32_t columnSpan = 1;
    std::vector<std::u16string> lines;
    friend bool operator==(const PastedCell&, const PastedCell&) = default;
};

/// A table of a paste. Cells may come in any order; positions no cell covers are empty.
struct PastedTable {
    std::vector<PastedCell> cells;
    bool containsTable = false;  ///< a cell holds a table of its own (S031, "Nested tables")
    friend bool operator==(const PastedTable&, const PastedTable&) = default;
};

/// A part of an HTML paste in source order: text outside tables (its lines as the source gives
/// them, one per paragraph and per line break) or a table.
struct PastedPart {
    std::vector<std::u16string> lines;  ///< text; empty for a table
    std::optional<PastedTable> table;
    friend bool operator==(const PastedPart&, const PastedPart&) = default;
};

/// A block to place: text (kept as plain text) or a converted table, lines joined by '\n'.
struct PasteBlock {
    bool table = false;
    std::u16string text;
    friend bool operator==(const PasteBlock&, const PasteBlock&) = default;
};

/// What the HTML of a paste holds, found by one linear scan before Qt's import (S031 review O1,
/// O9): no table start tag, tables only side by side, or a table inside a table. Tags are matched
/// without case; comments are skipped.
enum class HtmlTables : std::uint8_t { None, Flat, Nested };

/// Scans `html` for table start and end tags and tracks their depth (S031 review O1, O9).
[[nodiscard]] HtmlTables scanHtmlTables(std::u16string_view html) noexcept;

/// `html` without the style attributes of its tags, before Qt's import: Outlook writes about 1 KB
/// of markup per cell, most of it inline CSS that Qt parses for every element and that a pasted
/// table does not need (S031 review (code) O10, Armin, 2026-11-10). Qt 6.12's import ignores
/// `display: none` anyway, so no text appears or disappears. Only tags change: text, comments and
/// other attributes stay byte for byte. Linear in the input.
[[nodiscard]] std::u16string htmlWithoutStyles(std::u16string_view html);

/// Whether a paste at `line` lands in a block quote or a list item, so that it stays plain text
/// (S031 review O4): `line` starts one, continues the paragraph of one (also lazily), or is indented
/// under a list item after blank lines. `above(i)` is the i-th line above `line` (0 = the line
/// directly above), nothing at the start of the document. Conservative: a line that only looks
/// like a container's (in a code block above, say) keeps the paste plain text too.
[[nodiscard]] bool pasteLineInContainer(std::u16string_view line,
                                        const std::function<std::optional<std::u16string>(std::size_t)>& above);

/// Whether a table counts as a table (S031): at least two rows and two columns.
[[nodiscard]] bool countsAsTable(const PastedTable& table);

/// One line of copied cell text escaped for a cell (S031, "Escaping"): `|` as `\|`; `\`, `` ` ``,
/// `*`, `_`, `~`, `[`, `]`, `<` with a backslash; `&` before `#` or before letters and digits
/// and `;`. No trimming.
[[nodiscard]] std::u16string escapeTableCellText(std::u16string_view text);

/// The Markdown of a table (S031): header row, delimiter row, body rows, joined by '\n' without a
/// final line break. Lines in a cell are trimmed and joined with `<br>`, merged cells keep their
/// text in the top-left cell, every row is as wide as the widest.
[[nodiscard]] std::u16string pastedTableToMarkdown(const PastedTable& table);

/// The blocks of an HTML paste: tables that count as Markdown, the text around them and tables
/// that do not count as plain text (S031, "Text around tables"). Nothing if the paste stays plain
/// text: no table counts, or a table holds a table.
[[nodiscard]] std::optional<std::vector<PasteBlock>> pastedTablesToMarkdown(std::span<const PastedPart> parts);

/// Excel's plain text as a table (S031, "What counts as a table, plain text"), or nothing if the
/// text is no such table.
[[nodiscard]] std::optional<PastedTable> parseExcelPlainText(std::u16string_view text);

/// Where a paste lands: the caret's line split at the caret (after the selection is removed) and
/// the lines around it.
struct PastePlace {
    std::u16string_view head;                      ///< the caret's line before the caret
    std::u16string_view tail;                      ///< the caret's line after the caret
    std::optional<std::u16string_view> lineAbove;  ///< nothing at the start of the document
    std::optional<std::u16string_view> lineBelow;  ///< nothing at the end of the document
};

/// What to insert at the caret, lines joined by '\n', and the caret's offset in it afterwards.
struct PasteInsertion {
    std::u16string text;
    std::size_t caret = 0;
    friend bool operator==(const PasteInsertion&, const PasteInsertion&) = default;
};

/// Places the blocks at `place` (S031, "Where it lands"): each table its own block, exactly one
/// blank line between a table and text or another table, none at the start or end of the document
/// or next to a blank line; text continues the line as a plain paste would. The caret ends after
/// the last pasted character.
[[nodiscard]] PasteInsertion placePastedBlocks(std::span<const PasteBlock> blocks, const PastePlace& place);

}  // namespace hitokoto::core
