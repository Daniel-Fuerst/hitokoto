// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links and titles. Delimiters are concealed when the caret is elsewhere,
// and revealed with the marker colour on the active line.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace hitokoto::core {
namespace {

bool isAsciiPunctuation(char16_t c) noexcept {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

bool escaped(std::u16string_view line, std::size_t position) noexcept {
    std::size_t slashes = 0;
    while (position > slashes && line[position - slashes - 1] == u'\\') {
        ++slashes;
    }
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

std::size_t parseTitle(std::u16string_view line, std::size_t& pos, std::u16string& title) {
    const char16_t openDelim = line[pos];
    const char16_t closeDelim = (openDelim == u'(') ? u')' : openDelim;
    ++pos;
    const std::size_t titleStart = pos;
    while (pos < line.size()) {
        if (line[pos] == u'\\' && pos + 1 < line.size()) {
            pos += 2;
        } else if (line[pos] == closeDelim) {
            title = unescapePunctuation(line.substr(titleStart, pos - titleStart));
            ++pos;
            while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
                ++pos;
            }
            if (pos < line.size() && line[pos] == u')') {
                return pos;
            }
            return std::u16string_view::npos;
        } else if (line[pos] == u'\n' || line[pos] == u'\r') {
            return std::u16string_view::npos;
        } else {
            ++pos;
        }
    }
    return std::u16string_view::npos;
}

struct DestinationAndTitle {
    std::u16string destination;
    std::u16string title;
    std::size_t endParen = std::u16string_view::npos;
};

DestinationAndTitle parseDestinationAndTitle(std::u16string_view line, std::size_t pos) {
    while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
        ++pos;
    }
    if (pos >= line.size()) {
        return {};
    }

    if (line[pos] == u')') {
        return {u"", u"", pos};
    }

    DestinationAndTitle result;

    if (line[pos] == u'<') {
        ++pos;
        const std::size_t destStart = pos;
        bool validAngle = false;
        while (pos < line.size()) {
            if (line[pos] == u'\\' && pos + 1 < line.size()) {
                pos += 2;
            } else if (line[pos] == u'<' || line[pos] == u'\n' || line[pos] == u'\r') {
                break;
            } else if (line[pos] == u'>') {
                result.destination = unescapePunctuation(line.substr(destStart, pos - destStart));
                ++pos;
                validAngle = true;
                break;
            } else {
                ++pos;
            }
        }
        if (!validAngle) {
            return {};
        }
        bool hadSpace = false;
        while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
            hadSpace = true;
            ++pos;
        }
        if (pos < line.size() && line[pos] == u')') {
            result.endParen = pos;
            return result;
        }
        if (hadSpace && pos < line.size() && (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(')) {
            result.endParen = parseTitle(line, pos, result.title);
            if (result.endParen != std::u16string_view::npos) {
                return result;
            }
        }
        return {};
    }

    // Bare destination (or title if destination omitted)
    if (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(') {
        result.endParen = parseTitle(line, pos, result.title);
        if (result.endParen != std::u16string_view::npos) {
            return result;
        }
        return {};
    }

    const std::size_t destStart = pos;
    int parenDepth = 0;
    while (pos < line.size()) {
        if (line[pos] == u' ' || line[pos] == u'\t') {
            break;
        }
        if (line[pos] == u'\\' && pos + 1 < line.size()) {
            pos += 2;
        } else if (line[pos] == u'(') {
            ++parenDepth;
            ++pos;
        } else if (line[pos] == u')') {
            if (parenDepth > 0) {
                --parenDepth;
                ++pos;
            } else {
                break;
            }
        } else {
            ++pos;
        }
    }
    if (parenDepth != 0) {
        return {};
    }

    result.destination = unescapePunctuation(line.substr(destStart, pos - destStart));
    bool hadSpace = false;
    while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
        hadSpace = true;
        ++pos;
    }
    if (pos < line.size() && line[pos] == u')') {
        result.endParen = pos;
        return result;
    }
    if (hadSpace && pos < line.size() && (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(')) {
        result.endParen = parseTitle(line, pos, result.title);
        if (result.endParen != std::u16string_view::npos) {
            return result;
        }
    }
    return {};
}

class LinkElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "link"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"["; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        std::vector<bool> inCode(line.size(), false);
        for (const auto& span : out.opaqueSpans) {
            if (span.start < line.size()) {
                const auto end = std::min<std::size_t>(span.end(), line.size());
                std::fill(inCode.begin() + static_cast<std::ptrdiff_t>(span.start),
                          inCode.begin() + static_cast<std::ptrdiff_t>(end), true);
            }
        }

        struct ParsedLink {
            std::size_t openBracket = 0;
            std::size_t closeBracket = 0;
            std::size_t endParen = 0;
            std::u16string destination;
            std::u16string title;
        };
        std::vector<ParsedLink> parsedLinks;
        std::vector<std::size_t> openBrackets;

        for (std::size_t i = 0; i < line.size();) {
            if (inCode[i]) {
                ++i;
                continue;
            }
            if (line[i] == u'\\' && i + 1 < line.size()) {
                i += 2;
                continue;
            }
            if (line[i] == u'[') {
                // O8: ![alt](src) is an image in CommonMark; keep literal.
                if (i > 0 && line[i - 1] == u'!' && !escaped(line, i - 1)) {
                    ++i;
                    continue;
                }
                openBrackets.push_back(i);
                ++i;
                continue;
            }
            if (line[i] == u']') {
                if (openBrackets.empty()) {
                    ++i;
                    continue;
                }
                if (i + 1 < line.size() && line[i + 1] == u'(') {
                    auto parsed = parseDestinationAndTitle(line, i + 2);
                    if (parsed.endParen != std::u16string_view::npos) {
                        const std::size_t open = openBrackets.back();
                        parsedLinks.push_back(
                            {open, i, parsed.endParen, std::move(parsed.destination), std::move(parsed.title)});
                        // O6: CommonMark forbids links inside links; outer brackets cannot form links.
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
        for (auto& pl : parsedLinks) {
            LinkTarget target;
            target.span = {static_cast<std::uint32_t>(pl.openBracket + 1),
                           static_cast<std::uint32_t>(pl.closeBracket - pl.openBracket - 1)};
            target.full = {static_cast<std::uint32_t>(pl.openBracket),
                           static_cast<std::uint32_t>(pl.endParen - pl.openBracket + 1)};
            target.destination = std::move(pl.destination);
            target.title = std::move(pl.title);

            out.inlineContexts.push_back(target.span);
            out.links.push_back(std::move(target));

            const auto marker1 = Span{static_cast<std::uint32_t>(pl.openBracket), 1};
            const auto marker2 = Span{static_cast<std::uint32_t>(pl.closeBracket),
                                      static_cast<std::uint32_t>(pl.endParen - pl.closeBracket + 1)};

            out.opaqueSpans.push_back(marker1);
            out.opaqueSpans.push_back(marker2);

            if (!context.revealed) {
                out.hidden.push_back({marker1, HideMode::Collapse});
                out.hidden.push_back({marker2, HideMode::Collapse});
            } else {
                newRuns.push_back({marker1, Role::LinkMarker, 0});
                newRuns.push_back({marker2, Role::LinkMarker, 0});
            }

            const std::uint32_t textStart = static_cast<std::uint32_t>(pl.openBracket + 1);
            const std::uint32_t textEnd = static_cast<std::uint32_t>(pl.closeBracket);
            if (textStart < textEnd) {
                std::uint32_t current = textStart;
                for (std::size_t k = textStart; k < textEnd; ++k) {
                    if (inCode[k]) {
                        if (k > current) {
                            newRuns.push_back({{current, static_cast<std::uint32_t>(k - current)}, Role::LinkText, 0});
                        }
                        while (k < textEnd && inCode[k]) {
                            ++k;
                        }
                        current = static_cast<std::uint32_t>(k);
                    }
                }
                if (current < textEnd) {
                    newRuns.push_back({{current, textEnd - current}, Role::LinkText, 0});
                }
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
