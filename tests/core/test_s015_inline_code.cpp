// SPDX-License-Identifier: GPL-3.0-or-later
// S015: inline code spans preserve source and suppress Markdown parsing within their contents.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}
}  // namespace

TEST(S015_InlineCode, StylesCodeAndConcealsSingleBacktickDelimiters) {
    const auto render = show(u"before `**literal**` after");
    EXPECT_EQ(render.runs, (std::vector<StyleRun>{{{8, 11}, Role::InlineCode, 0}}));
    EXPECT_EQ(render.hidden, (std::vector<HiddenRange>{{{7, 1}, HideMode::Collapse}, {{19, 1}, HideMode::Collapse}}));
    EXPECT_TRUE(render.inlineStyles.empty());
}

TEST(S015_InlineCode, RevealedLineKeepsCodeStyleAndShowsBackticks) {
    const auto render = show(u"`code`", true);
    EXPECT_EQ(render.runs, (std::vector<StyleRun>{{{0, 1}, Role::InlineCodeMarker, 0},
                                                  {{1, 4}, Role::InlineCode, 0},
                                                  {{5, 1}, Role::InlineCodeMarker, 0}}))
        << "revealed backticks are inline-code markers (inline-code.marker)";
    EXPECT_TRUE(render.hidden.empty());
}

TEST(S015_InlineCode, NonMatchingAndEscapedBackticksStayLiteral) {
    for (const auto line : {u"`unclosed", u"\\`escaped\\`"}) {
        const auto render = show(line);
        EXPECT_TRUE(render.runs.empty());
        EXPECT_TRUE(render.hidden.empty());
    }
}

TEST(S015_InlineCode, CodeSpansDoNotApplyEmphasisOrHeadingRulesToContents) {
    const auto render = show(u"`# **literal**`");
    EXPECT_EQ(render.headingLevel, 0);
    EXPECT_TRUE(render.inlineStyles.empty());
    ASSERT_EQ(render.runs.size(), 1u);
    EXPECT_EQ(render.runs.front().role, Role::InlineCode);
}

TEST(S015_InlineCode, EmphasisOutsideCodeSpansStillRenders) {
    const auto render = show(u"**bold** `**literal**`");
    ASSERT_EQ(render.inlineStyles.size(), 1u);
    EXPECT_EQ(render.inlineStyles.front(), (InlineStyleRun{{2, 4}, InlineStyle::Bold}));
    ASSERT_EQ(render.runs.size(), 1u);
    EXPECT_EQ(render.runs.front().role, Role::InlineCode);
}

TEST(S015_InlineCode, CodeSpansKeepTheirBlockHeadingContext) {
    const auto render = show(u"# `code`");
    EXPECT_EQ(render.headingLevel, 1);
    ASSERT_EQ(render.runs.size(), 1u);
    EXPECT_EQ(render.runs.front(), (StyleRun{{3, 4}, Role::InlineCode, 0}));
}

// Theming baseline (ADR 0004).
TEST(S015_InlineCode, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[inline-code]\nbackground = \"#010203\"\ntext = \"#040506\"\nborder = \"#070809\"\n"
        "border-width = 1\nradius = 3\npadding = 4\nfont-family = \"Mono X\"\nfont-scale = 0.85\n"
        "marker = \"#0a0b0c\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << r.diagnostics.front().toString();
    const CodeStyle s = inlineCodeStyle(r.theme);
    EXPECT_EQ(s.background, (Color{1, 2, 3, 255}));
    EXPECT_EQ(s.text, (Color{4, 5, 6, 255}));
    EXPECT_EQ(s.border, (Color{7, 8, 9, 255}));
    EXPECT_DOUBLE_EQ(s.borderWidth, 1);
    EXPECT_DOUBLE_EQ(s.radius, 3);
    EXPECT_DOUBLE_EQ(s.padding, 4);
    EXPECT_EQ(s.fontFamily, "Mono X");
    EXPECT_DOUBLE_EQ(s.fontScale, 0.85);
    EXPECT_EQ(s.marker, (Color{10, 11, 12, 255}));
    EXPECT_TRUE(loadTheme("[inline-code]\npadding = 17\n", ColorScheme::Light).usedFallback);
}

TEST(S015_InlineCode, ThemeTokensFallBackToCodeBlockTokens) {
    const Theme& t = builtinTheme(ColorScheme::Light);
    const CodeStyle s = inlineCodeStyle(t);
    EXPECT_EQ(s.background, t.surface);
    EXPECT_EQ(s.text, t.text);
    EXPECT_EQ(s.border, t.border);
    EXPECT_DOUBLE_EQ(s.borderWidth, 0);
    EXPECT_DOUBLE_EQ(s.radius, t.radius);
    EXPECT_DOUBLE_EQ(s.padding, 2);
    EXPECT_EQ(s.fontFamily, "");
    EXPECT_DOUBLE_EQ(s.fontScale, 1);
    EXPECT_EQ(s.marker, t.marker);

    const auto block = loadTheme(
        "[code-block]\nbackground = \"#111111\"\ntext = \"#222222\"\nborder = \"#333333\"\n"
        "border-width = 3\nradius = 9\npadding = 20\nfont-family = \"Mono Y\"\nfont-scale = 1.5\n"
        "marker = \"#444444\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(block.usedFallback);
    const CodeStyle i = inlineCodeStyle(block.theme);
    EXPECT_EQ(i.background, (Color{0x11, 0x11, 0x11, 255}));
    EXPECT_EQ(i.text, (Color{0x22, 0x22, 0x22, 255}));
    EXPECT_EQ(i.border, (Color{0x33, 0x33, 0x33, 255}));
    EXPECT_DOUBLE_EQ(i.radius, 9);
    EXPECT_EQ(i.fontFamily, "Mono Y");
    EXPECT_DOUBLE_EQ(i.borderWidth, 0) << "fixed fallback, not the code block's";
    EXPECT_DOUBLE_EQ(i.padding, 2) << "fixed fallback, not the code block's";
    EXPECT_DOUBLE_EQ(i.fontScale, 1) << "fixed fallback, not the code block's";
    EXPECT_EQ(i.marker, t.marker) << "falls back to colors.marker, not code-block.marker";
}
