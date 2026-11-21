// SPDX-License-Identifier: GPL-3.0-or-later
// Any byte string either decodes and encodes back unchanged, or is rejected. Read as UTF-16 code
// units, the same bytes are text on its way into the editor: with its lone surrogates replaced it
// always encodes, keeps its length and changes nowhere else (S004, rework of 2026-11-21).
#include "hitokoto/core/text_codec.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void checkInsertable(const std::uint8_t* data, std::size_t size) {
    std::u16string units(size / 2, u'\0');
    std::memcpy(units.data(), data, units.size() * 2);
    using hitokoto::core::findLoneSurrogate;
    using hitokoto::core::replaceLoneSurrogates;
    const std::size_t npos = std::u16string_view::npos;
    const std::size_t first = findLoneSurrogate(units);
    const std::u16string out = replaceLoneSurrogates(units);
    if (out.size() != units.size() || findLoneSurrogate(out) != npos || replaceLoneSurrogates(out) != out)
        std::abort();
    if (!hitokoto::core::encodeUtf8(out, false, hitokoto::core::LineEnding::LF))
        std::abort();
    if (hitokoto::core::encodeUtf8(units, false, hitokoto::core::LineEnding::LF).has_value() != (first == npos))
        std::abort();
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (out[i] != units[i] && (out[i] != hitokoto::core::kReplacementCharacter || i < first))
            std::abort();
    }
    if (first != npos) {
        const auto place = hitokoto::core::placeOf(units, first);
        const auto end = hitokoto::core::placeOf(units, units.size());
        if (place.line < 1 || place.column < 1 || place.line > end.line ||
            (place.line == end.line && place.column > end.column))
            std::abort();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view bytes(reinterpret_cast<const char*>(data), size);
    const auto decoded = hitokoto::core::decodeUtf8(bytes);
    if (decoded.value.has_value() == decoded.error.has_value())
        std::abort();
    if (decoded.value) {
        const auto& d = *decoded.value;
        const auto encoded = hitokoto::core::encodeUtf8(d.text, d.hasBom, d.uniformEnding, d.perBreak);
        if (!encoded || *encoded != bytes)
            std::abort();
        if (hitokoto::core::findLoneSurrogate(d.text) != std::u16string_view::npos)
            std::abort();  // a decoded file never holds half of a character
    }
    checkInsertable(data, size);
    return 0;
}
