// SPDX-License-Identifier: GPL-3.0-or-later
// Any theme file loads: either applied, or rejected with diagnostics and the built-in fallback.
#include "hitokoto/core/theme.hpp"

#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view source(reinterpret_cast<const char*>(data), size);
    const auto r = hitokoto::core::loadTheme(source, hitokoto::core::ColorScheme::Light);
    if (r.usedFallback == r.diagnostics.empty())
        std::abort();
    for (const auto& d : r.diagnostics) {
        if (d.message.empty())
            std::abort();
    }
    return 0;
}
