// SPDX-License-Identifier: GPL-3.0-or-later
// S024 (ADR 0007): the properties of a note's front matter, read from its YAML with a parser of our
// own for a subset of YAML 1.2: block mappings and sequences by indentation, flow collections,
// plain, quoted and block scalars, comments, and the core schema for null, booleans and numbers.
// Everything outside the subset is an error with its line. Never throws on any input, no recursion
// deeper than kMaxDepth, linear in the length of the YAML. Expected values are checked against
// js-yaml 4 and ruamel.yaml 0.19 (YAML 1.2); differences the story decides on purpose are listed
// in docs/stories/024-front-matter.md.
#include "hitokoto/core/properties.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace hitokoto::core {
namespace {

using Reason = PropertiesErrorReason;

constexpr int kMaxDepth = 64;  ///< ADR 0007: nesting deeper than this is an error
constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

bool isWhite(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

bool isFlowIndicator(char16_t c) noexcept {
    return c == u',' || c == u'[' || c == u']' || c == u'{' || c == u'}';
}

bool isDigit(char16_t c) noexcept {
    return c >= u'0' && c <= u'9';
}

bool isHexDigit(char16_t c) noexcept {
    return isDigit(c) || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
}

int hexValue(char16_t c) noexcept {
    if (isDigit(c)) {
        return c - u'0';
    }
    if (c >= u'a' && c <= u'f') {
        return c - u'a' + 10;
    }
    return c - u'A' + 10;
}

/// Characters that cannot start a plain scalar (YAML 1.2, c-indicator), except `-`, `?` and `:`,
/// which can if a non-blank follows.
bool isIndicator(char16_t c) noexcept {
    switch (c) {
        case u',':
        case u'[':
        case u']':
        case u'{':
        case u'}':
        case u'#':
        case u'&':
        case u'*':
        case u'!':
        case u'|':
        case u'>':
        case u'\'':
        case u'"':
        case u'%':
        case u'@':
        case u'`':
            return true;
        default:
            return false;
    }
}

std::u16string_view trimRight(std::u16string_view s) noexcept {
    while (!s.empty() && isWhite(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

/// Narrow ASCII copy of `s`; false if `s` holds anything else.
bool toAscii(std::u16string_view s, std::string& out) {
    out.clear();
    out.reserve(s.size());
    for (const char16_t c : s) {
        if (c >= 0x80) {
            return false;
        }
        out += static_cast<char>(c);
    }
    return true;
}

// ---- Core schema (YAML 1.2, 10.3) ------------------------------------------------------------

bool isNull(std::u16string_view s) noexcept {
    return s.empty() || s == u"~" || s == u"null" || s == u"Null" || s == u"NULL";
}

/// 1 = true, 0 = false, -1 = no boolean.
int boolean(std::u16string_view s) noexcept {
    if (s == u"true" || s == u"True" || s == u"TRUE") {
        return 1;
    }
    if (s == u"false" || s == u"False" || s == u"FALSE") {
        return 0;
    }
    return -1;
}

bool allDigits(std::u16string_view s, bool (*pred)(char16_t) noexcept) noexcept {
    if (s.empty()) {
        return false;
    }
    for (const char16_t c : s) {
        if (!pred(c)) {
            return false;
        }
    }
    return true;
}

bool isOctalDigit(char16_t c) noexcept {
    return c >= u'0' && c <= u'7';
}

/// The value of a number of the core schema, or nothing if `s` is none.
std::optional<double> number(std::u16string_view s) {
    std::u16string_view body = s;
    bool negative = false;
    if (!body.empty() && (body[0] == u'-' || body[0] == u'+')) {
        negative = body[0] == u'-';
        body.remove_prefix(1);
    }
    // 0o and 0x take a sign too in js-yaml 4 and ruamel.yaml 0.19, though the core schema has none.
    if (body.size() >= 2 && body[0] == u'0' && (body[1] == u'o' || body[1] == u'x')) {
        const std::u16string_view digits = body.substr(2);
        const bool hex = body[1] == u'x';
        if (!allDigits(digits, hex ? isHexDigit : isOctalDigit)) {
            return std::nullopt;
        }
        double value = 0;
        for (const char16_t c : digits) {
            value = value * (hex ? 16 : 8) + hexValue(c);
        }
        return negative ? -value : value;
    }
    if (body == u".inf" || body == u".Inf" || body == u".INF") {
        return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }
    if (s == u".nan" || s == u".NaN" || s == u".NAN") {
        return std::numeric_limits<double>::quiet_NaN();
    }
    // [0-9]+ ( . [0-9]* )? | . [0-9]+, then ( [eE] [-+]? [0-9]+ )?
    std::size_t i = 0;
    std::size_t intDigits = 0;
    while (i < body.size() && isDigit(body[i])) {
        ++i;
        ++intDigits;
    }
    std::size_t fractionDigits = 0;
    if (i < body.size() && body[i] == u'.') {
        ++i;
        while (i < body.size() && isDigit(body[i])) {
            ++i;
            ++fractionDigits;
        }
    }
    if (intDigits == 0 && fractionDigits == 0) {
        return std::nullopt;
    }
    if (i < body.size() && (body[i] == u'e' || body[i] == u'E')) {
        ++i;
        if (i < body.size() && (body[i] == u'-' || body[i] == u'+')) {
            ++i;
        }
        std::size_t exponentDigits = 0;
        while (i < body.size() && isDigit(body[i])) {
            ++i;
            ++exponentDigits;
        }
        if (exponentDigits == 0) {
            return std::nullopt;
        }
    }
    if (i != body.size()) {
        return std::nullopt;
    }
    std::string ascii;
    if (!toAscii(body, ascii)) {
        return std::nullopt;
    }
    double value = 0;
    const auto result = std::from_chars(ascii.data(), ascii.data() + ascii.size(), value);
    if (result.ec == std::errc::result_out_of_range) {
        // Overflow or underflow: as JavaScript's Number() gives it.
        const bool tiny = ascii.find_first_of("123456789") == std::string::npos ||
                          ascii.find("e-") != std::string::npos || ascii.find("E-") != std::string::npos;
        value = tiny ? 0.0 : std::numeric_limits<double>::infinity();
    } else if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return negative ? -value : value;
}

/// ECMAScript Number::toString (ES2023 6.1.6.1.20) of a finite or infinite number.
std::u16string numberToString(double value) {
    if (std::isnan(value)) {
        return u"NaN";
    }
    if (value == 0) {
        return u"0";
    }
    std::u16string out;
    if (value < 0) {
        out += u'-';
        value = -value;
    }
    if (std::isinf(value)) {
        return out + u"Infinity";
    }
    std::array<char, 64> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
    const std::string_view scientific(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    const std::size_t e = scientific.find('e');
    std::string digits;
    for (const char c : scientific.substr(0, e)) {
        if (c != '.') {
            digits += c;
        }
    }
    int exponent = 0;
    const std::string_view exponentText = scientific.substr(e + 1);
    (void)std::from_chars(exponentText.data() + (exponentText[0] == '+' ? 1 : 0),
                          exponentText.data() + exponentText.size(), exponent);
    const int k = static_cast<int>(digits.size());
    const int n = exponent + 1;
    const auto append = [&out](std::string_view s) {
        for (const char c : s) {
            out += static_cast<char16_t>(c);
        }
    };
    if (k <= n && n <= 21) {
        append(digits);
        out.append(static_cast<std::size_t>(n - k), u'0');
    } else if (0 < n && n <= 21) {
        append(std::string_view(digits).substr(0, static_cast<std::size_t>(n)));
        out += u'.';
        append(std::string_view(digits).substr(static_cast<std::size_t>(n)));
    } else if (-6 < n && n <= 0) {
        out += u"0.";
        out.append(static_cast<std::size_t>(-n), u'0');
        append(digits);
    } else {
        out += static_cast<char16_t>(digits[0]);
        if (k > 1) {
            out += u'.';
            append(std::string_view(digits).substr(1));
        }
        out += u'e';
        out += n - 1 >= 0 ? u'+' : u'-';
        append(std::to_string(n - 1 >= 0 ? n - 1 : 1 - n));
    }
    return out;
}

bool isLeapYear(int y) noexcept {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int daysInMonth(int y, int m) noexcept {
    constexpr std::array<int, 12> kDays = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeapYear(y) ? 29 : kDays.at(static_cast<std::size_t>(m - 1));
}

/// `count` digits at `pos` as a number, or -1.
int digitsAt(std::u16string_view s, std::size_t pos, std::size_t count) noexcept {
    int value = 0;
    for (std::size_t i = pos; i < pos + count; ++i) {
        if (i >= s.size() || !isDigit(s[i])) {
            return -1;
        }
        value = value * 10 + (s[i] - u'0');
    }
    return value;
}

/// `YYYY-MM-DD` or `YYYY-MM-DDTHH:MM[:SS]`, a valid date (and time), as Obsidian stores them.
std::optional<PropertyDate> date(std::u16string_view s) noexcept {
    if (s.size() != 10 && s.size() != 16 && s.size() != 19) {
        return std::nullopt;
    }
    if (s[4] != u'-' || s[7] != u'-') {
        return std::nullopt;
    }
    PropertyDate d;
    d.year = digitsAt(s, 0, 4);
    d.month = digitsAt(s, 5, 2);
    d.day = digitsAt(s, 8, 2);
    if (d.year < 0 || d.month < 1 || d.month > 12 || d.day < 1 || d.day > daysInMonth(d.year, d.month)) {
        return std::nullopt;
    }
    if (s.size() == 10) {
        return d;
    }
    if (s[10] != u'T' || s[13] != u':') {
        return std::nullopt;
    }
    d.hour = digitsAt(s, 11, 2);
    d.minute = digitsAt(s, 14, 2);
    if (d.hour < 0 || d.hour > 23 || d.minute < 0 || d.minute > 59) {
        return std::nullopt;
    }
    if (s.size() == 19) {
        d.second = digitsAt(s, 17, 2);
        if (s[16] != u':' || d.second < 0 || d.second > 59) {
            return std::nullopt;
        }
    }
    return d;
}

// ---- The tree -------------------------------------------------------------------------------

struct Node {
    enum class Kind : std::uint8_t { Scalar, Sequence, Mapping };
    enum class Style : std::uint8_t { Plain, Quoted, Block };
    Kind kind = Kind::Scalar;
    Style style = Style::Plain;
    std::u16string text;                ///< Scalar: its value (plain: folded, as written)
    std::vector<std::size_t> children;  ///< Sequence: items; Mapping: values
    std::vector<std::u16string> keys;   ///< Mapping: keys as JavaScript strings (JSON)
};

/// A plain scalar as JavaScript sees it after js-yaml resolved it (null, boolean, number, string).
void appendJsonString(std::u16string_view s, std::u16string& out) {
    out += u'"';
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char16_t c = s[i];
        switch (c) {
            case u'"':
                out += u"\\\"";
                continue;
            case u'\\':
                out += u"\\\\";
                continue;
            case u'\b':
                out += u"\\b";
                continue;
            case u'\f':
                out += u"\\f";
                continue;
            case u'\n':
                out += u"\\n";
                continue;
            case u'\r':
                out += u"\\r";
                continue;
            case u'\t':
                out += u"\\t";
                continue;
            default:
                break;
        }
        const bool high = c >= 0xD800 && c <= 0xDBFF;
        const bool low = c >= 0xDC00 && c <= 0xDFFF;
        const bool paired = (high && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF);
        if (c < 0x20 || (low) || (high && !paired)) {
            constexpr std::u16string_view kHex = u"0123456789abcdef";
            out += u"\\u";
            out += kHex[(c >> 12U) & 0xFU];
            out += kHex[(c >> 8U) & 0xFU];
            out += kHex[(c >> 4U) & 0xFU];
            out += kHex[c & 0xFU];
            continue;
        }
        out += c;
        if (paired) {
            out += s[++i];
        }
    }
    out += u'"';
}

/// JSON of a scalar, as JSON.stringify writes js-yaml's value. Dates stay strings (S024).
void appendScalarJson(const Node& n, std::u16string& out) {
    if (n.style == Node::Style::Plain) {
        if (isNull(n.text)) {
            out += u"null";
            return;
        }
        if (const int b = boolean(n.text); b >= 0) {
            out += b == 1 ? u"true" : u"false";
            return;
        }
        if (const auto v = number(n.text)) {
            out += std::isfinite(*v) ? numberToString(*v) : u"null";
            return;
        }
    }
    appendJsonString(n.text, out);
}

/// A mapping key as JavaScript's object key: js-yaml resolves plain keys (null, booleans, numbers).
std::u16string jsKey(std::u16string_view text, bool plain) {
    if (!plain) {
        return std::u16string(text);
    }
    if (isNull(text)) {
        return u"null";
    }
    if (const int b = boolean(text); b >= 0) {
        return b == 1 ? u"true" : u"false";
    }
    if (const auto v = number(text)) {
        return numberToString(*v);
    }
    return std::u16string(text);
}

// ---- The parser -----------------------------------------------------------------------------

struct Pos {
    std::size_t line = 0;
    std::size_t col = 0;
};

/// A top-level key with where its value starts.
struct TopEntry {
    std::u16string key;
    std::size_t value = 0;
    std::size_t keyLine = 0;
    std::size_t valueLine = 0;
    std::size_t valueColumn = 0;
};

/// A key found at the start of a mapping entry.
struct Key {
    std::u16string text;  ///< without quotes, escapes resolved
    bool plain = true;
    std::size_t colon = 0;  ///< column of the `:`
};

class Parser {
public:
    explicit Parser(std::span<const std::u16string> lines) : lines_(lines) {}

    Properties run() {
        Properties result;
        const std::size_t first = skipToContent(0);
        if (!error_ && first < lines_.size()) {
            const std::u16string_view text = lineAt(first);
            if (text[0] == u'%') {
                fail(Reason::Directive, first);
            } else if (indent(first) > 0) {
                fail(Reason::BadIndentation, first);
            } else if (const auto key = scanKey(first, 0); !error_ && key) {
                (void)blockMapping(first, 0, 1, &top_);
                if (!error_) {
                    if (const std::size_t rest = skipToContent(next_); !error_ && rest < lines_.size()) {
                        fail(Reason::TextAfterValue, rest);
                    }
                }
            } else if (!error_ && text[0] == u'{') {
                Pos p{first, 0};
                (void)flowCollection(p, 1, &top_);
                if (!error_) {
                    trailing(p, false);
                }
                if (!error_) {
                    if (const std::size_t rest = skipToContent(p.line + 1); !error_ && rest < lines_.size()) {
                        fail(Reason::TextAfterValue, rest);
                    }
                }
            } else if (!error_) {
                // A sequence or a scalar: report what is wrong inside it first, then that it is no mapping.
                (void)blockNode(first, 0, -1, 1, false);
                if (!error_) {
                    fail(Reason::NotAMapping, first);
                }
            }
        }
        if (error_) {
            result.error = error_;
            return result;
        }
        result.rows.reserve(top_.size());
        for (const TopEntry& e : top_) {
            result.rows.push_back(row(e));
        }
        return result;
    }

private:
    // ---- lines ----------------------------------------------------------------------------

    [[nodiscard]] std::u16string_view lineAt(std::size_t i) const { return lines_[i]; }

    /// Leading spaces of line `i`.
    [[nodiscard]] std::size_t indent(std::size_t i) const {
        const std::u16string_view t = lineAt(i);
        std::size_t n = 0;
        while (n < t.size() && t[n] == u' ') {
            ++n;
        }
        return n;
    }

    [[nodiscard]] std::size_t firstNonWhite(std::size_t i) const {
        const std::u16string_view t = lineAt(i);
        std::size_t n = 0;
        while (n < t.size() && isWhite(t[n])) {
            ++n;
        }
        return n;
    }

    [[nodiscard]] bool isBlankLine(std::size_t i) const { return firstNonWhite(i) == lineAt(i).size(); }

    /// `---` or `...` at column 0, alone or followed by a blank: a document marker.
    [[nodiscard]] bool documentMarker(std::size_t i) const {
        const std::u16string_view t = lineAt(i);
        return (t.starts_with(u"---") || t.starts_with(u"...")) && (t.size() == 3 || isWhite(t[3]));
    }

    bool fail(Reason reason, std::size_t line) {
        if (!error_) {
            error_ = PropertiesError{reason, line};
        }
        return false;
    }

    /// Text where the subset expects something else; js-yaml names most of these "bad indentation
    /// of a mapping entry".

    /// The next line from `from` that is neither blank nor a comment; lines_.size() at the end or on
    /// an error. Checks what the structure of the block cannot hold: tabs in the indentation and
    /// document markers.
    std::size_t skipToContent(std::size_t from) {
        for (std::size_t i = from; i < lines_.size(); ++i) {
            const std::size_t f = firstNonWhite(i);
            const std::u16string_view t = lineAt(i);
            if (f == t.size() || t[f] == u'#') {
                continue;
            }
            if (f != indent(i)) {
                fail(Reason::TabInIndentation, i);
                return lines_.size();
            }
            if (documentMarker(i)) {
                fail(Reason::DocumentEnd, i);  // `...`, or `---` that starts a second document
                return lines_.size();
            }
            return i;
        }
        return lines_.size();
    }

    /// Whether line `i` has a sequence entry `- ` at column `col`.
    [[nodiscard]] bool isSequenceEntry(std::size_t i, std::size_t col) const {
        const std::u16string_view t = lineAt(i);
        return col < t.size() && t[col] == u'-' && (col + 1 == t.size() || isWhite(t[col + 1]));
    }

    std::size_t add(Node node) {
        nodes_.push_back(std::move(node));
        return nodes_.size() - 1;
    }

    std::size_t addNull() { return add(Node{}); }

    bool tooDeep(int depth, std::size_t line) { return depth > kMaxDepth && !fail(Reason::TooDeep, line); }

    // ---- keys -----------------------------------------------------------------------------

    /// A mapping key starting at (line, col): a quoted or plain scalar on the line, then `:` and a
    /// blank or the end of the line. Nothing if there is none (an error may be set).
    std::optional<Key> scanKey(std::size_t line, std::size_t col) {
        const std::u16string_view t = lineAt(line);
        if (col >= t.size()) {
            return std::nullopt;
        }
        const char16_t c = t[col];
        if (c == u'"' || c == u'\'') {
            Pos p{line, col};
            const std::size_t node = quoted(p);
            if (error_ || p.line != line) {
                return std::nullopt;
            }
            std::size_t i = p.col;
            while (i < t.size() && isWhite(t[i])) {
                ++i;
            }
            if (i < t.size() && t[i] == u':' && (i + 1 == t.size() || isWhite(t[i + 1]))) {
                return Key{nodes_[node].text, false, i};
            }
            return std::nullopt;
        }
        if (isIndicator(c)) {
            return std::nullopt;
        }
        if ((c == u'-' || c == u'?' || c == u':') && (col + 1 == t.size() || isWhite(t[col + 1]))) {
            return std::nullopt;
        }
        for (std::size_t i = col; i < t.size(); ++i) {
            if (t[i] == u':' && (i + 1 == t.size() || isWhite(t[i + 1]))) {
                return Key{std::u16string(trimRight(t.substr(col, i - col))), true, i};
            }
            if (isWhite(t[i]) && i + 1 < t.size() && t[i + 1] == u'#') {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    // ---- block collections ----------------------------------------------------------------

    /// The node starting at (line, col) in block context. `parent`: indentation of the enclosing
    /// collection (-1 at the document level); `sameLine`: the node follows its key on the key's
    /// line, where no block collection may start. Sets next_ to the line after the node.
    std::size_t blockNode(std::size_t line, std::size_t col, int parent, int depth, bool sameLine) {
        const std::u16string_view t = lineAt(line);
        const char16_t c = t[col];
        const bool blankAfter = col + 1 == t.size() || isWhite(t[col + 1]);
        if (c == u'-' && blankAfter) {
            if (sameLine) {
                fail(Reason::BadIndentation, line);
                return addNull();
            }
            return blockSequence(line, col, depth + 1);
        }
        if (c == u'?' && blankAfter) {
            fail(Reason::ComplexKey, line);
            return addNull();
        }
        if (c == u'&' || c == u'*') {
            fail(Reason::AnchorOrAlias, line);
            return addNull();
        }
        if (c == u'!') {
            fail(Reason::Tag, line);
            return addNull();
        }
        if (c == u'|' || c == u'>') {
            return blockScalar(line, col, parent);
        }
        if (const auto key = scanKey(line, col); key || error_) {
            if (error_) {
                return addNull();
            }
            if (sameLine) {
                fail(Reason::ColonInPlainValue, line);
                return addNull();
            }
            return blockMapping(line, col, depth + 1, nullptr);
        }
        if (c == u'"' || c == u'\'') {
            Pos p{line, col};
            const std::size_t node = quoted(p);
            if (!error_) {
                trailing(p, false);
            }
            next_ = p.line + 1;
            return node;
        }
        if (c == u'[' || c == u'{') {
            Pos p{line, col};
            const std::size_t node = flowCollection(p, depth + 1, nullptr);
            if (!error_) {
                trailing(p, true);
            }
            next_ = p.line + 1;
            return node;
        }
        return plainScalar(line, col, parent);
    }

    /// After a quoted scalar or a flow collection in block context: only blanks or a comment may
    /// follow on its line (a `:` would make it a complex key).
    void trailing(Pos p, bool collection) {
        const std::u16string_view t = lineAt(p.line);
        std::size_t i = p.col;
        while (i < t.size() && isWhite(t[i])) {
            ++i;
        }
        if (i == t.size() || t[i] == u'#') {
            return;  // js-yaml and ruamel.yaml take `#` right after a closing quote or bracket as a comment
        }
        if (collection && t[i] == u':' && (i + 1 == t.size() || isWhite(t[i + 1]))) {
            fail(Reason::ComplexKey, p.line);
            return;
        }
        fail(Reason::TextAfterValue, p.line);
    }

    /// A block mapping whose first key is at (line, col); its entries are at indentation `col`.
    std::size_t blockMapping(std::size_t line, std::size_t col, int depth, std::vector<TopEntry>* entries) {
        if (tooDeep(depth, line)) {
            return addNull();
        }
        Node map;
        map.kind = Node::Kind::Mapping;
        std::unordered_set<std::u16string> seen;
        std::size_t i = line;
        while (true) {
            const auto key = scanKey(i, col);
            if (error_) {
                return addNull();
            }
            if (!key) {
                if (isSequenceEntry(i, col)) {
                    fail(Reason::BadIndentation, i);
                } else {
                    fail(Reason::NotAMapping, i);
                }
                return addNull();
            }
            std::u16string name = jsKey(key->text, key->plain);
            if (!seen.insert(name).second) {
                fail(Reason::DuplicateKey, i);
                return addNull();
            }
            const std::u16string_view t = lineAt(i);
            std::size_t v = key->colon + 1;
            while (v < t.size() && isWhite(t[v])) {
                ++v;
            }
            std::size_t value = 0;
            Pos valuePos{i, key->colon + 1};
            if (v == t.size() || t[v] == u'#') {
                const std::size_t j = skipToContent(i + 1);
                if (error_) {
                    return addNull();
                }
                if (j < lines_.size() && indent(j) > col) {
                    valuePos = {j, indent(j)};
                    value = blockNode(j, indent(j), static_cast<int>(col), depth, false);
                } else if (j < lines_.size() && indent(j) == col && isSequenceEntry(j, col)) {
                    valuePos = {j, col};
                    value = blockSequence(j, col, depth + 1);
                } else {
                    value = addNull();
                    next_ = i + 1;
                }
            } else {
                valuePos = {i, v};
                blockContent_.reset();
                value = blockNode(i, v, static_cast<int>(col), depth, true);
                if ((t[v] == u'|' || t[v] == u'>') && blockContent_) {
                    valuePos = *blockContent_;  // code review O7: its first content line
                }
            }
            if (error_) {
                return addNull();
            }
            if (entries != nullptr) {
                entries->push_back(TopEntry{key->text, value, i, valuePos.line, valuePos.col});
            }
            map.keys.push_back(std::move(name));
            map.children.push_back(value);
            const std::size_t j = skipToContent(next_);
            if (error_ || j >= lines_.size() || indent(j) < col) {
                break;
            }
            if (indent(j) > col) {
                fail(Reason::BadIndentation, j);
                return addNull();
            }
            i = j;
        }
        return add(std::move(map));
    }

    /// A block sequence whose first `-` is at (line, col).
    std::size_t blockSequence(std::size_t line, std::size_t col, int depth) {
        if (tooDeep(depth, line)) {
            return addNull();
        }
        Node seq;
        seq.kind = Node::Kind::Sequence;
        std::size_t i = line;
        while (true) {
            const std::u16string_view t = lineAt(i);
            std::size_t v = col + 1;
            while (v < t.size() && isWhite(t[v])) {
                ++v;
            }
            std::size_t item = 0;
            if (v == t.size() || t[v] == u'#') {
                const std::size_t j = skipToContent(i + 1);
                if (error_) {
                    return addNull();
                }
                if (j < lines_.size() && indent(j) > col) {
                    item = blockNode(j, indent(j), static_cast<int>(col), depth, false);
                } else {
                    item = addNull();
                    next_ = i + 1;
                }
            } else {
                // A tab before a compact collection would be part of its indentation.
                const bool tab = t.substr(col + 1, v - col - 1).find(u'\t') != std::u16string_view::npos;
                if (tab && (isSequenceEntry(i, v) || scanKey(i, v))) {
                    fail(Reason::TabInIndentation, i);
                    return addNull();
                }
                item = blockNode(i, v, static_cast<int>(col), depth, false);
            }
            if (error_) {
                return addNull();
            }
            seq.children.push_back(item);
            const std::size_t j = skipToContent(next_);
            if (error_ || j >= lines_.size() || indent(j) < col) {
                break;
            }
            if (indent(j) > col) {
                fail(Reason::BadIndentation, j);
                return addNull();
            }
            if (!isSequenceEntry(j, col)) {
                break;  // a key of the mapping this sequence is the value of
            }
            i = j;
        }
        return add(std::move(seq));
    }

    // ---- scalars in block context ---------------------------------------------------------

    /// A plain scalar from (line, col), folded over the lines below that are indented beyond
    /// `parent`.
    std::size_t plainScalar(std::size_t line, std::size_t col, int parent) {
        const std::u16string_view t = lineAt(line);
        const char16_t c = t[col];
        if (isIndicator(c) || ((c == u'-' || c == u'?' || c == u':') && (col + 1 == t.size() || isWhite(t[col + 1])))) {
            fail(c == u':' ? Reason::ColonInPlainValue : Reason::ReservedCharacter, line);
            return addNull();
        }
        Node node;
        bool comment = false;
        if (!plainSegment(line, col, node.text, comment, false)) {
            return addNull();
        }
        std::size_t i = line + 1;
        std::size_t empties = 0;
        std::size_t consumed = line + 1;
        while (!comment && i < lines_.size()) {
            if (isBlankLine(i)) {
                ++empties;
                ++i;
                continue;
            }
            const std::size_t f = firstNonWhite(i);
            const std::u16string_view u = lineAt(i);
            if (static_cast<int>(indent(i)) <= parent || u[f] == u'#') {
                break;
            }
            if (f != indent(i)) {
                fail(Reason::TabInIndentation, i);
                return addNull();
            }
            if (documentMarker(i)) {
                break;
            }
            if (empties == 0) {
                node.text += u' ';
            } else {
                node.text.append(empties, u'\n');
            }
            empties = 0;
            if (!plainSegment(i, f, node.text, comment, true)) {
                return addNull();
            }
            ++i;
            consumed = i;
        }
        next_ = consumed;
        return add(std::move(node));
    }

    /// Appends the text of a plain scalar's line from `col` up to a comment or the end; no `: `
    /// may occur in it. `comment`: the line ends in a comment. `continuation`: a later line of the
    /// scalar, where `: ` is a key indented too far (code review O6).
    bool plainSegment(std::size_t line, std::size_t col, std::u16string& out, bool& comment, bool continuation) {
        const std::u16string_view t = lineAt(line);
        std::size_t end = t.size();
        for (std::size_t i = col; i < t.size(); ++i) {
            if (t[i] == u':' && (i + 1 == t.size() || isWhite(t[i + 1]))) {
                return fail(continuation ? Reason::BadIndentation : Reason::ColonInPlainValue, line);
            }
            if (isWhite(t[i]) && i + 1 < t.size() && t[i + 1] == u'#') {
                end = i;
                comment = true;
                break;
            }
        }
        out += trimRight(t.substr(col, end - col));
        return true;
    }

    /// A literal (`|`) or folded (`>`) block scalar whose header is at (line, col).
    std::size_t blockScalar(std::size_t line, std::size_t col, int parent) {
        const std::u16string_view t = lineAt(line);
        const bool literal = t[col] == u'|';
        char16_t chomp = 0;  // 0 = clip, '-' = strip, '+' = keep
        std::size_t i = col + 1;
        for (int k = 0; k < 2 && i < t.size(); ++k, ++i) {
            if ((t[i] == u'-' || t[i] == u'+') && chomp == 0) {
                chomp = t[i];
            } else if (t[i] >= u'1' && t[i] <= u'9') {
                fail(Reason::IndentationIndicator, line);
                return addNull();
            } else {
                break;
            }
        }
        const std::size_t headerEnd = i;
        while (i < t.size() && isWhite(t[i])) {
            ++i;
        }
        if (i < t.size() && !(t[i] == u'#' && i > headerEnd)) {
            fail(Reason::TextAfterValue, line);
            return addNull();
        }
        // The content's indentation: that of its first non-blank line.
        std::size_t first = line + 1;
        std::size_t leadingSpaces = 0;  // the longest leading blank line
        while (first < lines_.size() && isBlankLine(first)) {
            leadingSpaces = std::max(leadingSpaces, indent(first));
            ++first;
        }
        std::size_t contentIndent = kNone;
        if (first < lines_.size() && static_cast<int>(indent(first)) > parent) {
            contentIndent = indent(first);
            if (leadingSpaces > contentIndent) {
                fail(Reason::BadIndentation, first);
                return addNull();
            }
            blockContent_ = Pos{first, contentIndent};
        }
        // Collect the content lines (kNone = an empty line) up to the first less indented line.
        Node node;
        node.style = Node::Style::Block;
        std::size_t end = line + 1;
        std::size_t trailingEmpties = 0;
        std::u16string& out = node.text;
        bool any = false;
        bool previousMore = false;
        std::size_t empties = 0;
        for (std::size_t k = line + 1; k < lines_.size(); ++k) {
            const std::u16string_view u = lineAt(k);
            const std::size_t spaces = indent(k);
            const bool blank =
                isBlankLine(k) && (contentIndent == kNone || spaces <= contentIndent || firstNonWhite(k) != spaces);
            if (blank) {
                ++empties;
                end = k + 1;
                continue;
            }
            if (contentIndent == kNone || spaces < contentIndent) {
                break;
            }
            const std::u16string_view content = u.substr(contentIndent);
            const bool more = !content.empty() && isWhite(content[0]);
            if (!any) {
                out.append(empties, u'\n');
            } else if (!literal && !more && !previousMore) {
                // Folded: a line break between two lines of text becomes a space, empty lines stay.
                if (empties == 0) {
                    out += u' ';
                } else {
                    out.append(empties, u'\n');
                }
            } else {
                out.append(1 + empties, u'\n');  // literal, or next to a more indented line
            }
            out += content;
            any = true;
            previousMore = more;
            empties = 0;
            end = k + 1;
        }
        trailingEmpties = empties;
        if (any) {
            if (chomp == u'+') {
                out.append(1 + trailingEmpties, u'\n');
            } else if (chomp == 0) {
                out += u'\n';
            }
        } else if (chomp == u'+') {
            out.append(trailingEmpties, u'\n');
        }
        next_ = end;
        return add(std::move(node));
    }

    // ---- quoted scalars (block and flow context) ------------------------------------------

    /// A single- or double-quoted scalar from p, folded over lines; p ends after the closing quote.
    std::size_t quoted(Pos& p) {
        const Pos open = p;
        const char16_t q = lineAt(p.line)[p.col];
        ++p.col;
        Node node;
        node.style = Node::Style::Quoted;
        std::u16string& out = node.text;
        std::size_t whiteStart = kNone;  // where a run of literal blanks at the end of `out` starts
        while (true) {
            const std::u16string_view t = lineAt(p.line);
            bool escapedBreak = false;
            while (p.col < t.size()) {
                const char16_t c = t[p.col];
                if (q == u'\'' && c == u'\'') {
                    if (p.col + 1 < t.size() && t[p.col + 1] == u'\'') {
                        out += u'\'';
                        whiteStart = kNone;
                        p.col += 2;
                        continue;
                    }
                    ++p.col;
                    return add(std::move(node));
                }
                if (q == u'"' && c == u'"') {
                    ++p.col;
                    return add(std::move(node));
                }
                if (q == u'"' && c == u'\\') {
                    if (p.col + 1 == t.size()) {
                        escapedBreak = true;
                        p.col = t.size();
                        break;
                    }
                    if (!escape(t, p.col, out)) {
                        fail(Reason::UnknownEscape, p.line);
                        return addNull();
                    }
                    whiteStart = kNone;
                    continue;
                }
                if (isWhite(c)) {
                    if (whiteStart == kNone) {
                        whiteStart = out.size();
                    }
                } else {
                    whiteStart = kNone;
                }
                out += c;
                ++p.col;
            }
            if (!escapedBreak && whiteStart != kNone) {
                out.resize(whiteStart);
            }
            whiteStart = kNone;
            std::size_t empties = 0;
            ++p.line;
            while (p.line < lines_.size() && isBlankLine(p.line)) {
                ++empties;
                ++p.line;
            }
            if (p.line >= lines_.size()) {
                fail(Reason::UnclosedQuote, open.line);
                return addNull();
            }
            if (documentMarker(p.line)) {
                fail(Reason::DocumentEnd, p.line);
                return addNull();
            }
            p.col = firstNonWhite(p.line);
            if (!escapedBreak && empties == 0) {
                out += u' ';  // a line break between two lines of text
            } else {
                out.append(empties, u'\n');  // empty lines stay; an escaped break adds nothing
            }
        }
    }

    /// A double-quoted escape at t[pos] == '\\'; advances pos past it. False if it is unknown.
    static bool escape(std::u16string_view t, std::size_t& pos, std::u16string& out) {
        const char16_t c = t[pos + 1];
        pos += 2;
        switch (c) {
            case u'0':
                out += u'\0';
                return true;
            case u'a':
                out += u'\a';
                return true;
            case u'b':
                out += u'\b';
                return true;
            case u't':
            case u'\t':
                out += u'\t';
                return true;
            case u'n':
                out += u'\n';
                return true;
            case u'v':
                out += u'\v';
                return true;
            case u'f':
                out += u'\f';
                return true;
            case u'r':
                out += u'\r';
                return true;
            case u'e':
                out += u'\x1B';
                return true;
            case u' ':
            case u'"':
            case u'/':
            case u'\\':
                out += c;
                return true;
            case u'N':
                out += u'\u0085';
                return true;
            case u'_':
                out += u' ';
                return true;
            case u'L':
                out += u' ';
                return true;
            case u'P':
                out += u' ';
                return true;
            case u'x':
            case u'u':
            case u'U': {
                const std::size_t count = c == u'x' ? 2 : (c == u'u' ? 4 : 8);
                std::uint32_t code = 0;
                for (std::size_t i = 0; i < count; ++i) {
                    if (pos + i >= t.size() || !isHexDigit(t[pos + i])) {
                        return false;
                    }
                    code = code * 16 + static_cast<std::uint32_t>(hexValue(t[pos + i]));
                }
                pos += count;
                if (code > 0x10FFFF) {
                    return false;
                }
                if (code >= 0x10000) {
                    code -= 0x10000;
                    out += static_cast<char16_t>(0xD800 + (code >> 10U));
                    out += static_cast<char16_t>(0xDC00 + (code & 0x3FFU));
                } else {
                    out += static_cast<char16_t>(code);
                }
                return true;
            }
            default:
                return false;
        }
    }

    // ---- flow collections -----------------------------------------------------------------

    /// Skips blanks, line breaks and comments; false at the end of the input or on an error.
    bool skipFlowSpace(Pos& p) {
        while (p.line < lines_.size()) {
            const std::u16string_view t = lineAt(p.line);
            while (p.col < t.size() && isWhite(t[p.col])) {
                ++p.col;
            }
            if (p.col < t.size() && t[p.col] == u'#' && (p.col == 0 || isWhite(t[p.col - 1]))) {
                p.col = t.size();
            }
            if (p.col < t.size()) {
                return true;
            }
            ++p.line;
            p.col = 0;
            if (p.line < lines_.size() && documentMarker(p.line)) {
                return fail(Reason::DocumentEnd, p.line);
            }
        }
        return false;
    }

    [[nodiscard]] char16_t at(Pos p) const {
        const std::u16string_view t = lineAt(p.line);
        return p.col < t.size() ? t[p.col] : u'\n';
    }

    /// Whether the character after p ends a plain scalar's `:` in flow context.
    [[nodiscard]] bool colonEnds(Pos p) const {
        const std::u16string_view t = lineAt(p.line);
        return p.col + 1 >= t.size() || isWhite(t[p.col + 1]) || isFlowIndicator(t[p.col + 1]);
    }

    /// A flow sequence or mapping at p (`[` or `{`); p ends after its closing bracket. `entries`:
    /// collect the keys of the top-level mapping.
    std::size_t flowCollection(Pos& p, int depth, std::vector<TopEntry>* entries) {
        const Pos open = p;
        if (tooDeep(depth, p.line)) {
            return addNull();
        }
        const bool mapping = at(p) == u'{';
        const char16_t closing = mapping ? u'}' : u']';
        ++p.col;
        Node node;
        node.kind = mapping ? Node::Kind::Mapping : Node::Kind::Sequence;
        std::unordered_set<std::u16string> seen;
        const auto unclosed = [&]() {
            fail(Reason::UnclosedFlowCollection, open.line);
            return addNull();
        };
        while (true) {
            if (!skipFlowSpace(p)) {
                return error_ ? addNull() : unclosed();
            }
            if (at(p) == closing) {
                ++p.col;
                return add(std::move(node));
            }
            const Pos keyPos = p;
            if (at(p) == u'?' && colonEnds(p)) {
                fail(Reason::ComplexKey, p.line);
                return addNull();
            }
            const char16_t first = at(p);
            const bool jsonLike = first == u'"' || first == u'\'' || first == u'[' || first == u'{';
            const std::size_t key = flowNode(p, depth);
            if (error_) {
                return addNull();
            }
            if (!skipFlowSpace(p)) {
                return error_ ? addNull() : unclosed();
            }
            std::size_t value = kNone;
            Pos valuePos = p;
            if (at(p) == u':' && (jsonLike || colonEnds(p))) {
                if (p.line != keyPos.line) {
                    fail(Reason::TextAfterValue, p.line);  // an implicit key and its `:` stay on one line
                    return addNull();
                }
                ++p.col;
                if (!skipFlowSpace(p)) {
                    return error_ ? addNull() : unclosed();
                }
                valuePos = p;
                if (at(p) == u',' || at(p) == closing) {
                    value = addNull();
                } else {
                    value = flowNode(p, depth);
                    if (error_) {
                        return addNull();
                    }
                }
            }
            if (mapping || value != kNone) {
                if (nodes_[key].kind != Node::Kind::Scalar) {
                    fail(Reason::ComplexKey, keyPos.line);
                    return addNull();
                }
                if (value == kNone) {
                    value = addNull();
                }
                const Node& k = nodes_[key];
                std::u16string name = jsKey(k.text, k.style == Node::Style::Plain);
                if (mapping) {
                    if (!seen.insert(name).second) {
                        fail(Reason::DuplicateKey, keyPos.line);
                        return addNull();
                    }
                    if (entries != nullptr) {
                        entries->push_back(TopEntry{k.text, value, keyPos.line, valuePos.line, valuePos.col});
                    }
                    node.keys.push_back(std::move(name));
                    node.children.push_back(value);
                } else {
                    // `[a: b]`: a single pair, a mapping of one key.
                    if (tooDeep(depth + 1, keyPos.line)) {
                        return addNull();
                    }
                    Node pair;
                    pair.kind = Node::Kind::Mapping;
                    pair.keys.push_back(std::move(name));
                    pair.children.push_back(value);
                    node.children.push_back(add(std::move(pair)));
                }
            } else {
                node.children.push_back(key);
            }
            if (!skipFlowSpace(p)) {
                return error_ ? addNull() : unclosed();
            }
            if (at(p) == u',') {
                ++p.col;
            } else if (at(p) != closing) {
                fail(Reason::TextAfterValue, p.line);
                return addNull();
            }
        }
    }

    /// A node inside a flow collection at p.
    std::size_t flowNode(Pos& p, int depth) {
        const char16_t c = at(p);
        if (c == u'[' || c == u'{') {
            return flowCollection(p, depth + 1, nullptr);
        }
        if (c == u'"' || c == u'\'') {
            return quoted(p);
        }
        if (c == u'&' || c == u'*') {
            fail(Reason::AnchorOrAlias, p.line);
            return addNull();
        }
        if (c == u'!') {
            fail(Reason::Tag, p.line);
            return addNull();
        }
        if (isIndicator(c) || ((c == u'-' || c == u'?' || c == u':') && colonEnds(p))) {
            fail(c == u':' ? Reason::ColonInPlainValue : Reason::ReservedCharacter, p.line);
            return addNull();
        }
        // A plain scalar, folded over lines, up to a flow indicator, `: ` or a comment.
        Node node;
        while (true) {
            const std::u16string_view t = lineAt(p.line);
            std::size_t i = p.col;
            bool stopped = false;
            for (; i < t.size(); ++i) {
                if (isFlowIndicator(t[i]) || (t[i] == u':' && colonEnds({p.line, i})) ||
                    (isWhite(t[i]) && i + 1 < t.size() && t[i + 1] == u'#')) {
                    stopped = true;
                    break;
                }
            }
            node.text += trimRight(t.substr(p.col, i - p.col));
            p.col = i;
            if (stopped) {
                break;
            }
            // At the end of the line: the scalar goes on if the next non-blank line goes on with text.
            std::size_t next = p.line + 1;
            std::size_t empties = 0;
            while (next < lines_.size() && isBlankLine(next)) {
                ++empties;
                ++next;
            }
            if (next >= lines_.size() || documentMarker(next)) {
                break;
            }
            const std::size_t f = firstNonWhite(next);
            const std::u16string_view u = lineAt(next);
            if (isFlowIndicator(u[f]) || u[f] == u'#' || (u[f] == u':' && colonEnds({next, f}))) {
                break;
            }
            if (empties == 0) {
                node.text += u' ';
            } else {
                node.text.append(empties, u'\n');
            }
            p = {next, f};
        }
        return add(std::move(node));
    }

    // ---- rows -----------------------------------------------------------------------------

    void appendJson(std::size_t index, std::u16string& out) const {
        const Node& n = nodes_[index];
        switch (n.kind) {
            case Node::Kind::Scalar:
                appendScalarJson(n, out);
                return;
            case Node::Kind::Sequence:
                out += u'[';
                for (std::size_t i = 0; i < n.children.size(); ++i) {
                    if (i > 0) {
                        out += u',';
                    }
                    appendJson(n.children[i], out);
                }
                out += u']';
                return;
            case Node::Kind::Mapping:
                out += u'{';
                for (std::size_t i = 0; i < n.children.size(); ++i) {
                    if (i > 0) {
                        out += u',';
                    }
                    appendJsonString(n.keys[i], out);
                    out += u':';
                    appendJson(n.children[i], out);
                }
                out += u'}';
                return;
        }
    }

    /// The text of a list item or tag.
    [[nodiscard]] static std::u16string itemText(const Node& n) { return n.text; }

    [[nodiscard]] Property row(const TopEntry& e) const {
        Property p;
        p.key = e.key;
        p.keyLine = e.keyLine;
        p.valueLine = e.valueLine;
        p.valueColumn = e.valueColumn;
        const Node& v = nodes_[e.value];
        const bool tags = e.key == u"tags";
        const bool listKey = e.key == u"aliases" || e.key == u"cssclasses";
        const bool null = v.kind == Node::Kind::Scalar && v.style == Node::Style::Plain && isNull(v.text);
        if (v.kind == Node::Kind::Scalar && (tags || listKey)) {
            p.type = tags ? PropertyType::Tags : PropertyType::List;
            if (!null) {
                p.items.push_back(itemText(v));
            }
            return p;
        }
        if (v.kind == Node::Kind::Scalar) {
            p.text = v.text;
            if (v.style != Node::Style::Plain) {
                return p;
            }
            if (null) {
                p.text.clear();
            } else if (const int b = boolean(v.text); b >= 0) {
                p.type = PropertyType::Checkbox;
                p.checked = b == 1;
            } else if (number(v.text)) {
                p.type = PropertyType::Number;
            } else if (const auto d = date(v.text)) {
                p.type = d->hour < 0 ? PropertyType::Date : PropertyType::DateTime;
                p.date = *d;
            }
            return p;
        }
        if (v.kind == Node::Kind::Sequence) {
            bool scalars = true;
            for (const std::size_t c : v.children) {
                scalars = scalars && nodes_[c].kind == Node::Kind::Scalar;
            }
            if (scalars) {
                p.type = tags ? PropertyType::Tags : PropertyType::List;
                p.items.reserve(v.children.size());
                for (const std::size_t c : v.children) {
                    p.items.push_back(itemText(nodes_[c]));
                }
                return p;
            }
        }
        p.type = PropertyType::Unsupported;
        appendJson(e.value, p.text);
        return p;
    }

    std::span<const std::u16string> lines_;
    std::vector<Node> nodes_;
    std::vector<TopEntry> top_;
    std::optional<PropertiesError> error_;
    std::size_t next_ = 0;             ///< the line after the block node parsed last
    std::optional<Pos> blockContent_;  ///< first content of the block scalar parsed last
};

}  // namespace

std::u16string prettyJson(std::u16string_view json, std::size_t maxLines) {
    // The input is compact JSON as appendJson writes it: no white space outside strings.
    std::u16string out;
    std::size_t lines = 1;
    std::size_t depth = 0;
    const auto newline = [&] {  // false: cut here
        if (lines >= maxLines) {
            out += u"\n\u2026";
            return false;
        }
        ++lines;
        out += u'\n';
        out.append(2 * depth, u' ');
        return true;
    };
    bool inString = false;
    for (std::size_t i = 0; i < json.size(); ++i) {
        const char16_t c = json[i];
        if (inString) {
            out += c;
            if (c == u'\\' && i + 1 < json.size()) {
                out += json[++i];
            } else if (c == u'"') {
                inString = false;
            }
            continue;
        }
        switch (c) {
            case u'"':
                inString = true;
                out += c;
                break;
            case u'{':
            case u'[':
                out += c;
                if (i + 1 < json.size() && (json[i + 1] == u'}' || json[i + 1] == u']')) {
                    out += json[++i];  // empty: [] or {}
                    break;
                }
                ++depth;
                if (!newline()) {
                    return out;
                }
                break;
            case u'}':
            case u']':
                depth = depth > 0 ? depth - 1 : 0;
                if (!newline()) {
                    return out;
                }
                out += c;
                break;
            case u',':
                out += c;
                if (!newline()) {
                    return out;
                }
                break;
            case u':':
                out += u": ";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

bool isUnsupported(PropertiesErrorReason reason) noexcept {
    switch (reason) {
        case Reason::AnchorOrAlias:
        case Reason::Tag:
        case Reason::ComplexKey:
        case Reason::Directive:
        case Reason::IndentationIndicator:
            return true;
        default:
            return false;
    }
}

std::string_view reasonText(PropertiesErrorReason reason) noexcept {
    switch (reason) {
        case Reason::DuplicateKey:
            return "duplicate key";
        case Reason::BadIndentation:
            return "bad indentation";
        case Reason::TabInIndentation:
            return "tab in indentation";
        case Reason::UnclosedQuote:
            return "unclosed quote";
        case Reason::UnclosedFlowCollection:
            return "unclosed flow collection";
        case Reason::NotAMapping:
            return "not a mapping";
        case Reason::DocumentEnd:
            return "document end";
        case Reason::TooDeep:
            return "too deep";
        case Reason::AnchorOrAlias:
            return "anchor or alias";
        case Reason::Tag:
            return "tag";
        case Reason::ComplexKey:
            return "complex key";
        case Reason::Directive:
            return "directive";
        case Reason::IndentationIndicator:
            return "indentation indicator";
        case Reason::ColonInPlainValue:
            return "colon in a plain value (quote it)";
        case Reason::UnknownEscape:
            return "unknown escape";
        case Reason::TextAfterValue:
            return "text after a value";
        case Reason::ReservedCharacter:
            return "reserved character (quote it)";
    }
    return {};
}

Properties parseProperties(std::span<const std::u16string> yamlLines) {
    return Parser(yamlLines).run();
}

}  // namespace hitokoto::core
