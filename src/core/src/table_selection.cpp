// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/table_selection.hpp"

#include "hitokoto/core/table.hpp"

#include <algorithm>
#include <cstddef>

namespace hitokoto::core {

namespace {

/// The line of a row as the table view counts rows: the delimiter line lies between the header
/// (row 0) and the first body row.
std::size_t lineOfRow(int row) {
    return row == 0 ? 0 : static_cast<std::size_t>(row) + 1;
}

}  // namespace

TableCellRange TableCellRange::between(TableCellPos a, TableCellPos b) noexcept {
    return {std::min(a.row, b.row), std::min(a.column, b.column), std::max(a.row, b.row), std::max(a.column, b.column)};
}

bool TableCellRange::contains(int row, int column) const noexcept {
    return row >= top && row <= bottom && column >= left && column <= right;
}

TableCellSelection::TableCellSelection(int rows, int columns, TableCellPos anchor) noexcept
    : rows_(std::max(rows, 1)), columns_(std::max(columns, 1)), anchor_(anchor), focus_(anchor) {
    anchor_ = {std::clamp(anchor.row, 0, rows_ - 1), std::clamp(anchor.column, 0, columns_ - 1)};
    focus_ = anchor_;
}

TableCellSelection TableCellSelection::wholeTable(int rows, int columns) noexcept {
    TableCellSelection s(rows, columns, {0, 0});
    s.focus_ = {s.rows_ - 1, s.columns_ - 1};
    return s;
}

TableCellRange TableCellSelection::range() const noexcept {
    TableCellRange r = TableCellRange::between(anchor_, focus_);
    if (kind_ == Kind::Rows) {
        r.left = 0;
        r.right = columns_ - 1;
    } else if (kind_ == Kind::Columns) {
        r.top = 0;
        r.bottom = rows_ - 1;
    }
    return r;
}

bool TableCellSelection::coversWholeRows() const noexcept {
    const TableCellRange r = range();
    return r.left == 0 && r.right == columns_ - 1;
}

bool TableCellSelection::coversWholeColumns() const noexcept {
    const TableCellRange r = range();
    return r.top == 0 && r.bottom == rows_ - 1;
}

bool TableCellSelection::coversWholeTable() const noexcept {
    return coversWholeRows() && coversWholeColumns();
}

void TableCellSelection::extend(TableStep step) noexcept {
    const bool rowsOnly = kind_ == Kind::Rows;
    const bool columnsOnly = kind_ == Kind::Columns;
    TableCellPos f = focus_;
    switch (step) {
        case TableStep::Left:
            f.column -= rowsOnly ? 0 : 1;
            break;
        case TableStep::Right:
            f.column += rowsOnly ? 0 : 1;
            break;
        case TableStep::Up:
            f.row -= columnsOnly ? 0 : 1;
            break;
        case TableStep::Down:
            f.row += columnsOnly ? 0 : 1;
            break;
        case TableStep::RowStart:
            f.column = rowsOnly ? f.column : 0;
            break;
        case TableStep::RowEnd:
            f.column = rowsOnly ? f.column : columns_ - 1;
            break;
        case TableStep::TableStart:
            if (!rowsOnly && !columnsOnly) {
                f = {0, 0};
            }
            break;
        case TableStep::TableEnd:
            if (!rowsOnly && !columnsOnly) {
                f = {rows_ - 1, columns_ - 1};
            }
            break;
    }
    focus_ = {std::clamp(f.row, 0, rows_ - 1), std::clamp(f.column, 0, columns_ - 1)};
}

void TableCellSelection::extendTo(TableCellPos cell) noexcept {
    focus_ = {std::clamp(cell.row, 0, rows_ - 1), std::clamp(cell.column, 0, columns_ - 1)};
}

void TableCellSelection::selectRows() noexcept {
    const TableCellRange r = range();
    kind_ = Kind::Rows;
    anchor_ = {anchor_.row == r.top ? r.top : r.bottom, 0};
    focus_ = {anchor_.row == r.top ? r.bottom : r.top, columns_ - 1};
}

void TableCellSelection::selectColumns() noexcept {
    const TableCellRange r = range();
    kind_ = Kind::Columns;
    anchor_ = {0, anchor_.column == r.left ? r.left : r.right};
    focus_ = {rows_ - 1, anchor_.column == r.left ? r.right : r.left};
}

bool shiftStepSelectsCell(TableStep step, int position, int length, bool onFirstLine, bool onLastLine) noexcept {
    switch (step) {
        case TableStep::Right:
            return position >= length;
        case TableStep::Left:
            return position <= 0;
        case TableStep::Down:
            return onLastLine && position >= length;
        case TableStep::Up:
            return onFirstLine && position <= 0;
        default:
            return false;
    }
}

std::vector<std::u16string> emptyTableCells(const std::vector<std::u16string>& lines, TableCellRange range) {
    std::vector<std::u16string> out = lines;
    for (int row = std::max(range.top, 0); row <= range.bottom; ++row) {
        const std::size_t at = lineOfRow(row);
        if (at >= out.size()) {
            break;
        }
        const std::size_t cells = tableRowCells(out[at]).size();
        for (int column = std::max(range.left, 0); column <= range.right; ++column) {
            if (static_cast<std::size_t>(column) < cells) {
                out[at] = replaceTableCell(out[at], static_cast<std::size_t>(column), u"");
            }
        }
    }
    return out;
}

std::vector<std::u16string> removeTableRows(const std::vector<std::u16string>& lines, int first, int last) {
    std::vector<std::u16string> out;
    out.reserve(lines.size());
    const std::size_t from = lineOfRow(std::max(first, 1));
    const std::size_t to = lineOfRow(std::max(last, 0));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i < 2 || i < from || i > to) {
            out.push_back(lines[i]);
        }
    }
    return out;
}

std::vector<std::u16string> removeTableColumns(const std::vector<std::u16string>& lines, int first, int last) {
    if (lines.empty()) {
        return lines;
    }
    const int columns = static_cast<int>(tableRowCells(lines.front()).size());
    first = std::max(first, 0);
    last = std::min(last, columns - 1);
    if (first == 0 && last >= columns - 1) {
        first = 1;  // a table keeps its first column
    }
    std::vector<std::u16string> out = lines;
    for (int column = last; column >= first; --column) {
        for (auto& line : out) {
            line = removeTableColumn(line, static_cast<std::size_t>(column));
        }
    }
    return out;
}

}  // namespace hitokoto::core
