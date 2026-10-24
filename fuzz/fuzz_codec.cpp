// SPDX-License-Identifier: GPL-3.0-or-later
// Any byte string either decodes and encodes back unchanged, or is rejected.
#include "hitokoto/core/text_codec.hpp"

#include <cstdlib>

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
    }
    return 0;
}
