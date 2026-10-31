// SPDX-License-Identifier: GPL-3.0-or-later
// S020: backslash escapes for ASCII punctuation characters. Backslashes before ASCII punctuation
// are concealed when the line is not active, and styled with the marker colour when revealed.
// Inside inline code spans and fenced code blocks backslashes remain literal.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <vector>

namespace hitokoto::core {
namespace {

bool isAsciiPunctuation(char16_t c) noexcept {
    return (c >= u'!' && c <= u'/') || (c >= u':' && c <= u'@') || (c >= u'[' && c <= u'`') || (c >= u'{' && c <= u'~');
}

class EscapeElement final : public InlineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "escape"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"\\"; }

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
            std::fill(codeCharacters.begin() + static_cast<std::ptrdiff_t>(h.span.start),
                      codeCharacters.begin() + static_cast<std::ptrdiff_t>(h.span.end()), true);
        }

        for (std::size_t i = 0; i < line.size();) {
            if (codeCharacters[i]) {
                ++i;
                continue;
            }
            if (line[i] == u'\\' && i + 1 < line.size() && !codeCharacters[i + 1] && isAsciiPunctuation(line[i + 1])) {
                const auto span = Span{static_cast<std::uint32_t>(i), 1};
                out.runs.push_back({span, Role::EscapeMarker, 0});
                if (!context.revealed) {
                    out.hidden.push_back({span, HideMode::Collapse});
                }
                i += 2;
            } else {
                ++i;
            }
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeEscapeElement() {
    return std::make_unique<EscapeElement>();
}

}  // namespace hitokoto::core
