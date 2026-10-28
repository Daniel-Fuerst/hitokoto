// SPDX-License-Identifier: GPL-3.0-or-later
// S005: theme tokens, the theme file format and the built-in light and dark themes.
// The format is documented in docs/theming.md.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
    friend bool operator==(const Color&, const Color&) = default;
    /// "#rrggbb" or "#rrggbbaa" (alpha only if not opaque).
    [[nodiscard]] std::string toHex() const;
};

enum class ColorScheme : std::uint8_t { Light, Dark };
enum class ButtonStyle : std::uint8_t { Flat, Outlined, Filled };

/// Tokens of a code-like element (`[code-block]`, `[inline-code]`). Unset values fall back when
/// resolved (ADR 0004); UI code reads codeBlockStyle() / inlineCodeStyle(), never these directly.
struct CodeTokens {
    std::optional<Color> background, text, border, marker;
    std::optional<double> borderWidth, radius, padding, fontScale;
    std::optional<std::string> fontFamily;
    friend bool operator==(const CodeTokens&, const CodeTokens&) = default;
};

/// Tokens of `[emphasis]`; resolved by emphasisStyle().
struct EmphasisTokens {
    std::optional<double> boldWeight;
    std::optional<Color> boldText, italicText, strikethroughText, marker;
    friend bool operator==(const EmphasisTokens&, const EmphasisTokens&) = default;
};

/// Tokens of `[table]`; resolved by tableStyle().
struct TableTokens {
    std::optional<Color> background, headerBackground, stripeBackground, text, headerText, border;
    std::optional<double> headerWeight, borderWidth, radius, padding, fontScale;
    std::optional<std::string> fontFamily;
    friend bool operator==(const TableTokens&, const TableTokens&) = default;
};

/// Every value the UI uses for colours, fonts, radii and spacing. Hard-coded styling values
/// in UI code are bugs; add a token instead.
struct Theme {
    std::string name;
    ColorScheme scheme = ColorScheme::Light;

    // [colors]
    Color primary, secondary, accent, background, surface, border;
    Color text, textMuted, selection, selectionText, marker, bullet;

    // [fonts] (empty family = system default; size 0 = system default for the UI font)
    std::string uiFontFamily;
    double uiFontSize = 0;
    std::string editorFontFamily;
    double editorFontSize = 0;

    // [headings]
    std::array<double, 6> headingScale{};
    int headingWeight = 700;

    // [layout] (device-independent pixels)
    double radius = 0;
    double spacing = 0;
    double editorPadding = 0;
    double editorMaxWidth = 0;  ///< 0 = text uses the full window width

    // [buttons]
    ButtonStyle buttonStyle = ButtonStyle::Filled;

    // Element sections (ADR 0004)
    CodeTokens codeBlock;
    CodeTokens inlineCode;
    EmphasisTokens emphasis;
    TableTokens table;

    friend bool operator==(const Theme&, const Theme&) = default;
};

struct ThemeDiagnostic {
    std::size_t line = 0;    ///< 1-based
    std::size_t column = 0;  ///< 1-based
    std::string message;
    [[nodiscard]] std::string toString() const;
};

struct ThemeParseResult {
    std::optional<Theme> theme;  ///< empty if there was any diagnostic
    std::vector<ThemeDiagnostic> diagnostics;
};

/// Documentation of one token, e.g. {"colors", "accent", "color", "..."}.
struct ThemeTokenInfo {
    std::string_view section;  ///< empty for top-level keys
    std::string_view key;
    std::string_view type;
    std::string_view description;
    std::string_view fallback;  ///< element tokens (ADR 0004): value used when unset; empty = required
};

/// Resolved look of a code-like element: every element token with its fallback applied.
struct CodeStyle {
    Color background, text, border, marker;
    double borderWidth = 0;
    double radius = 0;
    double padding = 0;
    double fontScale = 1;    ///< relative to the surrounding text
    std::string fontFamily;  ///< empty = system fixed-width font
    friend bool operator==(const CodeStyle&, const CodeStyle&) = default;
};

struct EmphasisStyle {
    int boldWeight = 700;
    std::optional<Color> boldText, italicText, strikethroughText;  ///< empty = surrounding text colour
    Color marker;
    friend bool operator==(const EmphasisStyle&, const EmphasisStyle&) = default;
};

struct TableStyle {
    Color background, headerBackground, stripeBackground, text, headerText, border;
    int headerWeight = 700;
    double borderWidth = 1;
    double radius = 0;
    double padding = 0;
    double fontScale = 1;    ///< relative to the editor font
    std::string fontFamily;  ///< empty = editor font
    friend bool operator==(const TableStyle&, const TableStyle&) = default;
};

[[nodiscard]] CodeStyle codeBlockStyle(const Theme& theme);
[[nodiscard]] CodeStyle inlineCodeStyle(const Theme& theme);
[[nodiscard]] EmphasisStyle emphasisStyle(const Theme& theme);
[[nodiscard]] TableStyle tableStyle(const Theme& theme);

/// All tokens of the theme format, in documentation order.
std::span<const ThemeTokenInfo> themeTokens();

/// Parses a theme file. Values not set in `source` come from `base`.
/// With `requireAllTokens`, every token must be set (used for the built-in themes).
ThemeParseResult parseTheme(std::string_view source, const Theme& base, bool requireAllTokens = false);

std::string_view builtinThemeSource(ColorScheme scheme);
const Theme& builtinTheme(ColorScheme scheme);

struct ThemeLoadResult {
    Theme theme;
    bool usedFallback = false;  ///< the user theme was rejected
    std::vector<ThemeDiagnostic> diagnostics;
};

/// Loads a user theme on top of the built-in theme named by its `base` key (or the system
/// scheme if it has none). Any error rejects the whole file and returns the built-in theme for
/// the system scheme, so a broken theme never prevents startup.
ThemeLoadResult loadTheme(std::string_view userSource, ColorScheme systemScheme);

}  // namespace hitokoto::core
