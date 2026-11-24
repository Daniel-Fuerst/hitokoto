// SPDX-License-Identifier: GPL-3.0-or-later
// What the editor paints itself, beside the text Qt draws: the boxes of code (S014, S015, S021),
// the line of a concealed thematic break (S023) and the dots of bullet items (S003). Each draws
// one block, or one panel, from its rendering and its resolved tokens; EditorWidget walks the
// blocks on screen and hands them over.
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/math.hpp"
#include "hitokoto/core/theme.hpp"

#include <QBrush>
#include <QColor>
#include <QPointF>
#include <QRectF>

#include <optional>
#include <span>

class QFontMetricsF;
class QPainter;
class QTextBlock;

namespace hitokoto::ui {

class EditorFormats;

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

/// S046: a document range drawn over typeset formulas it touches (a selection, a find highlight):
/// the whole formula gets `background`, its glyphs `text` if given.
struct FormulaHighlight {
    int from = 0;  ///< document positions, [from, to)
    int to = 0;
    QBrush background;
    std::optional<QColor> text;
};

/// S046: the typeset inline formulas of `block` (`rect`, viewport coordinates), each at the first
/// character of its source, on the baseline of the line it stands in; a formula the engine could
/// not lay out is left to its source.
void paintFormulas(QPainter& painter, const QTextBlock& block, const QRectF& rect, const core::LineRender& render,
                   const EditorFormats& formats, std::span<const FormulaHighlight> highlights);

/// S047: a display formula's panel in the opening line's reserved `rect` (viewport coordinates):
/// the margins above and below, the background and border, the formula centred, its baseline the
/// padding plus its ascent below the panel's top.
void paintDisplayFormula(QPainter& painter, const QRectF& rect, const core::MathLayout& formula,
                         const core::MathStyle& style);

/// S047: a bar in math.error at the left edge of a display block that could not be typeset.
void paintFormulaErrorBar(QPainter& painter, const QRectF& rect, const core::MathStyle& style);

}  // namespace hitokoto::ui
