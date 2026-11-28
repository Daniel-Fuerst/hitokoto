// SPDX-License-Identifier: GPL-3.0-or-later
// S053: ordered list items show their number as written, follow the paragraph rules of CommonMark
// 5.2, and Enter, Tab and Shift+Tab keep the numbers of a list in order.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/ordered_list.hpp"
#include "hitokoto/core/theme.hpp"

#include "support.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace hitokoto::core;

namespace {
using hitokoto::test::narrow;
using hitokoto::test::registry;
using hitokoto::test::show;

std::vector<LineRender> showAll(const std::vector<std::u16string>& lines) {
    return registry().renderDocument(lines);
}

LineAt over(const std::vector<std::u16string>& lines) {
    return [&lines](std::size_t i) -> std::optional<std::u16string> {
        if (i >= lines.size()) {
            return std::nullopt;
        }
        return lines[i];
    };
}

std::vector<std::u16string> applied(std::vector<std::u16string> lines, const std::vector<NumberEdit>& edits) {
    for (const auto& edit : edits) {
        lines[edit.line].replace(edit.digits.start, edit.digits.length, edit.text);
    }
    return lines;
}

std::string joined(const std::vector<std::u16string>& lines) {
    std::string out;
    for (const auto& line : lines) {
        out += narrow(line) + " / ";
    }
    return out;
}
}  // namespace

TEST(S053_NumberedLists, NumberShownAsWrittenWhenCursorElsewhere) {
    const auto r = show(u"3. milk");
    EXPECT_TRUE(r.hidden.empty());
    EXPECT_TRUE(r.decorations.empty());
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{0, 2}, Role::ListNumber, 0}}));
    EXPECT_EQ(r.wrapIndent, 3U);
}

TEST(S053_NumberedLists, RawMarkerOnCursorLine) {
    const auto r = show(u"3. milk", true);
    EXPECT_TRUE(r.hidden.empty());
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{0, 2}, Role::ListNumberMarker, 0}}));
    EXPECT_EQ(r.wrapIndent, 3U);
}

TEST(S053_NumberedLists, WhatIsAnItem) {
    for (std::u16string_view line : {u"1. a", u"1) a", u"10. a", u"123456789. a", u"1.", u"1.\ta", u"  2. a"}) {
        const auto r = show(line);
        ASSERT_EQ(r.runs.size(), 1U) << narrow(line);
        EXPECT_EQ(r.runs[0].role, Role::ListNumber) << narrow(line);
    }
    for (std::u16string_view line : {u"1.a", u"1234567890. a", u"a. x", u"1 . x", u"-1. x"}) {
        const auto r = show(line);
        for (const auto& run : r.runs) {
            EXPECT_NE(run.role, Role::ListNumber) << narrow(line);
        }
    }
}

TEST(S053_NumberedLists, IndentedItemAndWrapIndent) {
    const auto r = show(u"    12) a");
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{4, 3}, Role::ListNumber, 0}}));
    EXPECT_EQ(r.wrapIndent, 8U);
    EXPECT_EQ(show(u"1.   a").wrapIndent, 5U);
}

TEST(S053_NumberedLists, InterruptingAParagraph) {
    EXPECT_EQ(showAll({u"Foo", u"1. bar"})[1].runs.size(), 1U) << "an item numbered 1 interrupts";
    for (const auto& lines : std::vector<std::vector<std::u16string>>{{u"Foo", u"2. bar"}, {u"Foo", u"1."}}) {
        const auto r = showAll(lines);
        EXPECT_TRUE(r[1].runs.empty()) << joined(lines);
        EXPECT_TRUE(r[0].flowsToNext) << joined(lines) << ": one paragraph";
    }
    EXPECT_EQ(showAll({u"Foo", u"2. "})[1].runs.size(), 1U) << "a marker and a blank is an empty item (as S050)";
    EXPECT_EQ(showAll({u"- foo", u"2. y"})[1].runs.size(), 1U) << "a lazy line starts any item";
}

TEST(S053_NumberedLists, ItemTextFlowsAndCanBeUnderlined) {
    const auto flowing = showAll({u"1. foo", u"   bar"});
    EXPECT_TRUE(flowing[0].flowsToNext);
    const auto underlined = showAll({u"1. foo", u"   ---"});
    EXPECT_EQ(underlined[1].setextUnderline, 2);
    const auto broken = showAll({u"1. foo", u"---"});
    EXPECT_TRUE(broken[1].thematicBreak);
}

TEST(S053_NumberedLists, ParseOrderedLine) {
    EXPECT_EQ(parseOrderedLine(u"  12)  x"), (OrderedLineInfo{2, {2, 2}, 12, u')', 7, false}));
    EXPECT_EQ(parseOrderedLine(u"\t1."), (OrderedLineInfo{4, {1, 1}, 1, u'.', 3, true}));
    EXPECT_EQ(parseOrderedLine(u"3.  "), (OrderedLineInfo{0, {0, 1}, 3, u'.', 4, true}));
    EXPECT_FALSE(parseOrderedLine(u"1.x").has_value());
    EXPECT_FALSE(parseOrderedLine(u"- 1. x").has_value());
}

TEST(S053_NumberedLists, PrefixAndIndentation) {
    const std::u16string line = u"  9)  x";
    const auto info = parseOrderedLine(line);
    const auto bare = parseOrderedLine(u"1.");
    if (!info || !bare) {
        FAIL() << "both are items";
    }
    EXPECT_EQ(nextOrderedPrefix(line, *info), u"  10)  ");
    EXPECT_EQ(nextOrderedPrefix(u"1.", *bare), u"2. ");
    EXPECT_EQ(orderedMarkerWidth(*info), 3U);
    EXPECT_EQ(indentOrderedLine(u"2. b"), u"   2. b");
    EXPECT_EQ(outdentOrderedLine(u"   2. b"), u"2. b");
    EXPECT_EQ(outdentOrderedLine(u"     10. b"), u" 10. b");
    EXPECT_EQ(outdentOrderedLine(u"\t2. b"), u"2. b");
    EXPECT_EQ(outdentOrderedLine(u"2. b"), u"2. b");
}

TEST(S053_NumberedLists, RenumberKeepsTheStartAndClosesGaps) {
    const std::vector<std::u16string> lines = {u"1. a", u"2. ", u"2. b", u"3. c"};
    EXPECT_EQ(applied(lines, renumberOrderedList(over(lines), 1)),
              (std::vector<std::u16string>{u"1. a", u"2. ", u"3. b", u"4. c"}));
    const std::vector<std::u16string> seven = {u"7) a", u"7) b", u"", u"   more", u"7) c"};
    EXPECT_EQ(applied(seven, renumberOrderedList(over(seven), 4)),
              (std::vector<std::u16string>{u"7) a", u"8) b", u"", u"   more", u"9) c"}));
}

TEST(S053_NumberedLists, RenumberRewritesOnlyChangedDigits) {
    const std::vector<std::u16string> lines = {u"1. a", u"2. b", u"2. c"};
    EXPECT_EQ(renumberOrderedList(over(lines), 0), (std::vector<NumberEdit>{{2, {0, 1}, u"3"}}));
    EXPECT_TRUE(renumberOrderedList(over({u"text"}), 0).empty());
}

TEST(S053_NumberedLists, RenumberStaysInsideItsList) {
    const std::vector<std::u16string> lines = {u"1. a", u"2. ", u"   1. x", u"   1. y",
                                               u"2. b", u"- c", u"1. d",    u"1) e"};
    EXPECT_EQ(
        applied(lines, renumberOrderedList(over(lines), 1)),
        (std::vector<std::u16string>{u"1. a", u"2. ", u"   1. x", u"   1. y", u"3. b", u"- c", u"1. d", u"1) e"}));
    const std::vector<std::u16string> delimiters = {u"1. a", u"1) b", u"1. c"};
    EXPECT_TRUE(renumberOrderedList(over(delimiters), 0).empty()) << "a change of delimiter ends the list";
    const std::vector<std::u16string> lazy = {u"1. a", u"text", u"1. b"};
    EXPECT_TRUE(renumberOrderedList(over(lazy), 2).empty()) << "a less indented line ends the list";
}

TEST(S053_NumberedLists, RenumberWithRestart) {
    const std::vector<std::u16string> lines = {u"1. a", u"   2. b", u"   3. c"};
    EXPECT_EQ(applied(lines, renumberOrderedList(over(lines), 1, true)),
              (std::vector<std::u16string>{u"1. a", u"   1. b", u"   2. c"}));
}

TEST(S053_NumberedLists, TabNestsAndRenumbers) {
    // `2. b` after Tab: it is the first of a nested list, and `3. c` follows `1. a`.
    const std::vector<std::u16string> lines = {u"1. a", u"   2. b", u"3. c"};
    EXPECT_EQ(applied(lines, renumberAfterReindent(over(lines), 1, 0, u'.', ReindentKind::Indented)),
              (std::vector<std::u16string>{u"1. a", u"   1. b", u"2. c"}));
}

TEST(S053_NumberedLists, ShiftTabJoinsTheParentList) {
    const std::vector<std::u16string> lines = {u"1. a", u"   1. x", u"2. y", u"   3. z", u"2. b"};
    EXPECT_EQ(applied(lines, renumberAfterReindent(over(lines), 2, 3, u'.', ReindentKind::Outdented)),
              (std::vector<std::u16string>{u"1. a", u"   1. x", u"2. y", u"   1. z", u"3. b"}));
    const std::vector<std::u16string> moved = {u"1. a", u"1. b", u"2. c"};
    EXPECT_EQ(applied(moved, renumberAfterReindent(over(moved), 1, 3, u'.', ReindentKind::Outdented)),
              (std::vector<std::u16string>{u"1. a", u"2. b", u"3. c"}));
}

TEST(S053_NumberedLists, RemovedMarkerRenumbersAcrossTheBlankLine) {
    const std::vector<std::u16string> lines = {u"1. a", u"", u"3. b"};
    EXPECT_EQ(applied(lines, renumberAfterReindent(over(lines), 1, 0, u'.', ReindentKind::Removed)),
              (std::vector<std::u16string>{u"1. a", u"", u"2. b"}));
}

TEST(S053_NumberedLists, ThemeTokensFallbackAndResolve) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& base = builtinTheme(scheme);
        const OrderedListStyle defaults = orderedListStyle(base);
        EXPECT_EQ(defaults.text, base.bullet);
        EXPECT_EQ(defaults.marker, base.marker);
        EXPECT_EQ(defaults.weight, 400);
        EXPECT_EQ(defaults.fontFamily, base.editorFontFamily);
    }
    const auto parsed = parseTheme(
        "[ordered-list]\n"
        "text = \"#112233\"\n"
        "marker = \"#445566\"\n"
        "weight = 700\n"
        "font-family = \"Mono Test\"\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    EXPECT_TRUE(parsed.diagnostics.empty());
    const OrderedListStyle custom = orderedListStyle(*parsed.theme);
    EXPECT_EQ(custom.text.toHex(), "#112233");
    EXPECT_EQ(custom.marker.toHex(), "#445566");
    EXPECT_EQ(custom.weight, 700);
    EXPECT_EQ(custom.fontFamily, "Mono Test");
}
