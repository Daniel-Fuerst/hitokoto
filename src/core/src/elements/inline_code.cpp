// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

namespace hitokoto::core {
namespace {

bool escaped(std::u16string_view line, std::size_t position) {
    std::size_t slashes = 0;
    while (position > slashes && line[position - slashes - 1] == u'\\') {
        ++slashes;
    }
    return slashes % 2 != 0;
}

class InlineCodeElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "inline-code"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"`"; }

    void render(std::u16string_view line, const LineContext& context, LineRender& out) const override {
        std::size_t opener = std::u16string_view::npos;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] != u'`' || escaped(line, i) || (i > 0 && line[i - 1] == u'`') ||
                (i + 1 < line.size() && line[i + 1] == u'`')) {
                continue;
            }
            if (opener == std::u16string_view::npos) {
                opener = i;
                continue;
            }
            if (i == opener + 1) {
                opener = i;
                continue;
            }

            out.runs.push_back({{static_cast<std::uint32_t>(opener + 1), static_cast<std::uint32_t>(i - opener - 1)},
                                Role::InlineCode,
                                0});
            // Markers are styled when revealed; when concealed the registry drops them under the hidden ranges.
            out.runs.push_back({{static_cast<std::uint32_t>(opener), 1}, Role::InlineCodeMarker, 0});
            out.runs.push_back({{static_cast<std::uint32_t>(i), 1}, Role::InlineCodeMarker, 0});
            if (!context.revealed) {
                out.hidden.push_back({{static_cast<std::uint32_t>(opener), 1}, HideMode::Collapse});
                out.hidden.push_back({{static_cast<std::uint32_t>(i), 1}, HideMode::Collapse});
            }
            opener = std::u16string_view::npos;
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeInlineCodeElement() {
    return std::make_unique<InlineCodeElement>();
}

}  // namespace hitokoto::core
