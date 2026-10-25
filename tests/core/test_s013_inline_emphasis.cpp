// SPDX-License-Identifier: GPL-3.0-or-later
// S013: inline emphasis stays in the plain-text document and maps to render spans.
#include "hitokoto/core/markdown.hpp"

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
    for (const auto& range : ranges) {
        out.push_back(range.span);
    }
    return out;
}
}  // namespace

TEST(S013_InlineEmphasis, ConcealsMatchedDelimitersAndReturnsStyledTextRanges) {
    const auto r = show(u"**bold** *italic* _under_ ~~gone~~");

    EXPECT_EQ(spans(r.hidden),
              (std::vector<Span>{{0, 2}, {6, 2}, {9, 1}, {16, 1}, {18, 1}, {24, 1}, {26, 2}, {32, 2}}));
    std::vector<Span> styled;
    for (const auto& run : r.inlineStyles) {
        styled.push_back(run.span);
    }
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
    std::vector<Span> styled;
    for (const auto& run : r.inlineStyles) {
        styled.push_back(run.span);
    }
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
    std::vector<Span> nestedRuns;
    for (const auto& run : nested.inlineStyles) {
        nestedRuns.push_back(run.span);
    }
    EXPECT_EQ(nestedRuns, (std::vector<Span>{{2, 5}, {8, 6}, {15, 5}}));
    EXPECT_EQ(spans(nested.hidden), (std::vector<Span>{{0, 2}, {7, 1}, {14, 1}, {20, 2}}));
    ASSERT_EQ(nested.inlineStyles.size(), 3u);
    EXPECT_EQ(nested.inlineStyles[0].style, InlineStyle::Bold);
    EXPECT_EQ(nested.inlineStyles[1].style, InlineStyle::Bold | InlineStyle::Italic);
    EXPECT_EQ(nested.inlineStyles[2].style, InlineStyle::Bold);

    const auto combined = show(u"***both***");
    std::vector<Span> combinedRuns;
    for (const auto& run : combined.inlineStyles) {
        combinedRuns.push_back(run.span);
    }
    EXPECT_EQ(combinedRuns, (std::vector<Span>{{3, 4}}));
    EXPECT_EQ(spans(combined.hidden), (std::vector<Span>{{0, 3}, {7, 3}}));
    ASSERT_EQ(combined.inlineStyles.size(), 1u);
    EXPECT_EQ(combined.inlineStyles[0].style, InlineStyle::Bold | InlineStyle::Italic);
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
