// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
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

struct Opening {
    Span span;
    char16_t marker = 0;
    InlineStyle style = InlineStyle::None;
    std::uint8_t runLength = 0;  ///< length of the delimiter run this opening came from
    bool canClose = false;       ///< that run could also close
};

struct Pair {
    Span opener;
    Span closer;
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

bool supportedRun(char16_t marker, std::size_t length) {
    return marker == u'~' ? length == 2 : length >= 1 && length <= 3;
}

void appendOpenings(std::vector<Opening>& openings, const Delimiter& delimiter, std::size_t offset,
                    std::size_t length) {
    const auto span = [&](std::size_t start, std::size_t count) {
        return Span{static_cast<std::uint32_t>(delimiter.span.start + start), static_cast<std::uint32_t>(count)};
    };
    if (delimiter.marker == u'~') {
        if (length == 2) {
            openings.push_back({span(offset, length), delimiter.marker, InlineStyle::Strikethrough, delimiter.length,
                                delimiter.canClose});
        }
    } else if (length == 1) {
        openings.push_back(
            {span(offset, length), delimiter.marker, InlineStyle::Italic, delimiter.length, delimiter.canClose});
    } else if (length == 2) {
        openings.push_back(
            {span(offset, length), delimiter.marker, InlineStyle::Bold, delimiter.length, delimiter.canClose});
    } else if (length == 3) {
        openings.push_back(
            {span(offset, 1), delimiter.marker, InlineStyle::Italic, delimiter.length, delimiter.canClose});
        openings.push_back(
            {span(offset + 1, 2), delimiter.marker, InlineStyle::Bold, delimiter.length, delimiter.canClose});
    }
}

std::vector<Delimiter> findDelimiters(std::u16string_view line, const std::vector<bool>& codeCharacters) {
    std::vector<Delimiter> delimiters;
    for (std::size_t i = 0; i < line.size();) {
        if (codeCharacters[i]) {
            ++i;
            continue;
        }
        i = line.find_first_of(u"*~_\\", i);
        if (i == std::u16string_view::npos) {
            break;
        }
        if (codeCharacters[i]) {
            ++i;
            continue;
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

/// CommonMark's "rule of 3": a run that can both open and close does not pair with a run when the
/// two run lengths add up to a multiple of 3, unless both are multiples of 3. It keeps the inner
/// `*` of `**2*3 is six**` literal.
bool ruleOfThreeForbids(const Opening& opening, const Delimiter& closer) {
    return (closer.canOpen || opening.canClose) && (opening.runLength + closer.length) % 3 == 0 &&
           (opening.runLength % 3 != 0 || closer.length % 3 != 0);
}

/// A closer pairs with the nearest earlier opening of the same marker that fits into what is left
/// of the closing run; openings skipped on the way stay literal (CommonMark). A run longer than the
/// opening closes several nested pairs, inner first.
std::vector<Pair> matchDelimiters(const std::vector<Delimiter>& delimiters) {
    std::vector<Pair> pairs;
    std::vector<Opening> openings;
    for (const auto& delimiter : delimiters) {
        std::size_t closeLength = 0;
        if (delimiter.canClose) {
            while (closeLength < delimiter.length) {
                const auto opening = std::find_if(openings.rbegin(), openings.rend(), [&](const Opening& candidate) {
                    return candidate.marker == delimiter.marker &&
                           candidate.span.length <= delimiter.length - closeLength &&
                           !ruleOfThreeForbids(candidate, delimiter);
                });
                if (opening == openings.rend()) {
                    break;
                }
                const Span closing{static_cast<std::uint32_t>(delimiter.span.start + closeLength),
                                   opening->span.length};
                pairs.push_back({opening->span, closing, opening->style});
                closeLength += opening->span.length;
                openings.erase(std::prev(opening.base()), openings.end());
            }
        }
        if (delimiter.canOpen && closeLength < delimiter.length) {
            appendOpenings(openings, delimiter, closeLength, delimiter.length - closeLength);
        }
    }
    return pairs;
}

std::uint32_t contextFor(Span span, const std::vector<Span>& contexts) noexcept {
    for (std::size_t i = 0; i < contexts.size(); ++i) {
        if (span.start >= contexts[i].start && span.end() <= contexts[i].end()) {
            return static_cast<std::uint32_t>(i + 1);
        }
    }
    return 0;
}

class EmphasisElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "emphasis"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"*_~"; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        std::vector<bool> codeCharacters(line.size(), false);
        for (const auto& span : out.opaqueSpans) {
            if (span.start < line.size()) {
                const auto end = std::min<std::size_t>(span.end(), line.size());
                std::fill(codeCharacters.begin() + static_cast<std::ptrdiff_t>(span.start),
                          codeCharacters.begin() + static_cast<std::ptrdiff_t>(end), true);
            }
        }
        for (const auto& h : out.hidden) {
            if (h.span.start < line.size()) {
                const auto end = std::min<std::size_t>(h.span.end(), line.size());
                std::fill(codeCharacters.begin() + static_cast<std::ptrdiff_t>(h.span.start),
                          codeCharacters.begin() + static_cast<std::ptrdiff_t>(end), true);
            }
        }
        const auto delimiters = findDelimiters(line, codeCharacters);
        if (delimiters.empty()) {
            return;
        }

        std::vector<std::vector<Delimiter>> byContext(out.inlineContexts.size() + 1);
        for (const auto& d : delimiters) {
            byContext[contextFor(d.span, out.inlineContexts)].push_back(d);
        }

        std::vector<Pair> pairs;
        for (const auto& group : byContext) {
            if (group.empty()) {
                continue;
            }
            const auto groupPairs = matchDelimiters(group);
            pairs.insert(pairs.end(), groupPairs.begin(), groupPairs.end());
        }
        if (pairs.empty()) {
            return;
        }

        std::vector<InlineStyle> styles(line.size(), InlineStyle::None);
        std::vector<bool> markerCharacters(line.size(), false);
        for (const auto& pair : pairs) {
            for (std::uint32_t i = pair.opener.end(); i < pair.closer.start; ++i) {
                styles[i] = styles[i] | pair.style;
            }
            for (const auto& delimiter : {pair.opener, pair.closer}) {
                std::fill(markerCharacters.begin() + delimiter.start, markerCharacters.begin() + delimiter.end(), true);
            }
        }

        for (std::size_t i = 0; i < line.size();) {
            while (i < line.size() && !markerCharacters[i]) {
                ++i;
            }
            const std::size_t start = i;
            while (i < line.size() && markerCharacters[i]) {
                ++i;
            }
            if (start == i) {
                continue;
            }
            const Span span{static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(i - start)};
            // Styled when revealed; when concealed the registry drops the run under the hidden range.
            out.runs.push_back({span, Role::EmphasisMarker, 0});
            if (!context.revealed) {
                out.hidden.push_back({span, HideMode::Collapse});
            }
        }

        for (std::size_t i = 0; i < line.size();) {
            while (i < line.size() && (styles[i] == InlineStyle::None || markerCharacters[i] || codeCharacters[i])) {
                ++i;
            }
            const std::size_t start = i;
            if (start == line.size()) {
                break;
            }
            const auto style = styles[i];
            while (i < line.size() && styles[i] == style && !markerCharacters[i] && !codeCharacters[i]) {
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
