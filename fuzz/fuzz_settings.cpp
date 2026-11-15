// SPDX-License-Identifier: GPL-3.0-or-later
// S038: settings parser fuzz target
#include "hitokoto/core/settings.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

using namespace hitokoto::core;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view src(reinterpret_cast<const char*>(data), size);
    const SettingsLoadResult first = parseSettings(src);
    for (const auto& d : first.diagnostics) {
        if (d.line == 0 || d.column == 0) {
            std::fprintf(stderr, "diagnostic without position\n");
            std::abort();
        }
        (void)d.toString();
    }
    const SettingsLoadResult second = parseSettings(serializeSettings(first.settings));
    if (!second.diagnostics.empty() || !(second.settings == first.settings)) {
        std::fprintf(stderr, "round trip differs\n");
        std::abort();
    }
    (void)builtinSchemeFromSetting(first.settings.theme);
    return 0;
}
