// SPDX-License-Identifier: GPL-3.0-or-later
// S002: headings render large; markers hidden unless the line is revealed.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/reveal.hpp"

#include <gtest/gtest.h>

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
}  // namespace

TEST(S002_Headings, MarkerHiddenWhenCursorElsewhere) {
    const auto r = show(u"## Plans");
    EXPECT_EQ(r.headingLevel, 2);
    ASSERT_EQ(r.hidden.size(), 1u);
    EXPECT_EQ(r.hidden[0], (HiddenRange{{0, 3}, HideMode::Collapse}));
    ASSERT_EQ(r.runs.size(), 1u);
    EXPECT_EQ(r.runs[0], (StyleRun{{3, 5}, Role::HeadingText, 2}));
    EXPECT_TRUE(r.decorations.empty());
}

TEST(S002_Headings, MarkerShownWhenRevealed) {
    const auto r = show(u"## Plans", true);
    EXPECT_EQ(r.headingLevel, 2);
    EXPECT_TRUE(r.hidden.empty());
    ASSERT_EQ(r.runs.size(), 2u);
    EXPECT_EQ(r.runs[0], (StyleRun{{0, 3}, Role::Marker, 2}));
    EXPECT_EQ(r.runs[1], (StyleRun{{3, 5}, Role::HeadingText, 2}));
}

TEST(S002_Headings, LevelsOneToSix) {
    EXPECT_EQ(show(u"# a").headingLevel, 1);
    EXPECT_EQ(show(u"###### a").headingLevel, 6);
    EXPECT_EQ(show(u"###\ta").headingLevel, 3);
}

TEST(S002_Headings, NonHeadingsArePlainText) {
    EXPECT_TRUE(show(u"####### seven").empty());
    EXPECT_TRUE(show(u"#hashtag").empty());
    EXPECT_TRUE(show(u"    # four spaces").empty());
    EXPECT_TRUE(show(u"text # not at start").empty());
    EXPECT_TRUE(show(u"").empty());
}

TEST(S002_Headings, UpToThreeSpacesOfIndentation) {
    const auto r = show(u"   # indented");
    EXPECT_EQ(r.headingLevel, 1);
    EXPECT_EQ(r.hidden.at(0).span, (Span{0, 5}));
}

TEST(S002_Headings, EmptyHeadingIsStillAHeading) {
    const auto r = show(u"#");
    EXPECT_EQ(r.headingLevel, 1);
    EXPECT_EQ(r.hidden.at(0).span, (Span{0, 1}));
    EXPECT_TRUE(r.runs.empty());
}

TEST(S002_Headings, MultipleBlanksAfterMarkerAreHiddenToo) {
    EXPECT_EQ(show(u"#  \t x").hidden.at(0).span, (Span{0, 5}));
}

TEST(S002_Headings, RevealedRangeCoversSelection) {
    EXPECT_EQ(revealedLines(5, 5), (LineRange{5, 5}));
    EXPECT_EQ(revealedLines(9, 4), (LineRange{4, 9}));
}

TEST(S002_Headings, OnlyLinesEnteringOrLeavingTheRevealedRangeAreRerendered) {
    EXPECT_TRUE(revealChanges({3, 3}, {3, 3}).empty());
    EXPECT_EQ(revealChanges({3, 3}, {4, 4}), (std::vector<LineRange>{{3, 4}}));
    EXPECT_EQ(revealChanges({3, 3}, {10, 10}), (std::vector<LineRange>{{3, 3}, {10, 10}}));
    EXPECT_EQ(revealChanges({2, 8}, {4, 6}), (std::vector<LineRange>{{2, 3}, {7, 8}}));
    EXPECT_EQ(revealChanges({2, 5}, {4, 9}), (std::vector<LineRange>{{2, 3}, {6, 9}}));
    EXPECT_EQ(revealChanges({}, {1, 2}), (std::vector<LineRange>{{1, 2}}));
}

// Property: for any line and reveal state, spans stay inside the line, hidden and visible
// runs never overlap, and a revealed line hides nothing.
TEST(S002_Headings, PropertyRenderIsWellFormed) {
    std::mt19937 rng(42);
    const char16_t alphabet[] = {u'#', u'#',  u'-', u'*',      u'+',   u'_',   u' ',
                                 u' ', u'\t', u'a', u'\u00E4', 0xD83D, 0xDE00, 0};
    for (int iteration = 0; iteration < 50000; ++iteration) {
        std::u16string line(rng() % 14, u' ');
        for (auto& c : line) {
            c = alphabet[rng() % std::size(alphabet)];
        }
        for (bool revealed : {false, true}) {
            const auto r = show(line, revealed);
            std::vector<int> cover(line.size(), 0);
            for (const auto& run : r.runs) {
                ASSERT_LE(run.span.end(), line.size());
                for (auto k = run.span.start; k < run.span.end(); ++k)
                    ++cover[k];
            }
            for (const auto& h : r.hidden) {
                ASSERT_FALSE(revealed) << "revealed lines must show all text";
                ASSERT_LE(h.span.end(), line.size());
                for (auto k = h.span.start; k < h.span.end(); ++k)
                    ++cover[k];
            }
            for (const auto& d : r.decorations) {
                ASSERT_LT(d.column, line.size());
            }
            for (int c : cover) {
                ASSERT_LE(c, 1) << "overlapping spans";
            }
        }
    }
}
