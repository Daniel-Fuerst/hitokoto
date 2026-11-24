// SPDX-License-Identifier: GPL-3.0-or-later
// S046: typeset formulas for the editor. Layouts of the core's math engine (S045) kept per source,
// size and style, and painting a layout with the bundled math font.
#pragma once

#include "hitokoto/core/math.hpp"

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QRawFont>

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

namespace hitokoto::ui {

/// Layouts of formulas, laid out once per source, pixel size and style. Dropped as a whole when it
/// holds `kCapacity` of them (S046, decision 4). GUI thread only.
class MathCache {
public:
    static constexpr std::size_t kCapacity = 4096;

    /// The layout of `source` at `pixelSize` in display or text style, laid out on first use with
    /// the bundled font (S045).
    const core::MathLayout& layout(std::u16string_view source, double pixelSize, bool display);
    /// How many layouts were made, for tests that check the cache is hit.
    [[nodiscard]] std::size_t layoutsMade() const noexcept { return made_; }

private:
    std::unordered_map<std::u16string, core::MathLayout> layouts_;
    std::size_t made_ = 0;
};

/// Paints `layout` with its baseline origin at `origin`, glyphs and rules in `color`. Raw fonts are
/// made once per size.
void paintMathLayout(QPainter& painter, const core::MathLayout& layout, QPointF origin, const QColor& color);

}  // namespace hitokoto::ui
