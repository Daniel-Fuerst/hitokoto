// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links and titles. Delimiters are concealed when the caret is elsewhere,
// and revealed with the marker colour on the active line.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
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

class LinkElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "link"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"["; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        std::vector<bool> inCode(line.size(), false);
        for (const auto& run : out.runs) {
            if (run.role == Role::InlineCode || run.role == Role::InlineCodeMarker) {
                std::fill(inCode.begin() + run.span.start, inCode.begin() + run.span.end(), true);
            }
        }

        for (std::size_t i = 0; i < line.size();) {
            if (inCode[i] || line[i] != u'[' || escaped(line, i)) {
                ++i;
                continue;
            }

            const std::size_t openBracket = i;
            std::size_t j = openBracket + 1;
            int bracketDepth = 0;
            std::size_t closeBracket = std::u16string_view::npos;

            while (j < line.size()) {
                if (inCode[j]) {
                    ++j;
                    continue;
                }
                if (line[j] == u'\\' && j + 1 < line.size()) {
                    j += 2;
                    continue;
                }
                if (line[j] == u'[') {
                    ++bracketDepth;
                    ++j;
                } else if (line[j] == u']') {
                    if (bracketDepth > 0) {
                        --bracketDepth;
                        ++j;
                    } else {
                        if (j + 1 < line.size() && line[j + 1] == u'(') {
                            closeBracket = j;
                        }
                        break;
                    }
                } else {
                    ++j;
                }
            }

            if (closeBracket == std::u16string_view::npos) {
                i = openBracket + 1;
                continue;
            }

            std::size_t pos = closeBracket + 2;  // right after '('
            while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
                ++pos;
            }

            std::u16string destination;
            std::u16string title;
            std::size_t endParen = std::u16string_view::npos;

            if (pos < line.size() && line[pos] == u')') {
                endParen = pos;
            } else if (pos < line.size() && line[pos] == u'<') {
                ++pos;
                const std::size_t destStart = pos;
                bool validAngle = false;
                while (pos < line.size()) {
                    if (line[pos] == u'\\' && pos + 1 < line.size()) {
                        pos += 2;
                    } else if (line[pos] == u'<' || line[pos] == u'\n' || line[pos] == u'\r') {
                        break;
                    } else if (line[pos] == u'>') {
                        destination = unescapePunctuation(line.substr(destStart, pos - destStart));
                        ++pos;
                        validAngle = true;
                        break;
                    } else {
                        ++pos;
                    }
                }
                if (!validAngle) {
                    i = openBracket + 1;
                    continue;
                }
                bool hadSpace = false;
                while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
                    hadSpace = true;
                    ++pos;
                }
                if (pos < line.size() && line[pos] == u')') {
                    endParen = pos;
                } else if (hadSpace && pos < line.size() &&
                           (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(')) {
                    endParen = parseTitle(line, pos, title);
                }
            } else {
                const std::size_t destStart = pos;
                int parenDepth = 0;
                bool validBare = true;
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
                    validBare = false;
                }
                if (!validBare) {
                    i = openBracket + 1;
                    continue;
                }
                destination = unescapePunctuation(line.substr(destStart, pos - destStart));
                bool hadSpace = false;
                while (pos < line.size() && (line[pos] == u' ' || line[pos] == u'\t')) {
                    hadSpace = true;
                    ++pos;
                }
                if (pos < line.size() && line[pos] == u')') {
                    endParen = pos;
                } else if (hadSpace && pos < line.size() &&
                           (line[pos] == u'"' || line[pos] == u'\'' || line[pos] == u'(')) {
                    endParen = parseTitle(line, pos, title);
                }
            }

            if (endParen == std::u16string_view::npos) {
                i = openBracket + 1;
                continue;
            }

            LinkTarget target;
            target.span = {static_cast<std::uint32_t>(openBracket + 1),
                           static_cast<std::uint32_t>(closeBracket - openBracket - 1)};
            target.full = {static_cast<std::uint32_t>(openBracket),
                           static_cast<std::uint32_t>(endParen - openBracket + 1)};
            target.destination = std::move(destination);
            target.title = std::move(title);
            out.links.push_back(std::move(target));

            const auto marker1 = Span{static_cast<std::uint32_t>(openBracket), 1};
            const auto marker2 =
                Span{static_cast<std::uint32_t>(closeBracket), static_cast<std::uint32_t>(endParen - closeBracket + 1)};

            if (!context.revealed) {
                out.hidden.push_back({marker1, HideMode::Collapse});
                out.hidden.push_back({marker2, HideMode::Collapse});
            } else {
                out.runs.push_back({marker1, Role::LinkMarker, 0});
                out.runs.push_back({marker2, Role::LinkMarker, 0});
            }

            const std::uint32_t textStart = static_cast<std::uint32_t>(openBracket + 1);
            const std::uint32_t textEnd = static_cast<std::uint32_t>(closeBracket);
            if (textStart < textEnd) {
                std::uint32_t current = textStart;
                for (const auto& run : out.runs) {
                    if (run.role != Role::InlineCode && run.role != Role::InlineCodeMarker) {
                        continue;
                    }
                    if (run.span.end() <= current || run.span.start >= textEnd) {
                        continue;
                    }
                    if (run.span.start > current) {
                        out.runs.push_back({{current, run.span.start - current}, Role::LinkText, 0});
                    }
                    current = std::max(current, run.span.end());
                }
                if (current < textEnd) {
                    out.runs.push_back({{current, textEnd - current}, Role::LinkText, 0});
                }
            }

            i = endParen + 1;
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeLinkElement() {
    return std::make_unique<LinkElement>();
}

}  // namespace hitokoto::core
