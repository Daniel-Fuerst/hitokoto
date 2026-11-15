// SPDX-License-Identifier: GPL-3.0-or-later
// S038: settings parser fuzz target
#include "hitokoto/core/settings.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

using namespace hitokoto::core;

namespace {

bool hasControlCharacters(std::string_view s) {
    for (char c : s) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7f) {
            return true;
        }
    }
    return false;
}

}  // namespace

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
    const bool themeRefused = hasControlCharacters(first.settings.theme);
    const SettingsLoadResult second = parseSettings(serializeSettings(first.settings));
    if (themeRefused) {
        if (!second.diagnostics.empty()) {
            std::fprintf(stderr, "diagnostics on serialized settings with refused theme\n");
            std::abort();
        }
        if (!second.settings.theme.empty()) {
            std::fprintf(stderr, "refused theme was not omitted\n");
            std::abort();
        }
        Settings expected = first.settings;
        expected.theme.clear();
        if (!(second.settings == expected)) {
            std::fprintf(stderr, "editor settings round trip differs when theme refused\n");
            std::abort();
        }
    } else {
        if (!second.diagnostics.empty() || !(second.settings == first.settings)) {
            std::fprintf(stderr, "round trip differs\n");
            std::abort();
        }
    }
    (void)builtinSchemeFromSetting(first.settings.theme);
    return 0;
}
