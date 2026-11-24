// SPDX-License-Identifier: GPL-3.0-or-later
// S047: a display math block is revealed as a whole: the caret on any of its lines shows the
// dollars of its `$$` lines, as code fences are (S021).
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/reveal.hpp"

#include "support.hpp"

#include <gtest/gtest.h>

#include <algorithm>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
using hitokoto::test::registry;

bool dollarsShown(const LineRender& render) {
    return render.mathDelimiter && render.hidden.empty() &&
           std::any_of(render.runs.begin(), render.runs.end(),
                       [](const StyleRun& r) { return r.role == Role::MathMarker; });
}
}  // namespace

TEST(S047_DisplayMath, TheCaretOnAFormulaLineRevealsBothDollarLines) {
    const std::vector<std::u16string> lines{u"above", u"$$", u"a = b", u"c = d", u"$$", u"below"};
    const auto shown = registry().renderDocument(lines, revealedLines(3, 3));
    EXPECT_TRUE(dollarsShown(shown[1]));
    EXPECT_TRUE(dollarsShown(shown[4]));
    const auto elsewhere = registry().renderDocument(lines, revealedLines(5, 5));
    EXPECT_FALSE(dollarsShown(elsewhere[1]));
    EXPECT_FALSE(dollarsShown(elsewhere[4]));
}

TEST(S047_DisplayMath, ASelectionIntoABlockRevealsItsDollarLines) {
    const std::vector<std::u16string> lines{u"above", u"$$", u"x", u"$$", u"below"};
    const auto shown = registry().renderDocument(lines, revealedLines(0, 2));
    EXPECT_TRUE(dollarsShown(shown[1]));
    EXPECT_TRUE(dollarsShown(shown[3]));
}

TEST(S047_DisplayMath, TextBesideTheDollarsIsRevealedToo) {
    const std::vector<std::u16string> lines{u"$$ \\begin{aligned}", u"a &= b", u"\\end{aligned} $$", u"x"};
    const auto shown = registry().renderDocument(lines, revealedLines(1, 1));
    EXPECT_TRUE(shown[0].hidden.empty());
    EXPECT_TRUE(shown[2].hidden.empty());
}
