// SPDX-License-Identifier: GPL-3.0-or-later
// Helpers shared by the core tests.
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <string>
#include <string_view>

namespace hitokoto::test {

/// The registry with all built-in elements.
inline const core::ElementRegistry& registry() {
    static const core::ElementRegistry r = core::ElementRegistry::withBuiltins();
    return r;
}

/// How the built-in elements show `line` on its own, revealed (the caret on it) or not.
inline core::LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}

/// `text` for a test's message: ASCII as it is, every other character as '?'.
inline std::string narrow(std::u16string_view text) {
    std::string out;
    for (const char16_t c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

}  // namespace hitokoto::test
