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
    // `[code-block]` only (S021): space above and below the panel, the language badge.
    std::optional<double> margin, badgeFontScale;
    std::optional<Color> badgeText;
    std::optional<std::string> badgeFontFamily;
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
    /// S040: selected cells, the current cell and the pointers over the areas of rows and columns.
    std::optional<Color> selectionBackground, selectionText, currentBackground, selectPointerFill, selectPointerOutline;
    std::optional<double> headerWeight, borderWidth, radius, padding, fontScale;
    std::optional<std::string> fontFamily;
    friend bool operator==(const TableTokens&, const TableTokens&) = default;
};

/// Tokens of `[escape]`; resolved by escapeStyle().
struct EscapeTokens {
    std::optional<Color> marker;
    friend bool operator==(const EscapeTokens&, const EscapeTokens&) = default;
};

/// Tokens of `[link]`; resolved by linkStyle().
struct LinkTokens {
    std::optional<Color> text, marker;
    friend bool operator==(const LinkTokens&, const LinkTokens&) = default;
};

/// Tokens of `[math]` (S044); resolved by mathStyle().
struct MathTokens {
    std::optional<Color> text, marker, error;
    std::optional<double> fontScale;
    // Display formulas (S047): the panel.
    std::optional<Color> displayBackground, displayBorder;
    std::optional<double> displayBorderWidth, displayRadius, displayPadding, displayMargin;
    std::optional<std::string> fontFamily;
    friend bool operator==(const MathTokens&, const MathTokens&) = default;
};

/// Tokens of `[thematic-break]` (S023); resolved by thematicBreakStyle().
struct ThematicBreakTokens {
    std::optional<Color> border, marker;
    std::optional<double> borderWidth, margin;
    friend bool operator==(const ThematicBreakTokens&, const ThematicBreakTokens&) = default;
};

/// Tokens of `[properties]` (S024); resolved by propertiesStyle().
struct PropertiesTokens {
    std::optional<Color> background, text, keyText, titleText, icon, unsupportedText, badgeText, checkbox,
        checkboxBackground, chipBackground, chipText, chipBorder, border, editorBackground, editorText, editorBorder,
        editorFocusBorder;
    std::optional<double> badgeFontScale, chipBorderWidth, chipRadius, chipPadding, borderWidth, radius, padding,
        margin, rowSpacing, keyWidth, fontScale, editorBorderWidth, editorRadius, editorPadding;
    std::optional<std::string> badgeFontFamily, fontFamily;
    friend bool operator==(const PropertiesTokens&, const PropertiesTokens&) = default;
};

/// Tokens of `[toolbar]` (S026); resolved by toolbarStyle().
struct ToolbarTokens {
    std::optional<Color> background, border, icon, hoverBackground, activeBackground, activeIcon;
    std::optional<double> borderWidth, radius, padding, spacing, margin, buttonSize, buttonRadius;
    friend bool operator==(const ToolbarTokens&, const ToolbarTokens&) = default;
};

/// Tokens of `[find]` (S028); resolved by findStyle().
struct FindTokens {
    std::optional<Color> background, border, text, fieldBackground, fieldText, fieldBorder, fieldFocusBorder,
        noMatchBackground, icon, hoverBackground, activeBackground, activeIcon, currentBackground, currentText,
        matchBackground, matchText;
    std::optional<double> borderWidth, padding, spacing, fontScale, fieldBorderWidth, fieldRadius, buttonSize,
        buttonRadius;
    std::optional<std::string> fontFamily;
    friend bool operator==(const FindTokens&, const FindTokens&) = default;
};

/// Tokens of `[menu-bar]` (S038); resolved by menuBarStyle().
struct MenuBarTokens {
    std::optional<Color> background, border, text, hoverText, activeText, hoverBackground, activeBackground, mnemonic;
    std::optional<double> borderWidth, padding, spacing, itemRadius, itemPaddingHorizontal, itemPaddingVertical,
        fontScale;
    std::optional<std::string> fontFamily;
    friend bool operator==(const MenuBarTokens&, const MenuBarTokens&) = default;
};

/// Tokens of `[switch]` (S043); resolved by switchStyle().
struct SwitchTokens {
    std::optional<Color> track, checkedTrack, knob, checkedKnob, border;
    std::optional<double> borderWidth, width, height, radius, knobMargin;
    friend bool operator==(const SwitchTokens&, const SwitchTokens&) = default;
};

/// Tokens of `[settings-section]` (S048); resolved by settingsSectionStyle().
struct SettingsSectionTokens {
    std::optional<Color> text, rule;
    std::optional<double> weight, ruleWidth, ruleGap;
    friend bool operator==(const SettingsSectionTokens&, const SettingsSectionTokens&) = default;
};

/// Tokens of `[task]` (S052); resolved by taskStyle().
struct TaskTokens {
    std::optional<Color> background, border, checkedBackground, checkedBorder, check, doneText;
    std::optional<double> borderWidth, radius, scale, gap, doneStrikethrough;
    friend bool operator==(const TaskTokens&, const TaskTokens&) = default;
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
    EscapeTokens escape;
    LinkTokens link;
    MathTokens math;
    ThematicBreakTokens thematicBreak;
    PropertiesTokens properties;
    ToolbarTokens toolbar;
    FindTokens find;
    MenuBarTokens menuBar;
    SwitchTokens toggleSwitch;  ///< `[switch]`; `switch` is a C++ keyword
    SettingsSectionTokens settingsSection;
    TaskTokens task;

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
    // Code block only (S021).
    double margin = 0;            ///< space above and below the panel
    Color badgeText;              ///< language badge colour
    std::string badgeFontFamily;  ///< empty = editor font
    double badgeFontScale = 1;    ///< relative to the editor font
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
    Color selectionBackground, selectionText, currentBackground, selectPointerFill, selectPointerOutline;  ///< S040
    int headerWeight = 700;
    double borderWidth = 1;
    double radius = 0;
    double padding = 0;
    double fontScale = 1;    ///< relative to the editor font
    std::string fontFamily;  ///< empty = editor font
    friend bool operator==(const TableStyle&, const TableStyle&) = default;
};

struct EscapeStyle {
    Color marker;
    friend bool operator==(const EscapeStyle&, const EscapeStyle&) = default;
};

struct LinkStyle {
    Color text, marker;
    friend bool operator==(const LinkStyle&, const LinkStyle&) = default;
};

/// Resolved look of formula source (S044).
struct MathStyle {
    Color text, marker;
    Color error;             ///< source of a formula that cannot be typeset (S046)
    std::string fontFamily;  ///< empty = editor font
    double fontScale = 1;    ///< relative to the editor font
    // The panel of a display formula (S047).
    Color displayBackground, displayBorder;
    double displayBorderWidth = 0;
    double displayRadius = 0;
    double displayPadding = 0;  ///< between the panel's edge and the formula
    double displayMargin = 0;   ///< above and below the panel
    friend bool operator==(const MathStyle&, const MathStyle&) = default;
};

/// Resolved look of a thematic break (S023).
struct ThematicBreakStyle {
    Color border;            ///< colour of the line
    double borderWidth = 0;  ///< width of the line
    double margin = 0;       ///< space above and below the line
    Color marker;            ///< characters when revealed
    friend bool operator==(const ThematicBreakStyle&, const ThematicBreakStyle&) = default;
};

/// Resolved look of the properties block of front matter (S024).
struct PropertiesStyle {
    Color background, text, keyText, titleText, icon, unsupportedText, badgeText, checkbox, checkboxBackground,
        chipBackground, chipText, chipBorder, border, editorBackground, editorText, editorBorder, editorFocusBorder;
    double badgeFontScale = 1;    ///< relative to the editor font
    std::string badgeFontFamily;  ///< empty = editor font
    double chipBorderWidth = 0;
    double chipRadius = 0;
    double chipPadding = 0;  ///< horizontal space inside a list item or tag
    double borderWidth = 0;
    double radius = 0;
    double padding = 0;      ///< space inside the block
    double margin = 0;       ///< space above and below the block
    double rowSpacing = 0;   ///< space between rows
    double keyWidth = 0;     ///< width of the key column
    double fontScale = 1;    ///< relative to the editor font
    std::string fontFamily;  ///< empty = editor font
    double editorBorderWidth = 1;
    double editorRadius = 0;
    double editorPadding = 0;
    friend bool operator==(const PropertiesStyle&, const PropertiesStyle&) = default;
};

/// Resolved look of the floating toolbar (S026).
struct ToolbarStyle {
    Color background, border, icon, hoverBackground, activeBackground, activeIcon;
    double borderWidth = 1;
    double radius = 0;
    double padding = 4;
    double spacing = 4;
    double margin = 12;
    double buttonSize = 28;
    double buttonRadius = 0;
    friend bool operator==(const ToolbarStyle&, const ToolbarStyle&) = default;
};

/// Resolved look of the find bar and its highlights (S028).
struct FindStyle {
    Color background, border, text, fieldBackground, fieldText, fieldBorder, fieldFocusBorder, noMatchBackground, icon,
        hoverBackground, activeBackground, activeIcon, currentBackground, currentText, matchBackground, matchText;
    double borderWidth = 1;
    double padding = 0;
    double spacing = 0;
    double fontScale = 1;
    double fieldBorderWidth = 1;
    double fieldRadius = 0;
    double buttonSize = 24;
    double buttonRadius = 0;
    std::string fontFamily;  ///< empty = UI font
    friend bool operator==(const FindStyle&, const FindStyle&) = default;
};

/// Resolved look of the top menu bar (S038).
struct MenuBarStyle {
    Color background, border, text, hoverText, activeText, hoverBackground, activeBackground;
    Color mnemonic;  ///< access-key hint shown only while Alt is held or the bar is keyboard-driven
    double borderWidth = 0;
    double padding = 6;
    double spacing = 4;
    double itemRadius = 0;
    double itemPaddingHorizontal = 8;
    double itemPaddingVertical = 4;
    double fontScale = 1;
    std::string fontFamily;  ///< empty = UI font
    friend bool operator==(const MenuBarStyle&, const MenuBarStyle&) = default;
};

/// Resolved look of a toggle switch in the settings flyout (S043).
struct SwitchStyle {
    Color track, checkedTrack, knob, checkedKnob, border;
    double borderWidth = 0;
    double width = 36;
    double height = 20;
    double radius = 10;
    double knobMargin = 2;
    friend bool operator==(const SwitchStyle&, const SwitchStyle&) = default;
};

/// Resolved look of a section heading in the settings flyout (S048): the heading and the line after it.
struct SettingsSectionStyle {
    Color text, rule;
    int weight = 600;
    double ruleWidth = 1;
    double ruleGap = 8;  ///< space between the heading and the line
    friend bool operator==(const SettingsSectionStyle&, const SettingsSectionStyle&) = default;
};

/// Resolved look of a task item's checkbox and of done text (S052).
struct TaskStyle {
    Color background, border;                       ///< the box when unchecked
    Color checkedBackground, checkedBorder, check;  ///< the box when checked, and its tick
    Color doneText;                                 ///< text of a checked item
    double borderWidth = 1.5;
    double radius = 3;
    double scale = 0.9;  ///< side of the box relative to the ascent of the text
    double gap = 6;      ///< space between the box and the text
    bool doneStrikethrough = true;
    friend bool operator==(const TaskStyle&, const TaskStyle&) = default;
};

[[nodiscard]] CodeStyle codeBlockStyle(const Theme& theme);
[[nodiscard]] CodeStyle inlineCodeStyle(const Theme& theme);
[[nodiscard]] EmphasisStyle emphasisStyle(const Theme& theme);
[[nodiscard]] TableStyle tableStyle(const Theme& theme);
[[nodiscard]] EscapeStyle escapeStyle(const Theme& theme);
[[nodiscard]] LinkStyle linkStyle(const Theme& theme);
[[nodiscard]] MathStyle mathStyle(const Theme& theme);
[[nodiscard]] ThematicBreakStyle thematicBreakStyle(const Theme& theme);
[[nodiscard]] PropertiesStyle propertiesStyle(const Theme& theme);
[[nodiscard]] ToolbarStyle toolbarStyle(const Theme& theme);
[[nodiscard]] FindStyle findStyle(const Theme& theme);
[[nodiscard]] MenuBarStyle menuBarStyle(const Theme& theme);
[[nodiscard]] SwitchStyle switchStyle(const Theme& theme);
[[nodiscard]] SettingsSectionStyle settingsSectionStyle(const Theme& theme);
/// S052: the checkbox of a task item and the text of a checked one.
[[nodiscard]] TaskStyle taskStyle(const Theme& theme);

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
