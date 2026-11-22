// SPDX-License-Identifier: GPL-3.0-or-later
// GFM pipe tables as lines (S018, S026, S031): rows and cells to read, change and lay out. The
// element that renders a table is registered in markdown.hpp; its cells there are TableCell.
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// ADR 0002, decision 6 (S018): adds a blank line before each line without `|` that directly
/// follows a table's rows, which GFM would make a row and Obsidian ends the table with. Added
/// breaks take the majority ending in `decoded.perBreak`. Returns how many lines were added.
std::size_t separateTextAfterTables(DecodedText& decoded, const ElementRegistry& registry);

/// Returns a blank GFM pipe-table row with `columns` empty cells, e.g. "|  |  |".
[[nodiscard]] std::u16string makeEmptyTableRow(std::size_t columns);
/// Returns a GFM pipe table with `columns` empty header cells, a delimiter row, and `bodyRows`
/// empty body rows, separated by '\n' (S026).
[[nodiscard]] std::u16string makeEmptyTable(std::size_t columns, std::size_t bodyRows = 1);
/// Appends one new column to a table line (`|  |` for header/body rows, `| --- |` for delimiter rows).
[[nodiscard]] std::u16string appendTableColumn(std::u16string_view line, bool delimiterRow);
/// Removes `column` (0-based) from a table header, delimiter or body row when the row has at least 2 columns.
[[nodiscard]] std::u16string removeTableColumn(std::u16string_view line, std::size_t column);
/// Ensures a table header or body row has at least `columns` cells by appending empty ` |` cells.
[[nodiscard]] std::u16string padTableRowCells(std::u16string_view line, std::size_t columns);
/// Every cell of a table row under GFM rules (also cells beyond the header's column count). A line
/// without a pipe is one cell.
[[nodiscard]] std::vector<TableCell> tableRowCells(std::u16string_view line);
/// The column alignments of a delimiter row, or nothing if `line` is not one.
[[nodiscard]] std::optional<std::vector<TableAlign>> tableDelimiterAligns(std::u16string_view line);
/// Whether the character at `pos` follows an odd number of backslashes, so that CommonMark
/// reads it escaped (S031 review O3: an escaped `<br>` is text).
[[nodiscard]] bool escapedAt(std::u16string_view text, std::size_t pos) noexcept;
/// Cell Markdown as text to edit: `\|` becomes `|`, `<br>` (any spelling, not escaped) a line break.
[[nodiscard]] std::u16string tableCellToText(std::u16string_view markdown);
/// Whether GFM shows the cell Markdown as exactly its characters in one plain paragraph: no
/// character that could start syntax (escapes, emphasis, code, links, HTML, entities, autolinks;
/// a colon only before `//`, an ampersand only before an entity) and no block marker at its start. The UI then lays the
/// cell out as plain text, which is several times faster than Qt's Markdown import (S031: sizing pasted tables).
/// Conservative: false does not mean the cell holds syntax.
[[nodiscard]] bool tableCellIsPlainText(std::u16string_view markdown) noexcept;
/// Cell Markdown with a block marker at its start escaped, for laying the cell out as a Markdown
/// document of its own: GFM reads a cell as inline text, so `# 12`, `> 5 kg`, `- 5 %`, `1. Jänner`,
/// `---` or a fence show their characters there, while a document of their own would show a
/// heading, quote, list, rule or code block (S031 review O3). Display only; the file keeps its
/// Markdown.
[[nodiscard]] std::u16string escapeTableCellBlockStart(std::u16string_view markdown);
/// The characters to lay a cell out as plain text, if GFM shows the cell Markdown as exactly them:
/// plain characters as for tableCellIsPlainText, backslash escapes of ASCII punctuation resolved,
/// and each `<br>` (any spelling, not escaped, no blank beside it, not at the start or end) as
/// U+2028, a line break inside one block (S031 review (code) O10: two-line cells laid out several
/// times faster than with Qt's Markdown import). Nothing if the cell may hold other syntax.
[[nodiscard]] std::optional<std::u16string> tableCellPlainLayout(std::u16string_view markdown);
/// Cell Markdown prepared for Qt's Markdown import of a cell, which the UI runs without raw HTML.
struct TableCellImport {
    std::u16string markdown;
    /// What stands for a line break in `markdown`: a space (U+2000) that the UI turns into U+2028
    /// after the import, because Markdown reads a space beside emphasis as it reads the `<br>` tag
    /// and U+2028 not; U+2028 itself if the cell holds U+2000 or an entity, which could spell it.
    char16_t lineBreak = u'\u2000';
};
/// Cell Markdown (escaped pipes already resolved) for Qt's Markdown import of the cell, which runs
/// without raw HTML so that tags show as their characters. An image shows as its characters, as in
/// the editor outside tables: the brackets of `![...]` are escaped, so `![x](y)` is text and a link
/// around it stays a link. Each `<br>` (any spelling, not escaped) becomes `lineBreak`. Code spans
/// and autolinks stay as they are; they, escapes and brackets are read as CommonMark reads them
/// (checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0). Display only; the file keeps its
/// Markdown.
[[nodiscard]] TableCellImport tableCellImport(std::u16string_view markdown);
/// Edited text as cell Markdown: `|` becomes `\|`, a line break `<br>`.
[[nodiscard]] std::u16string tableCellFromText(std::u16string_view text);
/// `line` with the content of cell `column` replaced by `markdown`; the rest stays byte for byte.
/// A missing cell is added after the existing ones.
[[nodiscard]] std::u16string replaceTableCell(std::u16string_view line, std::size_t column,
                                              std::u16string_view markdown);

}  // namespace hitokoto::core
