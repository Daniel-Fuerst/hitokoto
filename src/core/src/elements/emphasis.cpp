// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include "../inline_steps.hpp"

#include <algorithm>
#include <array>
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

std::vector<Delimiter> findDelimiters(std::u16string_view line, const InlineScan& scan, detail::InlineSteps& steps) {
    std::vector<Delimiter> delimiters;
    steps.add(line.size());  // every character is passed once, by the search or one at a time
    for (std::size_t i = 0; i < line.size();) {
        if (scan.isTaken(i)) {
            ++i;
            continue;
        }
        i = line.find_first_of(u"*~_\\", i);
        if (i == std::u16string_view::npos) {
            break;
        }
        if (scan.isTaken(i)) {
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

/// What a closing run looks for in the openings: its marker, how much of the run is left, and
/// what the rule of 3 needs to know about it. Openings that fit one search need not fit another.
constexpr std::size_t kSearches = std::size_t{3} * 3 * 3 * 2;

std::size_t searchOf(const Delimiter& closer, std::size_t remaining) noexcept {
    const std::size_t marker = closer.marker == u'*' ? 0U : closer.marker == u'_' ? 1U : 2U;
    return ((marker * 3 + (remaining - 1)) * 3 + (closer.length - 1U)) * 2 + (closer.canOpen ? 1U : 0U);
}

/// A closer pairs with the nearest earlier opening of the same marker that fits into what is left
/// of the closing run; openings skipped on the way stay literal (CommonMark). A run longer than the
/// opening closes several nested pairs, inner first.
///
/// Linear in the number of delimiters, like "process emphasis" of the CommonMark spec with its
/// `openers_bottom`: a search that found nothing remembers how far down it looked, and the next
/// search of the same kind stops there, because the openings below did not fit then and have not
/// changed since. So every opening is looked at once per kind of search, and once more when a pair
/// forms above it and removes it.
std::vector<Pair> matchDelimiters(const std::vector<Delimiter>& delimiters, detail::InlineSteps& steps) {
    std::vector<Pair> pairs;
    std::vector<Opening> openings;
    std::array<std::size_t, kSearches> searchedDownTo{};  // per kind of search: no opening below this index fits
    std::size_t highestMark = 0;                          // the largest value in searchedDownTo
    for (const auto& delimiter : delimiters) {
        std::size_t closeLength = 0;
        if (delimiter.canClose) {
            while (closeLength < delimiter.length) {
                const std::size_t remaining = delimiter.length - closeLength;
                std::size_t& bottom = searchedDownTo[searchOf(delimiter, remaining)];
                std::size_t found = openings.size();
                for (std::size_t i = openings.size(); i > bottom; --i) {
                    steps.add();
                    const Opening& candidate = openings[i - 1];
                    if (candidate.marker == delimiter.marker && candidate.span.length <= remaining &&
                        !ruleOfThreeForbids(candidate, delimiter)) {
                        found = i - 1;
                        break;
                    }
                }
                if (found == openings.size()) {
                    bottom = openings.size();
                    highestMark = std::max(highestMark, bottom);
                    break;
                }
                const Opening opening = openings[found];
                const Span closing{static_cast<std::uint32_t>(delimiter.span.start + closeLength), opening.span.length};
                pairs.push_back({opening.span, closing, opening.style});
                closeLength += opening.span.length;
                openings.erase(openings.begin() + static_cast<std::ptrdiff_t>(found), openings.end());
                if (highestMark > found) {
                    // Openings that come at these indexes from now on have not been looked at.
                    for (std::size_t& mark : searchedDownTo) {
                        mark = std::min(mark, found);
                    }
                    highestMark = found;
                    steps.add(kSearches);
                }
            }
        }
        if (delimiter.canOpen && closeLength < delimiter.length) {
            appendOpenings(openings, delimiter, closeLength, delimiter.length - closeLength);
        }
    }
    return pairs;
}

/// Index of the style's counter in `styleChanges`: bold, italic, strikethrough.
std::size_t counterOf(InlineStyle style) noexcept {
    return style == InlineStyle::Bold ? 0U : style == InlineStyle::Italic ? 1U : 2U;
}

std::uint32_t contextFor(Span span, const std::vector<Span>& contexts) noexcept {
    if (contexts.empty()) {
        return 0;
    }
    auto it = std::upper_bound(contexts.begin(), contexts.end(), span.start,
                               [](std::uint32_t val, const Span& ctx) { return val < ctx.start; });
    if (it != contexts.begin()) {
        --it;
        if (span.start >= it->start && span.end() <= it->end()) {
            return static_cast<std::uint32_t>(std::distance(contexts.begin(), it) + 1);
        }
    }
    return 0;
}

class EmphasisElement final : public InlineElement {
public:
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"*_~"; }

    void render(std::u16string_view line, const LineContext& context, const InlineScan& scan,
                LineRender& out) const override {
        detail::InlineSteps steps;
        const auto delimiters = findDelimiters(line, scan, steps);
        if (delimiters.empty()) {
            return;
        }

        std::vector<std::vector<Delimiter>> byContext(out.inlineContexts.size() + 1);
        steps.add(byContext.size() + delimiters.size());
        for (const auto& d : delimiters) {
            byContext[contextFor(d.span, out.inlineContexts)].push_back(d);
        }

        std::vector<Pair> pairs;
        for (const auto& group : byContext) {
            if (group.empty()) {
                continue;
            }
            const auto groupPairs = matchDelimiters(group, steps);
            pairs.insert(pairs.end(), groupPairs.begin(), groupPairs.end());
        }
        if (pairs.empty()) {
            return;
        }

        // A pair styles the text between its delimiters, and pairs nest: each pair notes where its
        // style begins and ends, and one pass along the line adds the notes up. Writing the style
        // pair by pair would visit a character once for every pair around it.
        const std::size_t length = line.size();
        std::vector<std::int32_t> styleChanges(3 * (length + 1), 0);
        std::vector<bool> markerCharacters(length, false);
        steps.add(pairs.size());
        for (const auto& pair : pairs) {
            if (pair.opener.end() < pair.closer.start) {
                const std::size_t counter = counterOf(pair.style) * (length + 1);
                ++styleChanges[counter + pair.opener.end()];
                --styleChanges[counter + pair.closer.start];
            }
            for (const auto& delimiter : {pair.opener, pair.closer}) {
                steps.add(delimiter.length);
                std::fill(markerCharacters.begin() + delimiter.start, markerCharacters.begin() + delimiter.end(), true);
            }
        }
        std::vector<InlineStyle> styles(length, InlineStyle::None);
        std::array<std::int32_t, 3> open{};
        steps.add(length);
        for (std::size_t i = 0; i < length; ++i) {
            for (std::size_t counter = 0; counter < open.size(); ++counter) {
                open[counter] += styleChanges[counter * (length + 1) + i];
            }
            styles[i] = (open[0] > 0 ? InlineStyle::Bold : InlineStyle::None) |
                        (open[1] > 0 ? InlineStyle::Italic : InlineStyle::None) |
                        (open[2] > 0 ? InlineStyle::Strikethrough : InlineStyle::None);
        }

        steps.add(2 * length);  // the two passes below
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
            while (i < line.size() && (styles[i] == InlineStyle::None || markerCharacters[i] || scan.isTaken(i))) {
                ++i;
            }
            const std::size_t start = i;
            if (start == line.size()) {
                break;
            }
            const auto style = styles[i];
            while (i < line.size() && styles[i] == style && !markerCharacters[i] && !scan.isTaken(i)) {
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
