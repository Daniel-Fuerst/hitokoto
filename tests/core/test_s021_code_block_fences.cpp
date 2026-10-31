// SPDX-License-Identifier: GPL-3.0-or-later
// S021: code block fences inside the panel - the language of an opening fence and the tokens of
// the panel margin and the language badge.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::u16string languageOf(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed, {}}).codeLanguage;
}
}  // namespace

// CommonMark 4.5: the first word of the info string is the language. Expected values checked
// with commonmark.js 0.31.2 and markdown-it-py (commonmark), which emit class="language-...".
TEST(S021_CodeBlockFences, LanguageIsTheFirstWordOfTheInfoString) {
    EXPECT_EQ(languageOf(u"```python"), u"python");
    EXPECT_EQ(languageOf(u"```python title=\"a b\""), u"python");
    EXPECT_EQ(languageOf(u"```  c++  "), u"c++");
    EXPECT_EQ(languageOf(u"~~~ java\tstrict"), u"java");
    EXPECT_EQ(languageOf(u"~~~ a`b c"), u"a`b");
    EXPECT_EQ(languageOf(u"```{python}"), u"{python}");
    EXPECT_EQ(languageOf(u"  ```rust"), u"rust");
    EXPECT_EQ(languageOf(u"```python", true), u"python");
}

TEST(S021_CodeBlockFences, LanguageHasBackslashEscapesResolved) {
    EXPECT_EQ(languageOf(u"```c\\+\\+"), u"c++");
    EXPECT_EQ(languageOf(u"```a\\b"), u"a\\b") << "a backslash before a letter is no escape";
}

TEST(S021_CodeBlockFences, EntityReferencesStayAsWritten) {
    // commonmark.js decodes them (language-öl); the badge shows them as written (story S021).
    EXPECT_EQ(languageOf(u"```&ouml;l"), u"&ouml;l");
}

TEST(S021_CodeBlockFences, NoLanguageWithoutInfoStringOrOutsideOpeningFences) {
    EXPECT_EQ(languageOf(u"```"), u"");
    EXPECT_EQ(languageOf(u"```   "), u"");
    EXPECT_EQ(languageOf(u"python"), u"");
    const BlockState open = registry().blockStateAfter(u"```python", {});
    EXPECT_EQ(registry().render(u"code line", {false, open}).codeLanguage, u"");
    EXPECT_EQ(registry().render(u"```", {false, open}).codeLanguage, u"") << "closing fence";
}

// Theming baseline (ADR 0004).
TEST(S021_CodeBlockFences, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[code-block]\nmargin = 6\nbadge-text = \"#0d0e0f\"\nbadge-font-family = \"Badge Sans\"\n"
        "badge-font-scale = 0.8\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << r.diagnostics.front().toString();
    const CodeStyle s = codeBlockStyle(r.theme);
    EXPECT_DOUBLE_EQ(s.margin, 6);
    EXPECT_EQ(s.badgeText, (Color{13, 14, 15, 255}));
    EXPECT_EQ(s.badgeFontFamily, "Badge Sans");
    EXPECT_DOUBLE_EQ(s.badgeFontScale, 0.8);

    EXPECT_TRUE(loadTheme("[code-block]\nmargin = 65\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[code-block]\nbadge-font-scale = 0.4\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[inline-code]\nmargin = 2\n", ColorScheme::Light).usedFallback)
        << "inline code has no margin";
}

TEST(S021_CodeBlockFences, ThemeTokensFallBack) {
    for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& t = builtinTheme(scheme);
        const CodeStyle s = codeBlockStyle(t);
        EXPECT_DOUBLE_EQ(s.margin, 3);
        EXPECT_EQ(s.badgeText, s.marker);
        EXPECT_EQ(s.badgeFontFamily, "");
        EXPECT_DOUBLE_EQ(s.badgeFontScale, 1);
    }
    // The badge follows code-block.marker, which follows colors.marker.
    const auto marker = loadTheme("[code-block]\nmarker = \"#112233\"\n", ColorScheme::Light);
    EXPECT_EQ(codeBlockStyle(marker.theme).badgeText, (Color{0x11, 0x22, 0x33, 255}));
    const auto general = loadTheme("[colors]\nmarker = \"#445566\"\n", ColorScheme::Light);
    EXPECT_EQ(codeBlockStyle(general.theme).badgeText, (Color{0x44, 0x55, 0x66, 255}));
}

// A fence directly after a table (GFM: the fence ends the table) opens a block, although the line is
// entered with the table's state; the closing fence closes it. Found with the whole-document
// reference of S023.
TEST(S021_CodeBlockFences, AFenceDirectlyAfterATableOpensABlock) {
    const std::vector<std::u16string> lines = {u"| a |", u"| - |", u"```python", u"code", u"```", u"after"};
    const auto r = registry().renderDocument(lines, LineRange{3, 3});
    EXPECT_TRUE(r[1].table);
    EXPECT_TRUE(r[2].fenceLine);
    EXPECT_EQ(r[2].codeLanguage, u"python") << "an opening fence reports its language";
    EXPECT_TRUE(r[4].fenceLine);
    EXPECT_TRUE(r[4].codeLanguage.empty());
    EXPECT_TRUE(r[2].hidden.empty()) << "the caret in the block reveals the opening fence";
    EXPECT_TRUE(r[4].hidden.empty()) << "and the closing fence";
}
