// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links and titles. Delimiters are concealed when the caret is elsewhere,
// and revealed with the marker colour on the active line.
#include "hitokoto/core/markdown.hpp"

#include "../inline_steps.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace hitokoto::core {
namespace {

bool isAsciiPunctuation(char16_t c) noexcept {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

bool escaped(std::u16string_view line, std::size_t position, detail::InlineSteps& steps) noexcept {
    std::size_t slashes = 0;
    while (position > slashes && line[position - slashes - 1] == u'\\') {
        ++slashes;
    }
    steps.add(slashes + 1);
    return slashes % 2 != 0;
}

std::u16string unescapePunctuation(std::u16string_view text) {
    std::u16string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\\' && i + 1 < text.size() && isAsciiPunctuation(text[i + 1])) {
            result.push_back(text[i + 1]);
            ++i;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

std::size_t findClosingBacktick(std::u16string_view line, std::size_t opener, detail::InlineSteps& steps) noexcept {
    for (std::size_t j = opener + 1; j < line.size(); ++j) {
        steps.add();
        if (line[j] == u'`' && line[j - 1] != u'`' && (j + 1 >= line.size() || line[j + 1] != u'`')) {
            return j;
        }
    }
    return std::u16string_view::npos;
}

bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

/// Where reading a link destination or a link title ends, for every position of a line it could
/// start at. A line can hold many `](` that never become a link (`[](` repeated: every one reads
/// on to the end of the line, to find its parentheses unbalanced there); answering each from its
/// position onwards made such a line quadratic. The answer for a position follows from the answers
/// for the positions to its right, so one pass from the end of the line gives them all. The tables
/// are built when the first `](` asks.
class LinkScans {
public:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFU;

    LinkScans(std::u16string_view line, detail::InlineSteps& steps) : line_(line), steps_(steps) {}

    /// A destination without angle brackets that starts at `pos`: the index where it stops with its
    /// parentheses balanced (a blank, a `)` that closes none of its own, or the end of the line),
    /// or kNone if a blank or the end of the line comes while a parenthesis is open. A backslash
    /// takes the next character with it.
    [[nodiscard]] std::uint32_t destinationEnd(std::size_t pos) {
        if (destination_.empty()) {
            buildDestination();
        }
        return destination_[pos];
    }

    /// The first `delimiter` (`"`, `'` or `)`) at or after `pos` that no backslash takes with it,
    /// or kNone if a line break or the end of the line comes first.
    [[nodiscard]] std::uint32_t titleEnd(std::size_t pos, char16_t delimiter) {
        auto& table = title_[delimiter == u'"' ? 0U : delimiter == u'\'' ? 1U : 2U];
        if (table.empty()) {
            buildTitle(table, delimiter);
        }
        return table[pos];
    }

private:
    void buildDestination() {
        const std::size_t size = line_.size();
        steps_.add(size + 1);
        destination_.assign(size + 1, static_cast<std::uint32_t>(size));
        for (std::size_t p = size; p-- > 0;) {
            const char16_t c = line_[p];
            if (isBlank(c)) {
                destination_[p] = static_cast<std::uint32_t>(p);
            } else if (c == u'\\' && p + 1 < size) {
                destination_[p] = destination_[p + 2];
            } else if (c == u'(') {
                // Balanced if what follows stops at a `)`: that one closes this parenthesis, and
                // the destination goes on behind it.
                const std::uint32_t inner = destination_[p + 1];
                destination_[p] =
                    inner != kNone && inner < size && line_[inner] == u')' ? destination_[inner + 1] : kNone;
            } else if (c == u')') {
                destination_[p] = static_cast<std::uint32_t>(p);
            } else {
                destination_[p] = destination_[p + 1];
            }
        }
    }

    void buildTitle(std::vector<std::uint32_t>& table, char16_t delimiter) {
        const std::size_t size = line_.size();
        steps_.add(size + 1);
        table.assign(size + 1, kNone);
        for (std::size_t p = size; p-- > 0;) {
            const char16_t c = line_[p];
            if (c == u'\\' && p + 1 < size) {
                table[p] = table[p + 2];
            } else if (c == delimiter) {
                table[p] = static_cast<std::uint32_t>(p);
            } else if (c == u'\n' || c == u'\r') {
                table[p] = kNone;
            } else {
                table[p] = table[p + 1];
            }
        }
    }

    std::u16string_view line_;
    detail::InlineSteps& steps_;
    std::vector<std::uint32_t> destination_;
    std::array<std::vector<std::uint32_t>, 3> title_;
};

/// What follows `](`: where the destination and the title lie in the line, and the closing `)`.
/// The texts are taken out of the line only once the link stands.
struct DestinationAndTitle {
    std::size_t endParen = std::u16string_view::npos;
    std::size_t destinationStart = 0;
    std::size_t destinationLength = 0;
    std::size_t titleStart = 0;
    std::size_t titleLength = 0;
};

/// The title that starts with the delimiter at `pos`, and the `)` behind it; `result.endParen`
/// stays npos if there is none.
void parseTitle(std::u16string_view line, std::size_t pos, LinkScans& scans, detail::InlineSteps& steps,
                DestinationAndTitle& result) {
    const char16_t openDelim = line[pos];
    const char16_t closeDelim = (openDelim == u'(') ? u')' : openDelim;
    const std::size_t titleStart = pos + 1;
    const std::uint32_t close = scans.titleEnd(titleStart, closeDelim);
    if (close == LinkScans::kNone) {
        return;
    }
    pos = close + std::size_t{1};
    while (pos < line.size() && isBlank(line[pos])) {
        steps.add();
        ++pos;
    }
    if (pos < line.size() && line[pos] == u')') {
        result.titleStart = titleStart;
        result.titleLength = close - titleStart;
        result.endParen = pos;
    }
}

/// After the destination, at `pos`: blanks, then `)`, or a title and `)`.
void parseAfterDestination(std::u16string_view line, std::size_t pos, LinkScans& scans, detail::InlineSteps& steps,
                           DestinationAndTitle& result) {
    bool hadSpace = false;
    while (pos < line.size() && isBlank(line[pos])) {
        steps.add();
        hadSpace = true;
        ++pos;
    }
    if (pos < line.size() && line[pos] == u')') {
        result.endParen = pos;
        return;
    }
    if (hadSpace && pos < line.size() && (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(')) {
        parseTitle(line, pos, scans, steps, result);
    }
}

DestinationAndTitle parseDestinationAndTitle(std::u16string_view line, std::size_t pos, LinkScans& scans,
                                             detail::InlineSteps& steps) {
    while (pos < line.size() && isBlank(line[pos])) {
        steps.add();
        ++pos;
    }
    if (pos >= line.size()) {
        return {};
    }

    DestinationAndTitle result;
    if (line[pos] == u')') {
        result.endParen = pos;
        return result;
    }

    if (line[pos] == u'<') {
        // Stops at the next `<` or `>`, and the `<` of a later destination is one: these scans
        // do not overlap.
        ++pos;
        const std::size_t destStart = pos;
        bool validAngle = false;
        while (pos < line.size()) {
            steps.add();
            if (line[pos] == u'\\' && pos + 1 < line.size()) {
                pos += 2;
            } else if (line[pos] == u'<' || line[pos] == u'\n' || line[pos] == u'\r') {
                break;
            } else if (line[pos] == u'>') {
                result.destinationStart = destStart;
                result.destinationLength = pos - destStart;
                ++pos;
                validAngle = true;
                break;
            } else {
                ++pos;
            }
        }
        if (validAngle) {
            parseAfterDestination(line, pos, scans, steps, result);
        }
        return result;
    }

    const std::uint32_t end = scans.destinationEnd(pos);
    if (end == LinkScans::kNone) {
        return {};
    }
    result.destinationStart = pos;
    result.destinationLength = end - pos;
    parseAfterDestination(line, end, scans, steps, result);
    return result;
}

class LinkElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "link"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"["; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        detail::InlineSteps steps;
        std::vector<bool> inCode(line.size(), false);
        for (const auto& span : out.opaqueSpans) {
            if (span.start < line.size()) {
                const auto end = std::min<std::size_t>(span.end(), line.size());
                steps.add(end - span.start);
                std::fill(inCode.begin() + static_cast<std::ptrdiff_t>(span.start),
                          inCode.begin() + static_cast<std::ptrdiff_t>(end), true);
            }
        }

        struct ParsedLink {
            std::size_t openBracket = 0;
            std::size_t closeBracket = 0;
            DestinationAndTitle target;
        };
        struct OpenBracket {
            std::size_t index = 0;
            bool isImage = false;
        };
        std::vector<ParsedLink> parsedLinks;
        std::vector<OpenBracket> openBrackets;
        LinkScans scans(line, steps);
        // Once no single backtick closes a span from some position on, none does from any later one.
        bool noClosingBacktick = false;

        for (std::size_t i = 0; i < line.size();) {
            steps.add();
            if (inCode[i]) {
                ++i;
                continue;
            }
            if (line[i] == u'\\' && i + 1 < line.size()) {
                i += 2;
                continue;
            }
            if (!noClosingBacktick && line[i] == u'`' && (i == 0 || line[i - 1] != u'`') &&
                (i + 1 >= line.size() || line[i + 1] != u'`')) {
                const auto close = findClosingBacktick(line, i, steps);
                if (close != std::u16string_view::npos) {
                    i = close + 1;
                    continue;
                }
                noClosingBacktick = true;
            }
            if (line[i] == u'[') {
                const bool isImage = (i > 0 && line[i - 1] == u'!' && !escaped(line, i - 1, steps));
                openBrackets.push_back({i, isImage});
                ++i;
                continue;
            }
            if (line[i] == u']') {
                if (openBrackets.empty()) {
                    ++i;
                    continue;
                }
                if (i + 1 < line.size() && line[i + 1] == u'(') {
                    const auto parsed = parseDestinationAndTitle(line, i + 2, scans, steps);
                    if (parsed.endParen != std::u16string_view::npos) {
                        const auto open = openBrackets.back();
                        openBrackets.pop_back();
                        if (open.isImage) {
                            // O8/O13: Images stay literal; does not form a link and does not deactivate outer links.
                            i = parsed.endParen + 1;
                            continue;
                        }
                        parsedLinks.push_back({open.index, i, parsed});
                        // O6: CommonMark forbids links inside links; outer brackets cannot form links.
                        steps.add(openBrackets.size());
                        openBrackets.clear();
                        i = parsed.endParen + 1;
                        continue;
                    }
                }
                openBrackets.pop_back();
                ++i;
                continue;
            }
            ++i;
        }

        std::vector<StyleRun> newRuns;
        for (const auto& link : parsedLinks) {
            const std::size_t endParen = link.target.endParen;
            // A link's destination and title lie inside it, and links do not overlap.
            steps.add(endParen - link.openBracket);
            LinkTarget target;
            target.span = {static_cast<std::uint32_t>(link.openBracket + 1),
                           static_cast<std::uint32_t>(link.closeBracket - link.openBracket - 1)};
            target.full = {static_cast<std::uint32_t>(link.openBracket),
                           static_cast<std::uint32_t>(endParen - link.openBracket + 1)};
            target.destination =
                unescapePunctuation(line.substr(link.target.destinationStart, link.target.destinationLength));
            target.title = unescapePunctuation(line.substr(link.target.titleStart, link.target.titleLength));

            out.inlineContexts.push_back(target.span);
            out.links.push_back(std::move(target));

            const auto marker1 = Span{static_cast<std::uint32_t>(link.openBracket), 1};
            const auto marker2 = Span{static_cast<std::uint32_t>(link.closeBracket),
                                      static_cast<std::uint32_t>(endParen - link.closeBracket + 1)};

            out.opaqueSpans.push_back(marker1);
            out.opaqueSpans.push_back(marker2);

            if (!context.revealed) {
                out.hidden.push_back({marker1, HideMode::Collapse});
                out.hidden.push_back({marker2, HideMode::Collapse});
            } else {
                newRuns.push_back({marker1, Role::LinkMarker, 0});
                newRuns.push_back({marker2, Role::LinkMarker, 0});
            }

            const std::uint32_t textStart = static_cast<std::uint32_t>(link.openBracket + 1);
            const std::uint32_t textEnd = static_cast<std::uint32_t>(link.closeBracket);
            if (textStart < textEnd) {
                newRuns.push_back({{textStart, textEnd - textStart}, Role::LinkText, 0});
            }
        }
        out.runs.insert(out.runs.end(), newRuns.begin(), newRuns.end());
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeLinkElement() {
    return std::make_unique<LinkElement>();
}

}  // namespace hitokoto::core
