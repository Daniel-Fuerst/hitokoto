// SPDX-License-Identifier: GPL-3.0-or-later
// S023: thematic breaks (CommonMark 0.31.2, 4.1) render as a horizontal line, setext headings (4.3)
// as headings, front matter (Obsidian) stays text. Specified against ElementRegistry::renderDocument,
// the whole-document reference of ADR 0006. Expectations checked on 2026-10-30 with commonmark.js
// 0.31.2 and markdown-it-py 4.2.0 (mode commonmark), which agree on every case used here, and with
// cmark-gfm 0.29.0.gfm (Python cmarkgfm 2025.10.22) for tables.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::string narrow(std::u16string_view text) {
    std::string out;
    for (const char16_t c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

/// The whole document rendered; line `revealed` (if any) has the caret.
std::vector<LineRender> showAll(const std::vector<std::u16string>& lines, std::int64_t revealed = -1) {
    return registry().renderDocument(lines, revealed < 0 ? LineRange{} : LineRange{revealed, revealed});
}

bool lastIsBreak(const std::vector<std::u16string>& lines) {
    return showAll(lines).back().thematicBreak;
}

std::string context(const std::vector<std::u16string>& lines) {
    std::string out;
    for (const auto& line : lines) {
        out += narrow(line) + "\\n";
    }
    return out;
}

Span whole(std::u16string_view line) {
    return {0, static_cast<std::uint32_t>(line.size())};
}

/// A concealed underline: the line keeps its space, nothing else.
void expectConcealedUnderline(const LineRender& r, std::u16string_view line, std::uint8_t level,
                              const std::string& where) {
    EXPECT_EQ(r.setextUnderline, level) << where;
    EXPECT_FALSE(r.thematicBreak) << where;
    EXPECT_EQ(r.headingLevel, 0) << where;
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{{whole(line), HideMode::KeepSpace}})) << where;
    EXPECT_TRUE(r.runs.empty()) << where;
    EXPECT_TRUE(r.decorations.empty()) << where;
    EXPECT_TRUE(r.inlineStyles.empty()) << where;
}

/// A revealed underline: the line as written, one marker run at body size.
void expectRevealedUnderline(const LineRender& r, std::u16string_view line, std::uint8_t level,
                             const std::string& where) {
    EXPECT_EQ(r.setextUnderline, level) << where;
    EXPECT_TRUE(r.hidden.empty()) << where;
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{whole(line), Role::Marker, 0}})) << where;
}

/// A heading line of a setext heading: heading level and heading text over the line.
void expectHeadingLine(const LineRender& r, std::u16string_view line, std::uint8_t level, const std::string& where) {
    EXPECT_EQ(r.headingLevel, level) << where;
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{whole(line), Role::HeadingText, level}})) << where;
    EXPECT_TRUE(r.hidden.empty()) << where;
}

/// A plain line: nothing at all.
void expectPlain(const LineRender& r, const std::string& where) {
    EXPECT_TRUE(r.empty()) << where;
}
}  // namespace

// ---- Thematic breaks (4.1) ------------------------------------------------------------------

// Examples 43, 47, 50-54 and 2.2 example 11.
TEST(S023_ThematicBreaks, ThreeOrMoreMatchingCharacters) {
    for (const std::u16string line :
         {u"---", u"***", u"___", u" ***", u"  ***", u"   ***", u"_____________________________________", u" - - -",
          u" **  * ** * ** * **", u"-     -      -      -", u"- - - -    ", u"*\t*\t*\t", u"_ _ _", u"---   ",
          u"   ---"}) {
        EXPECT_TRUE(lastIsBreak({line})) << narrow(line);
    }
}

// Examples 44-46, 48, 55, 56; a tab before the characters is four columns.
TEST(S023_ThematicBreaks, OtherLinesAreNoBreaks) {
    for (const std::u16string line :
         {u"+++", u"===", u"--", u"**", u"__", u"    ***", u"\t---", u"_ _ _ _ a", u"a------", u"---a---", u" *-*",
          u"--- a", u"\\---", u"|---|", u"-\u00a0-\u00a0-", u"", u"text"}) {
        EXPECT_FALSE(lastIsBreak({line})) << narrow(line);
    }
}

// Concealed: the whole line keeps its space (transparent) and nothing else is produced; the UI
// draws the line.
TEST(S023_ThematicBreaks, ConcealedLineIsHiddenWithItsSpaceKept) {
    for (const std::u16string line : {u"---", u"   * * *   ", u"___"}) {
        const auto r = showAll({line}).back();
        EXPECT_TRUE(r.thematicBreak) << narrow(line);
        EXPECT_EQ(r.setextUnderline, 0) << narrow(line);
        EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{{whole(line), HideMode::KeepSpace}})) << narrow(line);
        EXPECT_TRUE(r.runs.empty()) << narrow(line);
        EXPECT_TRUE(r.inlineStyles.empty()) << narrow(line);
        EXPECT_TRUE(r.decorations.empty()) << narrow(line);
        EXPECT_EQ(r.headingLevel, 0) << narrow(line);
        EXPECT_FALSE(r.empty()) << narrow(line);
    }
}

// Revealed: the line as written, as one marker run; still a break (the UI draws no line then).
TEST(S023_ThematicBreaks, RevealedLineIsOneMarkerRun) {
    for (const std::u16string line : {u"---", u"   * * *   ", u"___"}) {
        const auto r = showAll({line}, 0).back();
        EXPECT_TRUE(r.thematicBreak) << narrow(line);
        EXPECT_TRUE(r.hidden.empty()) << narrow(line);
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{{whole(line), Role::Marker, 0}})) << narrow(line);
        EXPECT_TRUE(r.inlineStyles.empty()) << narrow(line);
        EXPECT_TRUE(r.decorations.empty()) << narrow(line);
    }
}

// Example 60: a break takes precedence over a list item. Emphasis, escapes and headings do not
// claim the characters either.
TEST(S023_ThematicBreaks, NoOtherElementClaimsABreak) {
    for (const std::u16string line : {u"* * *", u"- - -", u"***", u"___", u"_ _ _", u"**  * ** * ** * **"}) {
        for (const bool revealed : {false, true}) {
            const auto r = showAll({line}, revealed ? 0 : -1).back();
            EXPECT_TRUE(r.thematicBreak) << narrow(line);
            EXPECT_TRUE(r.decorations.empty()) << narrow(line) << ": no bullet";
            EXPECT_TRUE(r.inlineStyles.empty()) << narrow(line) << ": no emphasis";
            EXPECT_EQ(r.headingLevel, 0) << narrow(line);
        }
    }
    const auto list = showAll({u"* Foo", u"* * *", u"* Bar"});
    EXPECT_EQ(list[0].decorations.size(), 1U);
    EXPECT_TRUE(list[1].thematicBreak);
    EXPECT_EQ(list[2].decorations.size(), 1U);
}

// Examples 57-58: no blank line needed; a break interrupts a paragraph, which goes on below it.
TEST(S023_ThematicBreaks, BreakInterruptsAParagraph) {
    for (const std::u16string marker : {u"***", u"___", u"- - -", u"-\t-\t-", u"  - - -", u"* * *"}) {
        const std::vector<std::u16string> lines = {u"Foo", marker, u"bar"};
        const auto r = showAll(lines);
        EXPECT_EQ(r[0].headingLevel, 0) << context(lines);
        EXPECT_TRUE(r[1].thematicBreak) << context(lines);
        EXPECT_EQ(r[2].headingLevel, 0) << context(lines);
    }
}

// Only paragraph text above makes dashes an underline.
TEST(S023_ThematicBreaks, DashesAfterOtherBlocksAreABreak) {
    const std::vector<std::vector<std::u16string>> breaks = {
        {u"Foo", u"", u"---"},    {u"Foo", u"   ", u"---"},
        {u"# Foo", u"---"},       {u"## Foo", u"***"},
        {u"***", u"---"},         {u"```", u"code", u"```", u"---"},
        {u"~~~", u"~~~", u"---"}, {u"Foo", u"===", u"---"},
        {u"Foo", u"---", u"---"}, {u"", u"---"},
        {u"- ", u"---"},
    };
    for (const auto& lines : breaks) {
        EXPECT_TRUE(lastIsBreak(lines)) << context(lines);
    }
}

// Blocks hitokoto does not render yet are no paragraph text where CommonMark lets them start: after
// them, `---` is a break. A link reference definition at the start counts as such a block (the
// reference parsers disagree there, story: known deviations).
TEST(S023_ThematicBreaks, DashesAfterUnrenderedBlocksAreABreak) {
    const std::vector<std::vector<std::u16string>> breaks = {
        {u"1. foo", u"---"},      {u"1) foo", u"---"},   {u"Foo", u"1. x", u"---"},          {u"> quote", u"---"},
        {u"Foo", u"> q", u"---"}, {u"    code", u"---"}, {u"Foo", u"", u"    code", u"---"}, {u"<!-- c -->", u"---"},
        {u"[a]: /url", u"---"},
    };
    for (const auto& lines : breaks) {
        EXPECT_TRUE(lastIsBreak(lines)) << context(lines);
    }
}

TEST(S023_ThematicBreaks, InsideACodeBlockItIsCode) {
    const auto r = showAll({u"```", u"---", u"***", u"Foo", u"===", u"```"});
    for (std::size_t i = 1; i <= 4; ++i) {
        EXPECT_TRUE(r[i].codeBlock) << i;
        EXPECT_FALSE(r[i].thematicBreak) << i;
        EXPECT_EQ(r[i].setextUnderline, 0) << i;
        EXPECT_EQ(r[i].headingLevel, 0) << i;
    }
    const auto unterminated = showAll({u"~~~", u"___"});
    EXPECT_TRUE(unterminated[1].codeBlock);
    EXPECT_FALSE(unterminated[1].thematicBreak);
}

// GFM (cmark-gfm): a break ends a table, also after a body row without pipes. In CommonMark
// without tables the same text would be a setext heading; hitokoto follows GFM for tables (S018).
TEST(S023_ThematicBreaks, ABreakEndsATable) {
    for (const std::u16string marker : {u"---", u"***", u"___", u"- - -"}) {
        const auto r = showAll({u"| a | b |", u"| - | - |", u"| 1 | 2 |", marker, u"after"});
        EXPECT_TRUE(r[2].table) << narrow(marker);
        EXPECT_FALSE(r[3].table) << narrow(marker);
        EXPECT_TRUE(r[3].thematicBreak) << narrow(marker);
        EXPECT_FALSE(r[4].table) << narrow(marker);
    }
    const auto lazyRow = showAll({u"| a |", u"| - |", u"| 1 |", u"foo", u"---"});
    EXPECT_TRUE(lazyRow[3].table) << "a line without a pipe is a row (GFM)";
    EXPECT_TRUE(lazyRow[4].thematicBreak);
}

// ---- Setext headings (4.3) ------------------------------------------------------------------

TEST(S023_ThematicBreaks, TextUnderlinedIsAHeading) {
    for (const auto& [underline, level] : std::vector<std::pair<std::u16string, std::uint8_t>>{
             {u"===", 1}, {u"=", 1}, {u"---", 2}, {u"-", 2}, {u"----------", 2}, {u"   ---", 2}, {u"---   ", 2}}) {
        const std::vector<std::u16string> lines = {u"Foo", underline, u"after"};
        const auto r = showAll(lines);
        expectHeadingLine(r[0], lines[0], level, context(lines));
        expectConcealedUnderline(r[1], lines[1], level, context(lines));
        expectPlain(r[2], context(lines));
    }
    // Example 59 and 4.3: the underline wins over a break and over an empty list item.
    EXPECT_EQ(showAll({u"Foo", u"-"})[1].decorations.size(), 0U) << "no bullet";
}

TEST(S023_ThematicBreaks, EveryLineOfTheParagraphIsAHeadingLine) {
    for (const std::vector<std::u16string>& lines : std::vector<std::vector<std::u16string>>{
             {u"Foo", u"bar", u"---"},
             {u"Foo", u"2. x", u"---"},
             {u"Foo", u"    bar", u"---"},
             {u"Foo", u"[a]: /u", u"---"},
             {u"Foo\\", u"bar", u"==="},
             {u"| a |", u"---"},
             {u"a | b", u"---"},
         }) {
        const auto r = showAll(lines);
        const std::uint8_t level = lines.back().front() == u'=' ? 1 : 2;
        for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
            EXPECT_EQ(r[i].headingLevel, level) << context(lines) << " line " << i;
            EXPECT_FALSE(r[i].table) << context(lines) << " line " << i;
        }
        EXPECT_EQ(r.back().setextUnderline, level) << context(lines);
    }
    // The paragraph starts after a blank line.
    const auto r = showAll({u"Foo", u"", u"bar", u"---"});
    EXPECT_EQ(r[0].headingLevel, 0);
    EXPECT_EQ(r[2].headingLevel, 2);
}

// Inline elements still run on heading lines (like ATX headings, S002 and S013).
TEST(S023_ThematicBreaks, HeadingLinesKeepTheirInlineStyles) {
    const auto r = showAll({u"*Foo* and `code`", u"---"});
    EXPECT_EQ(r[0].headingLevel, 2);
    EXPECT_FALSE(r[0].inlineStyles.empty()) << "italic";
    EXPECT_FALSE(r[0].hidden.empty()) << "concealed delimiters";
}

// The underline is revealed while the caret is on any line of its heading or on the underline.
TEST(S023_ThematicBreaks, UnderlineIsRevealedFromItsHeading) {
    const std::vector<std::u16string> lines = {u"before", u"", u"Foo", u"bar", u"---", u"", u"after"};
    for (const std::int64_t caret : {2, 3, 4}) {
        const auto r = showAll(lines, caret);
        expectRevealedUnderline(r[4], lines[4], 2, "caret on line " + std::to_string(caret));
        expectHeadingLine(r[2], lines[2], 2, "caret on line " + std::to_string(caret));
        expectHeadingLine(r[3], lines[3], 2, "caret on line " + std::to_string(caret));
    }
    for (const std::int64_t caret : {0, 1, 5, 6}) {
        expectConcealedUnderline(showAll(lines, caret)[4], lines[4], 2, "caret on line " + std::to_string(caret));
    }
    // A selection that touches the heading reveals it too.
    expectRevealedUnderline(registry().renderDocument(lines, LineRange{0, 2})[4], lines[4], 2, "selection 0-2");
}

// 4.3 and 5.2: an underline in a list item must be indented to the item's content column (marker,
// 1-4 spaces, a tab to the next multiple of 4); a lazy continuation line cannot be underlined, an
// empty item opens no paragraph.
TEST(S023_ThematicBreaks, DashesUnderAListItem) {
    const std::vector<std::vector<std::u16string>> breaks = {
        {u"- foo", u"---"},           {u"- foo", u" ---"},        {u"- foo", u"***"},
        {u"- foo", u"  bar", u"---"}, {u"- foo", u"bar", u"---"}, {u"- foo", u"", u"  ---"},
        {u"* foo", u"  ***"},         {u"- ", u"  ---"},          {u"-   foo", u"bar", u"  ---"},
        {u"-\tfoo", u"  ---"},
    };
    for (const auto& lines : breaks) {
        EXPECT_TRUE(lastIsBreak(lines)) << context(lines);
    }
    // Checked: `- foo\n  ---` is <li><h2>foo</h2></li>.
    const std::vector<std::vector<std::u16string>> underlines = {
        {u"- foo", u"  ---"},     {u"- foo", u"  bar", u"  ---"}, {u"- foo", u"bar", u"  ---"},
        {u"  - foo", u"    ---"}, {u"- foo", u"  ==="},
    };
    for (const auto& lines : underlines) {
        const auto r = showAll(lines);
        const std::uint8_t level = lines.back().back() == u'=' ? 1 : 2;
        EXPECT_FALSE(r.back().thematicBreak) << context(lines);
        EXPECT_EQ(r.back().setextUnderline, level) << context(lines);
        EXPECT_EQ(r[0].headingLevel, level) << context(lines);
        EXPECT_EQ(r[0].decorations.size(), 1U) << context(lines) << ": still a bullet";
    }
}

// ---- Front matter (Obsidian) ---------------------------------------------------------------

TEST(S023_ThematicBreaks, ClosedFrontMatterStaysText) {
    for (const std::vector<std::u16string>& lines : std::vector<std::vector<std::u16string>>{
             {u"---", u"title: x", u"# not a heading", u"tags: [a, b]", u"---", u"", u"text"},
             {u"---  ", u"title: x", u"---", u"text"},
             {u"---", u"---", u"text"},
         }) {
        const auto r = showAll(lines);
        std::size_t close = 1;
        while (narrow(lines[close]).rfind("---", 0) != 0) {
            ++close;
        }
        for (std::size_t i = 0; i <= close; ++i) {
            expectPlain(r[i], context(lines) + " line " + std::to_string(i));
        }
        for (const std::int64_t caret : {std::int64_t{0}, static_cast<std::int64_t>(close)}) {
            const auto revealed = showAll(lines, caret);
            expectPlain(revealed[static_cast<std::size_t>(caret)], context(lines) + " revealed");
        }
    }
    // Below the front matter the document renders as usual.
    const auto r = showAll({u"---", u"a: 1", u"---", u"***", u"Foo", u"---"});
    EXPECT_TRUE(r[3].thematicBreak);
    EXPECT_EQ(r[4].headingLevel, 2);
}

TEST(S023_ThematicBreaks, FrontMatterOnlyAtTheStartAndClosed) {
    const auto unclosed = showAll({u"---", u"title: x"});
    EXPECT_TRUE(unclosed[0].thematicBreak) << "without a closing line CommonMark applies";
    const auto indented = showAll({u" ---", u"title: x", u"---"});
    EXPECT_TRUE(indented[0].thematicBreak);
    EXPECT_EQ(indented[2].setextUnderline, 2);
    const auto later = showAll({u"text", u"", u"---", u"a: 1", u"---"});
    EXPECT_TRUE(later[2].thematicBreak);
    EXPECT_EQ(later[3].headingLevel, 2);
    EXPECT_EQ(later[4].setextUnderline, 2);
}

// ---- Theming baseline (ADR 0004) ------------------------------------------------------------

TEST(S023_ThematicBreaks, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[thematic-break]\nborder = \"#102030\"\nborder-width = 3\nmargin = 12\n"
        "marker = \"#405060\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << (r.diagnostics.empty() ? std::string() : r.diagnostics.front().toString());
    const ThematicBreakStyle s = thematicBreakStyle(r.theme);
    EXPECT_EQ(s.border, (Color{16, 32, 48, 255}));
    EXPECT_DOUBLE_EQ(s.borderWidth, 3);
    EXPECT_DOUBLE_EQ(s.margin, 12);
    EXPECT_EQ(s.marker, (Color{64, 80, 96, 255}));

    EXPECT_TRUE(loadTheme("[thematic-break]\nborder-width = 9\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[thematic-break]\nborder-width = -1\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[thematic-break]\nmargin = 65\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[thematic-break]\npadding = 2\n", ColorScheme::Light).usedFallback) << "no such token";
}

TEST(S023_ThematicBreaks, ThemeTokensFallBack) {
    for (const auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& t = builtinTheme(scheme);
        const ThematicBreakStyle s = thematicBreakStyle(t);
        EXPECT_EQ(s.border, t.border);
        EXPECT_DOUBLE_EQ(s.borderWidth, 1);
        EXPECT_DOUBLE_EQ(s.margin, 0);
        EXPECT_EQ(s.marker, t.marker);
    }
    // Fallbacks resolve when used: a theme that changes colors.border moves the line along.
    const auto r = loadTheme("[colors]\nborder = \"#123456\"\n", ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback);
    EXPECT_EQ(thematicBreakStyle(r.theme).border, (Color{0x12, 0x34, 0x56, 255}));
}

TEST(S023_ThematicBreaks, TokensAreDocumented) {
    for (const std::string_view key : {"border", "border-width", "margin", "marker"}) {
        bool found = false;
        for (const auto& info : themeTokens()) {
            found = found || (info.section == "thematic-break" && info.key == key);
        }
        EXPECT_TRUE(found) << key;
    }
}
