// SPDX-License-Identifier: GPL-3.0-or-later
// S018: the table helpers the UI calls on lines of a document and on the text of an edited cell.
// The input, read as UTF-16, is a line and the text typed into a cell. Nothing may crash; every
// cell lies inside its line, its content inside its raw span; the plain layout of a cell is never
// longer than its Markdown; cell Markdown made from text stays on one line; replacing a cell keeps
// the rest of the line byte for byte; the column helpers keep a row on one line.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/table.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool oneLine(std::u16string_view s) {
    return s.find(u'\n') == std::u16string_view::npos && s.find(u'\r') == std::u16string_view::npos;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace hitokoto::core;
    static const ElementRegistry registry = ElementRegistry::withBuiltins();
    if (size < 2) {
        return 0;
    }
    // Byte 0: the column (low three bits) and options; byte 1: where the line ends and the cell text
    // starts; then code units, two bytes each. A line of a document holds no line break.
    const std::size_t column = data[0] % 8U;
    std::u16string all((size - 2) / 2, u' ');
    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i] = static_cast<char16_t>(data[2 + (2 * i)] | (data[3 + (2 * i)] << 8U));
    }
    const std::size_t cut = all.empty() ? 0 : data[1] % (all.size() + 1);
    std::u16string line = all.substr(0, cut);
    std::replace(line.begin(), line.end(), u'\n', u' ');
    std::replace(line.begin(), line.end(), u'\r', u' ');
    const std::u16string cellText = all.substr(cut);

    const std::vector<TableCell> cells = tableRowCells(line);
    for (const TableCell& c : cells) {
        check(c.raw.end() <= line.size() && c.content.end() <= line.size());
        check(c.content.start >= c.raw.start && c.content.end() <= c.raw.end());
        const std::u16string_view md = std::u16string_view(line).substr(c.content.start, c.content.length);
        (void)tableCellToText(md);
        (void)tableCellIsPlainText(md);
        (void)tableCellImport(md);
        check(oneLine(escapeTableCellBlockStart(md)));
        if (const std::optional<std::u16string> plain = tableCellPlainLayout(md)) {
            check(plain->size() <= md.size());
        }
        (void)registry.renderInline(md);
    }
    (void)tableDelimiterAligns(line);

    const std::u16string md = tableCellFromText(cellText);
    check(oneLine(md));
    const std::u16string replaced = replaceTableCell(line, column, md);
    check(oneLine(replaced));
    if (column < cells.size()) {
        const Span content = cells[column].content;
        check(replaced.size() == line.size() - content.length + md.size());
        check(std::u16string_view(replaced).substr(0, content.start) ==
              std::u16string_view(line).substr(0, content.start));
        check(std::u16string_view(replaced).substr(content.start + md.size()) ==
              std::u16string_view(line).substr(content.end()));
    } else {
        check(tableRowCells(replaced).size() > column);
    }

    check(oneLine(appendTableColumn(line, (data[0] & 0x80U) != 0)));
    check(oneLine(removeTableColumn(line, column)));
    check(oneLine(padTableRowCells(line, column + 1)));
    check(!makeEmptyTable(column + 1, 1 + (data[0] >> 5U)).empty());
    return 0;
}
