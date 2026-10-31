// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullets render as dots through the same element mechanism as headings.
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

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

/// ASCII text of a line for failure messages; other characters as '?'.
std::string narrow(std::u16string_view text) {
    std::string out;
    for (const char16_t c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}
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
        EXPECT_EQ(show(line).decorations.size(), 1u) << narrow(line);
    }
}

TEST(S003_Bullets, IndentationIsNestingDepth) {
    EXPECT_EQ(show(u"  - a").decorations.at(0), (Decoration{DecorationKind::Bullet, 2, 1}));
    EXPECT_EQ(show(u"    * a").decorations.at(0), (Decoration{DecorationKind::Bullet, 4, 2}));
    EXPECT_EQ(show(u"\t+ a").decorations.at(0), (Decoration{DecorationKind::Bullet, 1, 2}));
}

TEST(S003_Bullets, NonItemsArePlainText) {
    for (std::u16string_view line : {u"-x", u"--", u"a - b", u"+1"}) {
        EXPECT_TRUE(show(line).empty()) << narrow(line);
    }
    // Thematic breaks are no items; since S023 they render as a horizontal line.
    for (std::u16string_view line : {u"---", u"- - -", u"* * *", u"***", u"___"}) {
        EXPECT_TRUE(show(line).decorations.empty()) << narrow(line);
        EXPECT_EQ(show(line).headingLevel, 0) << narrow(line);
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
