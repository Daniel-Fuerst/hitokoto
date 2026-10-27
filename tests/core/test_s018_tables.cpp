// SPDX-License-Identifier: GPL-3.0-or-later
// S018: interactive Markdown pipe tables (ADR 0005).
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, BlockState state = {}, bool revealed = false,
                std::u16string_view nextLine = {}) {
    return registry().render(line, {revealed, state, nextLine});
}

std::vector<Span> hiddenSpans(const LineRender& render) {
    std::vector<Span> spans;
    spans.reserve(render.hidden.size());
    for (const auto& hidden : render.hidden) {
        spans.push_back(hidden.span);
    }
    return spans;
}
}  // namespace

TEST(S018_Tables, RecognizesHeaderDelimiterAndBodyRowsAndAlwaysConcealsSyntax) {
    const std::u16string_view headerLine = u"| Name | Score | Rank |";
    const std::u16string_view delimLine = u"| :--- | :---: | ---: |";
    const std::u16string_view bodyLine = u"| Alice | 95 | 1 |";

    for (bool revealed : {false, true}) {
        const auto header = show(headerLine, {}, revealed, delimLine);
        ASSERT_TRUE(header.table);
        EXPECT_TRUE(header.tableHeader);
        EXPECT_FALSE(header.tableDelimiter);
        EXPECT_TRUE(header.blockState.active());
        ASSERT_EQ(header.cells.size(), 3u);
        EXPECT_EQ(header.cells[0].content, (Span{2, 4}));
        EXPECT_EQ(header.cells[0].align, TableAlign::Left);
        EXPECT_EQ(header.cells[1].content, (Span{9, 5}));
        EXPECT_EQ(header.cells[1].align, TableAlign::Center);
        EXPECT_EQ(header.cells[2].content, (Span{17, 4}));
        EXPECT_EQ(header.cells[2].align, TableAlign::Right);
        // Pipe syntax and padding spaces stay concealed even when revealed is true (ADR 0005).
        EXPECT_EQ(hiddenSpans(header), (std::vector<Span>{{0, 2}, {6, 3}, {14, 3}, {21, 2}}));
        ASSERT_EQ(header.runs.size(), 3u);
        EXPECT_EQ(header.runs[0], (StyleRun{{2, 4}, Role::TableHeader, 0}));
        EXPECT_EQ(header.runs[1], (StyleRun{{9, 5}, Role::TableHeader, 0}));
        EXPECT_EQ(header.runs[2], (StyleRun{{17, 4}, Role::TableHeader, 0}));

        const auto delim = show(delimLine, header.blockState, revealed);
        EXPECT_TRUE(delim.table);
        EXPECT_FALSE(delim.tableHeader);
        EXPECT_TRUE(delim.tableDelimiter);
        EXPECT_TRUE(delim.blockState.active());
        EXPECT_EQ(hiddenSpans(delim), (std::vector<Span>{{0, static_cast<std::uint32_t>(delimLine.size())}}));
        EXPECT_TRUE(delim.runs.empty());

        const auto body = show(bodyLine, delim.blockState, revealed);
        EXPECT_TRUE(body.table);
        EXPECT_FALSE(body.tableHeader);
        EXPECT_FALSE(body.tableDelimiter);
        EXPECT_TRUE(body.blockState.active());
        ASSERT_EQ(body.cells.size(), 3u);
        EXPECT_EQ(body.cells[0].content, (Span{2, 5}));
        EXPECT_EQ(body.cells[0].align, TableAlign::Left);
        EXPECT_EQ(body.cells[1].content, (Span{10, 2}));
        EXPECT_EQ(body.cells[1].align, TableAlign::Center);
        EXPECT_EQ(body.cells[2].content, (Span{15, 1}));
        EXPECT_EQ(body.cells[2].align, TableAlign::Right);
        EXPECT_EQ(hiddenSpans(body), (std::vector<Span>{{0, 2}, {7, 3}, {12, 3}, {16, 2}}));
        ASSERT_EQ(body.runs.size(), 3u);
        EXPECT_EQ(body.runs[0], (StyleRun{{2, 5}, Role::TableCell, 0}));
    }
}

TEST(S018_Tables, PipeLinesWithoutMatchingDelimiterStayLiteral) {
    // No next line, or next line is not a delimiter row, or column counts differ.
    for (const auto next : {std::u16string_view{}, std::u16string_view{u"plain text"}, std::u16string_view{u"| --- |"},
                            std::u16string_view{u"| --- | --- | --- |"}, std::u16string_view{u"| :: | --- |"}}) {
        const auto render = show(u"| A | B |", {}, false, next);
        EXPECT_FALSE(render.table) << "nextLine failed check";
        EXPECT_FALSE(render.blockState.active());
        EXPECT_TRUE(render.hidden.empty());
    }
}

TEST(S018_Tables, TableEndsAtBlankLineOrBlockElement) {
    const auto header = show(u"| A | B |", {}, false, u"| --- | --- |");
    const auto delim = show(u"| --- | --- |", header.blockState);
    const auto body = show(u"| 1 | 2 |", delim.blockState);
    ASSERT_TRUE(body.table);

    const auto blank = show(u"", body.blockState);
    EXPECT_FALSE(blank.table);
    EXPECT_FALSE(blank.blockState.active());

    const auto heading = show(u"# Heading", body.blockState);
    EXPECT_FALSE(heading.table);
    EXPECT_EQ(heading.headingLevel, 1);

    const auto fence = show(u"```", body.blockState);
    EXPECT_FALSE(fence.table);
    EXPECT_TRUE(fence.codeBlock);
}

TEST(S018_Tables, EscapedPipesAndInlineCodePipesDoNotSplitCells) {
    const std::u16string_view headerLine = u"| Expr | Code |";
    const std::u16string_view delimLine = u"| --- | --- |";
    const std::u16string_view bodyLine = u"| a \\| b | `x | y` |";

    const auto header = show(headerLine, {}, false, delimLine);
    const auto delim = show(delimLine, header.blockState);
    const auto body = show(bodyLine, delim.blockState, false);

    ASSERT_TRUE(body.table);
    ASSERT_EQ(body.cells.size(), 2u);
    // Cell 0 is "a \| b" at [2, 8); the backslash at [4, 5) is concealed so "a | b" is shown.
    EXPECT_EQ(body.cells[0].content, (Span{2, 6}));
    // Cell 1 is "`x | y`" at [11, 18); the pipe inside inline code did not split the cell.
    EXPECT_EQ(body.cells[1].content, (Span{11, 7}));

    const auto hidden = hiddenSpans(body);
    // Concealed: [0,2) "| ", [4,5) "\", [8,11) " | ", [11,12) "`", [17,20) "` |" (coalesced or adjacent).
    EXPECT_NE(std::find(hidden.begin(), hidden.end(), Span{4, 1}), hidden.end())
        << "backslash of escaped pipe is concealed";
}

TEST(S018_Tables, InlineEmphasisIsParsedPerCellAndDoesNotBleedAcrossPipes) {
    const auto header = show(u"| A | B |", {}, false, u"| --- | --- |");
    const auto delim = show(u"| --- | --- |", header.blockState);

    // Unmatched '*' in two separate cells must not pair across the pipe boundary.
    const auto across = show(u"| *left | right* |", delim.blockState, false);
    ASSERT_TRUE(across.table);
    EXPECT_TRUE(across.inlineStyles.empty());

    // Matched '**bold**' inside a single cell receives Bold inline style.
    const auto within = show(u"| **bold** | *italic* |", delim.blockState, false);
    ASSERT_EQ(within.inlineStyles.size(), 2u);
    EXPECT_EQ(within.inlineStyles[0], (InlineStyleRun{{4, 4}, InlineStyle::Bold}));
    EXPECT_EQ(within.inlineStyles[1], (InlineStyleRun{{14, 6}, InlineStyle::Italic}));
}

TEST(S018_Tables, EmptyCellsAndRowColumnManipulationHelpers) {
    EXPECT_EQ(makeEmptyTableRow(1), u"|  |");
    EXPECT_EQ(makeEmptyTableRow(3), u"|  |  |  |");

    const auto header = show(u"| A | B |", {}, false, u"| --- | --- |");
    const auto delim = show(u"| --- | --- |", header.blockState);
    const auto emptyRow = show(makeEmptyTableRow(2), delim.blockState);
    ASSERT_EQ(emptyRow.cells.size(), 2u);
    EXPECT_EQ(emptyRow.cells[0].content, (Span{2, 0}));
    EXPECT_EQ(emptyRow.cells[1].content, (Span{5, 0}));

    EXPECT_EQ(appendTableColumn(u"| A | B |", false), u"| A | B |  |");
    EXPECT_EQ(appendTableColumn(u"| :--- | ---: |", true), u"| :--- | ---: | --- |");
    EXPECT_EQ(removeTableColumn(u"| A | B | C |", 0), u"| B | C |");
    EXPECT_EQ(removeTableColumn(u"| A | B | C |", 1), u"| A | C |");
    EXPECT_EQ(removeTableColumn(u"| :--- | :---: | ---: |", 2), u"| :--- | :---: |");
    EXPECT_EQ(removeTableColumn(u"| Solo |", 0), u"| Solo |");
    EXPECT_EQ(padTableRowCells(u"| 1 |", 3), u"| 1 |  |  |");
}

TEST(S018_Tables, BlockStateAfterMatchesRender) {
    const std::u16string_view lines[] = {u"before", u"| A | B |", u"| --- | :---: |", u"| 1 | 2 |", u"", u"after"};
    BlockState state;
    for (std::size_t i = 0; i < std::size(lines); ++i) {
        const std::u16string_view next = i + 1 < std::size(lines) ? lines[i + 1] : std::u16string_view{};
        for (bool revealed : {false, true}) {
            EXPECT_EQ(show(lines[i], state, revealed, next).blockState,
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
    if (!r.theme.name.empty() && r.usedFallback) {
        FAIL() << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
    }
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

    // Out-of-range checks.
    EXPECT_TRUE(loadTheme("[table]\nborder-width = 9\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[table]\nheader-weight = 50\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[table]\nfont-scale = 0.2\n", ColorScheme::Light).usedFallback);

    // Fallbacks on built-in themes.
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
