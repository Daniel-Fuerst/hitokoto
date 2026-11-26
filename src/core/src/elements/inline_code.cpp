// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include "../inline_steps.hpp"

#include <algorithm>
#include <vector>

namespace hitokoto::core {
namespace {

/// Looks only at the backslashes directly before `position`, and a run of backslashes stands
/// before one backtick only: all calls together stay within the length of the line.
bool escaped(std::u16string_view line, std::size_t position, detail::InlineSteps& steps) {
    std::size_t slashes = 0;
    while (position > slashes && line[position - slashes - 1] == u'\\') {
        ++slashes;
    }
    steps.add(slashes + 1);
    return slashes % 2 != 0;
}

class InlineCodeElement final : public InlineElement {
public:
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"`"; }

    void render(std::u16string_view line, const LineContext& context, const InlineScan& scan,
                LineRender& out) const override {
        detail::InlineSteps steps;
        steps.add(line.size());
        std::size_t opener = std::u16string_view::npos;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (scan.isOpaque(i)) {
                opener = std::u16string_view::npos;
                continue;
            }
            if (line[i] != u'`' || (i > 0 && line[i - 1] == u'`') || (i + 1 < line.size() && line[i + 1] == u'`')) {
                continue;
            }
            if (opener == std::u16string_view::npos) {
                // A backslash escape keeps a backtick from opening a span. Inside a span backslashes
                // are literal (CommonMark), so the next single backtick always closes it.
                if (!escaped(line, i, steps)) {
                    opener = i;
                }
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
            out.opaqueSpans.push_back({static_cast<std::uint32_t>(opener), static_cast<std::uint32_t>(i - opener + 1)});
            opener = std::u16string_view::npos;
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeInlineCodeElement() {
    return std::make_unique<InlineCodeElement>();
}

}  // namespace hitokoto::core
