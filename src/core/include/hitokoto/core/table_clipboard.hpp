// SPDX-License-Identifier: GPL-3.0-or-later
// S041: what copying selected cells puts on the clipboard, and what pasting into cells does to a
// table's lines. The rules need no Qt; the table view and the editor read and write the clipboard
// and ask these functions for the text and the lines.
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/paste.hpp"
#include "hitokoto/core/table_selection.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// The most columns a table has (S018): GFM's limit as cmark-gfm reads tables.
inline constexpr std::size_t kMaxTableColumns = 255;

/// The cells of a table, row by row, each as its Markdown (trimmed, as in the file).
using TableCellGrid = std::vector<std::vector<std::u16string>>;

/// Decision 2: what copying `range` of a table puts into `text/plain` and `text/markdown`. `lines`
/// are the table's lines (header, delimiter, body rows). The whole table gives exactly its lines;
/// one cell gives its text as its editor shows it (`tableCellToText`); any other rectangle a GFM
/// table in S031's minimal form, its first row the header row, each column's alignment from the
/// delimiter row, each cell's Markdown as in the file. Lines are joined by '\n', without a final
/// line break.
[[nodiscard]] std::u16string copiedTableCells(const std::vector<std::u16string>& lines, TableCellRange range);

/// Decision 4: the cells of `range` as tab-separated text in Excel's form. Each row ends with
/// '\n'; cells are separated by a tab; a field is quoted only when it holds a tab or a line break,
/// with `"` inside doubled. Each field is the text the find bar searches in the cell (S028, "Table
/// cells"): inline syntax concealed, escapes resolved, `\|` as `|`, `<br>` as a line break.
[[nodiscard]] std::u16string copiedTableCellsAsTabSeparated(const ElementRegistry& registry,
                                                            const std::vector<std::u16string>& lines,
                                                            TableCellRange range);

/// Decision 5: the cells of `markdown` if it is exactly one GFM table as `registry` reads a
/// document of these lines (every line a line of that table; one final line break allowed), the
/// header row first. Nothing otherwise.
[[nodiscard]] std::optional<TableCellGrid> markdownTableCells(const ElementRegistry& registry,
                                                              std::u16string_view markdown);

/// Decision 7: the cells of an HTML or tab-separated table, each as S031 writes it into a cell
/// (`pastedTableToMarkdown`), the first row first.
[[nodiscard]] TableCellGrid pastedTableCells(const PastedTable& table);

/// Decision 7: the lines of a table with `cells` written from `at` on, row by row and column by
/// column; the first row of `cells` is an ordinary row there. Rows and columns are added as
/// needed (empty cells, as adding them does in S018); cells outside the block stay byte for byte.
/// Nothing if the table would need more than kMaxTableColumns columns.
[[nodiscard]] std::optional<std::vector<std::u16string>> pasteCellsIntoTable(const std::vector<std::u16string>& lines,
                                                                             TableCellPos at,
                                                                             const TableCellGrid& cells);

/// Decision 7: the lines of a table with every cell of `range` holding `text`, its line breaks as
/// `<br>` and `|` as `\|` (`tableCellFromText`). One line break at the end of `text` is dropped, as
/// a spreadsheet ends the copy of a single cell with one.
[[nodiscard]] std::vector<std::u16string> pasteTextIntoTableCells(const std::vector<std::u16string>& lines,
                                                                  TableCellRange range, std::u16string_view text);

}  // namespace hitokoto::core
