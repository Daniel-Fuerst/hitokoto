// SPDX-License-Identifier: GPL-3.0-or-later
// S041: what copying selected cells puts on the clipboard (a GFM table, tab-separated text) and
// what pasting a table or text into cells does to the table's lines. The Markdown expectations
// were checked with cmark-gfm (cmarkgfm 2025.10.22, GFM with tables) on 2026-11-26: each copied
// table renders as a table whose cells render as the source's cells.
#include "hitokoto/core/paste.hpp"
#include "hitokoto/core/table.hpp"
#include "hitokoto/core/table_clipboard.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {

using Lines = std::vector<std::u16string>;

// The table of the story's first scenario: alignments left, centre, right and none; cells with
// emphasis, an escaped pipe, a line break, a link, a pipe in code, escapes and an entity; the last
// row has one cell.
Lines scenarioTable() {
    return {
        u"| Name | Mitte | Rechts | X |",
        u"| :--- | :---: | ---: | --- |",
        u"| **bold** | a \\| b | x<br>y | 1 |",
        u"| [link](http://e.org) | `co\\|de` | \\*star\\* | &amp; |",
        u"| p |",
    };
}

// A table whose lines are not in S031's minimal form.
Lines looseTable() {
    return {u"|Name|Mitte  |", u"|:-|--:|", u"|a|b|"};
}

// The table that cells are pasted into: three columns, two body rows, one cell with extra blanks.
Lines pasteTarget() {
    return {u"| A | B | C |", u"| --- | --- | --- |", u"| a1 |  b1  | c1 |", u"| a2 | b2 | c2 |"};
}

std::u16string joined(const Lines& lines) {
    std::u16string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += u'\n';
        }
        out += lines[i];
    }
    return out;
}

}  // namespace

TEST(S041_TableCopyPaste, CopyingARectangleGivesAGfmTable) {
    // Scenario: copying cells gives a Markdown table. The first selected row is the header row,
    // each column keeps its alignment, each cell its Markdown.
    EXPECT_EQ(copiedTableCells(scenarioTable(), {1, 0, 2, 2}),
              u"| **bold** | a \\| b | x<br>y |\n"
              u"| :--- | :---: | ---: |\n"
              u"| [link](http://e.org) | `co\\|de` | \\*star\\* |");
    // A column without alignment keeps none; a missing cell of a short row is empty.
    EXPECT_EQ(copiedTableCells(scenarioTable(), {2, 1, 3, 3}),
              u"| `co\\|de` | \\*star\\* | &amp; |\n"
              u"| :---: | ---: | --- |\n"
              u"|  |  |  |");
}

TEST(S041_TableCopyPaste, CopyingTheHeaderRowKeepsItTheHeader) {
    EXPECT_EQ(copiedTableCells(scenarioTable(), {0, 2, 1, 3}),
              u"| Rechts | X |\n"
              u"| ---: | --- |\n"
              u"| x<br>y | 1 |");
}

TEST(S041_TableCopyPaste, TheWholeTableGivesExactlyItsLines) {
    EXPECT_EQ(copiedTableCells(looseTable(), {0, 0, 1, 1}), u"|Name|Mitte  |\n|:-|--:|\n|a|b|");
    EXPECT_EQ(copiedTableCells(scenarioTable(), {0, 0, 3, 3}), joined(scenarioTable()));
    // Less than the whole table is written in the minimal form.
    EXPECT_EQ(copiedTableCells(looseTable(), {0, 0, 1, 0}), u"| Name |\n| :--- |\n| a |");
    EXPECT_EQ(copiedTableCells(looseTable(), {0, 1, 1, 1}), u"| Mitte |\n| ---: |\n| b |");
}

TEST(S041_TableCopyPaste, OneCellGivesItsText) {
    // As copying the whole text of the cell gives it: `\|` as `|`, `<br>` as a line break.
    EXPECT_EQ(copiedTableCells(scenarioTable(), {1, 2, 1, 2}), u"x\ny");
    EXPECT_EQ(copiedTableCells(scenarioTable(), {1, 1, 1, 1}), u"a | b");
    EXPECT_EQ(copiedTableCells(scenarioTable(), {1, 0, 1, 0}), u"**bold**");
    EXPECT_EQ(copiedTableCells(scenarioTable(), {3, 2, 3, 2}), u"");
}

TEST(S041_TableCopyPaste, TabSeparatedTextHoldsTheShownText) {
    // Scenario: copying as tab-separated text. Inline syntax concealed, escapes resolved, a field
    // with a line break quoted.
    const ElementRegistry registry = ElementRegistry::withBuiltins();
    EXPECT_EQ(copiedTableCellsAsTabSeparated(registry, scenarioTable(), {1, 0, 2, 3}),
              u"bold\ta | b\t\"x\ny\"\t1\n"
              u"link\tco|de\t*star*\t&amp;\n");
    // A short row gives empty fields; the header row is a row like any other.
    EXPECT_EQ(copiedTableCellsAsTabSeparated(registry, scenarioTable(), {3, 0, 3, 2}), u"p\t\t\n");
    EXPECT_EQ(copiedTableCellsAsTabSeparated(registry, scenarioTable(), {0, 0, 0, 1}), u"Name\tMitte\n");
}

TEST(S041_TableCopyPaste, TabSeparatedTextQuotesAsExcelDoes) {
    const ElementRegistry registry = ElementRegistry::withBuiltins();
    const Lines table = {u"| A | B | C |", u"| --- | --- | --- |", u"| say \"hi\" | a<br>\"b\" | t\tu |"};
    // Quoted only with a tab or a line break; `"` doubled inside a quoted field.
    EXPECT_EQ(copiedTableCellsAsTabSeparated(registry, table, {1, 0, 1, 2}),
              u"say \"hi\"\t\"a\n\"\"b\"\"\"\t\"t\tu\"\n");
    // S031 reads it back as the same cells.
    const auto read = parseExcelPlainText(copiedTableCellsAsTabSeparated(registry, table, {0, 0, 1, 2}));
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE).
    if (!read) {
        FAIL() << "S031 reads no table";
    }
    const TableCellGrid cells = pastedTableCells(*read);
    ASSERT_EQ(cells.size(), 2U);
    EXPECT_EQ(cells[1], (std::vector<std::u16string>{u"say \"hi\"", u"a<br>\"b\"", u"t\tu"}));
}

TEST(S041_TableCopyPaste, OwnMarkdownThatIsOneTableGivesItsCells) {
    // Scenario: paste prefers hitokoto's own Markdown. What copying cells gives reads back as them.
    const ElementRegistry registry = ElementRegistry::withBuiltins();
    const TableCellGrid expected = {
        {u"**bold**", u"a \\| b", u"x<br>y"},
        {u"[link](http://e.org)", u"`co\\|de`", u"\\*star\\*"},
    };
    EXPECT_EQ(markdownTableCells(registry, copiedTableCells(scenarioTable(), {1, 0, 2, 2})), expected);
    EXPECT_EQ(markdownTableCells(registry, copiedTableCells(scenarioTable(), {1, 0, 2, 2}) + u"\n"), expected);
    EXPECT_EQ(markdownTableCells(registry, u"|Name|Mitte  |\n|:-|--:|\n|a|b|"),
              (TableCellGrid{{u"Name", u"Mitte"}, {u"a", u"b"}}));
    // A row with fewer cells is filled up to the header's, cells beyond it are not part of the
    // table (GFM); a line without a pipe continues the table as a row (S018).
    EXPECT_EQ(markdownTableCells(registry, u"| a | b |\n| - | - |\n| c |\n| d | e | f |\ng"),
              (TableCellGrid{{u"a", u"b"}, {u"c", u""}, {u"d", u"e"}, {u"g", u""}}));
}

TEST(S041_TableCopyPaste, OtherMarkdownIsNoTable) {
    const ElementRegistry registry = ElementRegistry::withBuiltins();
    EXPECT_EQ(markdownTableCells(registry, u""), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"Text1\n\n| a | b |\n| --- | --- |"), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"| a | b |\n| --- | --- |\n\nText2"), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"| a | b |"), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"a\n---"), std::nullopt);  // a setext heading
    EXPECT_EQ(markdownTableCells(registry, u"| a |\n| - |\n\n| b |\n| - |"), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"| a |\n| - |\n\n"), std::nullopt);
    EXPECT_EQ(markdownTableCells(registry, u"    | a |\n    | - |"), std::nullopt);  // a code block
}

TEST(S041_TableCopyPaste, PastedTablesGiveTheirCellsAsS031WritesThem) {
    PastedTable table;
    table.cells = {
        {0, 0, 1, 1, {u"a|b"}},
        {0, 1, 1, 1, {u"*x*"}},
        {1, 0, 1, 1, {u"one", u" two "}},
    };
    EXPECT_EQ(pastedTableCells(table), (TableCellGrid{{u"a\\|b", u"\\*x\\*"}, {u"one<br>two", u""}}));
    EXPECT_EQ(pastedTableCells(PastedTable{}), TableCellGrid{});
}

TEST(S041_TableCopyPaste, PastingCellsAddsRowsAndKeepsTheOthers) {
    // Scenario: a table of 2 x 3 cells pasted with the top-left cell in the last row.
    const auto lines = pasteCellsIntoTable(pasteTarget(), {2, 0}, {{u"1", u"2", u"3"}, {u"4", u"5", u"6"}});
    EXPECT_EQ(lines, (Lines{u"| A | B | C |", u"| --- | --- | --- |", u"| a1 |  b1  | c1 |", u"| 1 | 2 | 3 |",
                            u"| 4 | 5 | 6 |"}));
}

TEST(S041_TableCopyPaste, PastingCellsAddsColumns) {
    const auto lines = pasteCellsIntoTable(pasteTarget(), {1, 2}, {{u"x", u"y"}, {u"z", u"w"}});
    EXPECT_EQ(lines, (Lines{u"| A | B | C |  |", u"| --- | --- | --- | --- |", u"| a1 |  b1  | x | y |",
                            u"| a2 | b2 | z | w |"}));
}

TEST(S041_TableCopyPaste, ThePastedHeaderIsAnOrdinaryRow) {
    // Pasted from the header row on, the block's first row lands in the header; blanks around a
    // replaced cell stay.
    const auto lines = pasteCellsIntoTable(pasteTarget(), {0, 1}, {{u"h"}, {u"v"}});
    EXPECT_EQ(lines, (Lines{u"| A | h | C |", u"| --- | --- | --- |", u"| a1 |  v  | c1 |", u"| a2 | b2 | c2 |"}));
    // A short row gets the cells it needs.
    const auto shortRow = pasteCellsIntoTable({u"| A | B | C |", u"| --- | --- | --- |", u"| a2 |"}, {1, 2}, {{u"q"}});
    EXPECT_EQ(shortRow, (Lines{u"| A | B | C |", u"| --- | --- | --- |", u"| a2 |  | q |"}));
}

TEST(S041_TableCopyPaste, PastingStopsAt255Columns) {
    // Scenario: a paste that would need more than 255 columns changes nothing.
    EXPECT_EQ(pasteCellsIntoTable(pasteTarget(), {0, 254}, {{u"x", u"y"}}), std::nullopt);
    const auto widest = pasteCellsIntoTable(pasteTarget(), {0, 253}, {{u"x", u"y"}});
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE).
    if (!widest) {
        FAIL() << "a paste up to 255 columns changes the table";
    }
    EXPECT_EQ(tableRowCells(widest->front()).size(), kMaxTableColumns);
    EXPECT_EQ(tableDelimiterAligns((*widest)[1]).value_or(std::vector<TableAlign>{}).size(), kMaxTableColumns);
    EXPECT_EQ(widest->size(), pasteTarget().size());
}

TEST(S041_TableCopyPaste, TextReplacesEverySelectedCell) {
    // Scenario: one line of text pasted with three cells selected.
    EXPECT_EQ(pasteTextIntoTableCells(pasteTarget(), {1, 0, 1, 2}, u"a|b"),
              (Lines{u"| A | B | C |", u"| --- | --- | --- |", u"| a\\|b |  a\\|b  | a\\|b |", u"| a2 | b2 | c2 |"}));
    // Line breaks as `<br>`; the one at the end of a spreadsheet's copy is dropped.
    EXPECT_EQ(pasteTextIntoTableCells(pasteTarget(), {2, 1, 2, 1}, u"x\r\ny\r\n")[3], u"| a2 | x<br>y | c2 |");
    EXPECT_EQ(pasteTextIntoTableCells(pasteTarget(), {2, 1, 2, 1}, u"x\n\n")[3], u"| a2 | x<br> | c2 |");
    // A short row gets the cells it needs.
    EXPECT_EQ(pasteTextIntoTableCells({u"| A | B |", u"| - | - |", u"| a |"}, {1, 0, 1, 1}, u"t")[2], u"| t | t |");
    // A row that has its cells keeps every other byte, the blanks at its end too (ADR 0002).
    EXPECT_EQ(pasteTextIntoTableCells({u"| A | B |", u"| - | - |", u"| a | b |  "}, {1, 0, 1, 0}, u"t")[2],
              u"| t | b |  ");
}

TEST(S041_TableCopyPaste, InCellsOneRowIsATable) {
    // S041 review (code) O1 (Armin, 2026-11-27): in cells, only a single cell is text.
    const auto row = parseTabSeparatedCells(u"a\tb\tc\r\n");
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE).
    if (!row) {
        FAIL() << "one row of a spreadsheet is no table";
    }
    EXPECT_EQ(pastedTableCells(*row), (TableCellGrid{{u"a", u"b", u"c"}}));
    const auto quoted = parseTabSeparatedCells(u"\"x\ny\"\tz");
    if (!quoted) {
        FAIL() << "a quoted field with a line break is no table";
    }
    EXPECT_EQ(pastedTableCells(*quoted), (TableCellGrid{{u"x<br>y", u"z"}}));
    // As S031 reads two rows and more.
    const auto rows = parseTabSeparatedCells(u"1\t2\r\n3\t4\r\n");
    EXPECT_EQ(rows, parseExcelPlainText(u"1\t2\r\n3\t4\r\n"));
    EXPECT_TRUE(rows.has_value());
    // Text: one cell, lines without a tab, rows of different widths, indented text.
    EXPECT_EQ(parseTabSeparatedCells(u"a\r\n"), std::nullopt);
    EXPECT_EQ(parseTabSeparatedCells(u"a\r\nb\r\n"), std::nullopt);
    EXPECT_EQ(parseTabSeparatedCells(u"a\tb\r\nc\r\n"), std::nullopt);
    EXPECT_EQ(parseTabSeparatedCells(u"\ta\tb"), std::nullopt);
}

TEST(S041_TableCopyPaste, InCellsOneColumnOfAnHtmlTableIsATable) {
    PastedTable column;
    column.cells = {{0, 0, 1, 1, {u"a"}}, {1, 0, 1, 1, {u"b"}}};
    EXPECT_TRUE(countsAsCellTable(column));
    EXPECT_FALSE(countsAsTable(column));  // in the editor it stays text (S031)
    EXPECT_EQ(pastedTableCells(column), (TableCellGrid{{u"a"}, {u"b"}}));
    PastedTable single;
    single.cells = {{0, 0, 1, 1, {u"z"}}};
    EXPECT_FALSE(countsAsCellTable(single));
    EXPECT_FALSE(countsAsCellTable(PastedTable{}));
}
