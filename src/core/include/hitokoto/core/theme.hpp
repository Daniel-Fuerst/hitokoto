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
    std::string codeFontFamily;  ///< code blocks and inline code; empty = system fixed-width font

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
};

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
