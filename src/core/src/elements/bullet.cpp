// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullet list items ('-', '*', '+'). Indentation is treated as nesting depth; there are
// no indented code blocks in this version. Thematic breaks ("---", "* * *") are not items.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>

namespace hitokoto::core {
namespace {

constexpr std::uint32_t kTabWidth = 4;
constexpr std::uint32_t kIndentPerLevel = 2;

bool isBlank(char16_t c) {
    return c == u' ' || c == u'\t';
}

/// CommonMark thematic break: three or more of the same '-', '*' or '_', only blanks between.
bool isThematicBreak(std::u16string_view line) {
    char16_t marker = 0;
    int count = 0;
    for (char16_t c : line) {
        if (isBlank(c)) {
            continue;
        }
        if (c != u'-' && c != u'*' && c != u'_') {
            return false;
        }
        if (marker == 0) {
            marker = c;
        } else if (c != marker) {
            return false;
        }
        ++count;
    }
    return count >= 3;
}

class BulletElement final : public LineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "bullet"; }

    [[nodiscard]] std::optional<LineMatch> parse(std::u16string_view line) const override {
        std::size_t i = 0;
        std::uint32_t column = 0;
        while (i < line.size() && isBlank(line[i])) {
            column += line[i] == u'\t' ? kTabWidth - column % kTabWidth : 1;
            ++i;
        }
        if (i >= line.size() || (line[i] != u'-' && line[i] != u'*' && line[i] != u'+')) {
            return std::nullopt;
        }
        const std::size_t markerChar = i;
        ++i;
        if (i < line.size() && !isBlank(line[i])) {
            return std::nullopt;
        }
        if (isThematicBreak(line)) {
            return std::nullopt;
        }
        while (i < line.size() && isBlank(line[i])) {
            ++i;
        }
        LineMatch m;
        m.marker = Span{0, static_cast<std::uint32_t>(i)};
        m.markerChar = static_cast<std::uint32_t>(markerChar);
        m.level = static_cast<std::uint8_t>(std::min<std::uint32_t>(column / kIndentPerLevel, 255));
        return m;
    }

    void render(std::u16string_view /*line*/, const LineMatch& match, const LineContext& context,
                LineRender& out) const override {
        const Span markerChar{match.markerChar, 1};
        if (context.revealed) {
            out.runs.push_back({markerChar, Role::Marker, 0});
        } else {
            out.hidden.push_back({markerChar, HideMode::KeepSpace});
            out.decorations.push_back({DecorationKind::Bullet, match.markerChar, match.level});
        }
    }
};

}  // namespace

std::unique_ptr<LineElement> makeBulletElement() {
    return std::make_unique<BulletElement>();
}

}  // namespace hitokoto::core
