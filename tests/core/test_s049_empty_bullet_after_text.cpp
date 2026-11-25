// SPDX-License-Identifier: GPL-3.0-or-later
// S049: an empty bullet whose marker is followed by a blank (`- `, `* `, `+ `, what Enter and Tab
// produce) is an empty list item even directly after paragraph text, and `- ` underlines nothing.
// A deliberate deviation from CommonMark 0.31.2, 4.3 and 5.2: otherwise indenting a new bullet
// under a bullet with text turns that text into a level-2 heading while the user types.
#include "hitokoto/core/markdown.hpp"

#include "support.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
using hitokoto::test::narrow;
using hitokoto::test::registry;

std::vector<LineRender> showAll(const std::vector<std::u16string>& lines, std::int64_t revealed = -1) {
    return registry().renderDocument(lines, revealed < 0 ? LineRange{} : LineRange{revealed, revealed});
}

std::string context(const std::vector<std::u16string>& lines) {
    std::string out;
    for (const auto& line : lines) {
        out += narrow(line) + "\\n";
    }
    return out;
}
}  // namespace

// The reported case: Tab on a new bullet under `  - second bullet`, caret on the new bullet.
TEST(S049_EmptyBulletAfterText, IndentedEmptyBulletDoesNotUnderlineTheItemAbove) {
    const std::vector<std::u16string> lines = {u"# Title", u"", u"- first bullet", u"  - second bullet", u"    - "};
    for (const std::int64_t caret : {std::int64_t{4}, std::int64_t{-1}}) {
        const auto r = showAll(lines, caret);
        EXPECT_EQ(r[3].headingLevel, 0) << "caret " << caret << ": the item above stays a bullet";
        EXPECT_EQ(r[3].decorations.size(), 1U) << "caret " << caret;
        EXPECT_EQ(r[4].setextUnderline, 0) << "caret " << caret;
        if (caret < 0) {
            EXPECT_EQ(r[4].decorations.size(), 1U) << "concealed: the empty item shows its dot";
        }
    }
}

TEST(S049_EmptyBulletAfterText, EveryMarkerWithABlankStartsAnItemAfterText) {
    for (const std::vector<std::u16string>& lines : std::vector<std::vector<std::u16string>>{
             {u"Foo", u"- "},
             {u"Foo", u"* "},
             {u"Foo", u"+\t"},
             {u"- foo", u"  - "},
             {u"  - second", u"    *   "},
         }) {
        const auto r = showAll(lines);
        EXPECT_EQ(r[0].headingLevel, 0) << context(lines);
        EXPECT_EQ(r[1].setextUnderline, 0) << context(lines);
        EXPECT_EQ(r[1].decorations.size(), 1U) << context(lines) << ": an empty item";
    }
}

// A line of text after the empty item is no paragraph text of the item above: `---` under it is a
// break, not an underline of `Foo`.
TEST(S049_EmptyBulletAfterText, TheEmptyItemEndsTheParagraph) {
    const auto r = showAll({u"Foo", u"- ", u"---"});
    EXPECT_EQ(r[0].headingLevel, 0);
    EXPECT_TRUE(r[2].thematicBreak);
}

// Without a blank after the marker CommonMark still applies (S023).
TEST(S049_EmptyBulletAfterText, BareMarkerKeepsCommonMark) {
    const auto dash = showAll({u"Foo", u"-"});
    EXPECT_EQ(dash[0].headingLevel, 2);
    EXPECT_EQ(dash[1].setextUnderline, 2);
    const auto nested = showAll({u"  - second", u"    -"});
    EXPECT_EQ(nested[0].headingLevel, 2);
    const auto star = showAll({u"Foo", u"*", u"---"});
    EXPECT_EQ(star[0].headingLevel, 2) << "`*` continues the paragraph";
    EXPECT_EQ(star[1].decorations.size(), 0U);
    // Two or more dashes with a trailing blank are an underline as before.
    EXPECT_EQ(showAll({u"Foo", u"-- "})[0].headingLevel, 2);
    EXPECT_EQ(showAll({u"Foo", u"=  "})[0].headingLevel, 1);
}
