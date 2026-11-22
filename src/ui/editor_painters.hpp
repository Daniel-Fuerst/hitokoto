// SPDX-License-Identifier: GPL-3.0-or-later
// What the editor paints itself, beside the text Qt draws: the boxes of code (S014, S015, S021),
// the line of a concealed thematic break (S023) and the dots of bullet items (S003). Each draws
// one block, or one panel, from its rendering and its resolved tokens; EditorWidget walks the
// blocks on screen and hands them over.
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <QColor>
#include <QPointF>
#include <QRectF>

class QFontMetricsF;
class QPainter;
class QTextBlock;

namespace hitokoto::ui {

/// A rounded box with the background and border of `style` (a code block panel, an inline code box).
void drawCodeBox(QPainter& painter, const QRectF& rect, const core::CodeStyle& style);

/// The boxes behind the inline code of `block`, one per line the code runs over; `origin` is the
/// block's top left in viewport coordinates.
void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render, QPointF origin,
                          const core::CodeStyle& style);

/// S023: the line of a concealed thematic break across the text column, centred vertically in its
/// block with its margins (`rect`, viewport coordinates), max(1, round(border-width x device pixel
/// ratio)) device pixels thick and on whole device pixel rows; it ends at `viewportWidth` at the latest.
void paintThematicBreak(QPainter& painter, const QTextBlock& block, const QRectF& rect,
                        const core::ThematicBreakStyle& style, qreal devicePixelRatio, qreal viewportWidth);

/// The dots of the bullet items of `block` (`rect`, viewport coordinates): filled at even depths,
/// rings at odd ones, of `radius`, in `color`; `metrics` are those of the document's default font.
void paintBullets(QPainter& painter, const QTextBlock& block, const QRectF& rect, const core::LineRender& render,
                  const QColor& color, const QFontMetricsF& metrics, qreal radius);

}  // namespace hitokoto::ui
