// SPDX-License-Identifier: GPL-3.0-or-later
// S045: any formula lays out or gives an error with a reason and a position inside it, and any
// bytes read as a MATH table give a table or nothing; neither ever crashes.
#include "hitokoto/core/math.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace {

/// Every character has a glyph of fixed size, so the layout runs on every input.
class FixedFont final : public hitokoto::core::MathFont {
public:
    [[nodiscard]] double unitsPerEm() const override { return 1000; }
    [[nodiscard]] std::uint16_t glyph(char32_t c) const override { return static_cast<std::uint16_t>(c % 0xFFFF) + 1; }
    [[nodiscard]] double advance(std::uint16_t /*glyph*/) const override { return 500; }
    [[nodiscard]] hitokoto::core::GlyphBounds bounds(std::uint16_t /*glyph*/) const override {
        return {0, -100, 450, 650};
    }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace hitokoto::core;
    const std::span<const std::uint8_t> bytes(data, size);
    const auto table = MathTable::parse(bytes);
    std::u16string source;
    for (std::size_t i = 0; i + 1 < size; i += 2) {
        source.push_back(static_cast<char16_t>(data[i] | (static_cast<unsigned>(data[i + 1]) << 8U)));
    }
    static const FixedFont font;
    const MathTable used = table.value_or(MathTable{});
    for (const bool display : {false, true}) {
        const MathLayout layout = layoutMath(source, font, used, 16, display);
        if (layout.error) {
            if (layout.error->message.empty() || layout.error->position > source.size()) {
                std::abort();
            }
            if (!layout.glyphs.empty() || !layout.rules.empty()) {
                std::abort();
            }
        } else if (!std::isfinite(layout.width) || !std::isfinite(layout.ascent) || !std::isfinite(layout.descent)) {
            std::abort();
        }
    }
    return 0;
}
