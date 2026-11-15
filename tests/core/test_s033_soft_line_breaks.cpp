// SPDX-License-Identifier: GPL-3.0-or-later
// S033: soft line breaks (paragraphs flow) and CommonMark 6.7 hard line breaks.
#include "hitokoto/core/find.hpp"
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::vector<LineRender> showAll(const std::vector<std::u16string>& lines, LineRange revealed = {}) {
    return registry().renderDocument(lines, revealed);
}
}  // namespace

TEST(S033_SoftLineBreaks, ConsecutiveParagraphLinesFlowUntilTheLastLine) {
    const std::vector<std::u16string> lines = {u"First line", u"second line", u"third line."};
    const auto r = showAll(lines);
    ASSERT_EQ(r.size(), 3U);
    EXPECT_TRUE(r[0].flowsToNext);
    EXPECT_FALSE(r[0].hardBreak);
    EXPECT_TRUE(r[0].hidden.empty());

    EXPECT_TRUE(r[1].flowsToNext);
    EXPECT_FALSE(r[1].hardBreak);
    EXPECT_TRUE(r[1].hidden.empty());

    EXPECT_FALSE(r[2].flowsToNext);
    EXPECT_FALSE(r[2].hardBreak);
    EXPECT_TRUE(r[2].hidden.empty());
}

TEST(S033_SoftLineBreaks, LeadingAndTrailingBlanksCollapseOnFlowingParagraphLines) {
    // CommonMark 4.8 & 6.8: leading spaces/tabs on continuation lines and single trailing space
    // or trailing tabs before a soft line break are removed.
    const std::vector<std::u16string> lines = {u"  First line ", u"   second line\t", u"\tthird line"};
    const auto r = showAll(lines);
    ASSERT_EQ(r.size(), 3U);
    EXPECT_TRUE(r[0].flowsToNext);
    EXPECT_EQ(r[0].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}, {{12, 1}, HideMode::Collapse}}));

    EXPECT_TRUE(r[1].flowsToNext);
    EXPECT_EQ(r[1].hidden, (std::vector<HiddenRange>{{{0, 3}, HideMode::Collapse}, {{14, 1}, HideMode::Collapse}}));

    EXPECT_FALSE(r[2].flowsToNext);
    EXPECT_EQ(r[2].hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));
}

TEST(S033_SoftLineBreaks, TwoOrMoreTrailingSpacesCreateAHardLineBreak) {
    // CommonMark 6.7: 2+ trailing spaces before a continuation line create a hard line break;
    // a single space or tabs do not.
    const std::vector<std::u16string> lines = {
        u"Two spaces  ", u"five spaces     ", u"one space ", u"two tabs\t\t", u"last line   ",
    };
    const auto r = showAll(lines);
    ASSERT_EQ(r.size(), 5U);
    EXPECT_TRUE(r[0].hardBreak);
    EXPECT_FALSE(r[0].flowsToNext);
    EXPECT_EQ(r[0].hidden, (std::vector<HiddenRange>{{{10, 2}, HideMode::Collapse}}));

    EXPECT_TRUE(r[1].hardBreak);
    EXPECT_FALSE(r[1].flowsToNext);
    EXPECT_EQ(r[1].hidden, (std::vector<HiddenRange>{{{11, 5}, HideMode::Collapse}}));

    EXPECT_FALSE(r[2].hardBreak);
    EXPECT_TRUE(r[2].flowsToNext);
    EXPECT_EQ(r[2].hidden, (std::vector<HiddenRange>{{{9, 1}, HideMode::Collapse}}));

    EXPECT_FALSE(r[3].hardBreak);
    EXPECT_TRUE(r[3].flowsToNext);
    EXPECT_EQ(r[3].hidden, (std::vector<HiddenRange>{{{8, 2}, HideMode::Collapse}}));

    // Trailing spaces at the end of a paragraph are not a hard line break (CommonMark 6.7).
    EXPECT_FALSE(r[4].hardBreak);
    EXPECT_FALSE(r[4].flowsToNext);
}

TEST(S033_SoftLineBreaks, BackslashBeforeContinuationLineCreatesAHardLineBreak) {
    // CommonMark 6.7: an unescaped trailing `\` before a continuation line is a hard break;
    // `\\` is an escaped backslash followed by a soft break; `\\\` is an escaped backslash followed
    // by a hard break; a trailing `\` on the last line of a paragraph is literal text.
    const std::vector<std::u16string> lines = {
        u"one\\", u"two\\\\", u"three\\\\\\", u"`code\\`", u"end\\",
    };
    const auto concealed = showAll(lines);
    ASSERT_EQ(concealed.size(), 5U);

    // Line 0: `one\` -> hard break, `\` collapsed.
    EXPECT_TRUE(concealed[0].hardBreak);
    EXPECT_FALSE(concealed[0].flowsToNext);
    EXPECT_EQ(concealed[0].hidden, (std::vector<HiddenRange>{{{3, 1}, HideMode::Collapse}}));
    EXPECT_TRUE(concealed[0].runs.empty());

    // Line 1: `two\\` -> escaped backslash (first `\` hidden by S020), soft line break.
    EXPECT_FALSE(concealed[1].hardBreak);
    EXPECT_TRUE(concealed[1].flowsToNext);
    EXPECT_EQ(concealed[1].hidden, (std::vector<HiddenRange>{{{3, 1}, HideMode::Collapse}}));

    // Line 2: `three\\\` -> first `\` hidden by S020, third `\` hidden as hard break.
    EXPECT_TRUE(concealed[2].hardBreak);
    EXPECT_FALSE(concealed[2].flowsToNext);
    EXPECT_EQ(concealed[2].hidden,
              (std::vector<HiddenRange>{{{5, 1}, HideMode::Collapse}, {{7, 1}, HideMode::Collapse}}));

    // Line 3: `` `code\` `` -> `\` is inside an inline code span, followed by ` ` `, soft break.
    EXPECT_FALSE(concealed[3].hardBreak);
    EXPECT_TRUE(concealed[3].flowsToNext);

    // Line 4: `end\` -> last line of paragraph, literal `\`, no hard break and no escape marker.
    EXPECT_FALSE(concealed[4].hardBreak);
    EXPECT_FALSE(concealed[4].flowsToNext);
    EXPECT_TRUE(concealed[4].hidden.empty());
    EXPECT_TRUE(concealed[4].runs.empty());

    // When line 0 is revealed, the trailing `\` is visible and styled with Role::EscapeMarker.
    const auto revealed0 = showAll(lines, {0, 0});
    EXPECT_FALSE(revealed0[0].hardBreak);
    EXPECT_FALSE(revealed0[0].flowsToNext);
    EXPECT_TRUE(revealed0[0].hidden.empty());
    EXPECT_EQ(revealed0[0].runs, (std::vector<StyleRun>{{{3, 1}, Role::EscapeMarker, 0}}));
    // Line 2's trailing `\` is also revealed with Role::EscapeMarker while its inline `\\` stays concealed!
    EXPECT_EQ(revealed0[2].hidden, (std::vector<HiddenRange>{{{5, 1}, HideMode::Collapse}}));
    EXPECT_EQ(revealed0[2].runs, (std::vector<StyleRun>{{{7, 1}, Role::EscapeMarker, 0}}));
    // Line 4's trailing `\` at the end of the paragraph is still plain literal text.
    EXPECT_TRUE(revealed0[4].runs.empty());
}

TEST(S033_SoftLineBreaks, BulletListItemContinuationsFlowAtTheBulletWrapIndent) {
    const std::vector<std::u16string> lines = {
        u"  - first bullet line",
        u"    indented continuation\\",
        u"  lazy continuation",
        u"- next bullet item",
    };
    const auto r = showAll(lines);
    ASSERT_EQ(r.size(), 4U);

    // First bullet item spans lines 0..2 with content column 4 (`wrapIndent = 4`).
    EXPECT_TRUE(r[0].flowsToNext);
    EXPECT_EQ(r[0].wrapIndent, 4U);

    EXPECT_TRUE(r[1].hardBreak);
    EXPECT_FALSE(r[1].flowsToNext);
    EXPECT_EQ(r[1].wrapIndent, 4U);
    EXPECT_EQ(r[1].hidden, (std::vector<HiddenRange>{{{0, 4}, HideMode::Collapse}, {{25, 1}, HideMode::Collapse}}));

    EXPECT_FALSE(r[2].flowsToNext);
    EXPECT_FALSE(r[2].hardBreak);
    EXPECT_EQ(r[2].wrapIndent, 4U);
    EXPECT_EQ(r[2].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}}));

    // Line 3 starts a new bullet item; line 2 did not flow into it.
    EXPECT_FALSE(r[3].flowsToNext);
    EXPECT_EQ(r[3].wrapIndent, 2U);
    EXPECT_EQ(r[3].decorations.size(), 1U);
}

TEST(S033_SoftLineBreaks, RevealingAnyLineOfAParagraphRevealsTheParagraphFlowButKeepsOtherInlineMarkersConcealed) {
    const std::vector<std::u16string> lines = {
        u"First **bold** line", u"  second line\\", u"  third *italic* line", u"",
        u"Other paragraph",     u"continues here",
    };

    for (const std::int64_t caret : {0, 1, 2}) {
        const auto r = showAll(lines, {caret, caret});
        for (std::size_t i = 0; i < 3; ++i) {
            EXPECT_FALSE(r[i].flowsToNext) << "caret " << caret << " line " << i;
            EXPECT_FALSE(r[i].hardBreak) << "caret " << caret << " line " << i;
        }
        // Line 1's leading spaces and trailing `\` are revealed for any caret in 0..2.
        EXPECT_TRUE(r[1].hidden.empty()) << "caret " << caret;
        EXPECT_EQ(r[1].runs, (std::vector<StyleRun>{{{13, 1}, Role::EscapeMarker, 0}})) << "caret " << caret;

        // Inline emphasis on line 0 (`**`) is only revealed when caret == 0.
        EXPECT_EQ(r[0].hidden.empty(), caret == 0) << "caret " << caret;
        // Inline emphasis on line 2 (`*`) is only revealed when caret == 2; its leading spaces are always revealed.
        if (caret == 2) {
            EXPECT_TRUE(r[2].hidden.empty());
        } else {
            EXPECT_EQ(r[2].hidden,
                      (std::vector<HiddenRange>{{{8, 1}, HideMode::Collapse}, {{15, 1}, HideMode::Collapse}}));
        }

        // The second paragraph (lines 4..5) is unaffected and still flows.
        EXPECT_TRUE(r[4].flowsToNext) << "caret " << caret;
    }
}

TEST(S033_SoftLineBreaks, MultiLineSetextHeadingFlowsAndRevealsWithItsUnderline) {
    const std::vector<std::u16string> lines = {
        u"Heading one\\",
        u"  heading two",
        u"  heading three",
        u"---",
    };
    const auto concealed = showAll(lines);
    ASSERT_EQ(concealed.size(), 4U);
    EXPECT_EQ(concealed[0].headingLevel, 2);
    EXPECT_TRUE(concealed[0].hardBreak);
    EXPECT_EQ(concealed[0].hidden, (std::vector<HiddenRange>{{{11, 1}, HideMode::Collapse}}));

    EXPECT_EQ(concealed[1].headingLevel, 2);
    EXPECT_TRUE(concealed[1].flowsToNext);
    EXPECT_EQ(concealed[1].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}}));

    EXPECT_EQ(concealed[2].headingLevel, 2);
    EXPECT_FALSE(concealed[2].flowsToNext);
    EXPECT_EQ(concealed[2].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}}));

    EXPECT_EQ(concealed[3].setextUnderline, 2);
    EXPECT_FALSE(concealed[3].hidden.empty());

    // Caret on any of 0..3 (including the underline on line 3) reveals all paragraph lines and the underline.
    for (const std::int64_t caret : {0, 1, 2, 3}) {
        const auto rev = showAll(lines, {caret, caret});
        for (std::size_t i = 0; i < 4; ++i) {
            EXPECT_FALSE(rev[i].flowsToNext) << "caret " << caret << " line " << i;
            EXPECT_FALSE(rev[i].hardBreak) << "caret " << caret << " line " << i;
            EXPECT_TRUE(rev[i].hidden.empty()) << "caret " << caret << " line " << i;
        }
        // On line 0, `\` has Role::EscapeMarker and `Heading one` has Role::HeadingText.
        EXPECT_EQ(rev[0].runs,
                  (std::vector<StyleRun>{{{0, 11}, Role::HeadingText, 2}, {{11, 1}, Role::EscapeMarker, 0}}));
    }
}

TEST(S033_SoftLineBreaks, OrderedListAndBlockquoteContinuationsFlow) {
    // Ordered lists and blockquotes have BlockState::kLazyParagraph; their continuation lines
    // conceal leading spaces and flow with the item/quote while the caret is outside.
    const std::vector<std::u16string> orderedLines = {
        u"1. Implement `LineElement` for line-local block constructs, `MultiLineElement` for blocks that",
        u"   span lines (fenced code) or `InlineElement` for inline constructs in",
        u"   `src/core/src/elements/<name>.cpp`, with a factory function.",
        u"2. Second item",
    };
    const auto ordered = showAll(orderedLines);
    EXPECT_TRUE(ordered[0].flowsToNext);
    EXPECT_TRUE(ordered[1].flowsToNext);
    EXPECT_FALSE(ordered[2].flowsToNext);
    EXPECT_FALSE(ordered[3].flowsToNext);
    EXPECT_TRUE(std::any_of(ordered[1].hidden.begin(), ordered[1].hidden.end(),
                            [](const HiddenRange& h) { return h == HiddenRange{{0, 3}, HideMode::Collapse}; }));
    EXPECT_TRUE(std::any_of(ordered[2].hidden.begin(), ordered[2].hidden.end(),
                            [](const HiddenRange& h) { return h == HiddenRange{{0, 3}, HideMode::Collapse}; }));

    // Caret on line 1 reveals item 1's paragraph (0..2), leaving item 2 untouched.
    const auto rev = showAll(orderedLines, {1, 1});
    EXPECT_FALSE(rev[0].flowsToNext);
    EXPECT_FALSE(rev[1].flowsToNext);
    EXPECT_FALSE(rev[2].flowsToNext);

    const auto quote = showAll({u"> quote", u"  more"});
    EXPECT_TRUE(quote[0].flowsToNext);
    EXPECT_EQ(quote[1].hidden, (std::vector<HiddenRange>{{{0, 2}, HideMode::Collapse}}));
}

TEST(S033_SoftLineBreaks, FindInFileMatchesAcrossSoftAndHardBreaksInFlowingParagraphs) {
    const std::vector<std::u16string> lines = {
        u"First word\\",
        u"  second word  ",
        u"  third word",
    };
    const auto m1 = findInDocument(registry(), lines, u"word second word third");
    ASSERT_EQ(m1.size(), 1U);
    EXPECT_EQ(m1[0].start, (DocPos{0, 6}));
    EXPECT_EQ(m1[0].end, (DocPos{2, 7}));
}
