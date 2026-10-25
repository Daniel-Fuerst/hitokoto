// SPDX-License-Identifier: GPL-3.0-or-later
// S015: inline code spans preserve source and suppress Markdown parsing within their contents.
#include "hitokoto/core/markdown.hpp"

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
    EXPECT_EQ(render.runs, (std::vector<StyleRun>{{{1, 4}, Role::InlineCode, 0}}));
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
