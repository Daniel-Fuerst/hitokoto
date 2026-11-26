// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/table_selection.hpp"

namespace hitokoto::core {

TableCellRange TableCellRange::between(TableCellPos /*a*/, TableCellPos /*b*/) noexcept {
    return {};
}
bool TableCellRange::contains(int /*row*/, int /*column*/) const noexcept {
    return false;
}

TableCellSelection::TableCellSelection(int rows, int columns, TableCellPos anchor) noexcept
    : rows_(rows), columns_(columns), anchor_(anchor), focus_(anchor) {}
TableCellSelection TableCellSelection::wholeTable(int rows, int columns) noexcept {
    return {rows, columns, {}};
}
TableCellRange TableCellSelection::range() const noexcept {
    return {};
}
bool TableCellSelection::coversWholeRows() const noexcept {
    return false;
}
bool TableCellSelection::coversWholeColumns() const noexcept {
    return false;
}
bool TableCellSelection::coversWholeTable() const noexcept {
    return false;
}
void TableCellSelection::extend(TableStep /*step*/) noexcept {}
void TableCellSelection::extendTo(TableCellPos /*cell*/) noexcept {}
void TableCellSelection::selectRows() noexcept {}
void TableCellSelection::selectColumns() noexcept {}

bool shiftStepSelectsCell(TableStep /*step*/, int /*position*/, int /*length*/, bool /*onFirstLine*/,
                          bool /*onLastLine*/) noexcept {
    return false;
}

std::vector<std::u16string> emptyTableCells(const std::vector<std::u16string>& lines, TableCellRange /*range*/) {
    return lines;
}
std::vector<std::u16string> removeTableRows(const std::vector<std::u16string>& lines, int /*first*/, int /*last*/) {
    return lines;
}
std::vector<std::u16string> removeTableColumns(const std::vector<std::u16string>& lines, int /*first*/, int /*last*/) {
    return lines;
}

}  // namespace hitokoto::core
