// SPDX-License-Identifier: GPL-3.0-or-later
// S053: ordered list items (`1.`, `1)`). The number is shown as written, never renumbered on screen:
// the text is the only truth (ADR 0002). Indentation is the nesting depth, as for bullets (S003).
// An item with text opens paragraph text at its content column (S023, ADR 0006).
#include "hitokoto/core/ordered_list.hpp"
#include "hitokoto/core/markdown.hpp"

#include "../block_rules.hpp"

#include <algorithm>

namespace hitokoto::core {
namespace {

constexpr std::uint32_t kIndentPerLevel = 2;
constexpr std::size_t kMaxDigits = 9;  // CommonMark 5.2: a longer number would overflow some browsers
constexpr std::uint32_t kMaxNumber = 999'999'999;

using blocks::isBlank;

[[nodiscard]] constexpr bool isDigit(char16_t c) noexcept {
    return c >= u'0' && c <= u'9';
}

/// Where the parts of an item lie, or nullopt if `line` is none.
struct Parsed {
    std::size_t digitsStart = 0;
    std::size_t delimiter = 0;  ///< index of `.` or `)`
    std::size_t contentStart = 0;
    std::uint32_t column = 0;  ///< column of the first digit
    std::uint32_t number = 0;
};

std::optional<Parsed> parseItem(std::u16string_view line) noexcept {
    Parsed p;
    p.digitsStart = blocks::firstNonBlank(line);
    std::size_t i = p.digitsStart;
    while (i < line.size() && isDigit(line[i])) {
        if (i - p.digitsStart == kMaxDigits) {
            return std::nullopt;
        }
        p.number = p.number * 10U + static_cast<std::uint32_t>(line[i] - u'0');
        ++i;
    }
    if (i == p.digitsStart || i >= line.size() || (line[i] != u'.' && line[i] != u')')) {
        return std::nullopt;
    }
    p.delimiter = i;
    ++i;
    if (i < line.size() && !isBlank(line[i])) {
        return std::nullopt;
    }
    while (i < line.size() && isBlank(line[i])) {
        ++i;
    }
    p.contentStart = i;
    p.column = blocks::columnAfterBlanks(line, 0, 0);
    return p;
}

/// Content column of an item's paragraph text, or -1 if it has none.
std::int64_t contentColumn(std::u16string_view line, const Parsed& p) noexcept {
    const auto delimiterEndColumn = p.column + static_cast<std::uint32_t>(p.delimiter + 1U - p.digitsStart);
    return blocks::paragraphColumnAfterMarker(line, p.delimiter + 1U, delimiterEndColumn);
}

class OrderedListElement final : public LineElement {
public:
    [[nodiscard]] std::optional<LineMatch> parse(std::u16string_view line) const override {
        const auto p = parseItem(line);
        if (!p) {
            return std::nullopt;
        }
        LineMatch m;
        m.marker = Span{0, static_cast<std::uint32_t>(p->contentStart)};
        m.markerChar = static_cast<std::uint32_t>(p->digitsStart);
        m.level = static_cast<std::uint8_t>(std::min<std::uint32_t>(p->column / kIndentPerLevel, 255));
        return m;
    }

    [[nodiscard]] ParagraphRole paragraphRole(std::u16string_view line,
                                              const LineMatch& /*match*/) const noexcept override {
        const auto p = parseItem(line);
        const std::int64_t column = p ? contentColumn(line, *p) : -1;
        if (column < 0) {
            return {};
        }
        return {ParagraphRole::Kind::Opens, static_cast<std::uint32_t>(column)};
    }

    [[nodiscard]] bool startsAfterParagraph(std::u16string_view line, const LineMatch& /*match*/,
                                            bool lazy) const noexcept override {
        // CommonMark 5.2: only an item numbered 1 with content interrupts a paragraph; a lazy line
        // is outside the paragraph's container, so any item starts there. As S050 does for
        // bullets, a marker followed by blanks only (`2. `, what Enter writes) is an empty item.
        const auto p = parseItem(line);
        if (!p) {
            return false;
        }
        const bool hasContent = p->contentStart < line.size();
        const bool markerAndBlank = !hasContent && p->delimiter + 1U < line.size();
        return lazy || (p->number == 1 && hasContent) || markerAndBlank;
    }

    void render(std::u16string_view line, const LineMatch& match, const LineContext& context,
                LineRender& out) const override {
        const auto p = parseItem(line);
        if (!p) {
            return;
        }
        const Span number{match.markerChar, static_cast<std::uint32_t>(p->delimiter + 1U - p->digitsStart)};
        // Level 0: a run's level is a heading level to the UI; the nesting depth is in the indentation.
        out.runs.push_back({number, context.revealed ? Role::ListNumberMarker : Role::ListNumber, 0});
        out.wrapIndent = match.marker.end();
    }
};

/// How a line relates to a list at `column` with `delimiter`.
enum class Membership : std::uint8_t { Item, Inside, End };

Membership membership(std::u16string_view line, std::uint32_t column, char16_t delimiter) noexcept {
    const std::size_t first = blocks::firstNonBlank(line);
    if (first == line.size()) {
        return Membership::Inside;  // a blank line keeps the items around it in one list (5.3)
    }
    const std::uint32_t indent = blocks::columnAfterBlanks(line, 0, 0);
    if (indent > column) {
        return Membership::Inside;  // nested items, continuation lines
    }
    if (indent < column) {
        return Membership::End;
    }
    const auto p = parseItem(line);
    return p && line[p->delimiter] == delimiter ? Membership::Item : Membership::End;
}

/// The nearest item at `column` with `delimiter` from `index` on in direction `step` (+1 or -1),
/// walking over blank and further indented lines; nullopt if another line comes first.
std::optional<std::size_t> siblingFrom(const LineAt& lineAt, std::size_t index, int step, std::uint32_t column,
                                       char16_t delimiter) {
    std::size_t i = index;
    while (true) {
        if (step < 0 && i == 0) {
            return std::nullopt;
        }
        i = step < 0 ? i - 1 : i + 1;
        const auto line = lineAt(i);
        if (!line) {
            return std::nullopt;
        }
        switch (membership(*line, column, delimiter)) {
            case Membership::Item:
                return i;
            case Membership::End:
                return std::nullopt;
            case Membership::Inside:
                break;
        }
    }
}

}  // namespace

std::unique_ptr<LineElement> makeOrderedListElement() {
    return std::make_unique<OrderedListElement>();
}

std::optional<OrderedLineInfo> parseOrderedLine(std::u16string_view line) {
    const auto p = parseItem(line);
    if (!p) {
        return std::nullopt;
    }
    return OrderedLineInfo{
        .column = p->column,
        .digits =
            Span{static_cast<std::uint32_t>(p->digitsStart), static_cast<std::uint32_t>(p->delimiter - p->digitsStart)},
        .number = p->number,
        .delimiter = line[p->delimiter],
        .prefixLength = static_cast<std::uint32_t>(p->contentStart),
        .isEmpty = p->contentStart >= line.size(),
    };
}

std::uint32_t orderedMarkerWidth(const OrderedLineInfo& info) noexcept {
    return info.digits.length + 2U;
}

std::u16string indentOrderedLine(std::u16string_view line) {
    const auto info = parseOrderedLine(line);
    if (!info) {
        return std::u16string(line);
    }
    return std::u16string(orderedMarkerWidth(*info), u' ') + std::u16string(line);
}

std::u16string outdentOrderedLine(std::u16string_view line) {
    const auto info = parseOrderedLine(line);
    if (!info) {
        return std::u16string(line);
    }
    const std::uint32_t width = orderedMarkerWidth(*info);
    std::size_t i = 0;
    std::uint32_t column = 0;
    while (i < line.size() && isBlank(line[i]) && column < width) {
        column = blocks::columnAfterBlanks(line.substr(0, i + 1), i, column);
        ++i;
    }
    return std::u16string(line.substr(i));
}

std::u16string nextOrderedPrefix(std::u16string_view line, const OrderedLineInfo& info) {
    const auto number = std::min(info.number + 1U, kMaxNumber);
    std::u16string out(line.substr(0, info.digits.start));
    for (const char c : std::to_string(number)) {
        out += static_cast<char16_t>(c);
    }
    out += info.delimiter;
    const std::size_t blanksStart = info.digits.end() + 1U;
    const std::u16string_view blanks =
        line.substr(std::min<std::size_t>(blanksStart, line.size()),
                    info.prefixLength - std::min<std::size_t>(blanksStart, info.prefixLength));
    out += blanks.empty() ? std::u16string_view(u" ") : blanks;
    return out;
}

std::vector<NumberEdit> renumberOrderedList(const LineAt& lineAt, std::size_t index, bool restart) {
    std::vector<NumberEdit> edits;
    const auto line = lineAt(index);
    const auto info = line ? parseOrderedLine(*line) : std::nullopt;
    if (!info) {
        return edits;
    }
    std::size_t first = index;
    while (const auto above = siblingFrom(lineAt, first, -1, info->column, info->delimiter)) {
        first = *above;
    }
    std::optional<std::size_t> item = first;
    std::uint32_t expected = 0;
    bool isFirst = true;
    while (item) {
        const auto text = lineAt(*item);
        const auto current = parseOrderedLine(*text);
        if (isFirst) {
            expected = restart ? 1U : current->number;
            isFirst = false;
        }
        if (current->number != expected) {
            std::u16string digits;
            for (const char c : std::to_string(expected)) {
                digits += static_cast<char16_t>(c);
            }
            edits.push_back({*item, current->digits, std::move(digits)});
        }
        if (expected == kMaxNumber) {
            break;  // a tenth digit would make it no item
        }
        ++expected;
        item = siblingFrom(lineAt, *item, 1, info->column, info->delimiter);
    }
    return edits;
}

std::vector<NumberEdit> renumberAfterReindent(const LineAt& lineAt, std::size_t index, std::uint32_t oldColumn,
                                              char16_t delimiter, ReindentKind kind) {
    std::vector<NumberEdit> edits;
    const auto append = [&edits](std::vector<NumberEdit> more) {
        edits.insert(edits.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
    };
    if (kind != ReindentKind::Removed) {
        const auto line = lineAt(index);
        const auto info = line ? parseOrderedLine(*line) : std::nullopt;
        // Indented: an item that is the first of its new list starts it at 1.
        const bool firstOfList = info && !siblingFrom(lineAt, index, -1, info->column, info->delimiter).has_value();
        append(renumberOrderedList(lineAt, index, kind == ReindentKind::Indented && firstOfList));
    }
    // The list the item left: from the sibling above, which reaches past the item if it is still
    // inside (indented further or removed to a blank line). Without one, from the sibling below;
    // after an outdent, the siblings below start a nested list under the item, at 1.
    const auto above = siblingFrom(lineAt, index, -1, oldColumn, delimiter);
    if (above) {
        append(renumberOrderedList(lineAt, *above));
    }
    if (!above || kind == ReindentKind::Outdented) {
        if (const auto below = siblingFrom(lineAt, index, 1, oldColumn, delimiter)) {
            append(renumberOrderedList(lineAt, *below, kind == ReindentKind::Outdented));
        }
    }
    return edits;
}

}  // namespace hitokoto::core
