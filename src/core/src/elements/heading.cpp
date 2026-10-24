// SPDX-License-Identifier: GPL-3.0-or-later
// S002: ATX headings. Parse rule follows CommonMark for the opening sequence: up to three
// spaces of indentation, 1-6 '#', then a space, a tab or the end of the line.
// Closing sequences ("## Title ##") are shown as text for now.
#include "hitokoto/core/markdown.hpp"

namespace hitokoto::core {
namespace {

class HeadingElement final : public LineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "heading"; }

    [[nodiscard]] std::optional<LineMatch> parse(std::u16string_view line) const override {
        std::size_t i = 0;
        while (i < line.size() && i < 3 && line[i] == u' ') {
            ++i;
        }
        const std::size_t hashesStart = i;
        while (i < line.size() && line[i] == u'#') {
            ++i;
        }
        const std::size_t hashes = i - hashesStart;
        if (hashes < 1 || hashes > 6) {
            return std::nullopt;
        }
        if (i < line.size() && line[i] != u' ' && line[i] != u'\t') {
            return std::nullopt;
        }
        while (i < line.size() && (line[i] == u' ' || line[i] == u'\t')) {
            ++i;
        }
        LineMatch m;
        m.marker = Span{0, static_cast<std::uint32_t>(i)};
        m.level = static_cast<std::uint8_t>(hashes);
        m.markerChar = static_cast<std::uint32_t>(hashesStart);
        return m;
    }

    void render(std::u16string_view line, const LineMatch& match, const LineContext& context,
                LineRender& out) const override {
        out.headingLevel = match.level;
        if (context.revealed) {
            out.runs.push_back({match.marker, Role::Marker, match.level});
        } else {
            out.hidden.push_back({match.marker, HideMode::Collapse});
        }
        const auto textStart = match.marker.end();
        const auto lineLength = static_cast<std::uint32_t>(line.size());
        if (lineLength > textStart) {
            out.runs.push_back({Span{textStart, lineLength - textStart}, Role::HeadingText, match.level});
        }
    }
};

}  // namespace

std::unique_ptr<LineElement> makeHeadingElement() {
    return std::make_unique<HeadingElement>();
}

}  // namespace hitokoto::core
