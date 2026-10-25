// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <QTextBlockUserData>

#include <optional>

namespace hitokoto::ui {

class BlockData final : public QTextBlockUserData {
public:
    std::optional<core::LineEnding> ending;
    core::FenceState fenceState;
    bool fenceStateValid = false;
};

}  // namespace hitokoto::ui
