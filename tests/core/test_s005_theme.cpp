// SPDX-License-Identifier: GPL-3.0-or-later
// S005: theme files.
#include "hitokoto/core/theme.hpp"
#include "hitokoto/test/docs_files.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

using namespace hitokoto::core;

TEST(S005_Theme, BuiltinThemesAreCompleteAndDiffer) {
    for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const auto r = parseTheme(builtinThemeSource(scheme), Theme{}, true);
        // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
        if (!r.theme) {
            FAIL() << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
        }
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
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!r.theme) {
        FAIL() << (r.diagnostics.empty() ? "" : r.diagnostics.front().toString());
    }
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
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!r.theme) {
        FAIL() << "expected a theme";
    }
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
    std::ifstream in(test::docsFile<"theming.md">(), std::ios::binary);
    ASSERT_TRUE(in) << test::docsFile<"theming.md">() << " not found";
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

namespace {

std::string themingDoc() {
    std::ifstream in(test::docsFile<"theming.md">(), std::ios::binary);
    std::string doc{std::istreambuf_iterator<char>(in), {}};
    std::erase(doc, '\r');  // Windows checkouts use CRLF
    return doc;
}

std::string tokenName(const ThemeTokenInfo& t) {
    return t.section.empty() ? std::string(t.key) : std::string(t.section) + "." + std::string(t.key);
}

bool isTokenName(std::string_view text) {
    for (const auto& t : themeTokens()) {
        if (tokenName(t) == text) {
            return true;
        }
    }
    return false;
}

// A row of the token tables in docs/theming.md, written from the token list: a type with "|"
// says "or" (a pipe would end the cell), a fallback that names a token is code.
std::string tokenRow(const ThemeTokenInfo& t) {
    std::string type(t.type);
    for (std::size_t at = type.find(" | "); at != std::string::npos; at = type.find(" | ", at)) {
        type.replace(at, 3, " or ");
    }
    std::string row = "| `" + tokenName(t) + "` | " + type + " | " + std::string(t.description) + " |";
    if (!t.fallback.empty()) {
        const std::string fallback(t.fallback);
        row += " " + (isTokenName(fallback) ? "`" + fallback + "`" : fallback) + " |";
    }
    return row + "\n";
}

// The rows of the table under `header` in `doc`, or "" if there is no such table.
std::string tableRows(const std::string& doc, const std::string& header) {
    const auto start = doc.find(header);
    if (start == std::string::npos) {
        return {};
    }
    std::string rows;
    for (auto at = start + header.size(); at < doc.size() && doc[at] == '|';) {
        const auto end = doc.find('\n', at);
        const auto next = end == std::string::npos ? doc.size() : end + 1;
        rows += doc.substr(at, next - at);
        at = next;
    }
    return rows;
}

}  // namespace

// docs/theming.md holds the two token tables exactly as the token list makes them: the general
// tokens, then the element tokens with their fallbacks, in the order of themeTokens().
TEST(S005_Theme, TokenTablesAreTheTokenList) {
    const std::string doc = themingDoc();
    ASSERT_FALSE(doc.empty()) << test::docsFile<"theming.md">() << " not found";
    std::string general;
    std::string elements;
    for (const auto& t : themeTokens()) {
        (t.fallback.empty() ? general : elements) += tokenRow(t);
    }
    EXPECT_EQ(tableRows(doc, "| Token | Type | Meaning |\n|---|---|---|\n"), general)
        << "the table of general tokens in " << test::docsFile<"theming.md">() << " should be:\n"
        << general;
    EXPECT_EQ(tableRows(doc, "| Token | Type | Meaning | Fallback |\n|---|---|---|---|\n"), elements)
        << "the table of element tokens in " << test::docsFile<"theming.md">() << " should be:\n"
        << elements;
}

namespace {

std::string shown(const Color& c) {
    return c.toHex();
}
std::string shown(const std::optional<Color>& c) {
    return c ? c->toHex() : "(none)";
}
std::string shown(double v) {
    std::ostringstream out;
    out << v;
    return out.str();
}
std::string shown(int v) {
    return std::to_string(v);
}
std::string shown(const std::string& v) {
    return v;
}

using Resolved = std::string (*)(const Theme&);

// Every element token as the program reads it (resolved), and the general tokens element tokens
// fall back on. An element token added to the theme gets a line here.
#define GENERAL_TOKEN(name, field)                          \
    {                                                       \
        name, [](const Theme& t) { return shown(t.field); } \
    }
#define ELEMENT_TOKEN(name, style, field)                          \
    {                                                              \
        name, [](const Theme& t) { return shown(style(t).field); } \
    }
const std::map<std::string, Resolved>& resolvedTokens() {
    static const std::map<std::string, Resolved> tokens{
        GENERAL_TOKEN("colors.primary", primary),
        GENERAL_TOKEN("colors.secondary", secondary),
        GENERAL_TOKEN("colors.accent", accent),
        GENERAL_TOKEN("colors.background", background),
        GENERAL_TOKEN("colors.surface", surface),
        GENERAL_TOKEN("colors.border", border),
        GENERAL_TOKEN("colors.text", text),
        GENERAL_TOKEN("colors.text-muted", textMuted),
        GENERAL_TOKEN("colors.selection", selection),
        GENERAL_TOKEN("colors.selection-text", selectionText),
        GENERAL_TOKEN("colors.marker", marker),
        GENERAL_TOKEN("colors.bullet", bullet),
        GENERAL_TOKEN("headings.weight", headingWeight),
        GENERAL_TOKEN("layout.radius", radius),
        GENERAL_TOKEN("layout.spacing", spacing),
        ELEMENT_TOKEN("code-block.background", codeBlockStyle, background),
        ELEMENT_TOKEN("code-block.text", codeBlockStyle, text),
        ELEMENT_TOKEN("code-block.border", codeBlockStyle, border),
        ELEMENT_TOKEN("code-block.border-width", codeBlockStyle, borderWidth),
        ELEMENT_TOKEN("code-block.radius", codeBlockStyle, radius),
        ELEMENT_TOKEN("code-block.padding", codeBlockStyle, padding),
        ELEMENT_TOKEN("code-block.margin", codeBlockStyle, margin),
        ELEMENT_TOKEN("code-block.font-family", codeBlockStyle, fontFamily),
        ELEMENT_TOKEN("code-block.font-scale", codeBlockStyle, fontScale),
        ELEMENT_TOKEN("code-block.marker", codeBlockStyle, marker),
        ELEMENT_TOKEN("code-block.badge-text", codeBlockStyle, badgeText),
        ELEMENT_TOKEN("code-block.badge-font-family", codeBlockStyle, badgeFontFamily),
        ELEMENT_TOKEN("code-block.badge-font-scale", codeBlockStyle, badgeFontScale),
        ELEMENT_TOKEN("inline-code.background", inlineCodeStyle, background),
        ELEMENT_TOKEN("inline-code.text", inlineCodeStyle, text),
        ELEMENT_TOKEN("inline-code.border", inlineCodeStyle, border),
        ELEMENT_TOKEN("inline-code.border-width", inlineCodeStyle, borderWidth),
        ELEMENT_TOKEN("inline-code.radius", inlineCodeStyle, radius),
        ELEMENT_TOKEN("inline-code.padding", inlineCodeStyle, padding),
        ELEMENT_TOKEN("inline-code.font-family", inlineCodeStyle, fontFamily),
        ELEMENT_TOKEN("inline-code.font-scale", inlineCodeStyle, fontScale),
        ELEMENT_TOKEN("inline-code.marker", inlineCodeStyle, marker),
        ELEMENT_TOKEN("emphasis.bold-weight", emphasisStyle, boldWeight),
        ELEMENT_TOKEN("emphasis.bold-text", emphasisStyle, boldText),
        ELEMENT_TOKEN("emphasis.italic-text", emphasisStyle, italicText),
        ELEMENT_TOKEN("emphasis.strikethrough-text", emphasisStyle, strikethroughText),
        ELEMENT_TOKEN("emphasis.marker", emphasisStyle, marker),
        ELEMENT_TOKEN("table.background", tableStyle, background),
        ELEMENT_TOKEN("table.header-background", tableStyle, headerBackground),
        ELEMENT_TOKEN("table.stripe-background", tableStyle, stripeBackground),
        ELEMENT_TOKEN("table.text", tableStyle, text),
        ELEMENT_TOKEN("table.header-text", tableStyle, headerText),
        ELEMENT_TOKEN("table.header-weight", tableStyle, headerWeight),
        ELEMENT_TOKEN("table.border", tableStyle, border),
        ELEMENT_TOKEN("table.border-width", tableStyle, borderWidth),
        ELEMENT_TOKEN("table.radius", tableStyle, radius),
        ELEMENT_TOKEN("table.padding", tableStyle, padding),
        ELEMENT_TOKEN("table.font-family", tableStyle, fontFamily),
        ELEMENT_TOKEN("table.font-scale", tableStyle, fontScale),
        ELEMENT_TOKEN("table.selection-background", tableStyle, selectionBackground),
        ELEMENT_TOKEN("table.selection-text", tableStyle, selectionText),
        ELEMENT_TOKEN("table.current-background", tableStyle, currentBackground),
        ELEMENT_TOKEN("table.select-pointer-fill", tableStyle, selectPointerFill),
        ELEMENT_TOKEN("table.select-pointer-outline", tableStyle, selectPointerOutline),
        ELEMENT_TOKEN("escape.marker", escapeStyle, marker),
        ELEMENT_TOKEN("link.text", linkStyle, text),
        ELEMENT_TOKEN("link.marker", linkStyle, marker),
        ELEMENT_TOKEN("math.text", mathStyle, text),
        ELEMENT_TOKEN("math.marker", mathStyle, marker),
        ELEMENT_TOKEN("math.error", mathStyle, error),
        ELEMENT_TOKEN("math.font-family", mathStyle, fontFamily),
        ELEMENT_TOKEN("math.font-scale", mathStyle, fontScale),
        ELEMENT_TOKEN("math.display-background", mathStyle, displayBackground),
        ELEMENT_TOKEN("math.display-border", mathStyle, displayBorder),
        ELEMENT_TOKEN("math.display-border-width", mathStyle, displayBorderWidth),
        ELEMENT_TOKEN("math.display-radius", mathStyle, displayRadius),
        ELEMENT_TOKEN("math.display-padding", mathStyle, displayPadding),
        ELEMENT_TOKEN("math.display-margin", mathStyle, displayMargin),
        ELEMENT_TOKEN("thematic-break.border", thematicBreakStyle, border),
        ELEMENT_TOKEN("thematic-break.border-width", thematicBreakStyle, borderWidth),
        ELEMENT_TOKEN("thematic-break.margin", thematicBreakStyle, margin),
        ELEMENT_TOKEN("thematic-break.marker", thematicBreakStyle, marker),
        ELEMENT_TOKEN("properties.background", propertiesStyle, background),
        ELEMENT_TOKEN("properties.text", propertiesStyle, text),
        ELEMENT_TOKEN("properties.key-text", propertiesStyle, keyText),
        ELEMENT_TOKEN("properties.title-text", propertiesStyle, titleText),
        ELEMENT_TOKEN("properties.icon", propertiesStyle, icon),
        ELEMENT_TOKEN("properties.unsupported-text", propertiesStyle, unsupportedText),
        ELEMENT_TOKEN("properties.badge-text", propertiesStyle, badgeText),
        ELEMENT_TOKEN("properties.badge-font-family", propertiesStyle, badgeFontFamily),
        ELEMENT_TOKEN("properties.badge-font-scale", propertiesStyle, badgeFontScale),
        ELEMENT_TOKEN("properties.checkbox", propertiesStyle, checkbox),
        ELEMENT_TOKEN("properties.checkbox-background", propertiesStyle, checkboxBackground),
        ELEMENT_TOKEN("properties.chip-background", propertiesStyle, chipBackground),
        ELEMENT_TOKEN("properties.chip-text", propertiesStyle, chipText),
        ELEMENT_TOKEN("properties.chip-border", propertiesStyle, chipBorder),
        ELEMENT_TOKEN("properties.chip-border-width", propertiesStyle, chipBorderWidth),
        ELEMENT_TOKEN("properties.chip-radius", propertiesStyle, chipRadius),
        ELEMENT_TOKEN("properties.chip-padding", propertiesStyle, chipPadding),
        ELEMENT_TOKEN("properties.editor-background", propertiesStyle, editorBackground),
        ELEMENT_TOKEN("properties.editor-text", propertiesStyle, editorText),
        ELEMENT_TOKEN("properties.editor-border", propertiesStyle, editorBorder),
        ELEMENT_TOKEN("properties.editor-focus-border", propertiesStyle, editorFocusBorder),
        ELEMENT_TOKEN("properties.editor-border-width", propertiesStyle, editorBorderWidth),
        ELEMENT_TOKEN("properties.editor-radius", propertiesStyle, editorRadius),
        ELEMENT_TOKEN("properties.editor-padding", propertiesStyle, editorPadding),
        ELEMENT_TOKEN("properties.border", propertiesStyle, border),
        ELEMENT_TOKEN("properties.border-width", propertiesStyle, borderWidth),
        ELEMENT_TOKEN("properties.radius", propertiesStyle, radius),
        ELEMENT_TOKEN("properties.padding", propertiesStyle, padding),
        ELEMENT_TOKEN("properties.margin", propertiesStyle, margin),
        ELEMENT_TOKEN("properties.row-spacing", propertiesStyle, rowSpacing),
        ELEMENT_TOKEN("properties.key-width", propertiesStyle, keyWidth),
        ELEMENT_TOKEN("properties.font-family", propertiesStyle, fontFamily),
        ELEMENT_TOKEN("properties.font-scale", propertiesStyle, fontScale),
        ELEMENT_TOKEN("toolbar.background", toolbarStyle, background),
        ELEMENT_TOKEN("toolbar.border", toolbarStyle, border),
        ELEMENT_TOKEN("toolbar.border-width", toolbarStyle, borderWidth),
        ELEMENT_TOKEN("toolbar.radius", toolbarStyle, radius),
        ELEMENT_TOKEN("toolbar.padding", toolbarStyle, padding),
        ELEMENT_TOKEN("toolbar.spacing", toolbarStyle, spacing),
        ELEMENT_TOKEN("toolbar.margin", toolbarStyle, margin),
        ELEMENT_TOKEN("toolbar.icon", toolbarStyle, icon),
        ELEMENT_TOKEN("toolbar.button-size", toolbarStyle, buttonSize),
        ELEMENT_TOKEN("toolbar.button-radius", toolbarStyle, buttonRadius),
        ELEMENT_TOKEN("toolbar.hover-background", toolbarStyle, hoverBackground),
        ELEMENT_TOKEN("toolbar.active-background", toolbarStyle, activeBackground),
        ELEMENT_TOKEN("toolbar.active-icon", toolbarStyle, activeIcon),
        ELEMENT_TOKEN("find.background", findStyle, background),
        ELEMENT_TOKEN("find.border", findStyle, border),
        ELEMENT_TOKEN("find.border-width", findStyle, borderWidth),
        ELEMENT_TOKEN("find.padding", findStyle, padding),
        ELEMENT_TOKEN("find.spacing", findStyle, spacing),
        ELEMENT_TOKEN("find.text", findStyle, text),
        ELEMENT_TOKEN("find.font-family", findStyle, fontFamily),
        ELEMENT_TOKEN("find.font-scale", findStyle, fontScale),
        ELEMENT_TOKEN("find.field-background", findStyle, fieldBackground),
        ELEMENT_TOKEN("find.field-text", findStyle, fieldText),
        ELEMENT_TOKEN("find.field-border", findStyle, fieldBorder),
        ELEMENT_TOKEN("find.field-focus-border", findStyle, fieldFocusBorder),
        ELEMENT_TOKEN("find.field-border-width", findStyle, fieldBorderWidth),
        ELEMENT_TOKEN("find.field-radius", findStyle, fieldRadius),
        ELEMENT_TOKEN("find.no-match-background", findStyle, noMatchBackground),
        ELEMENT_TOKEN("find.icon", findStyle, icon),
        ELEMENT_TOKEN("find.button-size", findStyle, buttonSize),
        ELEMENT_TOKEN("find.button-radius", findStyle, buttonRadius),
        ELEMENT_TOKEN("find.hover-background", findStyle, hoverBackground),
        ELEMENT_TOKEN("find.active-background", findStyle, activeBackground),
        ELEMENT_TOKEN("find.active-icon", findStyle, activeIcon),
        ELEMENT_TOKEN("find.current-background", findStyle, currentBackground),
        ELEMENT_TOKEN("find.current-text", findStyle, currentText),
        ELEMENT_TOKEN("find.match-background", findStyle, matchBackground),
        ELEMENT_TOKEN("find.match-text", findStyle, matchText),
        ELEMENT_TOKEN("menu-bar.background", menuBarStyle, background),
        ELEMENT_TOKEN("menu-bar.border", menuBarStyle, border),
        ELEMENT_TOKEN("menu-bar.border-width", menuBarStyle, borderWidth),
        ELEMENT_TOKEN("menu-bar.padding", menuBarStyle, padding),
        ELEMENT_TOKEN("menu-bar.spacing", menuBarStyle, spacing),
        ELEMENT_TOKEN("menu-bar.text", menuBarStyle, text),
        ELEMENT_TOKEN("menu-bar.hover-text", menuBarStyle, hoverText),
        ELEMENT_TOKEN("menu-bar.active-text", menuBarStyle, activeText),
        ELEMENT_TOKEN("menu-bar.hover-background", menuBarStyle, hoverBackground),
        ELEMENT_TOKEN("menu-bar.active-background", menuBarStyle, activeBackground),
        ELEMENT_TOKEN("menu-bar.mnemonic", menuBarStyle, mnemonic),
        ELEMENT_TOKEN("menu-bar.item-radius", menuBarStyle, itemRadius),
        ELEMENT_TOKEN("menu-bar.item-padding-horizontal", menuBarStyle, itemPaddingHorizontal),
        ELEMENT_TOKEN("menu-bar.item-padding-vertical", menuBarStyle, itemPaddingVertical),
        ELEMENT_TOKEN("menu-bar.font-family", menuBarStyle, fontFamily),
        ELEMENT_TOKEN("menu-bar.font-scale", menuBarStyle, fontScale),
        ELEMENT_TOKEN("switch.track", switchStyle, track),
        ELEMENT_TOKEN("switch.checked-track", switchStyle, checkedTrack),
        ELEMENT_TOKEN("switch.knob", switchStyle, knob),
        ELEMENT_TOKEN("switch.checked-knob", switchStyle, checkedKnob),
        ELEMENT_TOKEN("switch.border", switchStyle, border),
        ELEMENT_TOKEN("switch.border-width", switchStyle, borderWidth),
        ELEMENT_TOKEN("switch.width", switchStyle, width),
        ELEMENT_TOKEN("switch.height", switchStyle, height),
        ELEMENT_TOKEN("switch.radius", switchStyle, radius),
        ELEMENT_TOKEN("switch.knob-margin", switchStyle, knobMargin),
        ELEMENT_TOKEN("settings-section.text", settingsSectionStyle, text),
        ELEMENT_TOKEN("settings-section.weight", settingsSectionStyle, weight),
        ELEMENT_TOKEN("settings-section.rule", settingsSectionStyle, rule),
        ELEMENT_TOKEN("settings-section.rule-width", settingsSectionStyle, ruleWidth),
        ELEMENT_TOKEN("settings-section.rule-gap", settingsSectionStyle, ruleGap),
    };
    return tokens;
}
#undef GENERAL_TOKEN
#undef ELEMENT_TOKEN

const ThemeTokenInfo* tokenInfo(std::string_view name) {
    for (const auto& t : themeTokens()) {
        if (tokenName(t) == name) {
            return &t;
        }
    }
    return nullptr;
}

// A value for `name` that a theme file may set and that differs from both built-in themes.
std::string probeValue(const ThemeTokenInfo& t) {
    if (t.type == "color") {
        return "\"#13579b\"";
    }
    if (t.type == "string") {
        return "\"Probe Sans\"";
    }
    return t.key.ends_with("weight") ? "300" : "13";
}

}  // namespace

// Each element token the theme leaves unset shows its documented fallback: the token it names
// (set here to a value of its own), the number or text it gives, or the colours of the built-in
// themes it lists.
TEST(S005_Theme, UnsetElementTokensShowTheirDocumentedFallback) {
    const auto& resolved = resolvedTokens();
    const Theme& light = builtinTheme(ColorScheme::Light);
    const Theme& dark = builtinTheme(ColorScheme::Dark);
    for (const auto& t : themeTokens()) {
        if (t.fallback.empty()) {
            continue;
        }
        const std::string name = tokenName(t);
        SCOPED_TRACE(name + " falls back on " + std::string(t.fallback));
        const auto element = resolved.find(name);
        ASSERT_NE(element, resolved.end()) << "add the token to resolvedTokens()";
        const std::string fallback(t.fallback);
        if (const ThemeTokenInfo* source = tokenInfo(fallback)) {
            const auto other = resolved.find(fallback);
            ASSERT_NE(other, resolved.end()) << "add the fallback to resolvedTokens()";
            const std::string file = "[" + std::string(source->section) + "]\n" + std::string(source->key) + " = " +
                                     probeValue(*source) + "\n";
            const auto r = loadTheme(file, ColorScheme::Light);
            ASSERT_FALSE(r.usedFallback) << file;
            ASSERT_NE(other->second(r.theme), other->second(light)) << "the probe value is the built-in one";
            EXPECT_EQ(element->second(r.theme), other->second(r.theme));
        } else if (const auto alpha = fallback.find(" at an alpha of "); alpha != std::string::npos) {
            // "<colour token> at an alpha of <n> of 255" (S040): that colour, opaque in the built-in
            // themes, with the alpha appended as two hex digits.
            const auto base = resolved.find(fallback.substr(0, alpha));
            ASSERT_NE(base, resolved.end()) << "add the fallback to resolvedTokens()";
            const int value = std::stoi(fallback.substr(alpha + 16));
            const char digits[] = "0123456789abcdef";
            const std::string hex{digits[value / 16], digits[value % 16]};
            EXPECT_EQ(element->second(light), base->second(light) + hex);
            EXPECT_EQ(element->second(dark), base->second(dark) + hex);
        } else if (fallback == "text colour of the surrounding element") {
            EXPECT_EQ(element->second(light), "(none)");
            EXPECT_EQ(element->second(dark), "(none)");
        } else if (fallback == "\"\"") {
            EXPECT_EQ(element->second(light), "");
            EXPECT_EQ(element->second(dark), "");
        } else if (const auto split = fallback.find(" (light), "); split != std::string::npos) {
            EXPECT_EQ(element->second(light) + " (light), " + element->second(dark) + " (dark)", fallback);
        } else {
            EXPECT_EQ(element->second(light), fallback);
            EXPECT_EQ(element->second(dark), fallback);
        }
    }
}

TEST(S005_Theme, ShippedThemesParseWithoutDiagnostics) {
    const std::filesystem::path themesDir = std::filesystem::path(HITOKOTO_SOURCE_DIR) / "themes";
    ASSERT_TRUE(std::filesystem::exists(themesDir));
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(themesDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".toml") {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        ASSERT_TRUE(in) << entry.path().string();
        const std::string src{std::istreambuf_iterator<char>(in), {}};
        const auto r = loadTheme(src, ColorScheme::Light);
        EXPECT_FALSE(r.usedFallback) << entry.path().filename().string()
                                     << (r.diagnostics.empty() ? "" : ": " + r.diagnostics.front().toString());
        EXPECT_TRUE(r.diagnostics.empty()) << entry.path().filename().string();
        EXPECT_FALSE(r.theme.name.empty()) << entry.path().filename().string();
        ++count;
    }
    EXPECT_GE(count, 6u);
}
