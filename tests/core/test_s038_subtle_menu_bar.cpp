// SPDX-License-Identifier: GPL-3.0-or-later
// S038: subtle top menu bar theme tokens and resolution.
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

TEST(S038_SubtleMenuBar, MenuBarTokensFallbackAndResolve) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme base = builtinTheme(scheme);
        const MenuBarStyle defaults = menuBarStyle(base);
        EXPECT_EQ(defaults.background, base.background);
        EXPECT_EQ(defaults.border, base.border);
        EXPECT_DOUBLE_EQ(defaults.borderWidth, 0.0);
        EXPECT_DOUBLE_EQ(defaults.padding, 6.0);
        EXPECT_DOUBLE_EQ(defaults.spacing, 4.0);
        EXPECT_EQ(defaults.text, base.textMuted);
        EXPECT_EQ(defaults.hoverText, base.text);
        EXPECT_EQ(defaults.activeText, base.text);
        EXPECT_EQ(defaults.hoverBackground, base.surface);
        EXPECT_EQ(defaults.activeBackground, base.border);
        EXPECT_DOUBLE_EQ(defaults.itemRadius, base.radius);
        EXPECT_DOUBLE_EQ(defaults.itemPaddingHorizontal, base.spacing);
        EXPECT_DOUBLE_EQ(defaults.itemPaddingVertical, 4.0);
        EXPECT_EQ(defaults.fontFamily, "");
        EXPECT_DOUBLE_EQ(defaults.fontScale, 1.0);
    }

    const Theme base = builtinTheme(ColorScheme::Dark);
    const auto parsed = parseTheme(
        "[menu-bar]\n"
        "background = \"#18191c\"\n"
        "border = \"#2b2d31\"\n"
        "border-width = 2\n"
        "padding = 8\n"
        "spacing = 6\n"
        "text = \"#8e9297\"\n"
        "hover-text = \"#ffffff\"\n"
        "active-text = \"#e0e4ff\"\n"
        "hover-background = \"#2e3035\"\n"
        "active-background = \"#3b3f47\"\n"
        "item-radius = 10\n"
        "item-padding-horizontal = 12\n"
        "item-padding-vertical = 5\n"
        "font-family = \"Inter\"\n"
        "font-scale = 1.2\n",
        base);
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    const MenuBarStyle custom = menuBarStyle(*parsed.theme);
    EXPECT_EQ(custom.background, (Color{0x18, 0x19, 0x1c, 0xff}));
    EXPECT_EQ(custom.border, (Color{0x2b, 0x2d, 0x31, 0xff}));
    EXPECT_DOUBLE_EQ(custom.borderWidth, 2.0);
    EXPECT_DOUBLE_EQ(custom.padding, 8.0);
    EXPECT_DOUBLE_EQ(custom.spacing, 6.0);
    EXPECT_EQ(custom.text, (Color{0x8e, 0x92, 0x97, 0xff}));
    EXPECT_EQ(custom.hoverText, (Color{0xff, 0xff, 0xff, 0xff}));
    EXPECT_EQ(custom.activeText, (Color{0xe0, 0xe4, 0xff, 0xff}));
    EXPECT_EQ(custom.hoverBackground, (Color{0x2e, 0x30, 0x35, 0xff}));
    EXPECT_EQ(custom.activeBackground, (Color{0x3b, 0x3f, 0x47, 0xff}));
    EXPECT_DOUBLE_EQ(custom.itemRadius, 10.0);
    EXPECT_DOUBLE_EQ(custom.itemPaddingHorizontal, 12.0);
    EXPECT_DOUBLE_EQ(custom.itemPaddingVertical, 5.0);
    EXPECT_EQ(custom.fontFamily, "Inter");
    EXPECT_DOUBLE_EQ(custom.fontScale, 1.2);
}
