// SPDX-License-Identifier: GPL-3.0-or-later
// S026: floating toolbar theme tokens and table generation helper.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

TEST(S026_FloatingToolbar, MakeEmptyTableProducesValidGfmPipeTable) {
    EXPECT_EQ(makeEmptyTable(2, 1), u"|  |  |\n| --- | --- |\n|  |  |");
    EXPECT_EQ(makeEmptyTable(3, 2), u"|  |  |  |\n| --- | --- | --- |\n|  |  |  |\n|  |  |  |");
    EXPECT_EQ(makeEmptyTable(1, 0), u"|  |\n| --- |");

    const auto registry = ElementRegistry::withBuiltins();
    const std::vector<std::u16string> lines = {u"|  |  |", u"| --- | --- |", u"|  |  |"};
    const auto rendered = registry.renderDocument(lines);
    ASSERT_EQ(rendered.size(), 3u);
    EXPECT_TRUE(rendered[0].table);
    EXPECT_TRUE(rendered[0].tableHeader);
    EXPECT_EQ(rendered[0].cells.size(), 2u);
    EXPECT_TRUE(rendered[1].table);
    EXPECT_TRUE(rendered[1].tableDelimiter);
    EXPECT_TRUE(rendered[2].table);
    EXPECT_FALSE(rendered[2].tableHeader);
    EXPECT_EQ(rendered[2].cells.size(), 2u);
}

TEST(S026_FloatingToolbar, ToolbarTokensFallbackAndResolve) {
    Theme base = builtinTheme(ColorScheme::Dark);
    const ToolbarStyle defaults = toolbarStyle(base);
    EXPECT_EQ(defaults.background, base.surface);
    EXPECT_EQ(defaults.border, base.border);
    EXPECT_DOUBLE_EQ(defaults.borderWidth, 1.0);
    EXPECT_DOUBLE_EQ(defaults.radius, base.radius);
    EXPECT_DOUBLE_EQ(defaults.padding, 4.0);
    EXPECT_DOUBLE_EQ(defaults.spacing, 4.0);
    EXPECT_DOUBLE_EQ(defaults.margin, 12.0);
    EXPECT_EQ(defaults.icon, base.text);
    EXPECT_DOUBLE_EQ(defaults.buttonSize, 28.0);
    EXPECT_DOUBLE_EQ(defaults.buttonRadius, base.radius);
    EXPECT_EQ(defaults.hoverBackground, base.border);
    EXPECT_EQ(defaults.activeBackground, base.accent);
    EXPECT_EQ(defaults.activeIcon, base.background);

    const auto parsed = parseTheme(
        "[toolbar]\n"
        "background = \"#202225\"\n"
        "border = \"#303338\"\n"
        "border-width = 2\n"
        "radius = 10\n"
        "padding = 6\n"
        "spacing = 5\n"
        "margin = 16\n"
        "icon = \"#f0f0f0\"\n"
        "button-size = 32\n"
        "button-radius = 8\n"
        "hover-background = \"#383c42\"\n"
        "active-background = \"#1d9bf0\"\n"
        "active-icon = \"#ffffff\"\n",
        base);
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    const ToolbarStyle custom = toolbarStyle(*parsed.theme);
    EXPECT_EQ(custom.background, (Color{0x20, 0x22, 0x25, 0xff}));
    EXPECT_EQ(custom.border, (Color{0x30, 0x33, 0x38, 0xff}));
    EXPECT_DOUBLE_EQ(custom.borderWidth, 2.0);
    EXPECT_DOUBLE_EQ(custom.radius, 10.0);
    EXPECT_DOUBLE_EQ(custom.padding, 6.0);
    EXPECT_DOUBLE_EQ(custom.spacing, 5.0);
    EXPECT_DOUBLE_EQ(custom.margin, 16.0);
    EXPECT_EQ(custom.icon, (Color{0xf0, 0xf0, 0xf0, 0xff}));
    EXPECT_DOUBLE_EQ(custom.buttonSize, 32.0);
    EXPECT_DOUBLE_EQ(custom.buttonRadius, 8.0);
    EXPECT_EQ(custom.hoverBackground, (Color{0x38, 0x3c, 0x42, 0xff}));
    EXPECT_EQ(custom.activeBackground, (Color{0x1d, 0x9b, 0xf0, 0xff}));
    EXPECT_EQ(custom.activeIcon, (Color{0xff, 0xff, 0xff, 0xff}));
}
