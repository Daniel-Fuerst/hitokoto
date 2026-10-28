// SPDX-License-Identifier: GPL-3.0-or-later
// S020: backslash escapes for ASCII punctuation characters.
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

LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}
}  // namespace

// The exact example reported by the user: 20 escaped ASCII punctuation characters.
TEST(S020_BackslashEscapes, UserExampleConcealsAllBackslashes) {
    const std::u16string line = u"\\\\ \\` \\* \\_ \\{ \\} \\[ \\] \\< \\> \\( \\) \\# \\+ \\- \\. \\! \\| \\~ \\$";
    const auto concealed = show(line, false);

    // Each escaped pair "\X" is at index 3 * k (indices 0, 3, 6, ..., 57).
    std::vector<HiddenRange> expectedHidden;
    std::vector<StyleRun> expectedRevealedRuns;
    for (std::uint32_t i = 0; i < 20; ++i) {
        const std::uint32_t pos = i * 3;
        expectedHidden.push_back({{pos, 1}, HideMode::Collapse});
        expectedRevealedRuns.push_back({{pos, 1}, Role::EscapeMarker, 0});
    }

    EXPECT_EQ(concealed.hidden, expectedHidden) << "all 20 backslashes are concealed when the line is not active";
    EXPECT_TRUE(concealed.runs.empty());
    EXPECT_TRUE(concealed.inlineStyles.empty());

    const auto revealed = show(line, true);
    EXPECT_TRUE(revealed.hidden.empty());
    EXPECT_EQ(revealed.runs, expectedRevealedRuns)
        << "all 20 backslashes are revealed as escape markers when the line is active";
}

// CommonMark 0.31.2, section 6.2: Any ASCII punctuation character may be backslash-escaped.
// Exactly 32 characters in ASCII:
// ! " # $ % & ' ( ) * + , - . / (33-47)
// : ; < = > ? @ (58-64)
// [ \ ] ^ _ ` (91-96)
// { | } ~ (123-126)
TEST(S020_BackslashEscapes, All32AsciiPunctuationCharactersCanBeEscaped) {
    const std::u16string punct = u"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    ASSERT_EQ(punct.size(), 32u);

    for (const char16_t c : punct) {
        const std::u16string line = std::u16string(u"\\") + c;
        const auto concealed = show(line, false);
        EXPECT_EQ(concealed.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}))
            << "backslash before '" << static_cast<char>(c) << "' should be concealed";
        EXPECT_TRUE(concealed.runs.empty());

        const auto revealed = show(line, true);
        EXPECT_TRUE(revealed.hidden.empty());
        EXPECT_EQ(revealed.runs, (std::vector<StyleRun>{{{0, 1}, Role::EscapeMarker, 0}}))
            << "backslash before '" << static_cast<char>(c) << "' should be an escape marker when revealed";
    }
}

// Backslashes before non-punctuation characters remain literal (CommonMark Example 309).
TEST(S020_BackslashEscapes, NonPunctuationCharactersStayLiteral) {
    for (const auto line : {u"\\a", u"\\Z", u"\\1", u"\\ ", u"\\   ", u"\\\t", u"\\\u03c6", u"\\"}) {
        const auto concealed = show(line, false);
        EXPECT_TRUE(concealed.hidden.empty());
        EXPECT_TRUE(concealed.runs.empty());

        const auto revealed = show(line, true);
        EXPECT_TRUE(revealed.hidden.empty());
        EXPECT_TRUE(revealed.runs.empty());
    }
}

// CommonMark: consecutive backslashes pair up from left to right.
TEST(S020_BackslashEscapes, ConsecutiveBackslashes) {
    // Two backslashes: first escapes second.
    const auto two = show(u"\\\\", false);
    EXPECT_EQ(two.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));

    // Three backslashes: first escapes second, third is trailing literal.
    const auto three = show(u"\\\\\\", false);
    EXPECT_EQ(three.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));

    // Four backslashes: first escapes second, third escapes fourth.
    const auto four = show(u"\\\\\\\\", false);
    EXPECT_EQ(four.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}, {{2, 1}, HideMode::Collapse}}));

    // Escaped backslash before italic: first backslash concealed, italic applies.
    const auto escapedBeforeItalic = show(u"\\\\*italic*", false);
    EXPECT_EQ(escapedBeforeItalic.hidden,
              (std::vector<HiddenRange>{
                  {{0, 1}, HideMode::Collapse}, {{2, 1}, HideMode::Collapse}, {{9, 1}, HideMode::Collapse}}));
    ASSERT_EQ(escapedBeforeItalic.inlineStyles.size(), 1u);
    EXPECT_EQ(escapedBeforeItalic.inlineStyles[0], (InlineStyleRun{{3, 6}, InlineStyle::Italic}));

    // Three backslashes before asterisk: first escapes second, third escapes asterisk -> no italic.
    const auto threeBeforeAsterisk = show(u"\\\\\\*not italic\\*", false);
    EXPECT_EQ(threeBeforeAsterisk.hidden,
              (std::vector<HiddenRange>{
                  {{0, 1}, HideMode::Collapse}, {{2, 1}, HideMode::Collapse}, {{14, 1}, HideMode::Collapse}}));
    EXPECT_TRUE(threeBeforeAsterisk.inlineStyles.empty());
}

// Escapes prevent block-level Markdown syntax.
TEST(S020_BackslashEscapes, EscapesSuppressBlockSyntax) {
    const auto heading = show(u"\\# Not a heading", false);
    EXPECT_EQ(heading.headingLevel, 0);
    EXPECT_EQ(heading.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));

    for (const auto line : {u"\\* not a bullet", u"\\- not a bullet", u"\\+ not a bullet"}) {
        const auto bullet = show(line, false);
        EXPECT_TRUE(bullet.decorations.empty());
        EXPECT_EQ(bullet.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));
    }
}

// Escapes prevent inline Markdown syntax.
TEST(S020_BackslashEscapes, EscapesSuppressInlineSyntax) {
    const auto italic = show(u"\\*not italic\\*", false);
    EXPECT_TRUE(italic.inlineStyles.empty());
    EXPECT_EQ(italic.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}, {{12, 1}, HideMode::Collapse}}));

    const auto bold = show(u"\\*\\*not bold\\*\\*", false);
    EXPECT_TRUE(bold.inlineStyles.empty());
    EXPECT_EQ(bold.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse},
                                                     {{2, 1}, HideMode::Collapse},
                                                     {{12, 1}, HideMode::Collapse},
                                                     {{14, 1}, HideMode::Collapse}}));

    const auto strike = show(u"\\~\\~not strike\\~\\~", false);
    EXPECT_TRUE(strike.inlineStyles.empty());
    EXPECT_EQ(strike.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse},
                                                       {{2, 1}, HideMode::Collapse},
                                                       {{14, 1}, HideMode::Collapse},
                                                       {{16, 1}, HideMode::Collapse}}));

    const auto code = show(u"\\`not code\\`", false);
    EXPECT_TRUE(code.runs.empty());
    EXPECT_EQ(code.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}, {{10, 1}, HideMode::Collapse}}));
}

// Escapes inside headings and emphasis.
TEST(S020_BackslashEscapes, EscapesWorkInsideHeadingsAndEmphasis) {
    // Heading with escaped characters.
    const auto heading = show(u"# Heading with \\* and \\#", false);
    EXPECT_EQ(heading.headingLevel, 1);
    EXPECT_EQ(heading.hidden,
              (std::vector<HiddenRange>{
                  {{0, 2}, HideMode::Collapse}, {{15, 1}, HideMode::Collapse}, {{22, 1}, HideMode::Collapse}}));
    // Heading text runs are carved around the concealed backslashes.
    EXPECT_EQ(heading.runs,
              (std::vector<StyleRun>{
                  {{2, 13}, Role::HeadingText, 1}, {{16, 6}, Role::HeadingText, 1}, {{23, 1}, Role::HeadingText, 1}}));

    // When revealed, heading markers and escape markers are distinct.
    const auto headingRev = show(u"# Heading with \\* and \\#", true);
    EXPECT_EQ(headingRev.headingLevel, 1);
    EXPECT_EQ(headingRev.runs, (std::vector<StyleRun>{{{0, 2}, Role::Marker, 1},
                                                      {{2, 13}, Role::HeadingText, 1},
                                                      {{15, 1}, Role::EscapeMarker, 0},
                                                      {{16, 6}, Role::HeadingText, 1},
                                                      {{22, 1}, Role::EscapeMarker, 0},
                                                      {{23, 1}, Role::HeadingText, 1}}));

    // Emphasis with an escaped character inside: inlineStyles do not overlap hidden backslash.
    const auto italic = show(u"*italic with \\* escaped*", false);
    EXPECT_EQ(italic.hidden,
              (std::vector<HiddenRange>{
                  {{0, 1}, HideMode::Collapse}, {{13, 1}, HideMode::Collapse}, {{23, 1}, HideMode::Collapse}}));
    ASSERT_EQ(italic.inlineStyles.size(), 2u);
    EXPECT_EQ(italic.inlineStyles[0], (InlineStyleRun{{1, 12}, InlineStyle::Italic}));
    EXPECT_EQ(italic.inlineStyles[1], (InlineStyleRun{{14, 9}, InlineStyle::Italic}));
}

// CommonMark: backslashes inside code blocks and inline code spans remain literal.
TEST(S020_BackslashEscapes, BackslashesInsideInlineCodeAndCodeBlocksStayLiteral) {
    const auto code = show(u"`C:\\*`", false);
    EXPECT_EQ(code.runs, (std::vector<StyleRun>{{{1, 4}, Role::InlineCode, 0}}));
    EXPECT_EQ(code.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}, {{5, 1}, HideMode::Collapse}}));

    // Fenced code block: backslashes are literal inside code blocks.
    ElementRegistry reg = ElementRegistry::withBuiltins();
    const auto fenceOpen = reg.render(u"```", {});
    const auto blockLine = reg.render(u"\\* \\# \\\\", {false, fenceOpen.blockState});
    EXPECT_TRUE(blockLine.codeBlock);
    EXPECT_TRUE(blockLine.hidden.empty());
    EXPECT_EQ(blockLine.runs, (std::vector<StyleRun>{{{0, 8}, Role::CodeBlock, 0}}));
}

// ADR 0004: [escape] section theme tokens and fallback.
TEST(S020_BackslashEscapes, ThemeTokensAreParsedAndFallback) {
    const auto custom = loadTheme("[escape]\nmarker = \"#112233\"\n", ColorScheme::Light);
    ASSERT_FALSE(custom.usedFallback) << (custom.diagnostics.empty() ? "" : custom.diagnostics.front().toString());
    EXPECT_EQ(custom.theme.escape.marker, (Color{0x11, 0x22, 0x33, 0xff}));
    EXPECT_EQ(escapeStyle(custom.theme).marker, (Color{0x11, 0x22, 0x33, 0xff}));

    const auto def = builtinTheme(ColorScheme::Light);
    EXPECT_FALSE(def.escape.marker.has_value());
    EXPECT_EQ(escapeStyle(def).marker, def.marker) << "escape.marker falls back to colors.marker";
}
