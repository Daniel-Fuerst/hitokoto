// SPDX-License-Identifier: GPL-3.0-or-later
// S052: task list items (`- [ ]`, `- [x]`, `- [X]`, and `- []` as unchecked) show as checkboxes.
// The prefix up to the text is never shown, with or without the caret on the line; a click
// toggles the box (the UI writes what toggledTask() says), Backspace removes it.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/task.hpp"
#include "hitokoto/core/theme.hpp"

#include "support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
using hitokoto::test::narrow;
using hitokoto::test::registry;
using hitokoto::test::show;

const Decoration* checkbox(const LineRender& r) {
    const auto it = std::find_if(r.decorations.begin(), r.decorations.end(),
                                 [](const Decoration& d) { return d.kind == DecorationKind::Checkbox; });
    return it == r.decorations.end() ? nullptr : &*it;
}

/// Whether every character in [from, to) lies in a hidden range.
bool allHidden(const LineRender& r, std::uint32_t from, std::uint32_t to) {
    for (std::uint32_t i = from; i < to; ++i) {
        const bool hidden = std::any_of(r.hidden.begin(), r.hidden.end(),
                                        [i](const HiddenRange& h) { return i >= h.span.start && i < h.span.end(); });
        if (!hidden) {
            return false;
        }
    }
    return true;
}
}  // namespace

TEST(S052_TaskCheckboxes, RecognisesTaskItems) {
    struct Case {
        std::u16string line;
        bool checked;
        std::uint32_t markerChar;
        std::uint32_t box;
        std::uint32_t contentStart;
        std::uint8_t depth;
    };
    for (const Case& c : std::vector<Case>{
             {u"- [ ] milk", false, 0, 2, 6, 0},
             {u"- [x] bread", true, 0, 2, 6, 0},
             {u"* [X] eggs", true, 0, 2, 6, 0},
             {u"  + [] tea", false, 2, 4, 7, 1},
             {u"-   [ ]\tjam", false, 0, 4, 8, 0},
             {u"- [ ]  two blanks", false, 0, 2, 7, 0},
             {u"- [ ] ", false, 0, 2, 6, 0},
         }) {
        const auto info = parseTaskLine(c.line);
        ASSERT_TRUE(info.has_value()) << narrow(c.line);
        EXPECT_EQ(info->checked, c.checked) << narrow(c.line);
        EXPECT_EQ(info->markerChar, c.markerChar) << narrow(c.line);
        EXPECT_EQ(info->box, c.box) << narrow(c.line);
        EXPECT_EQ(info->contentStart, c.contentStart) << narrow(c.line);
        EXPECT_EQ(info->isEmpty, c.contentStart == c.line.size()) << narrow(c.line);

        const LineRender r = show(c.line);
        const Decoration* box = checkbox(r);
        ASSERT_NE(box, nullptr) << narrow(c.line);
        EXPECT_EQ(box->column, c.markerChar) << narrow(c.line);
        EXPECT_EQ(box->depth, c.depth) << narrow(c.line);
        EXPECT_EQ(box->checked, c.checked) << narrow(c.line);
        EXPECT_EQ(r.decorations.size(), 1U) << narrow(c.line) << ": no bullet dot beside the box";
    }
}

TEST(S052_TaskCheckboxes, NotTaskItems) {
    for (const std::u16string line : {u"- [ ]", u"- [x]foo", u"- [](url)", u"[ ] milk", u"-[ ] milk", u"- [y] milk",
                                      u"- [  ] milk", u"- x [ ] y"}) {
        EXPECT_FALSE(parseTaskLine(line).has_value()) << narrow(line);
        EXPECT_EQ(checkbox(show(line)), nullptr) << narrow(line);
    }
    // The bullets among them stay bullets (S003).
    EXPECT_EQ(show(u"- [ ]").decorations.size(), 1U);
    EXPECT_EQ(show(u"- [ ]").decorations[0].kind, DecorationKind::Bullet);
}

// Decision 2: the prefix is hidden whatever the reveal; the blank before the text takes the box's
// width (HideMode::Box), the rest no width at all.
TEST(S052_TaskCheckboxes, HidesThePrefixWhetherRevealedOrNot) {
    for (const bool revealed : {false, true}) {
        const LineRender r = show(u"  - [x] milk", revealed);
        ASSERT_NE(checkbox(r), nullptr) << "revealed " << revealed;
        EXPECT_TRUE(allHidden(r, 2, 8)) << "revealed " << revealed;
        EXPECT_FALSE(allHidden(r, 0, 1)) << "the indentation stays";
        EXPECT_FALSE(allHidden(r, 8, 9)) << "the text stays";
        const auto boxSpace = std::find_if(r.hidden.begin(), r.hidden.end(),
                                           [](const HiddenRange& h) { return h.mode == HideMode::Box; });
        ASSERT_NE(boxSpace, r.hidden.end());
        EXPECT_EQ(boxSpace->span, (Span{7, 1}));
        EXPECT_TRUE(std::none_of(r.runs.begin(), r.runs.end(), [](const StyleRun& run) {
            return run.role == Role::Marker;
        })) << "no marker is ever shown";
    }
    // The whole document reference agrees, with the caret on the line.
    const std::vector<std::u16string> lines{u"- [ ] milk", u"- [x] bread"};
    const auto doc = registry().renderDocument(lines, LineRange{0, 1});
    EXPECT_NE(checkbox(doc[0]), nullptr);
    EXPECT_NE(checkbox(doc[1]), nullptr);
}

TEST(S052_TaskCheckboxes, CheckedTextIsDone) {
    EXPECT_EQ(show(u"- [x] bread").doneText, (Span{6, 5}));
    EXPECT_EQ(show(u"- [X] bread", true).doneText, (Span{6, 5}));
    EXPECT_EQ(show(u"- [ ] milk").doneText, (Span{}));
    EXPECT_EQ(show(u"- [x] ").doneText, (Span{}));
    // Inline elements still run on the text.
    const LineRender r = show(u"- [x] **bold**");
    EXPECT_FALSE(r.inlineStyles.empty());
}

TEST(S052_TaskCheckboxes, WrapIndentAfterTheBox) {
    EXPECT_EQ(show(u"- [ ] milk").wrapIndent, 6U);
    EXPECT_EQ(show(u"  * [] tea", true).wrapIndent, 7U);
}

// Decision 8: as a bullet with text, also directly after paragraph text and when empty.
TEST(S052_TaskCheckboxes, ParagraphRulesAsBullets) {
    const std::vector<std::u16string> lines{u"Foo", u"- [ ] ", u"- [x] bar"};
    const auto r = registry().renderDocument(lines);
    EXPECT_EQ(r[0].headingLevel, 0);
    EXPECT_NE(checkbox(r[1]), nullptr);
    EXPECT_NE(checkbox(r[2]), nullptr);
}

TEST(S052_TaskCheckboxes, ToggleWritesTheOtherState) {
    struct Case {
        std::u16string line;
        Span span;
        std::u16string text;
    };
    for (const Case& c : std::vector<Case>{
             {u"- [ ] milk", {2, 3}, u"[x]"},
             {u"- [x] bread", {2, 3}, u"[ ]"},
             {u"  * [X] eggs", {4, 3}, u"[ ]"},
             {u"+ [] tea", {2, 2}, u"[x]"},
         }) {
        const auto edit = toggledTask(c.line);
        ASSERT_TRUE(edit.has_value()) << narrow(c.line);
        EXPECT_EQ(edit->span, c.span) << narrow(c.line);
        EXPECT_EQ(edit->text, c.text) << narrow(c.line);
    }
    EXPECT_FALSE(toggledTask(u"- milk").has_value());
}

// Decision 4: Backspace at the start of the text removes the box and the blanks after it.
TEST(S052_TaskCheckboxes, RemoveBoxLeavesABullet) {
    const std::u16string line = u"  - [x]  milk";
    const auto info = parseTaskLine(line);
    ASSERT_TRUE(info.has_value());
    const Span removed = info->boxWithBlanks();
    EXPECT_EQ(removed, (Span{4, 5}));
    std::u16string after = line;
    after.erase(removed.start, removed.length);
    EXPECT_EQ(after, u"  - milk");
    EXPECT_EQ(show(after).decorations.at(0).kind, DecorationKind::Bullet);
}

// Decision 6: Enter continues with an unchecked box of the same indentation and marker.
TEST(S052_TaskCheckboxes, ContinuedPrefixIsUnchecked) {
    EXPECT_EQ(continuedTaskPrefix(u"  * [x] done"), u"  * [ ] ");
    EXPECT_EQ(continuedTaskPrefix(u"-   [] tea"), u"-   [ ] ");
    EXPECT_EQ(continuedTaskPrefix(u"- milk"), u"");
}

TEST(S052_TaskCheckboxes, ThemeTokensFallbackAndResolve) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& base = builtinTheme(scheme);
        const TaskStyle d = taskStyle(base);
        EXPECT_EQ(d.background, base.background);
        EXPECT_EQ(d.border, base.textMuted);
        EXPECT_EQ(d.checkedBackground, base.accent);
        EXPECT_EQ(d.checkedBorder, base.accent);
        EXPECT_EQ(d.check, base.background);
        EXPECT_EQ(d.doneText, base.textMuted);
        EXPECT_DOUBLE_EQ(d.borderWidth, 1.5);
        EXPECT_DOUBLE_EQ(d.radius, 3.0);
        EXPECT_DOUBLE_EQ(d.scale, 0.9);
        EXPECT_DOUBLE_EQ(d.gap, 6.0);
        EXPECT_TRUE(d.doneStrikethrough);
    }
    const auto parsed = parseTheme(
        "[task]\n"
        "background = \"#101010\"\n"
        "border = \"#202020\"\n"
        "checked-background = \"#303030\"\n"
        "check = \"#404040\"\n"
        "done-text = \"#505050\"\n"
        "border-width = 2\n"
        "radius = 4\n"
        "scale = 1.2\n"
        "gap = 10\n"
        "done-strikethrough = 0\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    const TaskStyle c = taskStyle(*parsed.theme);
    EXPECT_EQ(c.background.toHex(), "#101010");
    EXPECT_EQ(c.border.toHex(), "#202020");
    EXPECT_EQ(c.checkedBackground.toHex(), "#303030");
    EXPECT_EQ(c.checkedBorder.toHex(), "#303030") << "checked-border falls back to checked-background";
    EXPECT_EQ(c.check.toHex(), "#404040");
    EXPECT_EQ(c.doneText.toHex(), "#505050");
    EXPECT_DOUBLE_EQ(c.borderWidth, 2.0);
    EXPECT_DOUBLE_EQ(c.radius, 4.0);
    EXPECT_DOUBLE_EQ(c.scale, 1.2);
    EXPECT_DOUBLE_EQ(c.gap, 10.0);
    EXPECT_FALSE(c.doneStrikethrough);
}
