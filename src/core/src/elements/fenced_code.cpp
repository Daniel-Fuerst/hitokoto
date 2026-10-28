// SPDX-License-Identifier: GPL-3.0-or-later
// Fenced code block (S014): ``` or ~~~ fences (CommonMark 4.5). Everything between the fences is
// literal code; concealed fence lines keep their space but are hidden, revealed ones show their
// source as code (S021). An opening fence reports the language of its info string.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>

namespace hitokoto::core {
namespace {

// Element state: one flag bit for the marker, the fence length below it (never 0 while open).
constexpr std::uint32_t kBacktick = 1U << 30U;
constexpr std::uint32_t kTilde = 1U << 31U;
constexpr std::uint32_t kLengthMask = kBacktick - 1;

struct Fence {
    std::uint32_t marker = 0;  ///< kBacktick, kTilde or 0 (no fence)
    std::uint32_t length = 0;
    std::size_t end = 0;  ///< index after the fence characters
};

/// A run of at least three ` or ~ after optional leading whitespace.
Fence fenceAt(std::u16string_view line) noexcept {
    std::size_t start = 0;
    while (start < line.size() && (line[start] == u' ' || line[start] == u'\t')) {
        ++start;
    }
    if (start == line.size() || (line[start] != u'`' && line[start] != u'~')) {
        return {};
    }
    const char16_t c = line[start];
    std::size_t end = start;
    while (end < line.size() && line[end] == c) {
        ++end;
    }
    if (end - start < 3) {
        return {};
    }
    const auto length = static_cast<std::uint32_t>(std::min<std::size_t>(end - start, kLengthMask));
    return {c == u'`' ? kBacktick : kTilde, length, end};
}

bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

bool onlyBlanks(std::u16string_view text) noexcept {
    return std::all_of(text.begin(), text.end(), isBlank);
}

bool isAsciiPunctuation(char16_t c) noexcept {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

/// The language of an info string: its first word, with backslash escapes resolved (CommonMark
/// 4.5 and 2.4, as commonmark.js and markdown-it do). Entity references stay as written.
std::u16string languageOf(std::u16string_view info) {
    std::size_t start = 0;
    while (start < info.size() && isBlank(info[start])) {
        ++start;
    }
    std::u16string word;
    for (std::size_t i = start; i < info.size() && !isBlank(info[i]); ++i) {
        if (info[i] == u'\\' && i + 1 < info.size() && isAsciiPunctuation(info[i + 1])) {
            ++i;
        }
        word.push_back(info[i]);
    }
    return word;
}

class FencedCodeElement final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "fenced-code"; }

    [[nodiscard]] std::optional<BlockLine> parse(std::u16string_view line,
                                                 std::uint32_t state) const noexcept override {
        const Fence fence = fenceAt(line);
        if (state == 0) {
            // The info string of a backtick fence must not contain a backtick.
            if (fence.marker == 0 || (fence.marker == kBacktick && line.substr(fence.end).find(u'`') != line.npos)) {
                return std::nullopt;
            }
            return BlockLine{fence.marker | fence.length, true};
        }
        // Closing fence: same character, at least as long as the opening one, nothing but blanks after.
        const bool closes = fence.marker == (state & ~kLengthMask) && fence.length >= (state & kLengthMask) &&
                            onlyBlanks(line.substr(fence.end));
        return closes ? BlockLine{0, true} : BlockLine{state, false};
    }

    void render(std::u16string_view line, const BlockLine& parsed, const LineContext& context,
                LineRender& out) const override {
        out.codeBlock = true;
        out.fenceLine = parsed.delimiter;
        if (parsed.delimiter && !context.blockState.active()) {
            out.codeLanguage = languageOf(line.substr(fenceAt(line).end));
        }
        const Span whole{0, static_cast<std::uint32_t>(line.size())};
        if (whole.length == 0) {
            return;
        }
        if (parsed.delimiter && !context.revealed) {
            out.hidden.push_back({whole, HideMode::KeepSpace});
        } else {
            out.runs.push_back({whole, Role::CodeBlock, 0});
        }
    }
};

}  // namespace

std::unique_ptr<MultiLineElement> makeFencedCodeElement() {
    return std::make_unique<FencedCodeElement>();
}

}  // namespace hitokoto::core
