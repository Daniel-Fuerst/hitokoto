// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <QTextBlockUserData>

#include <optional>

namespace hitokoto::ui {

/// Per-block metadata that a block's text and user state cannot hold. A block carries it only
/// while there is something to keep: a line ending that differs from the file's majority
/// (ADR 0002), an open multi-line block after the line (BlockStyler), or a height the block
/// reserves for an embedded widget (BlockLayout, S019). Most blocks carry none.
class BlockData final : public QTextBlockUserData {
public:
    std::optional<core::LineEnding> ending;
    core::BlockState blockState;  ///< state after this line; only meaningful while active
    double reservedHeight = 0;    ///< > 0: the block is this many pixels high, its text not drawn

    [[nodiscard]] bool empty() const noexcept { return !ending && !blockState.active() && reservedHeight <= 0; }
};

}  // namespace hitokoto::ui
