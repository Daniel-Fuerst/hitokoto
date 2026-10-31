// SPDX-License-Identifier: GPL-3.0-or-later
// S023: front matter (Obsidian, not CommonMark). `---` on the first line of a document, closed by
// the next line `---` (both may end in blanks), is front matter; all its lines stay plain text and
// carry LineRender::frontMatter, so that the UI can show them as properties (S024). Without a
// closing line the first line is a thematic break: the caller confirms the closing line
// (ElementRegistry::documentStart, ADR 0006).
#include "hitokoto/core/markdown.hpp"

#include "../block_rules.hpp"

namespace hitokoto::core {
namespace {

constexpr std::uint32_t kDocumentStart = 1;  ///< state for the first line of the document
constexpr std::uint32_t kInside = 2;         ///< state inside the front matter

/// `---`, then only blanks; no indentation.
bool isDelimiter(std::u16string_view line) noexcept {
    if (!line.starts_with(u"---")) {
        return false;
    }
    for (std::size_t i = 3; i < line.size(); ++i) {
        if (!blocks::isBlank(line[i])) {
            return false;
        }
    }
    return true;
}

class FrontMatterElement final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "front-matter"; }

    [[nodiscard]] std::uint32_t documentStartState() const noexcept override { return kDocumentStart; }

    [[nodiscard]] std::optional<BlockLine> parse(std::u16string_view line,
                                                 std::uint32_t state) const noexcept override {
        if (state == kDocumentStart) {
            return isDelimiter(line) ? std::optional<BlockLine>{BlockLine{kInside, true}} : std::nullopt;
        }
        if (state == kInside) {
            return isDelimiter(line) ? BlockLine{0, true} : BlockLine{kInside, false};
        }
        return std::nullopt;  // front matter starts nowhere else
    }

    void render(std::u16string_view /*line*/, const BlockLine& /*parsed*/, const LineContext& /*context*/,
                LineRender& out) const override {
        // Plain text, revealed or not; the UI shows closed front matter as properties (S024).
        out.frontMatter = true;
    }
};

}  // namespace

std::unique_ptr<MultiLineElement> makeFrontMatterElement() {
    return std::make_unique<FrontMatterElement>();
}

}  // namespace hitokoto::core
