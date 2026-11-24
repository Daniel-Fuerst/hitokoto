// SPDX-License-Identifier: GPL-3.0-or-later
// S044: LaTeX formulas between `$` (inline) and `$$` (display) are recognised, their source styled
// as math, and their delimiters concealed off the caret's line.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include "line_work.hpp"
#include "support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {

using hitokoto::test::registry;
using hitokoto::test::show;

std::vector<StyleRun> runsWith(const LineRender& render, Role role) {
    std::vector<StyleRun> out;
    std::copy_if(render.runs.begin(), render.runs.end(), std::back_inserter(out),
                 [role](const StyleRun& run) { return run.role == role; });
    return out;
}

/// The source of every formula on a rendered line, in order.
std::vector<std::u16string> formulas(std::u16string_view line, const LineRender& render) {
    std::vector<std::u16string> out;
    for (const MathSpan& math : render.math) {
        out.emplace_back(line.substr(math.source.start, math.source.length));
    }
    return out;
}

std::vector<std::u16string> formulasOf(std::u16string_view line) {
    return formulas(line, show(line));
}

std::u16string widen(std::string_view text) {
    return {text.begin(), text.end()};  // the sample's lines are ASCII
}

std::vector<std::u16string> sampleLines() {
    std::ifstream in(std::string(HITOKOTO_SOURCE_DIR) + "/tests/data/s044-latex.md");
    std::vector<std::u16string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(widen(line));
    }
    return lines;
}

}  // namespace

TEST(S044_MathSource, InlineMathIsStyledAndItsDollarsConcealed) {
    const std::u16string_view line = u"Greek: $\\alpha_1, \\beta_2$.";
    const auto render = show(line);
    EXPECT_EQ(runsWith(render, Role::Math), (std::vector<StyleRun>{{{8, 17}, Role::Math, 0}}));
    EXPECT_EQ(render.hidden, (std::vector<HiddenRange>{{{7, 1}, HideMode::Collapse}, {{25, 1}, HideMode::Collapse}}));
    EXPECT_EQ(render.math, (std::vector<MathSpan>{{{8, 17}, false}}));
    EXPECT_TRUE(render.inlineStyles.empty()) << "the underscores are no emphasis";
}

TEST(S044_MathSource, RevealedLineShowsDollarsAsMathMarkers) {
    const auto render = show(u"a $x^2$ b", true);
    EXPECT_EQ(runsWith(render, Role::MathMarker),
              (std::vector<StyleRun>{{{2, 1}, Role::MathMarker, 0}, {{6, 1}, Role::MathMarker, 0}}));
    EXPECT_EQ(runsWith(render, Role::Math), (std::vector<StyleRun>{{{3, 3}, Role::Math, 0}}));
    EXPECT_TRUE(render.hidden.empty());
    EXPECT_EQ(render.math, (std::vector<MathSpan>{{{3, 3}, false}}));
}

TEST(S044_MathSource, DollarsAsCurrencyStayText) {
    EXPECT_TRUE(formulasOf(u"If an item costs $5.00 and another costs $10.00, does it?").empty());
    const std::u16string_view escaped = u"\\$5.00 and \\$10.00, vs real inline math $x = 5.00 + 10.00$";
    const auto render = show(escaped);
    EXPECT_EQ(formulas(escaped, render), (std::vector<std::u16string>{u"x = 5.00 + 10.00"}));
    EXPECT_EQ(render.hidden[0], (HiddenRange{{0, 1}, HideMode::Collapse})) << "S020 still conceals the backslash";
    EXPECT_EQ(render.hidden[1], (HiddenRange{{11, 1}, HideMode::Collapse}));
}

TEST(S044_MathSource, TheCurrencyLineOfTheSampleHoldsOneFormula) {
    const auto lines = sampleLines();
    ASSERT_GT(lines.size(), 6U);
    const std::u16string& line = lines[5];
    ASSERT_NE(line.find(u"Currency"), std::u16string::npos);
    EXPECT_EQ(formulasOf(line), (std::vector<std::u16string>{u"x = 5.00 + 10.00"}));
}

TEST(S044_MathSource, RulesOfTheClosingDollar) {
    for (const std::u16string_view line : {u"$ x$", u"$x $", u"$x$1", u"$$", u"$$$$", u"`$x$`", u"$", u"a $ b"}) {
        EXPECT_TRUE(formulasOf(line).empty()) << hitokoto::test::narrow(line);
    }
    // Pandoc: a `$` that cannot close belongs to the formula.
    EXPECT_EQ(formulasOf(u"$a $b$"), (std::vector<std::u16string>{u"a $b"}));
    EXPECT_EQ(formulasOf(u"$x$, $y$"), (std::vector<std::u16string>{u"x", u"y"}));
    EXPECT_EQ(formulasOf(u"$x$."), (std::vector<std::u16string>{u"x"}));
    // An escaped dollar inside a formula does not close it.
    EXPECT_EQ(formulasOf(u"$a\\$b$"), (std::vector<std::u16string>{u"a\\$b"}));
}

TEST(S044_MathSource, MarkdownInsideAFormulaStaysAsWritten) {
    const auto emphasis = show(u"$a_1 * b_2 * c$");
    EXPECT_TRUE(emphasis.inlineStyles.empty());
    const auto braces = show(u"$\\{x\\}$");
    EXPECT_EQ(braces.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}, {{6, 1}, HideMode::Collapse}}))
        << "no backslash inside the formula is concealed";
}

TEST(S044_MathSource, EmphasisAroundAFormulaStillWorks) {
    const std::u16string_view line = u"**bold** $x$";
    const auto render = show(line);
    EXPECT_FALSE(render.inlineStyles.empty());
    EXPECT_EQ(formulas(line, render), (std::vector<std::u16string>{u"x"}));
}

TEST(S044_MathSource, DisplayMathOnOneLine) {
    const std::u16string_view line = u"$$\\sum_{k=1}^n k$$";
    const auto render = show(line);
    EXPECT_EQ(render.math, (std::vector<MathSpan>{{{2, 14}, true}}));
    EXPECT_EQ(render.hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}, {{16, 2}, HideMode::Collapse}}));
    EXPECT_EQ(runsWith(render, Role::Math), (std::vector<StyleRun>{{{2, 14}, Role::Math, 0}}));
    // Blanks inside display dollars are allowed.
    EXPECT_EQ(formulasOf(u"$$ x $$"), (std::vector<std::u16string>{u" x "}));
}

TEST(S044_MathSource, ADisplayBlockIsMathAsAWhole) {
    const std::vector<std::u16string> lines{u"before", u"$$", u"\\left( a \\right)^2", u"$$", u"after $y$"};
    const auto shown = registry().renderDocument(lines, {});
    ASSERT_EQ(shown.size(), lines.size());
    EXPECT_FALSE(shown[0].mathBlock);
    EXPECT_TRUE(shown[1].mathBlock && shown[1].mathDelimiter);
    EXPECT_EQ(shown[1].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::KeepSpace}}));
    EXPECT_TRUE(shown[2].mathBlock && !shown[2].mathDelimiter);
    EXPECT_EQ(runsWith(shown[2], Role::Math), (std::vector<StyleRun>{{{0, 18}, Role::Math, 0}}));
    EXPECT_EQ(shown[2].math, (std::vector<MathSpan>{{{0, 18}, true}}));
    EXPECT_TRUE(shown[2].hidden.empty()) << "no inline element runs inside the block";
    EXPECT_TRUE(shown[3].mathBlock && shown[3].mathDelimiter);
    EXPECT_FALSE(shown[4].mathBlock);
    EXPECT_EQ(formulas(lines[4], shown[4]), (std::vector<std::u16string>{u"y"}));
}

TEST(S044_MathSource, ARevealedDelimiterLineShowsItsDollars) {
    const auto open = registry().render(u"$$", {true});
    EXPECT_TRUE(open.mathBlock && open.mathDelimiter);
    EXPECT_TRUE(open.hidden.empty());
    EXPECT_EQ(open.runs, (std::vector<StyleRun>{{{0, 2}, Role::MathMarker, 0}}));
}

TEST(S044_MathSource, TextBesideTheDelimitersBelongsToTheFormula) {
    const std::vector<std::u16string> lines{u"  $$ \\begin{aligned}", u"a &= b", u"\\end{aligned} $$  ", u"x"};
    const auto shown = registry().renderDocument(lines, {});
    EXPECT_TRUE(shown[0].mathDelimiter);
    EXPECT_EQ(shown[0].math, (std::vector<MathSpan>{{{4, 16}, true}}));
    EXPECT_EQ(shown[0].hidden, (std::vector<HiddenRange>{{{0, 4}, HideMode::Collapse}}));
    EXPECT_EQ(shown[2].math, (std::vector<MathSpan>{{{0, 14}, true}}));
    EXPECT_EQ(shown[2].hidden, (std::vector<HiddenRange>{{{14, 4}, HideMode::Collapse}}));
    EXPECT_FALSE(shown[3].mathBlock) << "the block closed on the line with text before $$";
}

TEST(S044_MathSource, AnUnclosedBlockRunsToTheEnd) {
    const std::vector<std::u16string> lines{u"$$", u"x", u"y"};
    const auto shown = registry().renderDocument(lines, {});
    EXPECT_TRUE(shown[2].mathBlock);
    EXPECT_TRUE(shown[2].blockState.active());
}

TEST(S044_MathSource, MathInACodeBlockIsCode) {
    const std::vector<std::u16string> lines{u"```", u"$$", u"$x$", u"```", u"$$"};
    const auto shown = registry().renderDocument(lines, {});
    for (std::size_t i = 1; i <= 2; ++i) {
        EXPECT_TRUE(shown[i].codeBlock);
        EXPECT_FALSE(shown[i].mathBlock);
        EXPECT_TRUE(shown[i].math.empty());
    }
    EXPECT_TRUE(shown[4].mathBlock) << "a $$ after the code block opens a math block";
}

TEST(S044_MathSource, DelimitersIndentedFourSpacesDoNotOpen) {
    EXPECT_FALSE(registry().render(u"    $$", {}).mathBlock);
}

TEST(S044_MathSource, EveryFormulaOfTheSampleIsFound) {
    const auto lines = sampleLines();
    const auto shown = registry().renderDocument(lines, {});
    std::size_t inlineCount = 0;
    std::size_t blockLines = 0;
    for (const auto& render : shown) {
        for (const MathSpan& math : render.math) {
            inlineCount += math.display ? 0 : 1;
        }
        blockLines += render.mathBlock ? 1 : 0;
    }
    // The heading `($\LaTeX$)`, line 4: one formula; line 5: two; line 6: one (the currency line).
    EXPECT_EQ(inlineCount, 5U);
    // Four display blocks: 3 + 8 + 13 + 3 lines.
    EXPECT_EQ(blockLines, 27U);
    EXPECT_FALSE(shown.back().blockState.active()) << "every block of the sample closes";
}

// Openers without closers, closers without openers and formulas between code spans must not make
// a line quadratic (S013's rule for inline elements).
TEST(S044_MathSource, ALineRendersInLinearTime) {
    using hitokoto::test::halves;
    using hitokoto::test::of;
    for (const auto& pattern : {
             of("$a ", u"$a "),
             of("a $", u"a $"),
             of("$x$ ", u"$x$ "),
             of("$$a ", u"$$a "),
             of("\\$", u"\\$"),
             halves("$a .. `b`", u"$a ", u"`b` "),
         }) {
        hitokoto::test::expectLinearWork(pattern);
    }
}

TEST(S044_MathSource, ThemeTokensFallBack) {
    const auto base = parseTheme("", Theme{});
    ASSERT_TRUE(base.theme.has_value());
    const MathStyle fallback = mathStyle(*base.theme);
    EXPECT_EQ(fallback.text, base.theme->text);
    EXPECT_EQ(fallback.marker, base.theme->marker);
    EXPECT_EQ(fallback.fontFamily, "");
    EXPECT_DOUBLE_EQ(fallback.fontScale, 1.0);

    const auto themed = parseTheme(
        "[math]\ntext = \"#112233\"\nmarker = \"#445566\"\nfont-family = \"STIX Two Math\"\nfont-scale = 1.2\n",
        Theme{});
    ASSERT_TRUE(themed.theme.has_value()) << (themed.diagnostics.empty() ? "" : themed.diagnostics[0].toString());
    const MathStyle style = mathStyle(*themed.theme);
    EXPECT_EQ(style.text, (Color{0x11, 0x22, 0x33}));
    EXPECT_EQ(style.marker, (Color{0x44, 0x55, 0x66}));
    EXPECT_EQ(style.fontFamily, "STIX Two Math");
    EXPECT_DOUBLE_EQ(style.fontScale, 1.2);
}
