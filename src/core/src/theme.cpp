// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/theme.hpp"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <variant>

namespace hitokoto::core {
namespace {

using Value = std::variant<std::string, double>;
using Apply = std::optional<std::string> (*)(Theme&, const Value&);

std::optional<Color> parseColor(std::string_view s) {
    if (s.empty() || s.front() != '#') {
        return std::nullopt;
    }
    s.remove_prefix(1);
    const auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (char c : s) {
        if (hex(c) < 0) {
            return std::nullopt;
        }
    }
    const auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(hex(s[i]) * 16 + hex(s[i + 1])); };
    const auto nibble = [&](std::size_t i) { return static_cast<std::uint8_t>(hex(s[i]) * 17); };
    switch (s.size()) {
        case 3:
            return Color{nibble(0), nibble(1), nibble(2), 255};
        case 6:
            return Color{byte(0), byte(2), byte(4), 255};
        case 8:
            return Color{byte(0), byte(2), byte(4), byte(6)};
        default:
            return std::nullopt;
    }
}

std::optional<std::string> colorValue(const Value& v, Color& out) {
    const auto* s = std::get_if<std::string>(&v);
    if (!s) {
        return "expected a colour string like \"#7c5cff\"";
    }
    const auto c = parseColor(*s);
    if (!c) {
        return "invalid colour \"" + *s + "\" (use #rgb, #rrggbb or #rrggbbaa)";
    }
    out = *c;
    return std::nullopt;
}

template <Color Theme::* Member>
std::optional<std::string> applyColor(Theme& t, const Value& v) {
    return colorValue(v, t.*Member);
}

// Element tokens (ADR 0004): `Group` is the element's section in Theme, `Member` the token.
template <auto Group, auto Member>
std::optional<std::string> applyOptColor(Theme& t, const Value& v) {
    Color c;
    if (auto err = colorValue(v, c)) {
        return err;
    }
    (t.*Group).*Member = c;
    return std::nullopt;
}

template <auto Group, auto Member>
std::optional<std::string> applyOptString(Theme& t, const Value& v) {
    const auto* s = std::get_if<std::string>(&v);
    if (!s) {
        return "expected a string in double quotes";
    }
    (t.*Group).*Member = *s;
    return std::nullopt;
}

template <std::string Theme::* Member>
std::optional<std::string> applyString(Theme& t, const Value& v) {
    const auto* s = std::get_if<std::string>(&v);
    if (!s) {
        return "expected a string in double quotes";
    }
    t.*Member = *s;
    return std::nullopt;
}

std::optional<std::string> numberIn(const Value& v, double lo, double hi, double& out) {
    const auto* d = std::get_if<double>(&v);
    if (!d) {
        return "expected a number";
    }
    if (*d < lo || *d > hi) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "value out of range (allowed %g to %g)", lo, hi);
        return std::string(buf);
    }
    out = *d;
    return std::nullopt;
}

template <double Theme::* Member, int Lo, int Hi>
std::optional<std::string> applyNumber(Theme& t, const Value& v) {
    return numberIn(v, Lo, Hi, t.*Member);
}

/// Range in tenths, so that bounds like 0.5 need no floating-point template arguments.
template <auto Group, auto Member, int LoTenths, int HiTenths>
std::optional<std::string> applyOptNumber(Theme& t, const Value& v) {
    double d = 0;
    if (auto err = numberIn(v, LoTenths / 10.0, HiTenths / 10.0, d)) {
        return err;
    }
    (t.*Group).*Member = d;
    return std::nullopt;
}

std::optional<std::string> applyEditorFontSize(Theme& t, const Value& v) {
    return numberIn(v, 6, 72, t.editorFontSize);
}
std::optional<std::string> applyUiFontSize(Theme& t, const Value& v) {
    double d = 0;
    if (auto err = numberIn(v, 0, 72, d)) {
        return err;
    }
    if (d != 0 && d < 6) {
        return "value out of range (0 = system default, otherwise 6 to 72)";
    }
    t.uiFontSize = d;
    return std::nullopt;
}

template <std::size_t Level>
std::optional<std::string> applyHeadingScale(Theme& t, const Value& v) {
    return numberIn(v, 0.5, 4.0, t.headingScale[Level]);
}

std::optional<std::string> applyHeadingWeight(Theme& t, const Value& v) {
    double d = 0;
    if (auto err = numberIn(v, 100, 900, d)) {
        return err;
    }
    t.headingWeight = static_cast<int>(d);
    return std::nullopt;
}

std::optional<std::string> applyMaxWidth(Theme& t, const Value& v) {
    double d = 0;
    if (auto err = numberIn(v, 0, 10000, d)) {
        return err;
    }
    if (d != 0 && d < 200) {
        return "value out of range (0 = full width, otherwise 200 to 10000)";
    }
    t.editorMaxWidth = d;
    return std::nullopt;
}

std::optional<std::string> applyButtonStyle(Theme& t, const Value& v) {
    const auto* s = std::get_if<std::string>(&v);
    if (s && *s == "flat") {
        t.buttonStyle = ButtonStyle::Flat;
    } else if (s && *s == "outlined") {
        t.buttonStyle = ButtonStyle::Outlined;
    } else if (s && *s == "filled") {
        t.buttonStyle = ButtonStyle::Filled;
    } else {
        return "expected \"flat\", \"outlined\" or \"filled\"";
    }
    return std::nullopt;
}

std::optional<std::string> applyBase(Theme& t, const Value& v) {
    const auto* s = std::get_if<std::string>(&v);
    if (s && *s == "light") {
        t.scheme = ColorScheme::Light;
    } else if (s && *s == "dark") {
        t.scheme = ColorScheme::Dark;
    } else {
        return "expected \"light\" or \"dark\"";
    }
    return std::nullopt;
}

struct Token {
    ThemeTokenInfo info;
    Apply apply;
};

// clang-format off
const Token kTokens[] = {
    {{"", "name", "string", "Display name of the theme", ""}, &applyString<&Theme::name>},
    {{"", "base", "\"light\" | \"dark\"", "Built-in theme that supplies every token not set in this file; default: the system colour scheme", ""}, &applyBase},
    {{"colors", "primary", "color", "Main brand colour (focus rings, default buttons)", ""}, &applyColor<&Theme::primary>},
    {{"colors", "secondary", "color", "Second brand colour for less prominent highlights", ""}, &applyColor<&Theme::secondary>},
    {{"colors", "accent", "color", "Accent colour (links, checked states, scroll handles on hover)", ""}, &applyColor<&Theme::accent>},
    {{"colors", "background", "color", "Editor and window background", ""}, &applyColor<&Theme::background>},
    {{"colors", "surface", "color", "Menus, dialogs and other raised surfaces", ""}, &applyColor<&Theme::surface>},
    {{"colors", "border", "color", "Separators and outlines", ""}, &applyColor<&Theme::border>},
    {{"colors", "text", "color", "Main text", ""}, &applyColor<&Theme::text>},
    {{"colors", "text-muted", "color", "Secondary text (disabled items, hints)", ""}, &applyColor<&Theme::textMuted>},
    {{"colors", "selection", "color", "Selection background", ""}, &applyColor<&Theme::selection>},
    {{"colors", "selection-text", "color", "Selected text", ""}, &applyColor<&Theme::selectionText>},
    {{"colors", "marker", "color", "Markdown syntax markers when revealed (e.g. '#')", ""}, &applyColor<&Theme::marker>},
    {{"colors", "bullet", "color", "Bullet dots of list items", ""}, &applyColor<&Theme::bullet>},
    {{"fonts", "ui-family", "string", "Font family for menus and dialogs; \"\" = system default", ""}, &applyString<&Theme::uiFontFamily>},
    {{"fonts", "ui-size", "number (pt)", "Font size for menus and dialogs; 0 = system default, else 6 to 72", ""}, &applyUiFontSize},
    {{"fonts", "editor-family", "string", "Font family of the editor text; \"\" = system default", ""}, &applyString<&Theme::editorFontFamily>},
    {{"fonts", "editor-size", "number (pt)", "Font size of the editor text, 6 to 72", ""}, &applyEditorFontSize},
    {{"headings", "h1-scale", "number", "Size of level-1 headings relative to the editor font, 0.5 to 4", ""}, &applyHeadingScale<0>},
    {{"headings", "h2-scale", "number", "Same for level 2", ""}, &applyHeadingScale<1>},
    {{"headings", "h3-scale", "number", "Same for level 3", ""}, &applyHeadingScale<2>},
    {{"headings", "h4-scale", "number", "Same for level 4", ""}, &applyHeadingScale<3>},
    {{"headings", "h5-scale", "number", "Same for level 5", ""}, &applyHeadingScale<4>},
    {{"headings", "h6-scale", "number", "Same for level 6", ""}, &applyHeadingScale<5>},
    {{"headings", "weight", "number", "Font weight of headings, 100 to 900", ""}, &applyHeadingWeight},
    {{"layout", "radius", "number (px)", "Corner radius of buttons, menus, scroll handles, 0 to 32", ""}, &applyNumber<&Theme::radius, 0, 32>},
    {{"layout", "spacing", "number (px)", "Base spacing; paddings in menus and buttons derive from it, 0 to 64", ""}, &applyNumber<&Theme::spacing, 0, 64>},
    {{"layout", "editor-padding", "number (px)", "Space around the editor text, 0 to 256", ""}, &applyNumber<&Theme::editorPadding, 0, 256>},
    {{"layout", "editor-max-width", "number (px)", "Maximum width of the text column (centred); 0 = full width, else 200 to 10000", ""}, &applyMaxWidth},
    {{"buttons", "style", "\"flat\" | \"outlined\" | \"filled\"", "Look of push buttons in dialogs", ""}, &applyButtonStyle},
    {{"code-block", "background", "color", "Background of the code block panel", "colors.surface"}, &applyOptColor<&Theme::codeBlock, &CodeTokens::background>},
    {{"code-block", "text", "color", "Code text", "colors.text"}, &applyOptColor<&Theme::codeBlock, &CodeTokens::text>},
    {{"code-block", "border", "color", "Border of the code block panel", "colors.border"}, &applyOptColor<&Theme::codeBlock, &CodeTokens::border>},
    {{"code-block", "border-width", "number (px)", "Border width of the panel, 0 to 8", "0"}, &applyOptNumber<&Theme::codeBlock, &CodeTokens::borderWidth, 0, 80>},
    {{"code-block", "radius", "number (px)", "Corner radius of the panel, 0 to 32", "layout.radius"}, &applyOptNumber<&Theme::codeBlock, &CodeTokens::radius, 0, 320>},
    {{"code-block", "padding", "number (px)", "Space between the panel edge and the code, 0 to 64", "layout.spacing"}, &applyOptNumber<&Theme::codeBlock, &CodeTokens::padding, 0, 640>},
    {{"code-block", "font-family", "string", "Font family of the code; \"\" = system fixed-width font", "\"\""}, &applyOptString<&Theme::codeBlock, &CodeTokens::fontFamily>},
    {{"code-block", "font-scale", "number", "Font size relative to the editor font, 0.5 to 2", "1"}, &applyOptNumber<&Theme::codeBlock, &CodeTokens::fontScale, 5, 20>},
    {{"code-block", "marker", "color", "Fence lines (fence characters and info string) when revealed", "colors.marker"}, &applyOptColor<&Theme::codeBlock, &CodeTokens::marker>},
    {{"inline-code", "background", "color", "Background of inline code", "code-block.background"}, &applyOptColor<&Theme::inlineCode, &CodeTokens::background>},
    {{"inline-code", "text", "color", "Inline code text", "code-block.text"}, &applyOptColor<&Theme::inlineCode, &CodeTokens::text>},
    {{"inline-code", "border", "color", "Border of the inline code background", "code-block.border"}, &applyOptColor<&Theme::inlineCode, &CodeTokens::border>},
    {{"inline-code", "border-width", "number (px)", "Border width, 0 to 8", "0"}, &applyOptNumber<&Theme::inlineCode, &CodeTokens::borderWidth, 0, 80>},
    {{"inline-code", "radius", "number (px)", "Corner radius of the background, 0 to 32", "code-block.radius"}, &applyOptNumber<&Theme::inlineCode, &CodeTokens::radius, 0, 320>},
    {{"inline-code", "padding", "number (px)", "Horizontal space inside the background on each side, 0 to 16; moves the following text", "2"}, &applyOptNumber<&Theme::inlineCode, &CodeTokens::padding, 0, 160>},
    {{"inline-code", "font-family", "string", "Font family; \"\" = system fixed-width font", "code-block.font-family"}, &applyOptString<&Theme::inlineCode, &CodeTokens::fontFamily>},
    {{"inline-code", "font-scale", "number", "Font size relative to the surrounding text, 0.5 to 2", "1"}, &applyOptNumber<&Theme::inlineCode, &CodeTokens::fontScale, 5, 20>},
    {{"inline-code", "marker", "color", "Backticks when revealed", "colors.marker"}, &applyOptColor<&Theme::inlineCode, &CodeTokens::marker>},
    {{"emphasis", "bold-weight", "number", "Font weight of bold text, 100 to 900", "700"}, &applyOptNumber<&Theme::emphasis, &EmphasisTokens::boldWeight, 1000, 9000>},
    {{"emphasis", "bold-text", "color", "Colour of bold text (takes precedence over italic and strikethrough)", "text colour of the surrounding element"}, &applyOptColor<&Theme::emphasis, &EmphasisTokens::boldText>},
    {{"emphasis", "italic-text", "color", "Colour of italic text (takes precedence over strikethrough)", "text colour of the surrounding element"}, &applyOptColor<&Theme::emphasis, &EmphasisTokens::italicText>},
    {{"emphasis", "strikethrough-text", "color", "Colour of struck-through text", "text colour of the surrounding element"}, &applyOptColor<&Theme::emphasis, &EmphasisTokens::strikethroughText>},
    {{"emphasis", "marker", "color", "Delimiters (* _ ~~) when revealed", "colors.marker"}, &applyOptColor<&Theme::emphasis, &EmphasisTokens::marker>},
    {{"table", "background", "color", "Background of table body rows", "colors.background"}, &applyOptColor<&Theme::table, &TableTokens::background>},
    {{"table", "header-background", "color", "Background of the table header row", "colors.surface"}, &applyOptColor<&Theme::table, &TableTokens::headerBackground>},
    {{"table", "stripe-background", "color", "Background of alternating (even-numbered) body rows", "table.background"}, &applyOptColor<&Theme::table, &TableTokens::stripeBackground>},
    {{"table", "text", "color", "Text colour of table body cells", "colors.text"}, &applyOptColor<&Theme::table, &TableTokens::text>},
    {{"table", "header-text", "color", "Text colour of table header cells", "table.text"}, &applyOptColor<&Theme::table, &TableTokens::headerText>},
    {{"table", "header-weight", "number", "Font weight of table header cells, 100 to 900", "headings.weight"}, &applyOptNumber<&Theme::table, &TableTokens::headerWeight, 1000, 9000>},
    {{"table", "border", "color", "Outer border, cell grid lines and add-row/column controls", "colors.border"}, &applyOptColor<&Theme::table, &TableTokens::border>},
    {{"table", "border-width", "number (px)", "Width of table and cell borders, 0 to 8", "1"}, &applyOptNumber<&Theme::table, &TableTokens::borderWidth, 0, 80>},
    {{"table", "radius", "number (px)", "Corner radius of the outer table border, 0 to 32", "layout.radius"}, &applyOptNumber<&Theme::table, &TableTokens::radius, 0, 320>},
    {{"table", "padding", "number (px)", "Horizontal and vertical space inside cells, 0 to 64", "layout.spacing"}, &applyOptNumber<&Theme::table, &TableTokens::padding, 0, 640>},
    {{"table", "font-family", "string", "Font family of table cells; \"\" = editor font", "\"\""}, &applyOptString<&Theme::table, &TableTokens::fontFamily>},
    {{"table", "font-scale", "number", "Font size of table cells relative to the editor font, 0.5 to 2", "1"}, &applyOptNumber<&Theme::table, &TableTokens::fontScale, 5, 20>},
};
// clang-format on

const Token* findToken(std::string_view section, std::string_view key) {
    for (const auto& t : kTokens) {
        if (t.info.section == section && t.info.key == key) {
            return &t;
        }
    }
    return nullptr;
}

bool knownSection(std::string_view s) {
    for (const auto& t : kTokens) {
        if (!t.info.section.empty() && t.info.section == s) {
            return true;
        }
    }
    return false;
}

/// Locale-independent "[-+]digits[.digits]" (std::from_chars for double is missing in older libc++).
std::optional<double> parseDecimal(std::string_view t) {
    std::size_t i = 0;
    bool negative = false;
    if (i < t.size() && (t[i] == '-' || t[i] == '+')) {
        negative = t[i] == '-';
        ++i;
    }
    double value = 0;
    std::size_t digits = 0;
    while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
        value = value * 10 + (t[i] - '0');
        ++i;
        ++digits;
    }
    if (i < t.size() && t[i] == '.') {
        ++i;
        double scale = 0.1;
        while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
            value += (t[i] - '0') * scale;
            scale /= 10;
            ++i;
            ++digits;
        }
    }
    if (digits == 0 || i != t.size() || digits > 15) {
        return std::nullopt;
    }
    return negative ? -value : value;
}

bool isKeyChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}
bool isBlank(char c) {
    return c == ' ' || c == '\t';
}

class Parser {
public:
    Parser(std::string_view src, Theme& theme) : src_(src), theme_(theme) {}

    void run() {
        std::size_t lineNo = 0;
        std::size_t pos = 0;
        while (pos <= src_.size()) {
            ++lineNo;
            const std::size_t nl = src_.find('\n', pos);
            std::string_view line = src_.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            parseLine(line, lineNo);
            if (nl == std::string_view::npos) {
                break;
            }
            pos = nl + 1;
        }
    }

    std::vector<ThemeDiagnostic> diagnostics;
    std::set<std::string> seen;

private:
    void error(std::size_t line, std::size_t col0, std::string msg) {
        diagnostics.push_back({line, col0 + 1, std::move(msg)});
    }

    static std::size_t skipBlanks(std::string_view l, std::size_t i) {
        while (i < l.size() && isBlank(l[i])) {
            ++i;
        }
        return i;
    }

    bool expectLineEnd(std::string_view l, std::size_t i, std::size_t lineNo) {
        i = skipBlanks(l, i);
        if (i < l.size() && l[i] != '#') {
            error(lineNo, i, "unexpected text; expected end of line or a comment");
            return false;
        }
        return true;
    }

    void parseLine(std::string_view l, std::size_t lineNo) {
        std::size_t i = skipBlanks(l, 0);
        if (i >= l.size() || l[i] == '#') {
            return;
        }
        if (l[i] == '[') {
            const std::size_t close = l.find(']', i);
            if (close == std::string_view::npos) {
                error(lineNo, i, "missing ']' after section name");
                return;
            }
            const std::string_view name = l.substr(i + 1, close - i - 1);
            if (!knownSection(name)) {
                error(lineNo, i + 1, "unknown section [" + std::string(name) + "]");
                section_ = "\x01";  // keys below are reported once via the section error
                return;
            }
            section_ = std::string(name);
            expectLineEnd(l, close + 1, lineNo);
            return;
        }
        const std::size_t keyStart = i;
        while (i < l.size() && isKeyChar(l[i])) {
            ++i;
        }
        if (i == keyStart) {
            error(lineNo, i, "expected a key, a [section] or a comment");
            return;
        }
        const std::string_view key = l.substr(keyStart, i - keyStart);
        i = skipBlanks(l, i);
        if (i >= l.size() || l[i] != '=') {
            error(lineNo, i, "expected '=' after key '" + std::string(key) + "'");
            return;
        }
        i = skipBlanks(l, i + 1);
        const std::size_t valueStart = i;
        Value value;
        if (i < l.size() && l[i] == '"') {
            std::string s;
            ++i;
            bool closed = false;
            while (i < l.size()) {
                const char c = l[i++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\' && i < l.size() && (l[i] == '"' || l[i] == '\\')) {
                    s.push_back(l[i++]);
                } else {
                    s.push_back(c);
                }
            }
            if (!closed) {
                error(lineNo, valueStart, "missing closing '\"'");
                return;
            }
            value = std::move(s);
        } else {
            std::size_t end = i;
            while (end < l.size() && !isBlank(l[end]) && l[end] != '#') {
                ++end;
            }
            const std::string_view text = l.substr(i, end - i);
            const auto number = parseDecimal(text);
            if (!number) {
                error(lineNo, valueStart,
                      text.empty() ? "expected a value" : "expected a number or a string in double quotes");
                return;
            }
            value = *number;
            i = end;
        }
        if (!expectLineEnd(l, i, lineNo)) {
            return;
        }
        if (section_ == "\x01") {
            return;
        }
        const Token* token = findToken(section_, key);
        const std::string full = section_.empty() ? std::string(key) : section_ + "." + std::string(key);
        if (!token) {
            error(lineNo, keyStart, "unknown key '" + full + "'");
            return;
        }
        if (!seen.insert(full).second) {
            error(lineNo, keyStart, "duplicate key '" + full + "'");
            return;
        }
        if (auto err = token->apply(theme_, value)) {
            error(lineNo, valueStart, full + ": " + *err);
        }
    }

    std::string_view src_;
    Theme& theme_;
    std::string section_;
};

// clang-format off
constexpr std::string_view kLight = R"(# hitokoto built-in light theme
name = "hitokoto Light"
base = "light"

[colors]
primary        = "#6d5dfc"
secondary      = "#0f9d9a"
accent         = "#6d5dfc"
background     = "#ffffff"
surface        = "#f5f5f7"
border         = "#e2e2e8"
text           = "#1f2328"
text-muted     = "#6b6f76"
selection      = "#dcd8fe"
selection-text = "#1f2328"
marker         = "#9a9ca5"
bullet         = "#6d5dfc"

[fonts]
ui-family     = ""
ui-size       = 0
editor-family = ""
editor-size   = 12

[headings]
h1-scale = 1.9
h2-scale = 1.55
h3-scale = 1.3
h4-scale = 1.15
h5-scale = 1.05
h6-scale = 1.0
weight   = 700

[layout]
radius           = 6
spacing          = 8
editor-padding   = 32
editor-max-width = 760

[buttons]
style = "filled"
)";

constexpr std::string_view kDark = R"(# hitokoto built-in dark theme
name = "hitokoto Dark"
base = "dark"

[colors]
primary        = "#8b7dff"
secondary      = "#2cc5bd"
accent         = "#8b7dff"
background     = "#1e1f22"
surface        = "#2a2c31"
border         = "#3a3d44"
text           = "#e4e5e9"
text-muted     = "#9a9ca5"
selection      = "#463d8f"
selection-text = "#ffffff"
marker         = "#7c7f88"
bullet         = "#8b7dff"

[fonts]
ui-family     = ""
ui-size       = 0
editor-family = ""
editor-size   = 12

[headings]
h1-scale = 1.9
h2-scale = 1.55
h3-scale = 1.3
h4-scale = 1.15
h5-scale = 1.05
h6-scale = 1.0
weight   = 700

[layout]
radius           = 6
spacing          = 8
editor-padding   = 32
editor-max-width = 760

[buttons]
style = "filled"
)";
// clang-format on

std::optional<ColorScheme> declaredBase(std::string_view source) {
    // Cheap scan for a top-level `base = "..."` line; full validation happens in parseTheme.
    std::size_t pos = 0;
    while (pos < source.size()) {
        const std::size_t nl = source.find('\n', pos);
        std::string_view l = source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        std::size_t i = 0;
        while (i < l.size() && isBlank(l[i]))
            ++i;
        l.remove_prefix(i);
        if (!l.empty() && l.front() == '[') {
            return std::nullopt;  // top-level keys must come before the first section
        }
        if (l.substr(0, 4) == "base") {
            if (l.find("\"dark\"") != std::string_view::npos)
                return ColorScheme::Dark;
            if (l.find("\"light\"") != std::string_view::npos)
                return ColorScheme::Light;
        }
        if (nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }
    return std::nullopt;
}

Theme mustParse(std::string_view source) {
    auto r = parseTheme(source, Theme{}, true);
    if (!r.theme) {
        std::abort();
    }
    return *r.theme;
}

}  // namespace

std::string Color::toHex() const {
    char buf[10];
    if (a == 255) {
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
    } else {
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x%02x", r, g, b, a);
    }
    return buf;
}

std::string ThemeDiagnostic::toString() const {
    return "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message;
}

std::span<const ThemeTokenInfo> themeTokens() {
    static const std::vector<ThemeTokenInfo> infos = [] {
        std::vector<ThemeTokenInfo> v;
        for (const auto& t : kTokens)
            v.push_back(t.info);
        return v;
    }();
    return infos;
}

ThemeParseResult parseTheme(std::string_view source, const Theme& base, bool requireAllTokens) {
    Theme theme = base;
    Parser parser(source, theme);
    parser.run();
    ThemeParseResult result;
    result.diagnostics = std::move(parser.diagnostics);
    if (requireAllTokens) {
        for (const auto& t : kTokens) {
            if (!t.info.fallback.empty()) {
                continue;  // element tokens may stay unset (ADR 0004)
            }
            const std::string full = t.info.section.empty()
                                         ? std::string(t.info.key)
                                         : std::string(t.info.section) + "." + std::string(t.info.key);
            if (!parser.seen.contains(full)) {
                result.diagnostics.push_back({0, 0, "missing token '" + full + "'"});
            }
        }
    }
    if (result.diagnostics.empty()) {
        result.theme = std::move(theme);
    }
    return result;
}

CodeStyle codeBlockStyle(const Theme& t) {
    const CodeTokens& c = t.codeBlock;
    return {
        .background = c.background.value_or(t.surface),
        .text = c.text.value_or(t.text),
        .border = c.border.value_or(t.border),
        .marker = c.marker.value_or(t.marker),
        .borderWidth = c.borderWidth.value_or(0),
        .radius = c.radius.value_or(t.radius),
        .padding = c.padding.value_or(t.spacing),
        .fontScale = c.fontScale.value_or(1),
        .fontFamily = c.fontFamily.value_or(""),
    };
}

CodeStyle inlineCodeStyle(const Theme& t) {
    const CodeStyle block = codeBlockStyle(t);
    const CodeTokens& c = t.inlineCode;
    return {
        .background = c.background.value_or(block.background),
        .text = c.text.value_or(block.text),
        .border = c.border.value_or(block.border),
        .marker = c.marker.value_or(t.marker),
        .borderWidth = c.borderWidth.value_or(0),
        .radius = c.radius.value_or(block.radius),
        .padding = c.padding.value_or(2),
        .fontScale = c.fontScale.value_or(1),
        .fontFamily = c.fontFamily.value_or(block.fontFamily),
    };
}

EmphasisStyle emphasisStyle(const Theme& t) {
    const EmphasisTokens& e = t.emphasis;
    return {
        .boldWeight = static_cast<int>(e.boldWeight.value_or(700)),
        .boldText = e.boldText,
        .italicText = e.italicText,
        .strikethroughText = e.strikethroughText,
        .marker = e.marker.value_or(t.marker),
    };
}

TableStyle tableStyle(const Theme& t) {
    const TableTokens& tb = t.table;
    const Color bg = tb.background.value_or(t.background);
    const Color txt = tb.text.value_or(t.text);
    return {
        .background = bg,
        .headerBackground = tb.headerBackground.value_or(t.surface),
        .stripeBackground = tb.stripeBackground.value_or(bg),
        .text = txt,
        .headerText = tb.headerText.value_or(txt),
        .border = tb.border.value_or(t.border),
        .headerWeight = static_cast<int>(tb.headerWeight.value_or(t.headingWeight)),
        .borderWidth = tb.borderWidth.value_or(1),
        .radius = tb.radius.value_or(t.radius),
        .padding = tb.padding.value_or(t.spacing),
        .fontScale = tb.fontScale.value_or(1),
        .fontFamily = tb.fontFamily.value_or(""),
    };
}

std::string_view builtinThemeSource(ColorScheme scheme) {
    return scheme == ColorScheme::Dark ? kDark : kLight;
}

const Theme& builtinTheme(ColorScheme scheme) {
    // The built-in sources are part of the program; a parse failure is a bug caught by the tests.
    static const Theme light = mustParse(kLight);
    static const Theme dark = mustParse(kDark);
    return scheme == ColorScheme::Dark ? dark : light;
}

ThemeLoadResult loadTheme(std::string_view userSource, ColorScheme systemScheme) {
    const ColorScheme baseScheme = declaredBase(userSource).value_or(systemScheme);
    Theme base = builtinTheme(baseScheme);
    base.name = "Custom";
    auto parsed = parseTheme(userSource, base);
    ThemeLoadResult r;
    if (parsed.theme) {
        r.theme = std::move(*parsed.theme);
    } else {
        r.theme = builtinTheme(systemScheme);
        r.usedFallback = true;
        r.diagnostics = std::move(parsed.diagnostics);
    }
    return r;
}

}  // namespace hitokoto::core
