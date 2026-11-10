// SPDX-License-Identifier: GPL-3.0-or-later
// S018: Markdown pipe tables (ADR 0005). Expectations checked against cmark-gfm 0.29.0.gfm
// (Python cmarkgfm 2025.10.22) on 2026-10-27.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

/// Renders `lines` top to bottom, each with the next line as lookahead.
std::vector<LineRender> showAll(const std::vector<std::u16string>& lines) {
    std::vector<LineRender> out;
    BlockState state;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::u16string_view next =
            i + 1 < lines.size() ? std::u16string_view{lines[i + 1]} : std::u16string_view{};
        out.push_back(registry().render(lines[i], {false, state, next}));
        state = out.back().blockState;
    }
    return out;
}

std::vector<std::u16string> cellTexts(std::u16string_view line) {
    std::vector<std::u16string> texts;
    for (const TableCell& cell : tableRowCells(line)) {
        texts.emplace_back(line.substr(cell.content.start, cell.content.length));
    }
    return texts;
}

/// ASCII text of a line for failure messages; other characters as '?'.
std::string narrow(std::u16string_view text) {
    std::string out;
    for (const char16_t c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}
}  // namespace

TEST(S018_Tables, RecognizesHeaderDelimiterAndBodyRows) {
    const auto r = showAll({u"| Name | Score | Rank |", u"| :--- | :---: | ---: |", u"| Alice | 95 | 1 |"});
    ASSERT_TRUE(r[0].table);
    EXPECT_TRUE(r[0].tableHeader);
    EXPECT_TRUE(r[1].table);
    EXPECT_TRUE(r[1].tableDelimiter);
    EXPECT_TRUE(r[2].table);
    EXPECT_FALSE(r[2].tableHeader);
    EXPECT_FALSE(r[2].tableDelimiter);
    ASSERT_EQ(r[2].cells.size(), 3u);
    EXPECT_EQ(r[2].cells[0].content, (Span{2, 5}));
    EXPECT_EQ(r[2].cells[1].content, (Span{10, 2}));
    EXPECT_EQ(r[2].cells[2].content, (Span{15, 1}));
    // The table view shows the lines (ADR 0005); the lines themselves carry no formats.
    for (const auto& line : r) {
        EXPECT_TRUE(line.hidden.empty());
        EXPECT_TRUE(line.runs.empty());
        EXPECT_TRUE(line.inlineStyles.empty());
    }
}

TEST(S018_Tables, PipeLinesWithoutMatchingDelimiterStayLiteral) {
    for (const std::u16string next : {u"", u"plain text", u"| --- |", u"| --- | --- | --- |", u"| :: | --- |"}) {
        const auto r = showAll({u"| A | B |", next});
        EXPECT_FALSE(r[0].table);
        EXPECT_FALSE(r[0].blockState.active());
    }
}

// cmark-gfm: every unescaped pipe separates cells, also inside a code span.
TEST(S018_Tables, PipesInCodeSpansSeparateCellsLikeGfm) {
    const auto r = showAll({u"| a | b |", u"| --- | --- |", u"| `x | y` | z |"});
    ASSERT_TRUE(r[2].table);
    ASSERT_EQ(r[2].cells.size(), 2u);  // the third cell is beyond the header's column count
    EXPECT_EQ(cellTexts(u"| `x | y` | z |"), (std::vector<std::u16string>{u"`x", u"y`", u"z"}));
    EXPECT_EQ(cellTexts(u"| `x \\| y` |"), (std::vector<std::u16string>{u"`x \\| y`"}));
}

// cmark-gfm: a pipe right after a backslash is escaped, however many backslashes precede it.
TEST(S018_Tables, APipeAfterABackslashIsEscaped) {
    EXPECT_EQ(cellTexts(u"| a \\| b | c |"), (std::vector<std::u16string>{u"a \\| b", u"c"}));
    EXPECT_EQ(cellTexts(u"| x\\\\|y |"), (std::vector<std::u16string>{u"x\\\\|y"}));
}

// cmark-gfm: a line without a pipe continues the table; blank lines and block starts end it.
TEST(S018_Tables, TableContinuesUntilBlankLineOrBlockStart) {
    const auto rows = showAll({u"| a | b |", u"| --- | --- |", u"| c | d |", u"bar", u"", u"bar"});
    EXPECT_TRUE(rows[3].table);
    EXPECT_EQ(cellTexts(u"bar"), (std::vector<std::u16string>{u"bar"}));
    EXPECT_FALSE(rows[4].table);
    EXPECT_FALSE(rows[5].table);

    for (const std::u16string end :
         {u"***", u"---", u"# H", u"```", u"<div>x</div>", u"    code", u"- item", u"> quote", u"1. item", u"|"}) {
        const auto r = showAll({u"| a |", u"| - |", u"| b |", end});
        EXPECT_TRUE(r[2].table);
        EXPECT_FALSE(r[3].table) << narrow(end);
    }
}

TEST(S018_Tables, ATableMayFollowAParagraph) {
    const auto r = showAll({u"Hello", u"| a |", u"| - |", u"| b |"});
    EXPECT_FALSE(r[0].table);
    EXPECT_TRUE(r[1].tableHeader);
    EXPECT_TRUE(r[3].table);
}

TEST(S018_Tables, DelimiterAlignmentsForEveryColumn) {
    std::u16string delimiter = u"|";
    for (int i = 0; i < 14; ++i) {
        delimiter += i == 13 ? u" ---: |" : (i == 12 ? u" :-: |" : u" --- |");
    }
    const auto aligns = tableDelimiterAligns(delimiter);
    const std::vector<TableAlign> fourteen = aligns.value_or(std::vector<TableAlign>{});
    ASSERT_EQ(fourteen.size(), 14u);
    EXPECT_EQ(fourteen[0], TableAlign::Left);
    EXPECT_EQ(fourteen[12], TableAlign::Center);
    EXPECT_EQ(fourteen[13], TableAlign::Right);
    EXPECT_FALSE(tableDelimiterAligns(u"| a |").has_value());
    EXPECT_EQ(tableDelimiterAligns(u"|:-:| -: |"), (std::vector<TableAlign>{TableAlign::Center, TableAlign::Right}));
}

TEST(S018_Tables, RowCellsIncludeEveryCell) {
    EXPECT_EQ(cellTexts(u"| c | d | e |"), (std::vector<std::u16string>{u"c", u"d", u"e"}));
    EXPECT_EQ(cellTexts(u"a | b"), (std::vector<std::u16string>{u"a", u"b"}));
    EXPECT_EQ(cellTexts(u"|  |  |"), (std::vector<std::u16string>{u"", u""}));
    const auto cells = tableRowCells(u"|  | x |");
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[0].content, (Span{2, 0}));  // an empty cell sits after its leading space
}

TEST(S018_Tables, CellTextForEditingAndBack) {
    EXPECT_EQ(tableCellToText(u"a<br>b<br/>c<br />d<BR>e"), u"a\nb\nc\nd\ne");
    EXPECT_EQ(tableCellToText(u"a \\| b"), u"a | b");
    EXPECT_EQ(tableCellToText(u"`x \\| y`"), u"`x | y`");
    EXPECT_EQ(tableCellToText(u"**bold**"), u"**bold**");

    EXPECT_EQ(tableCellFromText(u"a | b\nc"), u"a \\| b<br>c");
    EXPECT_EQ(tableCellFromText(u"x\r\ny"), u"x<br>y");
    EXPECT_EQ(tableCellFromText(tableCellToText(u"a \\| b<br>c")), u"a \\| b<br>c");
}

// S031 review O3: a `<br>` after an odd number of backslashes is text (CommonMark: the backslash
// escapes `<`); after an even number the backslashes escape each other and it is a line break.
TEST(S018_Tables, AnEscapedBreakStaysTextForEditing) {
    EXPECT_EQ(tableCellToText(u"x\\<br>y"), u"x\\<br>y");
    EXPECT_EQ(tableCellToText(u"x\\<BR/>y"), u"x\\<BR/>y");
    EXPECT_EQ(tableCellToText(u"x\\\\\\<br />y"), u"x\\\\\\<br />y");
    EXPECT_EQ(tableCellToText(u"x\\\\<br>y"), u"x\\\\\ny");
    EXPECT_EQ(tableCellToText(u"\\<br>"), u"\\<br>");
    EXPECT_EQ(tableCellToText(u"a<br>b\\<br>c"), u"a\nb\\<br>c");
    // Typed back unchanged: editing the cell keeps the Markdown.
    EXPECT_EQ(tableCellFromText(tableCellToText(u"x\\<br>y")), u"x\\<br>y");
    EXPECT_EQ(tableCellFromText(tableCellToText(u"x\\\\<br>y")), u"x\\\\<br>y");
}

// S031 (sizing pasted tables): cells GFM shows as exactly their characters are laid out as plain
// text. Expectations checked with cmark-gfm and markdown-it-py (tables on): each plain cell renders
// as its text alone.
TEST(S018_Tables, PlainCellsNeedNoMarkdown) {
    for (const std::u16string_view plain :
         {u"", u"r12c3", u"Green tea", u"12,50", u"Tea House North", u"5\" tin", u"say \"hi\"", u"a  b",
          u"Open daily (9 to 18)", u"1.5 kg", u"x.y", u"50 % off; ok?", u"a-b", u"a+b=c", u"$5", u"Grüße, 一言",
          u"no\u00a0break",
          // Review (code) O10, step 3 (Armin, 2026-11-10): a colon not followed by // and an
          // ampersand that starts no entity are text (cmark-gfm: each renders as its characters).
          u"R&D", u"A & B", u"09:30", u"Hinweis: siehe unten", u"x:y", u"http:/x"}) {
        EXPECT_TRUE(tableCellIsPlainText(plain)) << "plain cell of length " << plain.size();
    }
    for (const std::u16string_view syntax : {u"*x*",
                                             u"_x_",
                                             u"~x~",
                                             u"`c`",
                                             u"a\\|b",
                                             u"C:\\Dev",
                                             u"[l](u)",
                                             u"![i](u)",
                                             u"x<br>y",
                                             u"&amp;",
                                             u"&#38;",
                                             u"&copy;",
                                             u"R&D;",
                                             u"# h",
                                             u"- item",
                                             u"+ item",
                                             u"1. one",
                                             u"2) two",
                                             u"> quote",
                                             u"www.example.org",
                                             u"https://example.org",
                                             u"ftp://example.org",
                                             u"see https://example.org",
                                             u"mail@example.org",
                                             u"a\tb",
                                             u"    code",
                                             u"x\u2028y"}) {
        EXPECT_FALSE(tableCellIsPlainText(syntax)) << "syntax cell of length " << syntax.size();
    }
}

TEST(S018_Tables, ReplacingACellChangesNothingElse) {
    EXPECT_EQ(replaceTableCell(u"| a | b |", 1, u"x"), u"| a | x |");
    EXPECT_EQ(replaceTableCell(u"|a|bb|", 0, u"zz"), u"|zz|bb|");
    EXPECT_EQ(replaceTableCell(u"|  | b |", 0, u"x"), u"| x | b |");
    EXPECT_EQ(replaceTableCell(u"a | b", 1, u"y"), u"a | y");
    // A missing cell is added; the cells before it stay byte for byte.
    EXPECT_EQ(replaceTableCell(u"| a |", 2, u"z"), u"| a |  | z |");
    EXPECT_EQ(replaceTableCell(u"bar", 1, u"z"), u"bar | z |");
}

TEST(S018_Tables, RowAndColumnHelpers) {
    EXPECT_EQ(makeEmptyTableRow(1), u"|  |");
    EXPECT_EQ(makeEmptyTableRow(3), u"|  |  |  |");
    EXPECT_EQ(appendTableColumn(u"| A | B |", false), u"| A | B |  |");
    EXPECT_EQ(appendTableColumn(u"| :--- | ---: |", true), u"| :--- | ---: | --- |");
    EXPECT_EQ(removeTableColumn(u"| A | B | C |", 0), u"| B | C |");
    EXPECT_EQ(removeTableColumn(u"| A | B | C |", 1), u"| A | C |");
    EXPECT_EQ(removeTableColumn(u"| :--- | :---: | ---: |", 2), u"| :--- | :---: |");
    EXPECT_EQ(removeTableColumn(u"| Solo |", 0), u"| Solo |");
    EXPECT_EQ(padTableRowCells(u"| 1 |", 3), u"| 1 |  |  |");
}

TEST(S018_Tables, BlockStateAfterMatchesRender) {
    const std::vector<std::u16string> lines = {u"before", u"| A | B |", u"| --- | :---: |", u"| 1 | 2 |", u"tail",
                                               u"",       u"after"};
    BlockState state;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::u16string_view next =
            i + 1 < lines.size() ? std::u16string_view{lines[i + 1]} : std::u16string_view{};
        for (bool revealed : {false, true}) {
            EXPECT_EQ(registry().render(lines[i], {revealed, state, next}).blockState,
                      registry().blockStateAfter(lines[i], state, next));
        }
        state = registry().blockStateAfter(lines[i], state, next);
    }
}

TEST(S018_Tables, ThemeTokensAreParsedAndFallBackToGeneralTokens) {
    const auto r = loadTheme(
        "[table]\n"
        "background = \"#010203\"\n"
        "header-background = \"#040506\"\n"
        "stripe-background = \"#070809\"\n"
        "text = \"#0a0b0c\"\n"
        "header-text = \"#0d0e0f\"\n"
        "header-weight = 600\n"
        "border = \"#101112\"\n"
        "border-width = 2\n"
        "radius = 5\n"
        "padding = 10\n"
        "font-family = \"Table Sans\"\n"
        "font-scale = 1.2\n",
        ColorScheme::Light);
    EXPECT_FALSE(r.usedFallback);
    const TableStyle s = tableStyle(r.theme);
    EXPECT_EQ(s.background, (Color{1, 2, 3, 255}));
    EXPECT_EQ(s.headerBackground, (Color{4, 5, 6, 255}));
    EXPECT_EQ(s.stripeBackground, (Color{7, 8, 9, 255}));
    EXPECT_EQ(s.text, (Color{10, 11, 12, 255}));
    EXPECT_EQ(s.headerText, (Color{13, 14, 15, 255}));
    EXPECT_EQ(s.headerWeight, 600);
    EXPECT_EQ(s.border, (Color{16, 17, 18, 255}));
    EXPECT_DOUBLE_EQ(s.borderWidth, 2);
    EXPECT_DOUBLE_EQ(s.radius, 5);
    EXPECT_DOUBLE_EQ(s.padding, 10);
    EXPECT_EQ(s.fontFamily, "Table Sans");
    EXPECT_DOUBLE_EQ(s.fontScale, 1.2);

    EXPECT_TRUE(loadTheme("[table]\nborder-width = 9\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[table]\nheader-weight = 50\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[table]\nfont-scale = 0.2\n", ColorScheme::Light).usedFallback);

    for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& t = builtinTheme(scheme);
        const TableStyle def = tableStyle(t);
        EXPECT_EQ(def.background, t.background);
        EXPECT_EQ(def.headerBackground, t.surface);
        EXPECT_EQ(def.stripeBackground, t.background);
        EXPECT_EQ(def.text, t.text);
        EXPECT_EQ(def.headerText, t.text);
        EXPECT_EQ(def.headerWeight, t.headingWeight);
        EXPECT_EQ(def.border, t.border);
        EXPECT_DOUBLE_EQ(def.borderWidth, 1);
        EXPECT_DOUBLE_EQ(def.radius, t.radius);
        EXPECT_DOUBLE_EQ(def.padding, t.spacing);
        EXPECT_EQ(def.fontFamily, "");
        EXPECT_DOUBLE_EQ(def.fontScale, 1);
    }
}

// ADR 0002, decision 6 (S018, files from other editors): a line without a pipe directly after a
// table's rows gets a blank line before it, so it is a paragraph in GFM as in Obsidian.
TEST(S018_Tables, TextDirectlyBelowATableGetsABlankLineWhenOpened) {
    const ElementRegistry registry = ElementRegistry::withBuiltins();
    DecodedText decoded;
    decoded.text = u"| a | b |\n| --- | --- |\n| 1 | 2 |\nText\nmore\n\n| c |\n| --- |\n| 3 |\n| 4 |\n";
    EXPECT_EQ(separateTextAfterTables(decoded, registry), 1u);
    EXPECT_EQ(decoded.text, u"| a | b |\n| --- | --- |\n| 1 | 2 |\n\nText\nmore\n\n| c |\n| --- |\n| 3 |\n| 4 |\n");
    EXPECT_TRUE(decoded.perBreak.empty());

    // Left alone: a row with a pipe, text after a blank line, a heading, code, a pipe-less
    // header-like line that is no table.
    DecodedText untouched;
    untouched.text =
        u"| a |\n| --- |\n| 1 |\nx | y\n\nText\n\n| b |\n| --- |\n# Heading\n```\n| c |\n| --- |\ncode\n```\n";
    const std::u16string before = untouched.text;
    EXPECT_EQ(separateTextAfterTables(untouched, registry), 0u);
    EXPECT_EQ(untouched.text, before);

    // Files with mixed line endings: the added break takes the majority ending.
    DecodedText mixed;
    mixed.text = u"| a |\n| --- |\nText\nend";
    mixed.uniformEnding = LineEnding::CRLF;
    mixed.perBreak = {LineEnding::CRLF, LineEnding::LF, LineEnding::CRLF};
    EXPECT_EQ(separateTextAfterTables(mixed, registry), 1u);
    EXPECT_EQ(mixed.text, u"| a |\n| --- |\n\nText\nend");
    EXPECT_EQ(mixed.perBreak,
              (std::vector<LineEnding>{LineEnding::CRLF, LineEnding::LF, LineEnding::CRLF, LineEnding::CRLF}));
}

// S031 review O3: GFM reads a cell as inline text, so a block marker at its start shows as its
// characters (checked with cmark-gfm, cmarkgfm 2025.10.22: each cell below renders as exactly its
// text). Laid out as a Markdown document of its own, the cell needs the marker escaped; inline
// syntax stays as it is.
TEST(S018_Tables, BlockMarkersAtACellStartAreEscapedForDisplay) {
    const std::vector<std::pair<std::u16string_view, std::u16string_view>> escaped = {
        {u"1. Jänner 2026", u"1\\. Jänner 2026"},
        {u"12. März", u"12\\. März"},
        {u"2) zwei", u"2\\) zwei"},
        {u"3.", u"3\\."},
        {u"- 5 %", u"\\- 5 %"},
        {u"-", u"\\-"},
        {u"+ 2 Stk.", u"\\+ 2 Stk."},
        {u"* x", u"\\* x"},
        {u"# 12", u"\\# 12"},
        {u"###### six", u"\\###### six"},
        {u"#", u"\\#"},
        {u"> 5 kg", u"\\> 5 kg"},
        {u">", u"\\>"},
        {u"---", u"\\---"},
        {u"***", u"\\***"},
        {u"___", u"\\___"},
        {u"- - -", u"\\- - -"},
        {u"```", u"\\```"},
        {u"~~~ x", u"\\~~~ x"},
        {u"[a]: b", u"\\[a]: b"},
    };
    for (const auto& [cell, expected] : escaped) {
        EXPECT_EQ(escapeTableCellBlockStart(cell), expected);
    }
    for (const std::u16string_view unchanged : {u"", u"Green tea", u"1.5 kg", u"1234567890. ten digits", u"-5 %", u"+1",
                                                u"*emph*", u"**bold**", u"####### seven", u"#tag", u"a > b", u"``x``",
                                                u"```x```", u"__init__", u"[l](u)", u"\\- escaped", u"x<br>- y"}) {
        EXPECT_EQ(escapeTableCellBlockStart(unchanged), unchanged);
    }
}

// S031 review (code) O10: a cell whose Markdown holds nothing but plain characters, backslash
// escapes and `<br>` is laid out as plain text with U+2028 between its lines. Checked with
// cmark-gfm (cmarkgfm 2025.10.22): each cell below renders as exactly these characters and breaks.
TEST(S018_Tables, TwoLineCellsNeedNoMarkdown) {
    const std::vector<std::pair<std::u16string_view, std::u16string_view>> plain = {
        {u"Green tea", u"Green tea"},
        {u"r0c1<br>second line", u"r0c1\u2028second line"},
        {u"Tea House North<br>Tea House South", u"Tea House North\u2028Tea House South"},
        {u"12,50<br/>13,20", u"12,50\u202813,20"},
        {u"x<br />y", u"x\u2028y"},
        {u"x<BR>y", u"x\u2028y"},
        {u"first<br><br>third", u"first\u2028\u2028third"},
        {u"a\\_b", u"a_b"},
        {u"Tea\\|Pot", u"Tea|Pot"},
        {u"\\*x\\* and \\[l\\](u)", u"*x* and [l](u)"},
        {u"a\\\\b", u"a\\b"},
        {u"5\" tin<br>say \"hi\"", u"5\" tin\u2028say \"hi\""},
        {u"r0c2 R&D", u"r0c2 R&D"},
        {u"r0c3 09:30<br>a\\_b", u"r0c3 09:30\u2028a_b"},
    };
    for (const auto& [cell, expected] : plain) {
        EXPECT_EQ(tableCellPlainLayout(cell).value_or(u"(not plain)"), expected);
    }
    for (const std::u16string_view syntax :
         {u"&amp;<br>x", u"x<br>https://example.org", u"*x*", u"x<br>*y*", u"<br>x", u"x<br>", u"x <br>y", u"x<br> y",
          u"x\\<br>y", u"C:\\Dev", u"\\a", u"- item<br>two", u"1. one<br>two", u"www.example.org<br>x",
          u"x<br>www.y.org", u"x<b>y", u"x<brr>y", u"x\u2028y", u"a\tb"}) {
        EXPECT_FALSE(tableCellPlainLayout(syntax).has_value()) << "syntax cell of length " << syntax.size();
    }
}
