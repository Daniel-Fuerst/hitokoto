// SPDX-License-Identifier: GPL-3.0-or-later
// S044: the section headings of the settings flyout, in the accent colour and followed by a line.
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

TEST(S044_SettingsSectionHeadings, TokensFallbackAndResolve) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& base = builtinTheme(scheme);
        const SettingsSectionStyle defaults = settingsSectionStyle(base);
        EXPECT_EQ(defaults.text, base.accent);
        EXPECT_EQ(defaults.weight, 600);
        EXPECT_EQ(defaults.rule, base.border);
        EXPECT_DOUBLE_EQ(defaults.ruleWidth, 1.0);
        EXPECT_DOUBLE_EQ(defaults.ruleGap, base.spacing);
    }

    const auto parsed = parseTheme(
        "[settings-section]\n"
        "text = \"#112233\"\n"
        "weight = 800\n"
        "rule = \"#445566\"\n"
        "rule-width = 2\n"
        "rule-gap = 12\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {
        FAIL() << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().toString());
    }
    EXPECT_TRUE(parsed.diagnostics.empty());
    const SettingsSectionStyle custom = settingsSectionStyle(*parsed.theme);
    EXPECT_EQ(custom.text.toHex(), "#112233");
    EXPECT_EQ(custom.weight, 800);
    EXPECT_EQ(custom.rule.toHex(), "#445566");
    EXPECT_DOUBLE_EQ(custom.ruleWidth, 2.0);
    EXPECT_DOUBLE_EQ(custom.ruleGap, 12.0);
}
