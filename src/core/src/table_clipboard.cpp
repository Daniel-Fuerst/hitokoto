// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/table_clipboard.hpp"

#include "hitokoto/core/find.hpp"
#include "hitokoto/core/table.hpp"

#include <algorithm>
#include <cstddef>

namespace hitokoto::core {

namespace {

/// The line of a row as the table view counts rows: the delimiter line lies between the header
/// (row 0) and the first body row (as in table_selection.cpp).
std::size_t lineOfRow(int row) {
    return row <= 0 ? 0 : static_cast<std::size_t>(row) + 1;
}

/// The Markdown of cell `column` of a row, trimmed as in the file; empty if the row has no such cell.
std::u16string_view cellMarkdown(std::u16string_view line, const std::vector<TableCell>& cells, std::size_t column) {
    if (column >= cells.size()) {
        return {};
    }
    return line.substr(cells[column].content.start, cells[column].content.length);
}

/// The delimiter cell for a column whose delimiter in the file is `written`: the dashes of S031's
/// minimal form, with the colons that give the column its alignment (none, left, centre, right).
std::u16string delimiterCell(std::u16string_view written) {
    const bool left = !written.empty() && written.front() == u':';
    const bool right = !written.empty() && written.back() == u':';
    std::u16string out;
    if (left) {
        out += u':';
    }
    out += u"---";
    if (right) {
        out += u':';
    }
    return out;
}

void appendRow(std::u16string& out, const std::vector<std::u16string>& cells) {
    out += u'|';
    for (const auto& cell : cells) {
        out += u' ';
        out += cell;
        out += u" |";
    }
}

std::u16string joinLines(const std::vector<std::u16string>& lines) {
    std::u16string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += u'\n';
        }
        out += lines[i];
    }
    return out;
}

/// A field of tab-separated text as Excel writes it: quoted only with a tab or a line break.
void appendField(std::u16string& out, std::u16string_view text) {
    if (text.find_first_of(u"\t\n\r") == std::u16string_view::npos) {
        out += text;
        return;
    }
    out += u'"';
    for (const char16_t c : text) {
        if (c == u'"') {
            out += u'"';
        }
        out += c;
    }
    out += u'"';
}

/// `range` limited to a table of `rows` rows and `columns` columns.
TableCellRange clamped(TableCellRange range, int rows, int columns) {
    return {std::max(range.top, 0), std::max(range.left, 0), std::min(range.bottom, rows - 1),
            std::min(range.right, columns - 1)};
}

/// `line` with at least `columns` cells; unchanged if it has them.
std::u16string withCells(const std::u16string& line, std::size_t columns) {
    return tableRowCells(line).size() < columns ? padTableRowCells(line, columns) : line;
}

/// `text` without one line break at its end.
std::u16string_view withoutFinalLineBreak(std::u16string_view text) {
    if (text.ends_with(u"\r\n")) {
        text.remove_suffix(2);
    } else if (text.ends_with(u'\n') || text.ends_with(u'\r')) {
        text.remove_suffix(1);
    }
    return text;
}

/// The lines of `text` split at '\n', each without a '\r' at its end.
std::vector<std::u16string> splitLines(std::u16string_view text) {
    std::vector<std::u16string> lines;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = text.find(u'\n', start);
        std::u16string_view line =
            text.substr(start, end == std::u16string_view::npos ? text.size() - start : end - start);
        if (line.ends_with(u'\r')) {
            line.remove_suffix(1);
        }
        lines.emplace_back(line);
        if (end == std::u16string_view::npos) {
            return lines;
        }
        start = end + 1;
    }
}

}  // namespace

std::u16string copiedTableCells(const std::vector<std::u16string>& lines, TableCellRange range) {
    if (lines.size() < 2) {
        return {};
    }
    const int rows = static_cast<int>(lines.size()) - 1;
    const int columns = static_cast<int>(tableRowCells(lines.front()).size());
    const TableCellRange r = clamped(range, rows, columns);
    if (r.top > r.bottom || r.left > r.right) {
        return {};
    }
    if (r == TableCellRange{0, 0, rows - 1, columns - 1}) {
        return joinLines(lines);
    }
    if (r.top == r.bottom && r.left == r.right) {
        const std::u16string& line = lines[lineOfRow(r.top)];
        return tableCellToText(cellMarkdown(line, tableRowCells(line), static_cast<std::size_t>(r.left)));
    }
    const std::vector<TableCell> delimiters = tableRowCells(lines[1]);
    std::u16string out;
    std::vector<std::u16string> cells(static_cast<std::size_t>(r.right - r.left + 1));
    for (int row = r.top; row <= r.bottom; ++row) {
        const std::u16string& line = lines[lineOfRow(row)];
        const std::vector<TableCell> rowCells = tableRowCells(line);
        for (int column = r.left; column <= r.right; ++column) {
            cells[static_cast<std::size_t>(column - r.left)] =
                cellMarkdown(line, rowCells, static_cast<std::size_t>(column));
        }
        if (row > r.top) {
            out += u'\n';
        }
        appendRow(out, cells);
        if (row == r.top) {
            for (int column = r.left; column <= r.right; ++column) {
                cells[static_cast<std::size_t>(column - r.left)] =
                    delimiterCell(cellMarkdown(lines[1], delimiters, static_cast<std::size_t>(column)));
            }
            out += u'\n';
            appendRow(out, cells);
        }
    }
    return out;
}

std::u16string copiedTableCellsAsTabSeparated(const ElementRegistry& registry, const std::vector<std::u16string>& lines,
                                              TableCellRange range) {
    if (lines.size() < 2) {
        return {};
    }
    const int rows = static_cast<int>(lines.size()) - 1;
    const std::size_t columns = tableRowCells(lines.front()).size();
    const TableCellRange r = clamped(range, rows, static_cast<int>(columns));
    std::u16string out;
    for (int row = r.top; row <= r.bottom; ++row) {
        const std::size_t at = lineOfRow(row);
        std::vector<std::u16string> texts;
        for (FindUnit& unit : tableRowUnits(registry, lines[at], at, columns)) {
            if (unit.kind == FindUnitKind::Cell) {
                texts.push_back(std::move(unit.text));
            }
        }
        for (int column = r.left; column <= r.right; ++column) {
            if (column > r.left) {
                out += u'\t';
            }
            if (static_cast<std::size_t>(column) < texts.size()) {
                appendField(out, texts[static_cast<std::size_t>(column)]);
            }
        }
        out += u'\n';
    }
    return out;
}

std::optional<TableCellGrid> markdownTableCells(const ElementRegistry& registry, std::u16string_view markdown) {
    const std::vector<std::u16string> lines = splitLines(withoutFinalLineBreak(markdown));
    if (lines.size() < 2) {
        return std::nullopt;
    }
    const std::vector<LineRender> renders = registry.renderDocument(lines);
    for (std::size_t i = 0; i < renders.size(); ++i) {
        const LineRender& render = renders[i];
        const bool expected = render.table && render.tableHeader == (i == 0) && render.tableDelimiter == (i == 1);
        if (!expected) {
            return std::nullopt;
        }
    }
    const std::size_t columns = tableRowCells(lines.front()).size();
    TableCellGrid grid;
    grid.reserve(lines.size() - 1);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i == 1) {
            continue;
        }
        const std::vector<TableCell> cells = tableRowCells(lines[i]);
        std::vector<std::u16string> row(columns);
        for (std::size_t c = 0; c < columns; ++c) {
            row[c] = cellMarkdown(lines[i], cells, c);
        }
        grid.push_back(std::move(row));
    }
    return grid;
}

TableCellGrid pastedTableCells(const PastedTable& table) {
    const std::u16string markdown = pastedTableToMarkdown(table);
    if (markdown.empty()) {
        return {};
    }
    // S031 writes each cell on one line, with its line breaks as `<br>` and `|` escaped; reading
    // its rows back gives the cells as it wrote them.
    TableCellGrid grid;
    const std::vector<std::u16string> lines = splitLines(markdown);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i == 1) {
            continue;
        }
        const std::vector<TableCell> cells = tableRowCells(lines[i]);
        std::vector<std::u16string> row;
        row.reserve(cells.size());
        for (std::size_t c = 0; c < cells.size(); ++c) {
            row.emplace_back(cellMarkdown(lines[i], cells, c));
        }
        grid.push_back(std::move(row));
    }
    return grid;
}

std::optional<std::vector<std::u16string>> pasteCellsIntoTable(const std::vector<std::u16string>& lines,
                                                               TableCellPos at, const TableCellGrid& cells) {
    if (lines.size() < 2 || at.row < 0 || at.column < 0) {
        return std::nullopt;
    }
    std::size_t width = 0;
    for (const auto& row : cells) {
        width = std::max(width, row.size());
    }
    const auto firstColumn = static_cast<std::size_t>(at.column);
    const std::size_t neededColumns = firstColumn + width;
    if (neededColumns > kMaxTableColumns) {
        return std::nullopt;
    }
    std::vector<std::u16string> out = lines;
    for (std::size_t columns = tableRowCells(out.front()).size(); columns < neededColumns; ++columns) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = appendTableColumn(out[i], i == 1);
        }
    }
    const std::size_t columns = std::max(tableRowCells(out.front()).size(), neededColumns);
    const std::size_t neededLines = lineOfRow(at.row + static_cast<int>(cells.size()) - 1) + 1;
    while (out.size() < neededLines) {
        out.push_back(makeEmptyTableRow(columns));
    }
    for (std::size_t r = 0; r < cells.size(); ++r) {
        std::u16string& line = out[lineOfRow(at.row + static_cast<int>(r))];
        line = withCells(line, firstColumn + cells[r].size());
        for (std::size_t c = 0; c < cells[r].size(); ++c) {
            line = replaceTableCell(line, firstColumn + c, cells[r][c]);
        }
    }
    return out;
}

std::vector<std::u16string> pasteTextIntoTableCells(const std::vector<std::u16string>& lines, TableCellRange range,
                                                    std::u16string_view text) {
    if (lines.size() < 2) {
        return lines;
    }
    const int rows = static_cast<int>(lines.size()) - 1;
    const int columns = static_cast<int>(tableRowCells(lines.front()).size());
    const TableCellRange r = clamped(range, rows, columns);
    const std::u16string markdown = tableCellFromText(withoutFinalLineBreak(text));
    std::vector<std::u16string> out = lines;
    for (int row = r.top; row <= r.bottom; ++row) {
        std::u16string& line = out[lineOfRow(row)];
        line = withCells(line, static_cast<std::size_t>(r.right) + 1);
        for (int column = r.left; column <= r.right; ++column) {
            line = replaceTableCell(line, static_cast<std::size_t>(column), markdown);
        }
    }
    return out;
}

}  // namespace hitokoto::core
