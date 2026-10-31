// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <QTextBlockUserData>

#include <optional>

namespace hitokoto::ui {

/// Per-block metadata that a block's text and user state cannot hold. A block carries it only
/// while there is something to keep: a line ending that differs from the file's majority
/// (ADR 0002), an open multi-line block after the line (BlockStyler), a height the block
/// reserves for an embedded widget (BlockLayout, S019), or space above or below its lines (the
/// margin of a code block panel, S021). Most blocks carry none.
class BlockData final : public QTextBlockUserData {
public:
    std::optional<core::LineEnding> ending;
    core::BlockState blockState;  ///< state after this line; only meaningful while active
    double reservedHeight = 0;    ///< > 0: the block is this many pixels high, its text not drawn
    double spaceAbove = 0;        ///< pixels above the first line, part of the block's height
    double spaceBelow = 0;        ///< pixels below the last line, part of the block's height
    int wrapIndent = 0;           ///< columns to indent continuation lines (hanging indent, S003)

    [[nodiscard]] bool empty() const noexcept {
        return !ending && !blockState.active() && reservedHeight <= 0 && spaceAbove <= 0 && spaceBelow <= 0 &&
               wrapIndent <= 0;
    }
};

// QTextBlock::userState() bit layout:
// Bits 0-1 (0x3): style (0 = unstyled, 1 = concealed, 2 = revealed)
// Bit 2 (0x4): block state after line is known
// Bit 3 (0x8): block state is active (block carries BlockData)
// Bits 4-19 (0xFFFF0): paragraph of block state (S023, ADR 0006)
// Bit 20 (0x100000): setext level of paragraph is known
// Bits 21-22 (0x600000): setext level
// Bit 23 (0x800000): link to paragraph above is known
// Bits 24-25 (0x3000000): link (none, continues, underline)
// Bits 26-30 (0x7C000000): wrapIndent columns for hanging indent (S003); 0 = none
constexpr int kWrapIndentShift = 26;
constexpr int kWrapIndentMask = 0x1F << kWrapIndentShift;

}  // namespace hitokoto::ui
