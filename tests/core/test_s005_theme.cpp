// SPDX-License-Identifier: GPL-3.0-or-later
// S005: theme files.
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <random>

using namespace hitokoto::core;

TEST(S005_Theme, BuiltinThemesAreCompleteAndDiffer) {
    for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const auto r = parseTheme(builtinThemeSource(scheme), Theme{}, true);
        ASSERT_TRUE(r.theme) << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
        EXPECT_EQ(r.theme->scheme, scheme);
    }
    EXPECT_NE(builtinTheme(ColorScheme::Light).background, builtinTheme(ColorScheme::Dark).background);
    EXPECT_NE(builtinTheme(ColorScheme::Light).accent, builtinTheme(ColorScheme::Light).text)
        << "an accent colour, not grey";
}

TEST(S005_Theme, PartialThemeInheritsFromItsBase) {
    const auto r = loadTheme("base = \"dark\"\n[colors]\naccent = \"#ff8800\"\n", ColorScheme::Light);
    EXPECT_FALSE(r.usedFallback);
    EXPECT_EQ(r.theme.accent, (Color{0xff, 0x88, 0x00, 0xff}));
    EXPECT_EQ(r.theme.background, builtinTheme(ColorScheme::Dark).background);
    EXPECT_EQ(r.theme.scheme, ColorScheme::Dark);
}

TEST(S005_Theme, WithoutBaseTheSystemSchemeIsUsed) {
    const auto r = loadTheme("[fonts]\neditor-size = 15\n", ColorScheme::Dark);
    EXPECT_EQ(r.theme.background, builtinTheme(ColorScheme::Dark).background);
    EXPECT_EQ(r.theme.editorFontSize, 15);
}

TEST(S005_Theme, AllValueKinds) {
    const auto r = parseTheme(
        "name = \"Mine \\\"quoted\\\"\"  # comment\n"
        "[colors]\n"
        "accent = \"#abc\"\n"
        "selection = \"#11223344\"\n"
        "[fonts]\n"
        "editor-family = \"Iosevka\"\n"
        "editor-size = 13.5\n"
        "[headings]\n"
        "h1-scale = 2.25\n"
        "weight = 600\n"
        "[layout]\n"
        "editor-max-width = 0\n"
        "[buttons]\n"
        "style = \"outlined\"\n",
        builtinTheme(ColorScheme::Light));
    ASSERT_TRUE(r.theme) << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
    const Theme& t = *r.theme;
    EXPECT_EQ(t.name, "Mine \"quoted\"");
    EXPECT_EQ(t.accent, (Color{0xaa, 0xbb, 0xcc, 0xff}));
    EXPECT_EQ(t.selection, (Color{0x11, 0x22, 0x33, 0x44}));
    EXPECT_EQ(t.editorFontFamily, "Iosevka");
    EXPECT_DOUBLE_EQ(t.editorFontSize, 13.5);
    EXPECT_DOUBLE_EQ(t.headingScale[0], 2.25);
    EXPECT_EQ(t.headingWeight, 600);
    EXPECT_EQ(t.editorMaxWidth, 0);
    EXPECT_EQ(t.buttonStyle, ButtonStyle::Outlined);
}

TEST(S005_Theme, ErrorsCarryLineColumnAndReason) {
    const auto r = parseTheme("[colors]\naccent = \"#12\"\n[fonts]\neditor-size = 500\nbogus = 1\n[nope]\n",
                              builtinTheme(ColorScheme::Light));
    EXPECT_FALSE(r.theme);
    ASSERT_EQ(r.diagnostics.size(), 4u);
    EXPECT_EQ(r.diagnostics[0].line, 2u);
    EXPECT_EQ(r.diagnostics[0].column, 10u);
    EXPECT_NE(r.diagnostics[0].message.find("invalid colour"), std::string::npos);
    EXPECT_EQ(r.diagnostics[1].line, 4u);
    EXPECT_NE(r.diagnostics[1].message.find("out of range"), std::string::npos);
    EXPECT_NE(r.diagnostics[2].message.find("unknown key 'fonts.bogus'"), std::string::npos);
    EXPECT_NE(r.diagnostics[3].message.find("unknown section [nope]"), std::string::npos);
    EXPECT_EQ(r.diagnostics[0].toString().rfind("line 2, column 10: ", 0), 0u);
}

TEST(S005_Theme, SyntaxErrors) {
    const Theme& base = builtinTheme(ColorScheme::Light);
    EXPECT_FALSE(parseTheme("[colors\n", base).theme);
    EXPECT_FALSE(parseTheme("[colors]\naccent \"#fff\"\n", base).theme);
    EXPECT_FALSE(parseTheme("[colors]\naccent = \"#fff\n", base).theme);
    EXPECT_FALSE(parseTheme("[colors]\naccent = #fff\n", base).theme) << "colours need quotes";
    EXPECT_FALSE(parseTheme("[layout]\nradius = 4 px\n", base).theme);
    EXPECT_FALSE(parseTheme("[layout]\nradius = 4\nradius = 5\n", base).theme) << "duplicate";
    EXPECT_FALSE(parseTheme("Name = \"x\"\n", base).theme) << "keys are lower case";
}

TEST(S005_Theme, BrokenUserThemeFallsBackToBuiltin) {
    const auto r = loadTheme("[colors]\naccent = \"purple\"\n", ColorScheme::Dark);
    EXPECT_TRUE(r.usedFallback);
    EXPECT_EQ(r.theme, builtinTheme(ColorScheme::Dark));
    ASSERT_FALSE(r.diagnostics.empty());
}

TEST(S005_Theme, CrlfThemeFilesWork) {
    EXPECT_TRUE(parseTheme("[layout]\r\nradius = 3\r\n", builtinTheme(ColorScheme::Light)).theme);
}

TEST(S005_Theme, NumbersAreLocaleIndependent) {
    const auto r = parseTheme("[fonts]\neditor-size = 11.25\n", builtinTheme(ColorScheme::Light));
    ASSERT_TRUE(r.theme);
    EXPECT_DOUBLE_EQ(r.theme->editorFontSize, 11.25);
    EXPECT_FALSE(parseTheme("[fonts]\neditor-size = 11,25\n", builtinTheme(ColorScheme::Light)).theme);
}

TEST(S005_Theme, EveryTokenIsDocumented) {
    for (const auto& t : themeTokens()) {
        EXPECT_FALSE(t.key.empty());
        EXPECT_FALSE(t.type.empty());
        EXPECT_FALSE(t.description.empty()) << t.key;
    }
    EXPECT_GE(themeTokens().size(), 25u);
}

// ADR 0004: every token of an element section falls back to a documented value; the general
// tokens have no fallback (the built-in themes set all of them).
TEST(S005_Theme, ElementTokensDeclareFallbacks) {
    std::size_t elementTokens = 0;
    for (const auto& t : themeTokens()) {
        const bool general = t.section.empty() || t.section == "colors" || t.section == "fonts" ||
                             t.section == "headings" || t.section == "layout" || t.section == "buttons";
        if (general) {
            EXPECT_TRUE(t.fallback.empty()) << t.key;
        } else {
            EXPECT_FALSE(t.fallback.empty()) << t.section << "." << t.key;
            ++elementTokens;
        }
    }
    EXPECT_GE(elementTokens, 23u);
}

TEST(S005_Theme, ColorToHex) {
    EXPECT_EQ((Color{1, 2, 255, 255}).toHex(), "#0102ff");
    EXPECT_EQ((Color{1, 2, 255, 16}).toHex(), "#0102ff10");
}

// Property: arbitrary input never crashes the parser and either yields a theme or diagnostics.
TEST(S005_Theme, PropertyArbitraryInputIsSafe) {
    std::mt19937 rng(5);
    const std::string_view pieces[] = {"[colors]", "[fonts]", "accent",      "=",  " ", "\"", "#",  "#fff", "\n",
                                       "12",       "-3.5",    "editor-size", "\\", "]", "[",  "\r", "base", "\"dark\""};
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::string src;
        const int n = static_cast<int>(rng() % 16);
        for (int k = 0; k < n; ++k)
            src += pieces[rng() % std::size(pieces)];
        const auto r = loadTheme(src, ColorScheme::Light);
        ASSERT_EQ(r.usedFallback, !r.diagnostics.empty());
    }
}

// docs/theming.md documents every token, and its example themes are valid.
TEST(S005_Theme, DocumentationCoversAllTokensAndExamplesParse) {
    std::ifstream in(std::string(HITOKOTO_SOURCE_DIR) + "/docs/theming.md", std::ios::binary);
    ASSERT_TRUE(in) << "docs/theming.md not found";
    std::string doc{std::istreambuf_iterator<char>(in), {}};
    std::erase(doc, '\r');  // Windows checkouts use CRLF
    for (const auto& t : themeTokens()) {
        const std::string name =
            t.section.empty() ? std::string(t.key) : std::string(t.section) + "." + std::string(t.key);
        EXPECT_NE(doc.find("`" + name + "`"), std::string::npos) << name << " is not documented";
    }
    std::size_t examples = 0;
    for (std::size_t pos = doc.find("```toml\n"); pos != std::string::npos; pos = doc.find("```toml\n", pos)) {
        pos += 8;
        const auto end = doc.find("```", pos);
        ASSERT_NE(end, std::string::npos);
        const auto r = loadTheme(std::string_view(doc).substr(pos, end - pos), ColorScheme::Light);
        EXPECT_FALSE(r.usedFallback) << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
        ++examples;
        pos = end + 3;
    }
    EXPECT_GE(examples, 2u);
}
