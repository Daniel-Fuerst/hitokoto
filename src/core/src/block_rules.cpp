// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_rules.hpp"

#include <array>

namespace hitokoto::core::blocks {
namespace {

constexpr std::uint32_t kTabStop = 4;
constexpr std::uint32_t kMaxIndent = 3;  ///< more is indented code (CommonMark 4.4)

char16_t lower(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

bool isAsciiLetter(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

bool isDigit(char16_t c) noexcept {
    return c >= u'0' && c <= u'9';
}

/// Case-insensitive: does `line` continue with the ASCII word `word` at `at`?
bool hasWordAt(std::u16string_view line, std::size_t at, std::string_view word) noexcept {
    if (at + word.size() > line.size()) {
        return false;
    }
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (lower(line[at + i]) != static_cast<char16_t>(word[i])) {
            return false;
        }
    }
    return true;
}

bool onlyBlanksFrom(std::u16string_view line, std::size_t from) noexcept {
    for (std::size_t i = from; i < line.size(); ++i) {
        if (!isBlank(line[i])) {
            return false;
        }
    }
    return true;
}

// ---- HTML blocks (CommonMark 4.6), as commonmark.js recognises their start ------------------

constexpr std::array<std::string_view, 4> kRawTags = {"script", "pre", "textarea", "style"};
constexpr std::array<std::string_view, 62> kBlockTags = {
    "address",  "article",    "aside",  "base",    "basefont", "blockquote", "body",     "caption",  "center",
    "col",      "colgroup",   "dd",     "details", "dialog",   "dir",        "div",      "dl",       "dt",
    "fieldset", "figcaption", "figure", "footer",  "form",     "frame",      "frameset", "h1",       "h2",
    "h3",       "h4",         "h5",     "h6",      "head",     "header",     "hr",       "html",     "iframe",
    "legend",   "li",         "link",   "main",    "menu",     "menuitem",   "nav",      "noframes", "ol",
    "optgroup", "option",     "p",      "param",   "section",  "search",     "summary",  "table",    "tbody",
    "td",       "tfoot",      "th",     "thead",   "title",    "tr",         "track",    "ul"};

/// Length of the tag name at `at` ([A-Za-z][A-Za-z0-9-]*), 0 if there is none.
std::size_t tagNameLength(std::u16string_view line, std::size_t at) noexcept {
    if (at >= line.size() || !isAsciiLetter(line[at])) {
        return 0;
    }
    std::size_t i = at + 1;
    while (i < line.size() && (isAsciiLetter(line[i]) || isDigit(line[i]) || line[i] == u'-')) {
        ++i;
    }
    return i - at;
}

bool isTagName(std::u16string_view line, std::size_t at, std::size_t length, std::string_view name) noexcept {
    return length == name.size() && hasWordAt(line, at, name);
}

/// Types 1 to 6: blocks that may interrupt a paragraph.
bool startsInterruptingHtml(std::u16string_view line, std::size_t start) noexcept {
    const std::u16string_view rest = line.substr(start);
    if (rest.size() < 2 || rest[0] != u'<') {
        return false;
    }
    if (rest.starts_with(u"<!--") || rest[1] == u'?' || rest.starts_with(u"<![CDATA[")) {
        return true;  // types 2, 3, 5
    }
    if (rest[1] == u'!' && rest.size() > 2 && isAsciiLetter(rest[2])) {
        return true;  // type 4
    }
    const bool closing = rest[1] == u'/';
    const std::size_t nameAt = start + (closing ? 2 : 1);
    const std::size_t length = tagNameLength(line, nameAt);
    if (length == 0) {
        return false;
    }
    const std::size_t after = nameAt + length;
    const bool ends = after == line.size() || isBlank(line[after]) || line[after] == u'>';
    if (!closing) {
        for (const auto name : kRawTags) {
            if (ends && isTagName(line, nameAt, length, name)) {
                return true;  // type 1
            }
        }
    }
    const bool endsBlockTag = ends || (line[after] == u'/' && after + 1 < line.size() && line[after + 1] == u'>');
    if (!endsBlockTag) {
        return false;
    }
    for (const auto name : kBlockTags) {
        if (isTagName(line, nameAt, length, name)) {
            return true;  // type 6
        }
    }
    return false;
}

std::size_t skipBlanks(std::u16string_view line, std::size_t i) noexcept {
    while (i < line.size() && isBlank(line[i])) {
        ++i;
    }
    return i;
}

bool isAttributeNameStart(char16_t c) noexcept {
    return isAsciiLetter(c) || c == u'_' || c == u':';
}

bool isAttributeNameChar(char16_t c) noexcept {
    return isAttributeNameStart(c) || isDigit(c) || c == u'.' || c == u'-';
}

bool isUnquotedValueChar(char16_t c) noexcept {
    return c > u' ' && c != u'"' && c != u'\'' && c != u'=' && c != u'<' && c != u'>' && c != u'`';
}

/// Index after an attribute value at `i`, or 0 if there is none.
std::size_t attributeValueEnd(std::u16string_view line, std::size_t i) noexcept {
    if (i >= line.size()) {
        return 0;
    }
    if (line[i] == u'"' || line[i] == u'\'') {
        const std::size_t close = line.find(line[i], i + 1);
        return close == std::u16string_view::npos ? 0 : close + 1;
    }
    std::size_t end = i;
    while (end < line.size() && isUnquotedValueChar(line[end])) {
        ++end;
    }
    return end == i ? 0 : end;
}

/// Type 7: a complete open or closing tag and nothing but blanks after it. May not interrupt a
/// paragraph.
bool isLoneTag(std::u16string_view line, std::size_t start) noexcept {
    std::size_t i = start + 1;
    const bool closing = i < line.size() && line[i] == u'/';
    i += closing ? 1 : 0;
    const std::size_t length = tagNameLength(line, i);
    if (length == 0) {
        return false;
    }
    i += length;
    if (!closing) {
        for (;;) {
            const std::size_t name = skipBlanks(line, i);
            if (name == i || name >= line.size() || !isAttributeNameStart(line[name])) {
                break;
            }
            std::size_t end = name + 1;
            while (end < line.size() && isAttributeNameChar(line[end])) {
                ++end;
            }
            i = end;
            const std::size_t equals = skipBlanks(line, end);
            if (equals < line.size() && line[equals] == u'=') {
                const std::size_t valueEnd = attributeValueEnd(line, skipBlanks(line, equals + 1));
                if (valueEnd == 0) {
                    return false;
                }
                i = valueEnd;
            }
        }
        i = skipBlanks(line, i);
        if (i < line.size() && line[i] == u'/') {
            ++i;
        }
    } else {
        i = skipBlanks(line, i);
    }
    return i < line.size() && line[i] == u'>' && onlyBlanksFrom(line, i + 1);
}

/// `[label]:` at `start` (CommonMark 4.7); the destination is not checked.
bool startsLinkReferenceDefinition(std::u16string_view line, std::size_t start) noexcept {
    constexpr std::size_t kMaxLabel = 999;
    if (line[start] != u'[') {
        return false;
    }
    bool text = false;
    for (std::size_t i = start + 1; i < line.size() && i - start <= kMaxLabel + 1; ++i) {
        const char16_t c = line[i];
        if (c == u'\\' && i + 1 < line.size()) {
            ++i;
            text = true;
        } else if (c == u'[') {
            return false;
        } else if (c == u']') {
            return text && i + 1 < line.size() && line[i + 1] == u':';
        } else if (!isBlank(c)) {
            text = true;
        }
    }
    return false;
}

}  // namespace

std::size_t firstNonBlank(std::u16string_view line) noexcept {
    return skipBlanks(line, 0);
}

std::uint32_t columnAfterBlanks(std::u16string_view line, std::size_t from, std::uint32_t column) noexcept {
    for (std::size_t i = from; i < line.size() && isBlank(line[i]); ++i) {
        column += line[i] == u'\t' ? kTabStop - column % kTabStop : 1;
    }
    return column;
}

std::uint32_t indentWidth(std::u16string_view line) noexcept {
    return columnAfterBlanks(line, 0, 0);
}

bool isThematicBreakFrom(std::u16string_view line, std::size_t start) noexcept {
    if (start >= line.size()) {
        return false;
    }
    const char16_t marker = line[start];
    if (marker != u'-' && marker != u'*' && marker != u'_') {
        return false;
    }
    std::size_t count = 0;
    for (std::size_t i = start; i < line.size(); ++i) {
        if (line[i] == marker) {
            ++count;
        } else if (!isBlank(line[i])) {
            return false;
        }
    }
    return count >= 3;
}

bool isThematicBreak(std::u16string_view line) noexcept {
    return indentWidth(line) <= kMaxIndent && isThematicBreakFrom(line, firstNonBlank(line));
}

std::uint8_t setextUnderline(std::u16string_view line, std::uint32_t column) noexcept {
    const std::uint32_t indent = indentWidth(line);
    if (indent < column || indent - column > kMaxIndent) {
        return 0;
    }
    const std::size_t start = firstNonBlank(line);
    if (start >= line.size() || (line[start] != u'=' && line[start] != u'-')) {
        return 0;
    }
    std::size_t end = start;
    while (end < line.size() && line[end] == line[start]) {
        ++end;
    }
    if (!onlyBlanksFrom(line, end)) {
        return 0;
    }
    return line[start] == u'=' ? 1 : 2;
}

bool isAtxHeadingFrom(std::u16string_view line, std::size_t start) noexcept {
    std::size_t hashes = 0;
    while (start + hashes < line.size() && hashes < 7 && line[start + hashes] == u'#') {
        ++hashes;
    }
    return hashes >= 1 && hashes <= 6 && (start + hashes == line.size() || isBlank(line[start + hashes]));
}

bool isFenceFrom(std::u16string_view line, std::size_t start) noexcept {
    if (start >= line.size() || (line[start] != u'`' && line[start] != u'~')) {
        return false;
    }
    std::size_t run = 0;
    while (start + run < line.size() && line[start + run] == line[start]) {
        ++run;
    }
    return run >= 3;
}

std::int64_t paragraphColumnAfterMarker(std::u16string_view line, std::size_t markerEnd,
                                        std::uint32_t markerEndColumn) noexcept {
    constexpr std::uint32_t kCodePadding = 5;  // CommonMark 5.2: five or more columns = one + code
    const std::size_t text = skipBlanks(line, markerEnd);
    if (text >= line.size()) {
        return -1;
    }
    const std::uint32_t padding = columnAfterBlanks(line, markerEnd, markerEndColumn) - markerEndColumn;
    if (padding >= kCodePadding || isThematicBreakFrom(line, text) || isAtxHeadingFrom(line, text) ||
        isFenceFrom(line, text)) {
        return -1;
    }
    return static_cast<std::int64_t>(markerEndColumn) + padding;
}

OtherBlock otherBlockStart(std::u16string_view line, After after) noexcept {
    const std::size_t start = firstNonBlank(line);
    if (start >= line.size()) {
        return OtherBlock::None;
    }
    const std::uint32_t indent = indentWidth(line);
    if (indent > kMaxIndent) {
        // Indented code (4.4) cannot interrupt a paragraph.
        return after == After::Paragraph || after == After::Lazy ? OtherBlock::None : OtherBlock::Opaque;
    }
    const char16_t c = line[start];
    const auto textAfter = [&](std::size_t markerEnd) {
        const std::uint32_t column = columnAfterBlanks(line, 0, 0) + static_cast<std::uint32_t>(markerEnd - start);
        return paragraphColumnAfterMarker(line, markerEnd, column) >= 0 ? OtherBlock::LazyText : OtherBlock::Opaque;
    };
    if (c == u'>') {
        return textAfter(start + 1);  // block quote (5.1)
    }
    if (isDigit(c)) {
        // Ordered list item (5.2): 1-9 digits, `.` or `)`, then a blank or the end of the line.
        std::size_t i = start;
        std::uint32_t number = 0;
        while (i < line.size() && i - start < 9 && isDigit(line[i])) {
            number = number * 10 + static_cast<std::uint32_t>(line[i] - u'0');
            ++i;
        }
        if (i >= line.size() || isDigit(line[i]) || (line[i] != u'.' && line[i] != u')') ||
            (i + 1 < line.size() && !isBlank(line[i + 1]))) {
            return OtherBlock::None;
        }
        const OtherBlock item = textAfter(i + 1);
        // Only an item numbered 1 with content interrupts a paragraph; a lazy line is outside the
        // paragraph's container, so any item starts there.
        if (after == After::Paragraph && (number != 1 || onlyBlanksFrom(line, i + 1))) {
            return OtherBlock::None;
        }
        return item;
    }
    if (c == u'<') {
        if (startsInterruptingHtml(line, start)) {
            return OtherBlock::Opaque;
        }
        return after == After::Block && isLoneTag(line, start) ? OtherBlock::Opaque : OtherBlock::None;
    }
    if (c == u'[' && after == After::Block && startsLinkReferenceDefinition(line, start)) {
        return OtherBlock::Opaque;
    }
    return OtherBlock::None;
}

}  // namespace hitokoto::core::blocks
