// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullets render as dots through the same element mechanism as headings.
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}
LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}

// A test-only element: proves that a new element is added without touching existing ones.
class ShoutElement final : public LineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "shout"; }
    [[nodiscard]] std::optional<LineMatch> parse(std::u16string_view line) const override {
        if (line.size() >= 2 && line.substr(0, 2) == u"! ") {
            return LineMatch{{0, 2}, 0, 0};
        }
        return std::nullopt;
    }
    void render(std::u16string_view, const LineMatch& m, const LineContext&, LineRender& out) const override {
        out.hidden.push_back({m.marker, HideMode::Collapse});
    }
};
}  // namespace

TEST(S003_Bullets, DotReplacesMarkerWhenCursorElsewhere) {
    const auto r = show(u"- milk");
    EXPECT_EQ(r.headingLevel, 0);
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::KeepSpace}}));
    EXPECT_EQ(r.decorations, (std::vector<Decoration>{{DecorationKind::Bullet, 0, 0}}));
    EXPECT_TRUE(r.runs.empty());
}

TEST(S003_Bullets, RawMarkerOnCursorLine) {
    const auto r = show(u"- milk", true);
    EXPECT_TRUE(r.hidden.empty());
    EXPECT_TRUE(r.decorations.empty());
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{0, 1}, Role::Marker, 0}}));
}

TEST(S003_Bullets, AllThreeMarkers) {
    for (std::u16string_view line : {u"- a", u"* a", u"+ a", u"-", u"-\ta"}) {
        EXPECT_EQ(show(line).decorations.size(), 1u) << std::string(line.begin(), line.end());
    }
}

TEST(S003_Bullets, IndentationIsNestingDepth) {
    EXPECT_EQ(show(u"  - a").decorations.at(0), (Decoration{DecorationKind::Bullet, 2, 1}));
    EXPECT_EQ(show(u"    * a").decorations.at(0), (Decoration{DecorationKind::Bullet, 4, 2}));
    EXPECT_EQ(show(u"\t+ a").decorations.at(0), (Decoration{DecorationKind::Bullet, 1, 2}));
}

TEST(S003_Bullets, WrapIndentPointsToContentStart) {
    EXPECT_EQ(show(u"- milk").wrapIndent, 2u);
    EXPECT_EQ(show(u"- milk", true).wrapIndent, 2u);
    EXPECT_EQ(show(u"  - milk").wrapIndent, 4u);
    EXPECT_EQ(show(u"    * item").wrapIndent, 6u);
    EXPECT_EQ(show(u"-   three spaces").wrapIndent, 4u);
    EXPECT_EQ(show(u"plain text").wrapIndent, 0u);
    EXPECT_EQ(show(u"# Heading").wrapIndent, 0u);
}

TEST(S003_Bullets, ParseBulletLineIdentifiesPrefixAndEmptyState) {
    const auto item1 = parseBulletLine(u"- milk");
    ASSERT_TRUE(item1.has_value());
    EXPECT_EQ(*item1, (BulletLineInfo{2, 0, u'-', false}));

    const auto item2 = parseBulletLine(u"  * foo bar");
    ASSERT_TRUE(item2.has_value());
    EXPECT_EQ(*item2, (BulletLineInfo{4, 2, u'*', false}));

    const auto empty1 = parseBulletLine(u"- ");
    ASSERT_TRUE(empty1.has_value());
    EXPECT_EQ(*empty1, (BulletLineInfo{2, 0, u'-', true}));

    const auto empty2 = parseBulletLine(u"    +   ");
    ASSERT_TRUE(empty2.has_value());
    EXPECT_EQ(empty2->markerChar, 4u);
    EXPECT_EQ(empty2->marker, u'+');
    EXPECT_TRUE(empty2->isEmpty);

    EXPECT_FALSE(parseBulletLine(u"plain text").has_value());
    EXPECT_FALSE(parseBulletLine(u"# Heading").has_value());
    EXPECT_FALSE(parseBulletLine(u"---").has_value());
}

TEST(S003_Bullets, IndentAndOutdentBulletLine) {
    EXPECT_EQ(indentBulletLine(u"- milk"), u"  - milk");
    EXPECT_EQ(indentBulletLine(u"  * milk"), u"    * milk");

    EXPECT_EQ(outdentBulletLine(u"    * milk"), u"  * milk");
    EXPECT_EQ(outdentBulletLine(u"  - milk"), u"- milk");
    EXPECT_EQ(outdentBulletLine(u"- milk"), u"- milk");
    EXPECT_EQ(outdentBulletLine(u"\t+ milk"), u"+ milk");
    EXPECT_EQ(outdentBulletLine(u" + milk"), u"+ milk");
}

TEST(S003_Bullets, NonItemsArePlainText) {
    for (std::u16string_view line : {u"-x", u"--", u"---", u"- - -", u"* * *", u"***", u"___", u"a - b", u"+1"}) {
        EXPECT_TRUE(show(line).empty()) << std::string(line.begin(), line.end());
    }
}

TEST(S003_Bullets, HeadingAndBulletShareTheRegistry) {
    EXPECT_EQ(ElementRegistry::withBuiltins().size(), 2u);
    EXPECT_EQ(show(u"# - not a bullet").headingLevel, 1);
    EXPECT_TRUE(show(u"# - not a bullet").decorations.empty());
}

TEST(S003_Bullets, NewElementIsAddedByRegistrationOnly) {
    ElementRegistry r = ElementRegistry::withBuiltins();
    r.add(std::make_unique<ShoutElement>());
    EXPECT_EQ(r.render(u"! hey", {false}).hidden.at(0).span, (Span{0, 2}));
    EXPECT_EQ(r.render(u"- milk", {false}).decorations.size(), 1u) << "existing elements unaffected";
}
