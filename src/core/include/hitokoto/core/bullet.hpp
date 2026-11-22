// SPDX-License-Identifier: GPL-3.0-or-later
// Bullet list items as lines: the prefix of an item and its indentation (S003).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hitokoto::core {

struct BulletLineInfo {
    std::uint32_t prefixLength = 0;  ///< length of the full prefix up to content (e.g. 4 for "  - ")
    std::uint32_t markerChar = 0;    ///< column of '-', '*', '+'
    char16_t marker = 0;             ///< '-', '*', or '+'
    bool isEmpty = false;            ///< true if content after prefix is empty or only whitespace
    friend bool operator==(const BulletLineInfo&, const BulletLineInfo&) = default;
};

/// If `line` is a bullet item, returns its prefix information; else nullopt.
[[nodiscard]] std::optional<BulletLineInfo> parseBulletLine(std::u16string_view line);

/// Returns the new line with one level of indentation added (2 spaces before marker).
[[nodiscard]] std::u16string indentBulletLine(std::u16string_view line);

/// Returns the new line with one level of indentation removed (up to 2 spaces before marker),
/// or the original line if it is already at the top level.
[[nodiscard]] std::u16string outdentBulletLine(std::u16string_view line);

}  // namespace hitokoto::core
