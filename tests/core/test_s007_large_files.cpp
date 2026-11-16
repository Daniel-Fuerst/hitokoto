// SPDX-License-Identifier: GPL-3.0-or-later
// S007: rendering stays cheap for the lines of large files.
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>

using namespace hitokoto::core;

namespace {
// Counts how often the registry asks it to render.
class CountingElement final : public InlineElement {
public:
    CountingElement(std::u16string_view triggers, int& calls) : triggers_(triggers), calls_(calls) {}
    [[nodiscard]] std::string_view id() const noexcept override { return "test-counting"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return triggers_; }
    void render(std::u16string_view, const LineContext&, LineRender&) const override { ++calls_; }

private:
    std::u16string_view triggers_;
    int& calls_;
};

const ElementRegistry& builtins() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::u16string repeated(std::u16string_view unit, std::size_t count) {
    std::u16string out;
    out.reserve(unit.size() * count);
    for (std::size_t i = 0; i < count; ++i) {
        out += unit;
    }
    return out;
}

/// No inline element left anything on the line.
bool showsAsText(const LineRender& r) {
    return r.runs.empty() && r.inlineStyles.empty() && r.hidden.empty() && r.links.empty() && r.opaqueSpans.empty() &&
           r.inlineContexts.empty();
}

bool hasRole(const LineRender& r, Role role) {
    return std::any_of(r.runs.begin(), r.runs.end(), [role](const StyleRun& run) { return run.role == role; });
}
}  // namespace

// Most lines contain none of the characters that can start an inline element; the registry
// scans each line once and skips those elements instead of letting each one scan the line.
TEST(S007_LargeFiles, InlineElementsRunOnlyWhenTheirCharactersOccur) {
    int backtick = 0;
    int always = 0;
    int nonAscii = 0;
    ElementRegistry r;
    r.addInline(std::make_unique<CountingElement>(u"`", backtick));
    r.addInline(std::make_unique<CountingElement>(u"", always));
    r.addInline(std::make_unique<CountingElement>(u"§", nonAscii));

    (void)r.render(u"plain prose without any trigger", {});
    EXPECT_EQ(backtick, 0);
    EXPECT_EQ(always, 1) << "no trigger characters declared: runs on every line";
    EXPECT_EQ(nonAscii, 0);

    (void)r.render(u"with `code`", {});
    EXPECT_EQ(backtick, 1);
    (void)r.render(u"§ 1", {});
    EXPECT_EQ(nonAscii, 1) << "non-ASCII triggers work too";
    EXPECT_EQ(always, 3);
}

TEST(S007_LargeFiles, BuiltinInlineElementsDeclareTheirTriggers) {
    EXPECT_EQ(makeInlineCodeElement()->triggerCharacters(), u"`");
    EXPECT_EQ(makeLinkElement()->triggerCharacters(), u"[");
    EXPECT_EQ(makeEmphasisElement()->triggerCharacters(), u"*_~");
    EXPECT_EQ(makeEscapeElement()->triggerCharacters(), u"\\");
}

// Rework of 2026-11-16: a line above one of two bounds shows as text. What a line costs grows with
// its length and with its formatted pieces; within the bounds a keystroke in it stays in budget.
TEST(S007_LargeFiles, EveryRegistryStartsWithTheBounds) {
    EXPECT_EQ(builtins().inlineBounds(), kInlineBounds);
    EXPECT_EQ(ElementRegistry{}.inlineBounds(), kInlineBounds);
    EXPECT_GT(kInlineBounds.lineLength, kInlineBounds.syntaxCharacters);
}

TEST(S007_LargeFiles, ALineAtTheLengthBoundShowsItsFormattingAndOneCharacterMoreShowsAsText) {
    const std::u16string start = u"**bold** and `code` ";
    const std::u16string atBound = start + std::u16string(kInlineBounds.lineLength - start.size(), u'x');
    ASSERT_EQ(atBound.size(), kInlineBounds.lineLength);
    for (const bool revealed : {false, true}) {
        const LineRender within = builtins().render(atBound, {revealed});
        ASSERT_FALSE(within.inlineStyles.empty());
        EXPECT_EQ(within.inlineStyles.front(), (InlineStyleRun{{2, 4}, InlineStyle::Bold}));
        EXPECT_TRUE(hasRole(within, Role::InlineCode));
        EXPECT_EQ(within.hidden.empty(), revealed);

        const LineRender above = builtins().render(atBound + u"x", {revealed});
        EXPECT_TRUE(showsAsText(above)) << "revealed: " << revealed;
    }
}

TEST(S007_LargeFiles, ALineAtTheSyntaxBoundShowsItsFormattingAndOneSyntaxCharacterMoreShowsAsText) {
    // Two backticks per span: exactly as many syntax characters as the bound allows.
    const std::u16string atBound = repeated(u"`a` ", kInlineBounds.syntaxCharacters / 2);
    ASSERT_LE(atBound.size(), kInlineBounds.lineLength);
    for (const bool revealed : {false, true}) {
        const LineRender within = builtins().render(atBound, {revealed});
        EXPECT_EQ(within.opaqueSpans.size(), kInlineBounds.syntaxCharacters / 2);
        EXPECT_TRUE(hasRole(within, Role::InlineCode));
        // One more of any character that can start inline syntax, wherever it stands.
        for (const std::u16string_view extra : {u"*", u"_", u"~", u"`", u"\\", u"["}) {
            EXPECT_TRUE(showsAsText(builtins().render(atBound + std::u16string(extra), {revealed})));
            EXPECT_TRUE(showsAsText(builtins().render(u"x" + std::u16string(extra) + u" " + atBound, {revealed})));
        }
    }
}

TEST(S007_LargeFiles, CharactersThatStartNoSyntaxDoNotCount) {
    const std::u16string line = u"**bold** " + repeated(u"(x) 1+2=3; <a> \"q\" ", 150);
    ASSERT_LE(line.size(), kInlineBounds.lineLength);
    ASSERT_GT(line.size() / 2, kInlineBounds.syntaxCharacters);
    const LineRender r = builtins().render(line, {});
    ASSERT_EQ(r.inlineStyles.size(), 1U);
    EXPECT_EQ(r.inlineStyles.front(), (InlineStyleRun{{2, 4}, InlineStyle::Bold}));
}

TEST(S007_LargeFiles, AboveTheBoundsNoInlineElementRuns) {
    int backtick = 0;
    int always = 0;
    int nonAscii = 0;
    ElementRegistry r;
    r.addInline(std::make_unique<CountingElement>(u"`", backtick));
    r.addInline(std::make_unique<CountingElement>(u"", always));
    r.addInline(std::make_unique<CountingElement>(u"§", nonAscii));
    r.setInlineBounds({20, 3});
    EXPECT_EQ(r.inlineBounds(), (InlineBounds{20, 3}));

    (void)r.render(u"`a` and ` and §", {});  // 15 characters, 3 backticks: the non-ASCII one makes 4
    EXPECT_EQ(backtick + always + nonAscii, 0);
    (void)r.render(u"`a` and § words", {});  // 3 syntax characters
    EXPECT_EQ(backtick, 1);
    EXPECT_EQ(always, 1);
    EXPECT_EQ(nonAscii, 1);
    (void)r.render(u"`a` and twenty-one ch", {});  // 21 characters
    (void)r.renderInline(u"`a` and twenty-one ch");
    (void)r.renderInline(u"````");
    EXPECT_EQ(backtick, 1);
    EXPECT_EQ(always, 1) << "also an element that runs on every line";
    (void)r.renderInline(u"`a` and exactly 20 c");
    EXPECT_EQ(backtick, 2);
    EXPECT_EQ(always, 2);
}

TEST(S007_LargeFiles, BlockElementsStillApplyAboveTheBounds) {
    const std::u16string text = repeated(u"*a* ", kInlineBounds.syntaxCharacters);
    ASSERT_LE(text.size(), kInlineBounds.lineLength);

    const LineRender heading = builtins().render(u"## " + text, {});
    EXPECT_EQ(heading.headingLevel, 2);
    EXPECT_EQ(heading.hidden, (std::vector<HiddenRange>{{{0, 3}, HideMode::Collapse}}));
    ASSERT_EQ(heading.runs.size(), 1U);
    EXPECT_EQ(heading.runs.front().role, Role::HeadingText);
    EXPECT_TRUE(heading.inlineStyles.empty());

    const LineRender bullet = builtins().render(u"- " + text, {});
    EXPECT_EQ(bullet.decorations.size(), 1U);
    EXPECT_TRUE(bullet.inlineStyles.empty());
    EXPECT_FALSE(hasRole(bullet, Role::EmphasisMarker));

    // A paragraph still flows into its next line (S033).
    const LineRender flowing = builtins().render(text, {false, {}, u"next line"});
    EXPECT_TRUE(flowing.flowsToNext);
    EXPECT_TRUE(flowing.inlineStyles.empty());
}

TEST(S007_LargeFiles, TheTextOfATableCellFollowsTheBounds) {
    EXPECT_FALSE(showsAsText(builtins().renderInline(u"*a* and `b`")));
    EXPECT_TRUE(showsAsText(builtins().renderInline(repeated(u"*a* ", kInlineBounds.syntaxCharacters))));
    EXPECT_TRUE(showsAsText(builtins().renderInline(u"*a* " + std::u16string(kInlineBounds.lineLength, u'x'))));
}
