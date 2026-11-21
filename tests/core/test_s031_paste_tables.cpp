// SPDX-License-Identifier: GPL-3.0-or-later
// S031: pasting a table from Outlook, Excel or a web page as a Markdown table (ADR 0011). The core
// part: the Markdown of a pasted table, escaping, lines in cells, merged and nested cells, width,
// text around tables, Excel's plain text and the placement in the document. Expected tables were
// checked with cmark-gfm (cmarkgfm 2025.10.22, GFM) and markdown-it-py 4.2.0 (tables on): each
// parses as a table with the copied cell text.
#include "hitokoto/core/paste.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace hitokoto::core;

namespace {

/// A cell at (row, column) whose lines are `text` split at '\n'.
PastedCell cell(std::uint32_t row, std::uint32_t column, std::u16string_view text, std::uint32_t rowSpan = 1,
                std::uint32_t columnSpan = 1) {
    PastedCell c;
    c.row = row;
    c.column = column;
    c.rowSpan = rowSpan;
    c.columnSpan = columnSpan;
    std::size_t start = 0;
    for (;;) {
        const std::size_t nl = text.find(u'\n', start);
        c.lines.emplace_back(
            text.substr(start, nl == std::u16string_view::npos ? std::u16string_view::npos : nl - start));
        if (nl == std::u16string_view::npos) {
            break;
        }
        start = nl + 1;
    }
    return c;
}

/// A table without spans, row by row; '\n' in a cell separates its lines.
PastedTable grid(std::initializer_list<std::initializer_list<std::u16string_view>> rows) {
    PastedTable t;
    std::uint32_t r = 0;
    for (const auto& row : rows) {
        std::uint32_t c = 0;
        for (const auto text : row) {
            t.cells.push_back(cell(r, c++, text));
        }
        ++r;
    }
    return t;
}

PastedPart textPart(std::initializer_list<std::u16string_view> lines) {
    PastedPart p;
    for (const auto line : lines) {
        p.lines.emplace_back(line);
    }
    return p;
}

PastedPart tablePart(PastedTable t) {
    PastedPart p;
    p.table = std::move(t);
    return p;
}

PasteBlock textBlock(std::u16string_view text) {
    return {false, std::u16string(text)};
}

PasteBlock tableBlock(std::u16string_view text) {
    return {true, std::u16string(text)};
}

// The story's example (T1): two paragraphs in one cell, a line break in another.
PastedTable exampleTable() {
    return grid({{u"Product", u"Supplier", u"Price per kg"},
                 {u"Green tea", u"Tea House North\nTea House South", u"12,50\n13,20"},
                 {u"Black tea", u"Tea House West", u"9,80"}});
}

constexpr std::u16string_view kExample =
    u"| Product | Supplier | Price per kg |\n"
    u"| --- | --- | --- |\n"
    u"| Green tea | Tea House North<br>Tea House South | 12,50<br>13,20 |\n"
    u"| Black tea | Tea House West | 9,80 |";

constexpr std::u16string_view kSmall =
    u"| a | b |\n"
    u"| --- | --- |\n"
    u"| 1 | 2 |";

}  // namespace

// --- Markdown of a table ---------------------------------------------------------------------------

TEST(S031_PasteTables, TheStoryExampleBecomesItsMarkdown) {
    EXPECT_TRUE(countsAsTable(exampleTable()));
    EXPECT_EQ(pastedTableToMarkdown(exampleTable()), kExample);
}

TEST(S031_PasteTables, TheFirstRowIsTheHeaderAndTheDelimiterHasNoAlignment) {
    EXPECT_EQ(pastedTableToMarkdown(grid({{u"a", u"b"}, {u"1", u"2"}})), kSmall);
    EXPECT_EQ(pastedTableToMarkdown(grid({{u"", u""}, {u"1", u"2"}})), u"|  |  |\n| --- | --- |\n| 1 | 2 |");
}

TEST(S031_PasteTables, AtLeastTwoRowsAndTwoColumnsCount) {
    EXPECT_FALSE(countsAsTable(grid({{u"Sencha", u"Assam", u"Oolong"}})));
    EXPECT_FALSE(countsAsTable(grid({{u"Sencha"}, {u"Assam"}, {u"Oolong"}})));
    EXPECT_FALSE(countsAsTable(PastedTable{}));
    EXPECT_TRUE(countsAsTable(grid({{u"a", u"b"}, {u"c", u"d"}})));
    // A table whose second column comes only from a span still has two columns.
    PastedTable spanned;
    spanned.cells = {cell(0, 0, u"a", 1, 2), cell(1, 0, u"b"), cell(1, 1, u"c")};
    EXPECT_TRUE(countsAsTable(spanned));
}

// --- Escaping --------------------------------------------------------------------------------------

TEST(S031_PasteTables, EscapingKeepsTheCopiedText) {
    EXPECT_EQ(escapeTableCellText(u"a|b"), u"a\\|b");
    EXPECT_EQ(escapeTableCellText(u"*x*"), u"\\*x\\*");
    EXPECT_EQ(escapeTableCellText(u"C:\\Dev"), u"C:\\\\Dev");
    EXPECT_EQ(escapeTableCellText(u"`c`"), u"\\`c\\`");
    EXPECT_EQ(escapeTableCellText(u"[l](u)"), u"\\[l\\](u)");
    EXPECT_EQ(escapeTableCellText(u"x<br>y"), u"x\\<br>y");
    EXPECT_EQ(escapeTableCellText(u"~t~ _u_"), u"\\~t\\~ \\_u\\_");
}

TEST(S031_PasteTables, AnAmpersandIsEscapedOnlyWhereAReferenceCouldStart) {
    EXPECT_EQ(escapeTableCellText(u"R&D"), u"R&D");
    EXPECT_EQ(escapeTableCellText(u"A & B"), u"A & B");
    EXPECT_EQ(escapeTableCellText(u"&amp;"), u"\\&amp;");
    EXPECT_EQ(escapeTableCellText(u"&#38;"), u"\\&#38;");
    EXPECT_EQ(escapeTableCellText(u"&#x26;"), u"\\&#x26;");
    EXPECT_EQ(escapeTableCellText(u"&foo;"), u"\\&foo;");  // review O9: also names that are none
    EXPECT_EQ(escapeTableCellText(u"R&D;"), u"R\\&D;");
    EXPECT_EQ(escapeTableCellText(u"&;"), u"&;");
    EXPECT_EQ(escapeTableCellText(u"&a b;"), u"&a b;");
    EXPECT_EQ(escapeTableCellText(u"a&"), u"a&");
}

TEST(S031_PasteTables, OtherCharactersStayAsTheyAre) {
    EXPECT_EQ(escapeTableCellText(u"# not a heading"), u"# not a heading");
    EXPECT_EQ(escapeTableCellText(u"> no quote"), u"> no quote");
    EXPECT_EQ(escapeTableCellText(u"- 1. + 'x' \"y\" ! = $ % ^"), u"- 1. + 'x' \"y\" ! = $ % ^");
    EXPECT_EQ(escapeTableCellText(u"Grüße, 一言"), u"Grüße, 一言");
    EXPECT_EQ(escapeTableCellText(u""), u"");
}

TEST(S031_PasteTables, CellsAreEscapedInTheMarkdown) {
    const PastedTable t = grid({{u"Text 1", u"Text 2", u"Text 3", u"Text 4"},
                                {u"a|b", u"*x*", u"C:\\Dev", u"`c`"},
                                {u"R&D", u"&amp;", u"[l](u)", u"x<br>y"},
                                {u"A & B", u"&#38;", u"&foo;", u"~t~ _u_"}});
    EXPECT_EQ(pastedTableToMarkdown(t),
              u"| Text 1 | Text 2 | Text 3 | Text 4 |\n"
              u"| --- | --- | --- | --- |\n"
              u"| a\\|b | \\*x\\* | C:\\\\Dev | \\`c\\` |\n"
              u"| R&D | \\&amp; | \\[l\\](u) | x\\<br>y |\n"
              u"| A & B | \\&#38; | \\&foo; | \\~t\\~ \\_u\\_ |");
}

// --- Lines in a cell -------------------------------------------------------------------------------

TEST(S031_PasteTables, EachLineOfACellIsJoinedWithABreak) {
    const PastedTable t = grid({{u"Case", u"Cell"},
                                {u"spaces around", u"\u00a0\u00a0padded\u00a0\u00a0"},
                                {u"empty line inside", u"first\n\nthird"},
                                {u"empty paragraph inside", u"first\n\u00a0\nthird"},
                                {u"empty lines around", u"\nmiddle\n\n"},
                                {u"empty cell", u""},
                                {u"no-break spaces", u"no\u00a0break"},
                                {u"tabs and spaces", u"\t a \t\n  b  "},
                                {u"only blanks", u" \n\u00a0\n\t"}});
    EXPECT_EQ(pastedTableToMarkdown(t),
              u"| Case | Cell |\n"
              u"| --- | --- |\n"
              u"| spaces around | padded |\n"
              u"| empty line inside | first<br><br>third |\n"
              u"| empty paragraph inside | first<br><br>third |\n"
              u"| empty lines around | middle |\n"
              u"| empty cell |  |\n"
              u"| no-break spaces | no break |\n"
              u"| tabs and spaces | a<br>b |\n"
              u"| only blanks |  |");
}

TEST(S031_PasteTables, ACellWithoutLinesIsEmpty) {
    PastedTable t = grid({{u"a", u"b"}, {u"c", u"d"}});
    t.cells[3].lines.clear();
    EXPECT_EQ(pastedTableToMarkdown(t), u"| a | b |\n| --- | --- |\n| c |  |");
}

// --- Merged cells, width ---------------------------------------------------------------------------

TEST(S031_PasteTables, MergedCellsKeepTheirTextInTheTopLeftCell) {
    // T3: colspan, rowspan and both in one cell.
    PastedTable t;
    t.cells = {cell(0, 0, u"Region"), cell(0, 1, u"Spring"),
               cell(0, 2, u"Summer"), cell(0, 3, u"Autumn"),
               cell(1, 0, u"North"),  cell(1, 1, u"Closed", 1, 2),
               cell(1, 3, u"40"),     cell(2, 0, u"South", 2, 1),
               cell(2, 1, u"25"),     cell(2, 2, u"Holiday break", 2, 2),
               cell(3, 1, u"30")};
    EXPECT_EQ(pastedTableToMarkdown(t),
              u"| Region | Spring | Summer | Autumn |\n"
              u"| --- | --- | --- | --- |\n"
              u"| North | Closed |  | 40 |\n"
              u"| South | 25 | Holiday break |  |\n"
              u"|  | 30 |  |  |");
}

TEST(S031_PasteTables, CellsMayComeInAnyOrder) {
    PastedTable t;
    t.cells = {cell(1, 1, u"2"), cell(0, 1, u"b"), cell(1, 0, u"1"), cell(0, 0, u"a")};
    EXPECT_EQ(pastedTableToMarkdown(t), kSmall);
}

TEST(S031_PasteTables, EveryRowIsAsWideAsTheWidestAndEmptyColumnsAndRowsStay) {
    PastedTable ragged;
    ragged.cells = {cell(0, 0, u"a"), cell(1, 0, u"1"), cell(1, 1, u"2"), cell(1, 2, u"3"), cell(2, 0, u"x")};
    EXPECT_EQ(pastedTableToMarkdown(ragged),
              u"| a |  |  |\n"
              u"| --- | --- | --- |\n"
              u"| 1 | 2 | 3 |\n"
              u"| x |  |  |");
    // T7: a column and a row that are empty throughout.
    const PastedTable t7 = grid({{u"Product", u"Origin", u"", u"Stock"},
                                 {u"Green tea", u"Japan", u"", u"12"},
                                 {u"", u"", u"", u""},
                                 {u"Black tea", u"India", u"", u"7"}});
    EXPECT_EQ(pastedTableToMarkdown(t7),
              u"| Product | Origin |  | Stock |\n"
              u"| --- | --- | --- | --- |\n"
              u"| Green tea | Japan |  | 12 |\n"
              u"|  |  |  |  |\n"
              u"| Black tea | India |  | 7 |");
}

TEST(S031_PasteTables, SizeChangesNothing) {
    // Review O4/O11: no size limit; 5,000 x 10 cells by the same rules as four.
    PastedTable t;
    constexpr std::uint32_t rows = 5000;
    constexpr std::uint32_t columns = 10;
    for (std::uint32_t r = 0; r < rows; ++r) {
        for (std::uint32_t c = 0; c < columns; ++c) {
            t.cells.push_back(cell(r, c, r == 7 && c == 3 ? u"a|b\nc" : u"x"));
        }
    }
    const std::u16string md = pastedTableToMarkdown(t);
    std::size_t lines = 1;
    for (const char16_t ch : md) {
        lines += ch == u'\n' ? 1 : 0;
    }
    EXPECT_EQ(lines, rows + 1);
    EXPECT_NE(md.find(u"| x | x | x | a\\|b<br>c | x |"), std::u16string::npos);
    EXPECT_EQ(md.back(), u'|');
}

// --- The blocks of an HTML paste -------------------------------------------------------------------

TEST(S031_PasteTables, ATableAloneBecomesOneBlock) {
    const std::vector<PastedPart> parts = {tablePart(exampleTable())};
    const auto blocks = pastedTablesToMarkdown(parts);
    if (!blocks) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "blocks: no value";
    }
    EXPECT_EQ(*blocks, (std::vector<PasteBlock>{tableBlock(kExample)}));
}

TEST(S031_PasteTables, TextAroundTablesStaysPlainText) {
    // T2 from Outlook: a paragraph before, the table, a line after; no escapes in text.
    const std::vector<PastedPart> parts = {
        textPart({u"The spring order is below."}),
        tablePart(grid({{u"Product", u"Boxes"}, {u"Oolong", u"4"}, {u"Rooibos", u"2"}})),
        textPart({u"Please confirm by *Friday*."})};
    const auto blocks = pastedTablesToMarkdown(parts);
    if (!blocks) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "blocks: no value";
    }
    EXPECT_EQ(*blocks, (std::vector<PasteBlock>{textBlock(u"The spring order is below."),
                                                tableBlock(u"| Product | Boxes |\n"
                                                           u"| --- | --- |\n"
                                                           u"| Oolong | 4 |\n"
                                                           u"| Rooibos | 2 |"),
                                                textBlock(u"Please confirm by *Friday*.")}));
}

TEST(S031_PasteTables, TextLinesAreTrimmedAndEmptyLinesAtTheEdgesDropped) {
    const std::vector<PastedPart> parts = {textPart({u"", u"\u00a0", u"  first\u00a0", u"", u"\tsecond  ", u" "}),
                                           tablePart(grid({{u"a", u"b"}, {u"1", u"2"}})), textPart({u"\u00a0", u""})};
    const auto blocks = pastedTablesToMarkdown(parts);
    if (!blocks) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "blocks: no value";
    }
    EXPECT_EQ(*blocks, (std::vector<PasteBlock>{textBlock(u"first\n\nsecond"), tableBlock(kSmall)}));
}

TEST(S031_PasteTables, TwoTablesWithTextBetween) {
    // T9 from Firefox.
    const std::vector<PastedPart> parts = {tablePart(grid({{u"Tea", u"Cups"}, {u"Sencha", u"3"}})),
                                           textPart({u"Text between the tables."}),
                                           tablePart(grid({{u"Shop", u"Town"}, {u"Leaf and Cup", u"Riverside"}}))};
    const auto blocks = pastedTablesToMarkdown(parts);
    if (!blocks) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "blocks: no value";
    }
    EXPECT_EQ(*blocks,
              (std::vector<PasteBlock>{tableBlock(u"| Tea | Cups |\n| --- | --- |\n| Sencha | 3 |"),
                                       textBlock(u"Text between the tables."),
                                       tableBlock(u"| Shop | Town |\n| --- | --- |\n| Leaf and Cup | Riverside |")}));
}

TEST(S031_PasteTables, ASmallerTableCountsAsTextLineByLine) {
    const std::vector<PastedPart> parts = {tablePart(grid({{u"a", u"b"}, {u"1", u"2"}})),
                                           tablePart(grid({{u"Sencha", u" Assam\nblack ", u"Oolong"}})),
                                           tablePart(grid({{u"x"}, {u"y"}}))};
    const auto blocks = pastedTablesToMarkdown(parts);
    if (!blocks) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "blocks: no value";
    }
    ASSERT_EQ(blocks->size(), 2U);
    EXPECT_EQ((*blocks)[0], tableBlock(kSmall));
    EXPECT_EQ((*blocks)[1], textBlock(u"Sencha\nAssam\nblack\nOolong\nx\ny"));
}

TEST(S031_PasteTables, WithoutATableThatCountsThePasteStaysPlainText) {
    EXPECT_FALSE(pastedTablesToMarkdown(std::vector<PastedPart>{}).has_value());
    EXPECT_FALSE(pastedTablesToMarkdown(std::vector<PastedPart>{textPart({u"just text"})}).has_value());
    EXPECT_FALSE(pastedTablesToMarkdown(std::vector<PastedPart>{tablePart(grid({{u"Sencha", u"Assam", u"Oolong"}}))})
                     .has_value());
    EXPECT_FALSE(
        pastedTablesToMarkdown(std::vector<PastedPart>{tablePart(grid({{u"Sencha"}, {u"Assam"}}))}).has_value());
}

TEST(S031_PasteTables, ANestedTableKeepsTheWholePastePlainText) {
    // T12: the outer table holds a table in a cell; Markdown has no nested tables.
    PastedTable outer = grid({{u"Tea", u"Blend"}, {u"House blend", u"Sencha 60 %\nAssam 40 %"}});
    outer.containsTable = true;
    EXPECT_FALSE(pastedTablesToMarkdown(std::vector<PastedPart>{tablePart(outer)}).has_value());
    EXPECT_FALSE(
        pastedTablesToMarkdown(std::vector<PastedPart>{tablePart(exampleTable()), textPart({u"x"}), tablePart(outer)})
            .has_value());
}

// --- Excel's plain text ----------------------------------------------------------------------------

TEST(S031_PasteTables, ExcelsPlainTextBecomesATable) {
    // excel-t6-example.txt: a quoted cell with a line break, a `"` in an unquoted cell, an empty cell.
    const auto t = parseExcelPlainText(
        u"Product\tNote\tPrice per kg\n"
        u"Green tea\t\"first line\nsecond line\"\t12,50\n"
        u"Black tea\t5\" tin\t9,80\n"
        u"Oolong\tloose leaf\t15,00\n"
        u"Rooibos\t\t7,25\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t),
              u"| Product | Note | Price per kg |\n"
              u"| --- | --- | --- |\n"
              u"| Green tea | first line<br>second line | 12,50 |\n"
              u"| Black tea | 5\" tin | 9,80 |\n"
              u"| Oolong | loose leaf | 15,00 |\n"
              u"| Rooibos |  | 7,25 |");
}

TEST(S031_PasteTables, ExcelQuotesOnlyCellsWithALineBreakOrATab) {
    // excel-quotes.txt (T13): each cell as typed in Excel.
    const auto t = parseExcelPlainText(
        u"Case\tText\n"
        u"start\t\"Leaf\" Co\n"
        u"inside\tsay \"hi\"\n"
        u"break\t\"a \"\"b\"\"\nc\"\n"
        u"\"\"\"a\nb\"\"\"\t\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t),
              u"| Case | Text |\n"
              u"| --- | --- |\n"
              u"| start | \"Leaf\" Co |\n"
              u"| inside | say \"hi\" |\n"
              u"| break | a \"b\"<br>c |\n"
              u"| \"a<br>b\" |  |");
}

TEST(S031_PasteTables, AQuotedFieldNeedsALineBreakOrATabInside) {
    // A field in quotes without a line break or tab is text, quotes and all.
    auto t = parseExcelPlainText(u"\"x\"\ty\n\"\"x\tz\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t), u"| \"x\" | y |\n| --- | --- |\n| \"\"x | z |");
    // A tab inside quotes stays in the cell.
    t = parseExcelPlainText(u"a\tb\n\"c\td\"\te\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t), u"| a | b |\n| --- | --- |\n| c\td | e |");
    // The closing quote must stand before a tab, a line break or the end; otherwise `"` is text.
    t = parseExcelPlainText(u"a\tb\n\"c\nd\" e\tf\n");
    EXPECT_FALSE(t.has_value());  // "c starts no field that closes: three rows of different widths
    // A quote that never closes is text too.
    t = parseExcelPlainText(u"a\t\"b\nc\td\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t), u"| a | \"b |\n| --- | --- |\n| c | d |");
}

TEST(S031_PasteTables, AFinalLineBreakEndsTheLastRow) {
    const auto with = parseExcelPlainText(u"a\tb\n1\t2\n");
    const auto without = parseExcelPlainText(u"a\tb\n1\t2");
    if (!with) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "with: no value";
    }
    if (!without) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "without: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*with), kSmall);
    EXPECT_EQ(pastedTableToMarkdown(*without), kSmall);
    // An empty last field still counts.
    const auto empty = parseExcelPlainText(u"a\tb\n1\t\n");
    if (!empty) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "empty: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*empty), u"| a | b |\n| --- | --- |\n| 1 |  |");
}

TEST(S031_PasteTables, PlainTextThatIsNoTableIsRefused) {
    EXPECT_FALSE(parseExcelPlainText(u"").has_value());
    EXPECT_FALSE(parseExcelPlainText(u"just a line\n").has_value());
    EXPECT_FALSE(parseExcelPlainText(u"a\tb\tc\n").has_value());         // one row
    EXPECT_FALSE(parseExcelPlainText(u"a\nb\nc\n").has_value());         // one column
    EXPECT_FALSE(parseExcelPlainText(u"a\tb\nc\td\te\n").has_value());   // different widths
    EXPECT_FALSE(parseExcelPlainText(u"\ta\tb\n\tc\td\n").has_value());  // indented text
    EXPECT_FALSE(parseExcelPlainText(u"a\tb\n\nc\td\n").has_value());    // an empty line
    // outlook-t1-example.txt: lines of a cell as raw lines, not reconstructable.
    EXPECT_FALSE(parseExcelPlainText(u"Product\tSupplier\tPrice per kg\n"
                                     u"Green tea\tTea House North\n"
                                     u"Tea House South\t12,50\n"
                                     u"13,20\n"
                                     u"Black tea\tTea House West\t9,80\n")
                     .has_value());
}

TEST(S031_PasteTables, SomeRowsMayStartWithATab) {
    const auto t = parseExcelPlainText(u"a\tb\n\tc\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    EXPECT_EQ(pastedTableToMarkdown(*t), u"| a | b |\n| --- | --- |\n|  | c |");
}

TEST(S031_PasteTables, ExcelsTableCarriesNoSpans) {
    const auto t = parseExcelPlainText(u"a\tb\n1\t\"2\n3\"\n");
    if (!t) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "t: no value";
    }
    ASSERT_EQ(t->cells.size(), 4U);
    for (const auto& c : t->cells) {
        EXPECT_EQ(c.rowSpan, 1U);
        EXPECT_EQ(c.columnSpan, 1U);
    }
    EXPECT_FALSE(t->containsTable);
    EXPECT_EQ(t->cells[3].lines, (std::vector<std::u16string>{u"2", u"3"}));
}

// --- Placement -------------------------------------------------------------------------------------

namespace {
const std::vector<PasteBlock> kTableOnly = {tableBlock(kSmall)};
const std::size_t kSmallSize = kSmall.size();
}  // namespace

TEST(S031_PasteTables, OnAnEmptyLineBetweenParagraphs) {
    const auto p = placePastedBlocks(kTableOnly, {u"", u"", u"Para one.", u"Para two."});
    EXPECT_EQ(p.text, u"\n" + std::u16string(kSmall) + u"\n");
    EXPECT_EQ(p.caret, 1 + kSmallSize);
}

TEST(S031_PasteTables, InTheMiddleOfALineTheLineIsSplit) {
    const auto p = placePastedBlocks(kTableOnly, {u"Hello ", u"world", u"above", u"below"});
    EXPECT_EQ(p.text, u"\n\n" + std::u16string(kSmall) + u"\n\n");
    EXPECT_EQ(p.caret, 2 + kSmallSize);  // after the table's last row
}

TEST(S031_PasteTables, AtTheEndOfALine) {
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"Hello", u"", std::nullopt, std::nullopt}).text,
              u"\n\n" + std::u16string(kSmall));
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"Hello", u"", std::nullopt, u"next"}).text,
              u"\n\n" + std::u16string(kSmall) + u"\n");
    // Next to an existing blank line none is added.
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"Hello", u"", std::nullopt, u""}).text, u"\n\n" + std::u16string(kSmall));
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"Hello", u"", std::nullopt, u"  "}).text,
              u"\n\n" + std::u16string(kSmall));
}

TEST(S031_PasteTables, AtTheStartOfALineOrOfTheDocument) {
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"", u"Hello", std::nullopt, std::nullopt}).text,
              std::u16string(kSmall) + u"\n\n");
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"", u"Hello", u"", std::nullopt}).text, std::u16string(kSmall) + u"\n\n");
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"", u"Hello", u"text", std::nullopt}).text,
              u"\n" + std::u16string(kSmall) + u"\n\n");
    const auto empty = placePastedBlocks(kTableOnly, {u"", u"", std::nullopt, std::nullopt});
    EXPECT_EQ(empty.text, kSmall);
    EXPECT_EQ(empty.caret, kSmallSize);
}

TEST(S031_PasteTables, OnTheBlankLineBelowATableOneBlankLineSeparatesTheTables) {
    // Review O7: the placement replaces S018's beforeWritingBelow for the paste.
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"", u"", u"| 1 | 2 |", std::nullopt}).text,
              u"\n" + std::u16string(kSmall));
    EXPECT_EQ(placePastedBlocks(kTableOnly, {u"", u"", u"| 1 | 2 |", u"after"}).text,
              u"\n" + std::u16string(kSmall) + u"\n");
}

TEST(S031_PasteTables, SeveralTablesAreSeparatedByOneBlankLine) {
    const std::vector<PasteBlock> blocks = {tableBlock(kSmall), tableBlock(kSmall)};
    const auto p = placePastedBlocks(blocks, {u"", u"", std::nullopt, std::nullopt});
    EXPECT_EQ(p.text, std::u16string(kSmall) + u"\n\n" + std::u16string(kSmall));
    EXPECT_EQ(p.caret, p.text.size());
}

TEST(S031_PasteTables, PastedTextContinuesTheLinesAsAPlainPaste) {
    // Scenario "text before and after a table from Outlook": the caret on an empty line between two
    // paragraphs; the text before goes on the caret's line, the text after continues the line.
    const std::vector<PasteBlock> blocks = {textBlock(u"The spring order is below."), tableBlock(kSmall),
                                            textBlock(u"Please confirm by Friday.")};
    const auto p = placePastedBlocks(blocks, {u"", u"", u"Para one.", u"Para two."});
    const std::u16string expected =
        u"The spring order is below.\n\n" + std::u16string(kSmall) + u"\n\nPlease confirm by Friday.";
    EXPECT_EQ(p.text, expected);
    EXPECT_EQ(p.caret, expected.size());
    // In the middle of a line: the text joins the text before and after the caret.
    const auto mid = placePastedBlocks(blocks, {u"x", u"y", std::nullopt, std::nullopt});
    EXPECT_EQ(mid.text, expected);
    EXPECT_EQ(mid.caret, expected.size());
}

TEST(S031_PasteTables, TextBetweenTablesHasABlankLineOnEachSide) {
    const std::vector<PasteBlock> blocks = {tableBlock(kSmall), textBlock(u"between\nsecond line"), tableBlock(kSmall)};
    const auto p = placePastedBlocks(blocks, {u"", u"", std::nullopt, std::nullopt});
    EXPECT_EQ(p.text, std::u16string(kSmall) + u"\n\nbetween\nsecond line\n\n" + std::u16string(kSmall));
}

// Review O1 and O9: one linear scan of the HTML decides before Qt's import whether there is a
// table at all and whether a table holds a table.
TEST(S031_PasteTables, ScanFindsNoTableFlatTablesOrNestedTables) {
    EXPECT_EQ(scanHtmlTables(u""), HtmlTables::None);
    EXPECT_EQ(scanHtmlTables(u"<p>a</p><pre><span>int</span> x;</pre>"), HtmlTables::None);
    EXPECT_EQ(scanHtmlTables(u"<p>a &lt;table&gt; b</p><tablex>"), HtmlTables::None);
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td>a</td></tr></table>"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<TABLE class=MsoNormalTable>x</TABLE>\n<Table\nborder=1>y</tAbLe >"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<table/>"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td><table><tr><td>x</td></tr></table></td></tr></table>"),
              HtmlTables::Nested);
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td><TABLE\tborder=0>"), HtmlTables::Nested);
    // Comments are skipped, also Office's conditional comments.
    EXPECT_EQ(scanHtmlTables(u"<!-- <table> --><p>a</p>"), HtmlTables::None);
    EXPECT_EQ(scanHtmlTables(u"<table><!--[if gte mso 9]><table><![endif]--></table>"), HtmlTables::Flat);
    // An end tag without a start tag changes nothing; an unclosed comment ends the scan.
    EXPECT_EQ(scanHtmlTables(u"</table><table></table>"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<!-- <table>"), HtmlTables::None);
}

TEST(S031_PasteTables, ScanIsLinearInDeepNesting) {
    // 100,000 nested tables (about 2.4 MB): the scan stops at the second level.
    std::u16string html;
    for (int i = 0; i < 100000; ++i) {
        html += u"<table><tr><td>";
    }
    EXPECT_EQ(scanHtmlTables(html), HtmlTables::Nested);
}

// Same result at every size, within bounds (rework of 2026-11-20): Qt's import of a paste's HTML
// may take half of what S031 grants its cells. The scan reads the HTML as Qt does and estimates the
// import from what it counts; above the bound the paste stays plain text and Qt's import does not
// run. The cases below are the shapes whose import grows far beyond their cells.
namespace {

std::u16string repeat(std::u16string_view s, int n) {
    std::u16string out;
    out.reserve(s.size() * static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out += s;
    }
    return out;
}

/// A table of two rows whose last cell holds `inner`.
std::u16string inCell(std::u16string_view inner) {
    return u"<table><tr><td>a</td><td>b</td></tr><tr><td>c</td><td>" + std::u16string(inner) + u"</td></tr></table>";
}

std::u16string number(std::uint64_t n) {
    std::u16string out;
    for (const char c : std::to_string(n)) {
        out += static_cast<char16_t>(c);
    }
    return out;
}

/// A table of `rows` rows of two cells below a first cell spanning `columns` columns.
std::u16string spannedTable(std::uint64_t columns, int rows) {
    return u"<table><tr><td colspan=" + number(columns) + u">a</td><td>b</td></tr>" +
           repeat(u"<tr><td>1</td><td>2</td></tr>", rows) + u"</table>";
}

}  // namespace

TEST(S031_PasteTables, AGridFarBeyondItsCellsStaysPlainText) {
    // A first cell spanning 20,000 columns above 2,000 rows (58 KB): 4,002 cells, but Qt's import
    // builds 40 million positions and takes minutes.
    EXPECT_EQ(scanHtmlTables(spannedTable(20000, 2000)), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(spannedTable(20000, 5)), HtmlTables::OverBudget);
    // A span above Qt's limit counts as Qt's 20,480 columns; one in an entity as that many.
    EXPECT_EQ(scanHtmlTables(spannedTable(2000000000, 5)), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td colspan='2&#48;000'>a</td><td>b</td></tr>" +
                             repeat(u"<tr><td>1</td><td>2</td></tr>", 5) + u"</table>"),
              HtmlTables::OverBudget);
    // Rows without cells below a wide row, and cells spanning many rows, widen the grid as well.
    EXPECT_EQ(scanHtmlTables(u"<table><tr>" + repeat(u"<td>x</td>", 1000) + u"</tr>" + repeat(u"<tr></tr>", 100) +
                             u"</table>"),
              HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(u"<table><tr>" + repeat(u"<td rowspan=1000>r</td>", 100) + u"</tr>" +
                             repeat(u"<tr><td>1</td><td>2</td></tr>", 1000) + u"</table>"),
              HtmlTables::OverBudget);
}

TEST(S031_PasteTables, SpansAsTablesHaveThemConvert) {
    // A heading across ten columns above 1,000 rows of ten cells, and a first column whose cells
    // span two rows each: positions and cells grow together.
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td colspan=10>Heading</td></tr>" +
                             repeat(u"<tr>" + repeat(u"<td>x</td>", 10) + u"</tr>", 1000) + u"</table>"),
              HtmlTables::Flat);
    EXPECT_EQ(
        scanHtmlTables(u"<table>" +
                       repeat(u"<tr><td rowspan=2>a</td><td>b</td><td>c</td></tr><tr><td>d</td><td>e</td></tr>", 500) +
                       u"</table>"),
        HtmlTables::Flat);
}

TEST(S031_PasteTables, TheGridBoundSitsWhereItsCostsSay) {
    // Rows of two cells below a cell spanning 100 columns: each row adds 99 positions no cell
    // starts on and two cells. The bound sits where their cost reaches what the cells allow.
    const PasteHtmlBounds& b = kPasteHtmlBounds;
    const std::uint64_t perRow = 99 * b.costs.positionWithoutCell;
    const auto rows = static_cast<int>(b.floorPs / perRow);
    EXPECT_EQ(scanHtmlTables(spannedTable(100, rows / 2)), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(spannedTable(100, rows * 2)), HtmlTables::OverBudget);
    const HtmlScan scan = scanHtml(spannedTable(100, rows * 2));
    EXPECT_GT(scan.importPs, scan.allowedPs);
    EXPECT_EQ(scan.cells, static_cast<std::uint64_t>(2 * rows * 2 + 2));
}

TEST(S031_PasteTables, NestingDeeperThanTheBoundStaysPlainText) {
    // 200,000 nested div in a cell (2 MB): each element is checked against all around it.
    const int deep = 200000;
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<div>", deep) + u"x" + repeat(u"</div>", deep))), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<div>", deep) + u"x")), HtmlTables::OverBudget);
    // The bound counts the elements Qt keeps open: the table, its row and cell are three.
    const auto inside = static_cast<int>(kPasteHtmlBounds.maxDepth) - 3;
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<span>", inside) + u"x" + repeat(u"</span>", inside))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<span>", inside + 1) + u"x" + repeat(u"</span>", inside + 1))),
              HtmlTables::OverBudget);
}

TEST(S031_PasteTables, ElementsQtDoesNotNestStayFlat) {
    // Qt closes a p at the next p and an li at the next li, an end tag closes everything inside its
    // element, and img, br, hr and meta never hold anything: none of this nests (300 items are
    // deeper than the bound if they nested, and few enough for one list).
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<p>x", 1000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul>" + repeat(u"<li>x", 300) + u"</ul>")), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<div><span><b>x</div>", 1000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"x<br>", 1000) + repeat(u"<hr>", 300))), HtmlTables::Flat);
    // Rows and cells without end tags, as older HTML writes them.
    EXPECT_EQ(scanHtmlTables(u"<table>" + repeat(u"<tr><td>a<td>b<td>c", 1000) + u"</table>"), HtmlTables::Flat);
    // Excel's columns stay open until the first row closes them.
    EXPECT_EQ(scanHtmlTables(u"<table>" + repeat(u"<col width=64>", 200) +
                             repeat(u"<tr><td>a</td><td>b</td></tr>", 10) + u"</table>"),
              HtmlTables::Flat);
}

TEST(S031_PasteTables, MarkupFarBeyondItsCellsStaysPlainText) {
    // 100,000 bold words in one cell of four: Qt makes an element and a format for each.
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<b>x</b>", 100000))), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<b>x</b>", 1000))), HtmlTables::Flat);
    // Attributes cost by their length; all but the spans of a cell leave before the import, and
    // the scan reads the HTML as Qt gets it.
    const std::u16string attributes = inCell(u"<span" + repeat(u" a=b", 200000) + u">x</span>");
    EXPECT_EQ(scanHtmlTables(attributes), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(htmlWithoutFormatting(attributes)), HtmlTables::Flat);
    const std::u16string styled = inCell(u"<span style='" + repeat(u"color:red;", 200000) + u"'>x</span>");
    EXPECT_EQ(scanHtmlTables(styled), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(htmlWithoutFormatting(styled)), HtmlTables::Flat);
    // Text, entities and comments are content: they cost Qt less than a plain paste of them.
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"word ", 800000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"&amp;", 800000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<!--" + repeat(u"x", 4000000) + u"-->")), HtmlTables::Flat);
}

TEST(S031_PasteTables, ManyCellsAllowMoreMarkup) {
    // What a paste may cost grows with its cells: 5,000 rows of ten cells with a link each.
    const std::u16string row =
        u"<tr>" + repeat(u"<td><a href='https://example.org/'>x</a> <b>y</b></td>", 10) + u"</tr>";
    const HtmlScan scan = scanHtml(u"<table>" + repeat(row, 5000) + u"</table>");
    EXPECT_EQ(scan.tables, HtmlTables::Flat);
    EXPECT_EQ(scan.cells, 50000U);
    EXPECT_EQ(scan.allowedPs, 50000 * kPasteHtmlBounds.perCellPs);
    EXPECT_EQ(scanHtml(u"<table><tr><td>a</td></tr></table>").allowedPs, kPasteHtmlBounds.floorPs);
}

TEST(S031_PasteTables, StyleSheetsLeaveBeforeTheImport) {
    // The text of a style element is a style sheet. Qt checks every selector against every element
    // after it, and for `b ~ span` against every sibling before each span: 8,785 spans in a cell
    // took 30 s. The sheet leaves before the import; the paste converts without it.
    const std::u16string spans = repeat(u"<span/>", 8785);
    const std::u16string sibling = u"<style>b ~ span{color:red}</style>" + inCell(spans);
    EXPECT_EQ(htmlWithoutFormatting(sibling), u"<style></style>" + inCell(spans));
    const HtmlScan without = scanHtml(htmlWithoutFormatting(sibling));
    EXPECT_EQ(without.tables, HtmlTables::Flat);
    EXPECT_LT(without.importPs, without.allowedPs);
    // A sheet the scan still meets is not estimated: the paste stays plain text, also with a short
    // sheet. A sheet inside a comment, as Office writes it, is no sheet to Qt.
    EXPECT_EQ(scanHtmlTables(sibling), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(u"<style>p{margin:0} td{padding:2px}</style>" + inCell(u"x")), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(u"<style><!--p{margin:0}--></style>" + inCell(u"x")), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<style/>p{margin:0}" + inCell(u"x")), HtmlTables::Flat);
}

TEST(S031_PasteTables, StyleElementsLoseTheTextQtReadsAsTheirSheet) {
    // Qt's sheet is the text from the start tag to the next tag; a comment does not end it, and Qt
    // reads it at the next end tag of any element. All of it leaves, the rest stays.
    const std::vector<std::pair<std::u16string_view, std::u16string_view>> cases = {
        {u"<style>b ~ span{color:red}</style><p>x</p>", u"<style></style><p>x</p>"},
        {u"<STYLE type=\"text/css\">a{}<!-- b{} -->c{}</STYLE>", u"<STYLE><!-- b{} --></STYLE>"},
        {u"<div><style>a{}</div>x", u"<div><style></div>x"},
        {u"<style>a{}<p>b{}</p>", u"<style><p>b{}</p>"},
        {u"<style>&lt;a&gt;{}</style>", u"<style></style>"},
        {u"<style/>a{}", u"<style/>a{}"},
        {u"<p>style{}</p><!-- <style>a{}</style> -->", u"<p>style{}</p><!-- <style>a{}</style> -->"},
    };
    for (const auto& [html, expected] : cases) {
        EXPECT_EQ(htmlWithoutFormatting(html), expected);
    }
}

TEST(S031_PasteTables, ManyImagesInOneBlockStayPlainText) {
    // Qt steps over the whole block for each image it inserts.
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<img src=a.png>", 5000))), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<img src=a.png>", 100))), HtmlTables::Flat);
    // One image per cell: each starts its own block.
    EXPECT_EQ(scanHtmlTables(u"<table>" + repeat(u"<tr>" + repeat(u"<td><img src=a.png></td>", 10) + u"</tr>", 500) +
                             u"</table>"),
              HtmlTables::Flat);
}

TEST(S031_PasteTables, FormatsQtComparesWithEachOtherLeaveBeforeTheImport) {
    // Qt stores a format after comparing it with every format before it whose hash is the same.
    // The hash of a list is its type (an id, the name of a link, the face of a font, the widths of
    // a table's columns), and the hash of a format is a sum (a font's size and its colour add up).
    // Each such format takes longer than the one before; the attributes leave before the import.
    const auto hex = [](std::uint64_t n) {
        std::u16string out = u"#";
        for (int shift = 20; shift >= 0; shift -= 4) {
            out += u"0123456789abcdef"[(n >> static_cast<unsigned>(shift)) & 15U];
        }
        return out;
    };
    std::u16string names;
    std::u16string faces;
    std::u16string ids;
    std::u16string sums;
    std::u16string widths;
    for (std::uint64_t i = 1; i <= 5000; ++i) {
        names += u"<a name=n" + number(i) + u">x</a>";
        faces += u"<font face=f" + number(i) + u">x</font>";
        ids += u"<span id=i" + number(i) + u">x</span>";
        sums += u"<font size=" + number((std::uint64_t{1} << 24U) - i + 3) + u" color=" + hex(i) + u">x</font>";
        widths += u"<table><tr><td width=" + number(i) + u">a</td><td>b</td></tr><tr><td>c</td><td>d</td></tr></table>";
    }
    for (const std::u16string& html : {inCell(names), inCell(faces), inCell(ids), inCell(sums), widths}) {
        const std::u16string without = htmlWithoutFormatting(html);
        EXPECT_EQ(without.find(u'='), std::u16string::npos);
        EXPECT_EQ(scanHtmlTables(without), HtmlTables::Flat);
    }
    EXPECT_EQ(htmlWithoutFormatting(inCell(u"<a name=_Hlk1>x</a><span id=top>y</span><font face=Arial>z</font>")),
              inCell(u"<a>x</a><span>y</span><font>z</font>"));
}

TEST(S031_PasteTables, OneLongListStaysPlainText) {
    // Qt marks every block of a list again for each block that joins it, so one list costs Qt the
    // square of its items: 4,000 items took 0.4 to 0.6 s, 16,000 items 7 to 11 s.
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul>" + repeat(u"<li>x</li>", 4000) + u"</ul>")), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ol>" + repeat(u"<li>x", 4000) + u"</ol>")), HtmlTables::OverBudget);
    // 2,000 cells, one of them with a list of 15,908 items (181 KB): 10.5 s.
    const std::u16string first =
        u"<tr><td><ul>" + repeat(u"<li>x</li>", 15908) + u"</ul></td>" + repeat(u"<td>a</td>", 9) + u"</tr>";
    const HtmlScan large =
        scanHtml(u"<table>" + first + repeat(u"<tr>" + repeat(u"<td>a</td>", 10) + u"</tr>", 199) + u"</table>");
    EXPECT_EQ(large.cells, 2000U);
    EXPECT_EQ(large.tables, HtmlTables::OverBudget);
    // Items of a list inside an item count for the list around it too.
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul><li>a<ol>" + repeat(u"<li>x</li>", 4000) + u"</ol></li></ul>")),
              HtmlTables::OverBudget);
    // Many short lists cost nothing extra, nor do items without a list, and a short list converts.
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<ul><li>x</li></ul>", 1000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(repeat(u"<li>x</li>", 1000))), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ol>" + repeat(u"<li>x</li>", 300) + u"</ol>")), HtmlTables::Flat);
}

TEST(S031_PasteTables, BlocksThatJoinAListCountAsItsItems) {
    // A paragraph separator, and a line break in a pre, start a new block of the item they are in,
    // which joins its list: as costly as an item. As an entity too.
    for (const std::u16string& lines :
         {repeat(u"x\u2029", 4000), repeat(u"x&#8233;", 4000), u"<pre>" + repeat(u"x\n", 4000) + u"</pre>",
          u"<pre>" + repeat(u"x&#10;", 4000) + u"</pre>", u"<pre>" + repeat(u"x&#x0A;", 4000) + u"</pre>"}) {
        EXPECT_EQ(scanHtmlTables(inCell(u"<ul><li>" + lines + u"</li></ul>")), HtmlTables::OverBudget);
    }
    // Outside a list, and line breaks outside a pre, which Qt reads as spaces, cost nothing extra.
    EXPECT_EQ(scanHtmlTables(inCell(u"<pre>" + repeat(u"x\n", 4000) + u"</pre>")), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul><li>a</li></ul><pre>" + repeat(u"x\n", 4000) + u"</pre>")), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul><li>" + repeat(u"x\n", 4000) + u"</li></ul>")), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(inCell(u"<ul><li>" + repeat(u"x&#8217;", 4000) + u"</li></ul>")), HtmlTables::Flat);
}

TEST(S031_PasteTables, TheScanReadsTagsAsQtDoes) {
    // A table tag in an attribute value is no table; a blank after `<` and a quoted name are tags.
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td title=\"<table>\">a</td><td>b</td></tr></table>"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"< table><tr><td>a</td></tr></table>"), HtmlTables::Flat);
    EXPECT_EQ(scanHtmlTables(u"<\"table\"><tr><td>a</td></tr></table>"), HtmlTables::Flat);
    // A cell span after a value that holds `>` still counts.
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td title=\"a>b\" colspan=20000>a</td></tr>" +
                             repeat(u"<tr><td>1</td></tr>", 5) + u"</table>"),
              HtmlTables::OverBudget);
    // A cell straight in a table: Qt puts a table of its own there.
    EXPECT_EQ(scanHtmlTables(u"<table><td>a</td></table>"), HtmlTables::Nested);
    // An entity in a tag or attribute name: no program writes it, and Qt would decode it.
    EXPECT_EQ(scanHtmlTables(u"<table><tr><t&#100;>a</td></tr></table>"), HtmlTables::OverBudget);
    EXPECT_EQ(scanHtmlTables(u"<table><tr><td c&#111;lspan=9>a</td></tr></table>"), HtmlTables::OverBudget);
}

namespace {

/// `above` for pasteLineInContainer: `lines` from the top of the document to the line above.
std::function<std::optional<std::u16string>(std::size_t)> linesAbove(std::vector<std::u16string> lines) {
    return [lines = std::move(lines)](std::size_t i) -> std::optional<std::u16string> {
        if (i >= lines.size()) {
            return std::nullopt;
        }
        return lines[lines.size() - 1 - i];
    };
}

}  // namespace

// Review O4: a paste in a block quote or list item stays plain text; where a table would split the
// container, cmark-gfm (cmarkgfm 2025.10.22) shows the rest of the line outside it.
TEST(S031_PasteTables, BlockQuotesAndListItemsAreContainers) {
    for (const std::u16string_view line :
         {u"> quoted text here", u">", u"   > indented quote", u"- item one", u"+ item", u"* item", u"-",
          u"1. step one", u"2) step two", u"123456789. nine digits", u"  - nested item", u"> - item in a quote"}) {
        EXPECT_TRUE(pasteLineInContainer(line, linesAbove({}))) << "container line of length " << line.size();
    }
    for (const std::u16string_view line : {u"Para one.", u"", u"   ", u"-item", u"1.5 kg", u"1234567890. ten digits",
                                           u"---", u"* * *", u"- - -", u"# heading", u"| a | b |", u"a > b"}) {
        EXPECT_FALSE(pasteLineInContainer(line, linesAbove({}))) << "plain line of length " << line.size();
    }
}

TEST(S031_PasteTables, ParagraphLinesOfAContainerAreInIt) {
    // A paragraph continues the quote or item above it, also lazily.
    EXPECT_TRUE(pasteLineInContainer(u"lazy line", linesAbove({u"> quoted"})));
    EXPECT_TRUE(pasteLineInContainer(u"   more of step one", linesAbove({u"1. step one"})));
    EXPECT_TRUE(pasteLineInContainer(u"third line", linesAbove({u"- item", u"second line"})));
    // A blank line ends a quote and a lazy paragraph.
    EXPECT_FALSE(pasteLineInContainer(u"Para", linesAbove({u"> quoted", u""})));
    EXPECT_FALSE(pasteLineInContainer(u"Para", linesAbove({u"- item", u""})));
    // Indented under a list item after blank lines: the item's next paragraph.
    EXPECT_TRUE(pasteLineInContainer(u"   second paragraph", linesAbove({u"1. step one", u"", u""})));
    EXPECT_TRUE(pasteLineInContainer(u"  more", linesAbove({u"- item", u"", u"  first", u""})));
    // Indented after blank lines below a paragraph: no container.
    EXPECT_FALSE(pasteLineInContainer(u"  indented", linesAbove({u"Para one.", u""})));
    EXPECT_FALSE(pasteLineInContainer(u"Para two.", linesAbove({u"Para one."})));
    EXPECT_FALSE(pasteLineInContainer(u"Para", linesAbove({})));
}

// Style attributes leave the HTML before Qt's import since 2026-11-10 (Armin), which halves the
// import of Outlook's markup; since the rework of 2026-11-20 every attribute does but the spans of
// a cell and the name and content of meta. Qt 6.8.3 and 6.12.0 ignore display: none, so no text
// changes. Text, comments and end tags are kept as they are.
TEST(S031_PasteTables, FormattingLeavesTheHtmlBeforeTheImport) {
    const std::vector<std::pair<std::u16string_view, std::u16string_view>> cases = {
        {u"<td style='border:solid #888888 1.0pt' rowspan=2>a</td>", u"<td rowspan=2>a</td>"},
        {u"<td width=120 colspan=\"2\" class=xl65>a</td>", u"<td colspan=\"2\">a</td>"},
        {u"<TH WIDTH=\"50%\" ROWSPAN=3>a</TH>", u"<TH ROWSPAN=3>a</TH>"},
        {u"<p class=MsoNormal style=\"margin:0\"><span lang=EN-US style='font-size:11.0pt'>x</span></p>",
         u"<p><span>x</span></p>"},
        {u"<font size=16777218 color=#000001 face=Arial>x</font>", u"<font>x</font>"},
        {u"<a href='https://example.org/' name=_Hlk1>x</a>", u"<a>x</a>"},
        {u"<div colspan=2 rowspan=2>x</div>", u"<div>x</div>"},
        {u"<meta name=qrichtext content=1 charset=utf-8>", u"<meta name=qrichtext content=1>"},
        {u"<td\nstyle='a:b'\n colspan=3>x</td>", u"<td\n colspan=3>x</td>"},
        {u"<img alt='a > b' style='x' src=y>", u"<img>"},
        {u"<br style='mso-data-placement:same-cell'>", u"<br>"},
        {u"<td nowrap colspan=2>x</td>", u"<td colspan=2>x</td>"},
        {u"<td title=\"a\"colspan=2>x</td>", u"<td colspan=2>x</td>"},
        {u"<td a=1/>", u"<td />"},
        {u"<td a=\"1\"/x colspan=2>", u"<td /x colspan=2>"},
        {u"a < b style='x'", u"a < b"},
        {u"<td style='unclosed", u"<td"},
        // Kept byte for byte: text, comments, end tags, a tag with an entity in a name and all after
        // it (Qt would decode the entity; the scan keeps such a paste plain text).
        {u"<p>a=b style='x'</p>", u"<p>a=b style='x'</p>"},
        {u"<!-- <td style='x'> --><td style='y'>z</td class=x>", u"<!-- <td style='x'> --><td>z</td class=x>"},
        {u"<td width=3>a</td><td c&#111;lspan=9 width=3>b", u"<td>a</td><td c&#111;lspan=9 width=3>b"},
        {u"<td colspan=2 colspan=3>x</td>", u"<td colspan=2 colspan=3>x</td>"},
    };
    for (const auto& [html, expected] : cases) {
        EXPECT_EQ(htmlWithoutFormatting(html), expected);
        EXPECT_EQ(htmlWithoutFormatting(expected), expected);
    }
}

TEST(S031_PasteTables, FormattingLeavesInLinearTime) {
    std::u16string html;
    std::u16string expected;
    for (int i = 0; i < 100000; ++i) {
        html += u"<td style='border:solid #888888 1.0pt' width=120><p class=MsoNormal style='margin:0'>x</p></td>";
        expected += u"<td><p>x</p></td>";
    }
    EXPECT_EQ(htmlWithoutFormatting(html), expected);
}
