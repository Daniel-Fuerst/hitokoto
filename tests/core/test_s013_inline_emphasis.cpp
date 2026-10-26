// SPDX-License-Identifier: GPL-3.0-or-later
// S013: inline emphasis stays in the plain-text document and maps to render spans.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <iterator>
#include <random>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}

std::vector<Span> spans(const std::vector<HiddenRange>& ranges) {
    std::vector<Span> out;
    out.reserve(ranges.size());
    for (const auto& range : ranges) {
        out.push_back(range.span);
    }
    return out;
}

std::vector<Span> spans(const std::vector<InlineStyleRun>& runs) {
    std::vector<Span> out;
    out.reserve(runs.size());
    for (const auto& run : runs) {
        out.push_back(run.span);
    }
    return out;
}
}  // namespace

TEST(S013_InlineEmphasis, ConcealsMatchedDelimitersAndReturnsStyledTextRanges) {
    const auto r = show(u"**bold** *italic* _under_ ~~gone~~");

    EXPECT_EQ(spans(r.hidden),
              (std::vector<Span>{{0, 2}, {6, 2}, {9, 1}, {16, 1}, {18, 1}, {24, 1}, {26, 2}, {32, 2}}));
    const auto styled = spans(r.inlineStyles);
    EXPECT_EQ(styled, (std::vector<Span>{{2, 4}, {10, 6}, {19, 5}, {28, 4}}));
    ASSERT_EQ(r.inlineStyles.size(), 4u);
    EXPECT_EQ(r.inlineStyles[0].style, InlineStyle::Bold);
    EXPECT_EQ(r.inlineStyles[1].style, InlineStyle::Italic);
    EXPECT_EQ(r.inlineStyles[2].style, InlineStyle::Italic);
    EXPECT_EQ(r.inlineStyles[3].style, InlineStyle::Strikethrough);
}

TEST(S013_InlineEmphasis, RevealedLineShowsDelimitersAndKeepsStyledRanges) {
    const auto r = show(u"**bold** *italic* ~~gone~~", true);

    EXPECT_TRUE(r.hidden.empty());
    const auto styled = spans(r.inlineStyles);
    EXPECT_EQ(styled, (std::vector<Span>{{2, 4}, {10, 6}, {20, 4}}));
}

TEST(S013_InlineEmphasis, FormatsPunctuationAtEndOfEmphasizedLabels) {
    const auto r = show(u"**Projekt:** \u2610 Bayernsschach \u2610 Dame \u2610 Clobber");

    EXPECT_EQ(spans(r.hidden), (std::vector<Span>{{0, 2}, {10, 2}}));
    ASSERT_EQ(r.inlineStyles.size(), 1u);
    EXPECT_EQ(r.inlineStyles[0], (InlineStyleRun{{2, 8}, InlineStyle::Bold}));
}

TEST(S013_InlineEmphasis, NestedAndCombinedDelimitersProduceSeparateTextRuns) {
    const auto nested = show(u"**bold *italic* bold**");
    const auto nestedRuns = spans(nested.inlineStyles);
    EXPECT_EQ(nestedRuns, (std::vector<Span>{{2, 5}, {8, 6}, {15, 5}}));
    EXPECT_EQ(spans(nested.hidden), (std::vector<Span>{{0, 2}, {7, 1}, {14, 1}, {20, 2}}));
    ASSERT_EQ(nested.inlineStyles.size(), 3u);
    EXPECT_EQ(nested.inlineStyles[0].style, InlineStyle::Bold);
    EXPECT_EQ(nested.inlineStyles[1].style, InlineStyle::Bold | InlineStyle::Italic);
    EXPECT_EQ(nested.inlineStyles[2].style, InlineStyle::Bold);

    const auto combined = show(u"***both***");
    const auto combinedRuns = spans(combined.inlineStyles);
    EXPECT_EQ(combinedRuns, (std::vector<Span>{{3, 4}}));
    EXPECT_EQ(spans(combined.hidden), (std::vector<Span>{{0, 3}, {7, 3}}));
    ASSERT_EQ(combined.inlineStyles.size(), 1u);
    EXPECT_EQ(combined.inlineStyles[0].style, InlineStyle::Bold | InlineStyle::Italic);
}

TEST(S013_InlineEmphasis, AdjacentDelimiterRunsCloseNestedStyles) {
    const auto boldThenItalic = show(u"**bold *boldanditalic***");
    EXPECT_EQ(spans(boldThenItalic.hidden), (std::vector<Span>{{0, 2}, {7, 1}, {21, 3}}));
    ASSERT_EQ(boldThenItalic.inlineStyles.size(), 2u);
    EXPECT_EQ(boldThenItalic.inlineStyles[0], (InlineStyleRun{{2, 5}, InlineStyle::Bold}));
    EXPECT_EQ(boldThenItalic.inlineStyles[1], (InlineStyleRun{{8, 13}, InlineStyle::Bold | InlineStyle::Italic}));

    const auto italicThenBold = show(u"*italic **italicandbold***");
    EXPECT_EQ(spans(italicThenBold.hidden), (std::vector<Span>{{0, 1}, {8, 2}, {23, 3}}));
    ASSERT_EQ(italicThenBold.inlineStyles.size(), 2u);
    EXPECT_EQ(italicThenBold.inlineStyles[0], (InlineStyleRun{{1, 7}, InlineStyle::Italic}));
    EXPECT_EQ(italicThenBold.inlineStyles[1], (InlineStyleRun{{10, 13}, InlineStyle::Bold | InlineStyle::Italic}));

    const auto underscore = show(u"__bold _boldanditalic___");
    EXPECT_EQ(spans(underscore.hidden), (std::vector<Span>{{0, 2}, {7, 1}, {21, 3}}));
    ASSERT_EQ(underscore.inlineStyles.size(), 2u);
    EXPECT_EQ(underscore.inlineStyles[0], (InlineStyleRun{{2, 5}, InlineStyle::Bold}));
    EXPECT_EQ(underscore.inlineStyles[1], (InlineStyleRun{{8, 13}, InlineStyle::Bold | InlineStyle::Italic}));
}

TEST(S013_InlineEmphasis, TripleOpeningRunCanCloseAcrossSeparateRuns) {
    const auto r = show(u"***bold italic** and italic*");
    EXPECT_EQ(spans(r.hidden), (std::vector<Span>{{0, 3}, {14, 2}, {27, 1}}));
    ASSERT_EQ(r.inlineStyles.size(), 2u);
    EXPECT_EQ(r.inlineStyles[0], (InlineStyleRun{{3, 11}, InlineStyle::Bold | InlineStyle::Italic}));
    EXPECT_EQ(r.inlineStyles[1], (InlineStyleRun{{16, 11}, InlineStyle::Italic}));
}

TEST(S013_InlineEmphasis, StrikethroughNestsWithCombinedBoldAndItalic) {
    const auto r = show(u"~~strike ***allthree*** strike~~");
    EXPECT_EQ(spans(r.hidden), (std::vector<Span>{{0, 2}, {9, 3}, {20, 3}, {30, 2}}));
    ASSERT_EQ(r.inlineStyles.size(), 3u);
    EXPECT_EQ(r.inlineStyles[0], (InlineStyleRun{{2, 7}, InlineStyle::Strikethrough}));
    EXPECT_EQ(r.inlineStyles[1],
              (InlineStyleRun{{12, 8}, InlineStyle::Bold | InlineStyle::Italic | InlineStyle::Strikethrough}));
    EXPECT_EQ(r.inlineStyles[2], (InlineStyleRun{{23, 7}, InlineStyle::Strikethrough}));
}

TEST(S013_InlineEmphasis, UnderscoreStrongAndCombinedRunsAreSupported) {
    const auto r = show(u"__bold__ ___both___");
    EXPECT_EQ(spans(r.hidden), (std::vector<Span>{{0, 2}, {6, 2}, {9, 3}, {16, 3}}));
    ASSERT_EQ(r.inlineStyles.size(), 2u);
    EXPECT_EQ(r.inlineStyles[0], (InlineStyleRun{{2, 4}, InlineStyle::Bold}));
    EXPECT_EQ(r.inlineStyles[1], (InlineStyleRun{{12, 4}, InlineStyle::Bold | InlineStyle::Italic}));
}

TEST(S013_InlineEmphasis, EmphasisIsAppliedInsideHeadings) {
    const auto r = show(u"# **Title**");
    EXPECT_EQ(r.headingLevel, 1);
    EXPECT_EQ(spans(r.hidden), (std::vector<Span>{{0, 2}, {2, 2}, {9, 2}}));
    ASSERT_EQ(r.inlineStyles.size(), 1u);
    EXPECT_EQ(r.inlineStyles[0].span, (Span{4, 5}));
    EXPECT_EQ(r.inlineStyles[0].style, InlineStyle::Bold);
}

TEST(S013_InlineEmphasis, PropertyRangesStayInsideLineAndDoNotOverlapHiddenMarkers) {
    std::mt19937 rng(73013);
    constexpr char16_t alphabet[] = {u'*', u'*', u'_', u'~', u'\\', u' ', u'\t', u'a', u'b', u'\u00e4'};
    for (int iteration = 0; iteration < 5000; ++iteration) {
        std::u16string line(rng() % 32, u' ');
        for (auto& c : line) {
            c = alphabet[rng() % std::size(alphabet)];
        }
        const auto r = show(line);
        std::uint32_t previousEnd = 0;
        for (const auto& run : r.inlineStyles) {
            ASSERT_LE(run.span.end(), line.size());
            ASSERT_GT(run.span.length, 0u);
            ASSERT_NE(run.style, InlineStyle::None);
            ASSERT_GE(run.span.start, previousEnd);
            previousEnd = run.span.end();
            for (const auto& hidden : r.hidden) {
                ASSERT_TRUE(run.span.end() <= hidden.span.start || hidden.span.end() <= run.span.start);
            }
        }
        for (const auto& hidden : r.hidden) {
            ASSERT_LE(hidden.span.end(), line.size());
        }
    }
}

TEST(S013_InlineEmphasis, UnmatchedEmptyWhitespaceAndEscapedDelimitersStayLiteral) {
    for (const auto line : {u"x *open", u"x **open", u"x open~~", u"x ****", u"x * *", u"x ** **", u"x * spaced*",
                            u"x *spaced *", u"x \\*escaped\\*", u"x a_b_c"}) {
        EXPECT_TRUE(show(line).empty()) << "unexpected emphasis in input";
    }
}

// Theming baseline (ADR 0004): revealed delimiters carry their own role, so emphasis.marker
// applies to them; concealed ones are hidden only.
TEST(S013_InlineEmphasis, RevealedDelimitersAreEmphasisMarkers) {
    EXPECT_EQ(show(u"**b** x", true).runs,
              (std::vector<StyleRun>{{{0, 2}, Role::EmphasisMarker, 0}, {{3, 2}, Role::EmphasisMarker, 0}}));
    EXPECT_TRUE(show(u"**b** x").runs.empty());

    const auto heading = show(u"# **b**", true);
    EXPECT_EQ(heading.headingLevel, 1);
    EXPECT_EQ(heading.runs, (std::vector<StyleRun>{{{0, 2}, Role::Marker, 1},
                                                   {{2, 2}, Role::EmphasisMarker, 0},
                                                   {{4, 1}, Role::HeadingText, 1},
                                                   {{5, 2}, Role::EmphasisMarker, 0}}))
        << "inline markers take precedence over the heading text run; runs never overlap";
}

TEST(S013_InlineEmphasis, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[emphasis]\nbold-weight = 600\nbold-text = \"#010203\"\nitalic-text = \"#040506\"\n"
        "strikethrough-text = \"#070809\"\nmarker = \"#0a0b0c\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << r.diagnostics.front().toString();
    const EmphasisStyle s = emphasisStyle(r.theme);
    EXPECT_EQ(s.boldWeight, 600);
    EXPECT_EQ(s.boldText, (Color{1, 2, 3, 255}));
    EXPECT_EQ(s.italicText, (Color{4, 5, 6, 255}));
    EXPECT_EQ(s.strikethroughText, (Color{7, 8, 9, 255}));
    EXPECT_EQ(s.marker, (Color{10, 11, 12, 255}));
    EXPECT_TRUE(loadTheme("[emphasis]\nbold-weight = 50\n", ColorScheme::Light).usedFallback);
}

TEST(S013_InlineEmphasis, ThemeTokensFallBack) {
    const Theme& t = builtinTheme(ColorScheme::Dark);
    const EmphasisStyle s = emphasisStyle(t);
    EXPECT_EQ(s.boldWeight, 700);
    EXPECT_FALSE(s.boldText) << "unset: the surrounding text colour";
    EXPECT_FALSE(s.italicText);
    EXPECT_FALSE(s.strikethroughText);
    EXPECT_EQ(s.marker, t.marker);
}
