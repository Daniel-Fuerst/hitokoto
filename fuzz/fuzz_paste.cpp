// SPDX-License-Identifier: GPL-3.0-or-later
// S031 (review O6): untrusted clipboard text through the paste rules. The input, read as UTF-16,
// goes to Excel's plain-text parser, is escaped as cell text, and builds a random cell model (spans
// that overlap or reach past the table included). Nothing may crash; escaped text reads back as
// the input; every table's Markdown has a delimiter row and rows of equal width; the placement
// keeps every block, in order, and puts the caret after the last one. Review (code) O1, O3, O4: the
// HTML scan, the container check and the display escape of a cell's block marker take the input
// too. Qt's HTML import is not fuzzed (Qt's code; it would need Qt in the fuzz build).
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/paste.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using hitokoto::core::PasteBlock;
using hitokoto::core::PastedCell;
using hitokoto::core::PastedPart;
using hitokoto::core::PastedTable;

void check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool isAsciiPunctuation(char16_t c) {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

/// CommonMark's backslash escapes resolved: what a cell shows of escaped text.
std::u16string unescape(std::u16string_view s) {
    std::u16string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == u'\\' && i + 1 < s.size() && isAsciiPunctuation(s[i + 1])) {
            ++i;
        }
        out += s[i];
    }
    return out;
}

std::vector<std::u16string_view> split(std::u16string_view s, char16_t at) {
    std::vector<std::u16string_view> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = s.find(at, start);
        out.push_back(s.substr(start, end == std::u16string_view::npos ? std::u16string_view::npos : end - start));
        if (end == std::u16string_view::npos) {
            return out;
        }
        start = end + 1;
    }
}

/// The Markdown of a table that counts: header, delimiter and body rows of one width.
void checkTableMarkdown(const PastedTable& table, const std::u16string& md) {
    if (!hitokoto::core::countsAsTable(table)) {
        return;
    }
    check(!md.empty() && md.back() != u'\n' && md.find(u'\r') == std::u16string::npos);
    const auto lines = split(md, u'\n');
    check(lines.size() >= 3);
    const auto aligns = hitokoto::core::tableDelimiterAligns(lines[1]);
    if (!aligns) {
        std::abort();  // explicit, so that bugprone-unchecked-optional-access sees the check
    }
    const std::size_t columns = aligns->size();
    check(columns >= 2);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 1) {
            check(hitokoto::core::tableRowCells(lines[i]).size() == columns);
        }
    }
}

/// A cell model from the input: dimensions and spans from the first code units, the text split
/// into cells at U+001F and into lines at U+001E.
PastedTable randomTable(std::u16string_view text) {
    PastedTable t;
    if (text.size() < 2) {
        return t;
    }
    const std::uint32_t rows = 1U + text[0] % 6U;
    const std::uint32_t columns = 1U + text[1] % 6U;
    t.containsTable = (text[0] & 0x100U) != 0;
    const auto cells = split(text.substr(2), u'\x1f');
    std::size_t k = 0;
    for (std::uint32_t r = 0; r < rows && k < cells.size(); ++r) {
        for (std::uint32_t c = 0; c < columns && k < cells.size(); ++c, ++k) {
            PastedCell cell;
            const std::u16string_view raw = cells[k];
            const char16_t shape = raw.empty() ? u'\0' : raw.front();
            cell.row = (shape & 0x40U) != 0 ? r + (shape & 0x3U) : r;  // may overlap or reach past the table
            cell.column = c;
            cell.rowSpan = 1U + ((shape >> 2U) & 0x3U);
            cell.columnSpan = 1U + ((shape >> 4U) & 0x3U);
            for (const auto line : split(raw, u'\x1e')) {
                cell.lines.emplace_back(line);
            }
            t.cells.push_back(std::move(cell));
        }
    }
    return t;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    std::u16string text(size / 2, u' ');
    for (std::size_t i = 0; i < text.size(); ++i) {
        text[i] = static_cast<char16_t>(data[2 * i] | (data[2 * i + 1] << 8));
    }

    // Escaping keeps the text: a cell shows exactly what was copied, and no pipe splits it.
    for (const auto line : split(text, u'\n')) {
        const std::u16string escaped = hitokoto::core::escapeTableCellText(line);
        check(unescape(escaped) == line);
        check(hitokoto::core::tableRowCells(u"| " + escaped + u" |").size() == 1);
    }

    // The scan before Qt's import (review (code) O1, O9): any input, linear, one of three answers.
    const auto tables = hitokoto::core::scanHtmlTables(text);
    check(tables == hitokoto::core::HtmlTables::None || tables == hitokoto::core::HtmlTables::Flat ||
          tables == hitokoto::core::HtmlTables::Nested);
    if (tables == hitokoto::core::HtmlTables::None) {
        check(hitokoto::core::scanHtmlTables(text + u"<table>") != hitokoto::core::HtmlTables::None ||
              text.find(u"<!--") != std::u16string::npos);
    }

    // The container check walks up the input's lines (review (code) O4).
    {
        const auto all = split(text, u'\n');
        const std::u16string_view last = all.back();
        (void)hitokoto::core::pasteLineInContainer(last, [&all](std::size_t i) -> std::optional<std::u16string> {
            if (i + 1 >= all.size()) {
                return std::nullopt;
            }
            return std::u16string(all[all.size() - 2 - i]);
        });
    }

    // A cell's block marker escaped for display adds at most one backslash (review (code) O3).
    {
        const std::u16string escaped = hitokoto::core::escapeTableCellBlockStart(text);
        check(escaped == text || escaped.size() == text.size() + 1);
    }

    // Excel's plain text.
    if (const auto table = hitokoto::core::parseExcelPlainText(text)) {
        check(hitokoto::core::countsAsTable(*table) && !table->containsTable);
        checkTableMarkdown(*table, hitokoto::core::pastedTableToMarkdown(*table));
    }

    // A random cell model, alone and between text parts, and its placement.
    const PastedTable table = randomTable(text);
    checkTableMarkdown(table, hitokoto::core::pastedTableToMarkdown(table));
    std::vector<PastedPart> parts(3);
    parts[0].lines = {u"before", std::u16string(text.substr(0, std::min<std::size_t>(text.size(), 8)))};
    parts[1].table = table;
    parts[2].lines = {u"after"};
    const auto blocks = hitokoto::core::pastedTablesToMarkdown(parts);
    if (!blocks) {
        return 0;
    }
    check(!table.containsTable && hitokoto::core::countsAsTable(table));
    const auto lines = split(text, u'\n');
    hitokoto::core::PastePlace place;
    place.head = lines.front();
    place.tail = lines.size() > 1 ? lines[1] : std::u16string_view{};
    if (lines.size() > 2) {
        place.lineAbove = lines[2];
    }
    if (lines.size() > 3) {
        place.lineBelow = lines[3];
    }
    const auto inserted = hitokoto::core::placePastedBlocks(*blocks, place);
    check(inserted.caret <= inserted.text.size());
    std::size_t from = 0;
    for (const PasteBlock& b : *blocks) {
        const std::size_t at = inserted.text.find(b.text, from);
        check(at != std::u16string::npos);
        from = at + b.text.size();
    }
    check(inserted.caret == from);  // after the last pasted character
    return 0;
}
