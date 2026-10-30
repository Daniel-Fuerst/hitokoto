// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullet list items ('-', '*', '+'). Indentation is treated as nesting depth; there are
// no indented code blocks in this version. Thematic breaks ("---", "* * *") are not items.
// S023: a bullet with text opens paragraph text at its content column (ADR 0006).
#include "hitokoto/core/markdown.hpp"

#include "../block_rules.hpp"

#include <algorithm>

namespace hitokoto::core {
namespace {

constexpr std::uint32_t kTabWidth = 4;
constexpr std::uint32_t kIndentPerLevel = 2;

using blocks::isBlank;

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
        if (blocks::isThematicBreakFrom(line, markerChar)) {
            return std::nullopt;  // at any indentation: a break inside an item is no item either
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

    [[nodiscard]] std::optional<std::uint32_t> paragraphColumn(std::u16string_view line,
                                                               const LineMatch& match) const noexcept override {
        // CommonMark 5.2: the content column is after the marker and one to four columns of
        // padding; with five or more the text is indented code inside the item.
        const std::uint32_t markerColumn = blocks::columnAfterBlanks(line, 0, 0);
        const std::int64_t column = blocks::paragraphColumnAfterMarker(line, match.markerChar + 1U, markerColumn + 1U);
        if (column < 0) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(column);
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
