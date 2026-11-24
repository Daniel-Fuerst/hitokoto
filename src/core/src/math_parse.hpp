// SPDX-License-Identifier: GPL-3.0-or-later
// S045: the LaTeX math subset read into a tree, which math_layout.cpp lays out. Internal to the core.
#pragma once

#include "hitokoto/core/math.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core::math {

/// TeX's classes of math atoms; they decide the space between neighbours (The TeXbook, ch. 18).
enum class AtomClass : std::uint8_t { Ord, Op, Bin, Rel, Open, Close, Punct, Inner };

struct Node {
    enum class Kind : std::uint8_t {
        Null,       ///< nothing (a missing script, a `.` delimiter)
        Symbol,     ///< one character: `ch`, `cls`
        Group,      ///< `children` in a row
        Scripts,    ///< children: base, superscript, subscript (Null when absent)
        Fraction,   ///< children: numerator, denominator; `rule` false for \binom; `left`/`right` delimiters
        Radical,    ///< children: radicand, degree (Null for \sqrt without one)
        Delimited,  ///< children: body; `left`, `right` (0 = null delimiter)
        LargeOp,    ///< `ch`; `limits` for limits above and below in display style
        Text,       ///< `text` upright, from \text, \mathrm, operator names; `cls`, `limits`
        Space,      ///< `amount` in em (negative for \!)
        Array,      ///< children: cells row by row; `rows`, `columns`, `align`, `left`, `right`
        XArrow,     ///< children: above, below; `ch` the arrow
        Raise,      ///< children: one node raised by `amount` em; `scale` of the size (the \LaTeX logo)
    };
    Kind kind = Kind::Null;
    AtomClass cls = AtomClass::Ord;
    char32_t ch = 0;
    char32_t plain = 0;  ///< Symbol: the character as written, if `ch` is a styled form the font may lack
    char32_t left = 0;
    char32_t right = 0;
    bool rule = true;
    bool limits = false;
    bool displayCells = false;  ///< Array: cells in display style (aligned), else text style
    double amount = 0;
    double scale = 1;
    std::uint16_t rows = 0;
    std::uint16_t columns = 0;
    double columnGap = 0;  ///< Array: em between columns (between pairs for aligned)
    double rowGap = 0;     ///< Array: extra em between rows (\jot)
    std::u32string text;
    std::string align;  ///< Array: 'l', 'c' or 'r' per column
    std::vector<Node> children;
};

struct ParseResult {
    Node root;
    std::optional<MathError> error;
};

/// Reads a formula; never throws. At most 64 levels deep.
[[nodiscard]] ParseResult parse(std::u16string_view source);

}  // namespace hitokoto::core::math
