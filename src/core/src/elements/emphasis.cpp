// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include <cstdint>
#include <vector>

namespace hitokoto::core {
namespace {

struct Delimiter {
    Span span;
    char16_t marker = 0;
    std::uint8_t length = 0;
    bool canOpen = false;
    bool canClose = false;
};

struct Pair {
    std::size_t opener = 0;
    std::size_t closer = 0;
    InlineStyle style = InlineStyle::None;
};

bool isWhitespace(char16_t c) {
    return c == u' ' || c == u'\t' || c == 0x00a0 || (c >= 0x2000 && c <= 0x200a) || c == 0x202f || c == 0x205f ||
           c == 0x3000;
}

bool isAsciiPunctuation(char16_t c) {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

bool canEscape(char16_t c) {
    return isAsciiPunctuation(c);
}

InlineStyle styleFor(char16_t marker, std::uint8_t length) {
    if (marker == u'~') {
        return InlineStyle::Strikethrough;
    }
    if (length == 1) {
        return InlineStyle::Italic;
    }
    if (length == 2) {
        return InlineStyle::Bold;
    }
    return InlineStyle::Bold | InlineStyle::Italic;
}

bool supportedRun(char16_t marker, std::size_t length) {
    return marker == u'~' ? length == 2 : length >= 1 && length <= 3;
}

std::vector<Delimiter> findDelimiters(std::u16string_view line) {
    std::vector<Delimiter> delimiters;
    for (std::size_t i = 0; i < line.size();) {
        i = line.find_first_of(u"*~_\\", i);
        if (i == std::u16string_view::npos) {
            break;
        }
        if (line[i] == u'\\' && i + 1 < line.size() && canEscape(line[i + 1])) {
            i += 2;
            continue;
        }
        const char16_t marker = line[i];
        if (marker != u'*' && marker != u'_' && marker != u'~') {
            ++i;
            continue;
        }
        std::size_t end = i + 1;
        while (end < line.size() && line[end] == marker) {
            ++end;
        }
        const std::size_t runLength = end - i;
        if (!supportedRun(marker, runLength)) {
            i = end;
            continue;
        }

        const bool leftWhitespace = i == 0 || isWhitespace(line[i - 1]);
        const bool rightWhitespace = end == line.size() || isWhitespace(line[end]);
        const bool leftPunctuation = i > 0 && isAsciiPunctuation(line[i - 1]);
        const bool rightPunctuation = end < line.size() && isAsciiPunctuation(line[end]);
        const bool leftFlanking = !rightWhitespace && (!rightPunctuation || leftWhitespace || leftPunctuation);
        const bool rightFlanking = !leftWhitespace && (!leftPunctuation || rightWhitespace || rightPunctuation);
        const bool canOpen =
            marker == u'_' ? leftFlanking && (!rightFlanking || leftWhitespace || leftPunctuation) : leftFlanking;
        const bool canClose =
            marker == u'_' ? rightFlanking && (!leftFlanking || rightWhitespace || rightPunctuation) : rightFlanking;
        if (canOpen || canClose) {
            delimiters.push_back({{static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(runLength)},
                                  marker,
                                  static_cast<std::uint8_t>(runLength),
                                  canOpen,
                                  canClose});
        }
        i = end;
    }
    return delimiters;
}

std::vector<Pair> matchDelimiters(const std::vector<Delimiter>& delimiters) {
    std::vector<Pair> pairs;
    std::vector<std::size_t> openers;
    for (std::size_t i = 0; i < delimiters.size(); ++i) {
        const auto& delimiter = delimiters[i];
        const bool matchesTop = !openers.empty() && delimiters[openers.back()].marker == delimiter.marker &&
                                delimiters[openers.back()].length == delimiter.length;
        if (delimiter.canClose && matchesTop) {
            pairs.push_back({openers.back(), i, styleFor(delimiter.marker, delimiter.length)});
            openers.pop_back();
        } else if (delimiter.canOpen) {
            openers.push_back(i);
        }
    }
    return pairs;
}

class EmphasisElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "emphasis"; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        const auto delimiters = findDelimiters(line);
        const auto pairs = matchDelimiters(delimiters);
        if (pairs.empty()) {
            return;
        }

        std::vector<InlineStyle> styles(line.size(), InlineStyle::None);
        std::vector<bool> markerCharacters(line.size(), false);
        for (const auto& pair : pairs) {
            const auto& opener = delimiters[pair.opener];
            const auto& closer = delimiters[pair.closer];
            for (std::uint32_t i = opener.span.end(); i < closer.span.start; ++i) {
                styles[i] = styles[i] | pair.style;
            }
            for (const auto& delimiter : {opener, closer}) {
                for (std::uint32_t i = delimiter.span.start; i < delimiter.span.end(); ++i) {
                    markerCharacters[i] = true;
                }
                if (!context.revealed) {
                    out.hidden.push_back({delimiter.span, HideMode::Collapse});
                }
            }
        }

        for (std::size_t i = 0; i < line.size();) {
            while (i < line.size() && (styles[i] == InlineStyle::None || markerCharacters[i])) {
                ++i;
            }
            const std::size_t start = i;
            if (start == line.size()) {
                break;
            }
            const auto style = styles[i];
            while (i < line.size() && styles[i] == style && !markerCharacters[i]) {
                ++i;
            }
            out.inlineStyles.push_back(
                {{static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(i - start)}, style});
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeEmphasisElement() {
    return std::make_unique<EmphasisElement>();
}

}  // namespace hitokoto::core
