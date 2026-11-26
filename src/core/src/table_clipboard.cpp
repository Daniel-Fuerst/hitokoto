// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/table_clipboard.hpp"

namespace hitokoto::core {

std::u16string copiedTableCells(const std::vector<std::u16string>& /*lines*/, TableCellRange /*range*/) {
    return {};
}

std::u16string copiedTableCellsAsTabSeparated(const ElementRegistry& /*registry*/,
                                              const std::vector<std::u16string>& /*lines*/, TableCellRange /*range*/) {
    return {};
}

std::optional<TableCellGrid> markdownTableCells(const ElementRegistry& /*registry*/, std::u16string_view /*markdown*/) {
    return std::nullopt;
}

TableCellGrid pastedTableCells(const PastedTable& /*table*/) {
    return {};
}

std::optional<std::vector<std::u16string>> pasteCellsIntoTable(const std::vector<std::u16string>& lines,
                                                               TableCellPos /*at*/, const TableCellGrid& /*cells*/) {
    return lines;
}

std::vector<std::u16string> pasteTextIntoTableCells(const std::vector<std::u16string>& lines, TableCellRange /*range*/,
                                                    std::u16string_view /*text*/) {
    return lines;
}

}  // namespace hitokoto::core
