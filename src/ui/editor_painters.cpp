// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_painters.hpp"

#include "editor_formats.hpp"
#include "math_render.hpp"
#include "qt_bridge.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QTextBlock>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

void drawCodeBox(QPainter& painter, const QRectF& rect, const core::CodeStyle& style) {
    const qreal border = style.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(style.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style.background));
    const QRectF inner = rect.adjusted(border / 2, border / 2, -border / 2, -border / 2);
    const qreal radius = std::max<qreal>(0, style.radius - border / 2);
    painter.drawRoundedRect(inner, radius, radius);
}

void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render, QPointF origin,
                          const core::CodeStyle& style) {
    const QTextLayout* layout = block.layout();
    origin += layout->position();
    for (const auto& run : render.runs) {
        if (run.role != core::Role::InlineCode) {
            continue;
        }
        // The box covers the backticks around the code (the concealed ones reserve the padding).
        const int from = static_cast<int>(run.span.start) - 1;
        const int to = static_cast<int>(run.span.end()) + 1;
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            const int a = std::max(from, line.textStart());
            const int z = std::min(to, line.textStart() + line.textLength());
            if (a >= z) {
                continue;
            }
            const qreal x1 = line.cursorToX(a);
            const qreal x2 = line.cursorToX(z);
            drawCodeBox(painter, QRectF(origin.x() + x1, origin.y() + line.y(), x2 - x1, line.height()), style);
        }
    }
}

void paintThematicBreak(QPainter& painter, const QTextBlock& block, const QRectF& rect,
                        const core::ThematicBreakStyle& style, qreal devicePixelRatio, qreal viewportWidth) {
    // On whole device pixel rows, so the line stays crisp at fractional scaling.
    const QTextLayout* layout = block.layout();
    if (style.borderWidth <= 0 || style.border.a == 0 || layout == nullptr || layout->lineCount() == 0) {
        return;
    }
    const qreal dpr = devicePixelRatio;
    const int thickness = std::max(1, static_cast<int>(std::lround(style.borderWidth * dpr)));
    const QTextLine line = layout->lineAt(0);
    const qreal left = rect.left() + layout->position().x() + line.x();
    const qreal right = std::min(left + line.width(), viewportWidth);
    const int top = static_cast<int>(std::lround(rect.center().y() * dpr - thickness / 2.0));
    const bool antialiased = painter.testRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(QRectF(left, top / dpr, right - left, thickness / dpr), toQColor(style.border));
    painter.setRenderHint(QPainter::Antialiasing, antialiased);
}

void paintBullets(QPainter& painter, const QTextBlock& block, const QRectF& rect, const core::LineRender& render,
                  const QColor& color, const QFontMetricsF& metrics, qreal radius) {
    const QTextLayout* layout = block.layout();
    for (const auto& d : render.decorations) {
        if (d.kind != core::DecorationKind::Bullet) {
            continue;
        }
        const int column = static_cast<int>(d.column);
        const QTextLine line = layout->lineForTextPosition(column);
        if (!line.isValid()) {
            continue;
        }
        const qreal x1 = line.cursorToX(column);
        const qreal x2 = line.cursorToX(column + 1);
        const QPointF centre(rect.left() + layout->position().x() + (x1 + x2) / 2.0,
                             rect.top() + layout->position().y() + line.y() + line.ascent() - metrics.xHeight() / 2.0);
        if (d.depth % 2 == 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        } else {
            painter.setPen(QPen(color, std::max<qreal>(1.0, radius / 2.5)));
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawEllipse(centre, radius, radius);
    }
}

QRectF checkboxRect(const QTextBlock& block, const core::Decoration& d, QPointF origin, qreal side,
                    const QFontMetricsF& metrics) {
    const QTextLayout* layout = block.layout();
    if (layout == nullptr || layout->lineCount() == 0) {
        return {};
    }
    const int column = static_cast<int>(d.column);
    const QTextLine line = layout->lineForTextPosition(column);
    if (!line.isValid()) {
        return {};
    }
    const QPointF base = origin + layout->position();
    const qreal centreY = base.y() + line.y() + line.ascent() - metrics.xHeight() / 2.0;
    const qreal left = base.x() + line.cursorToX(column);
    return {std::round(left), std::round(centreY - side / 2.0), side, side};
}

void paintCheckboxes(QPainter& painter, const QTextBlock& block, const QRectF& rect, const core::LineRender& render,
                     const core::TaskStyle& style, qreal side, const QFontMetricsF& metrics) {
    for (const auto& d : render.decorations) {
        if (d.kind != core::DecorationKind::Checkbox) {
            continue;
        }
        const QRectF box = checkboxRect(block, d, rect.topLeft(), side, metrics);
        if (box.isEmpty()) {
            continue;
        }
        const qreal border = style.borderWidth;
        const QColor borderColor = toQColor(d.checked ? style.checkedBorder : style.border);
        painter.setPen(border > 0 ? QPen(borderColor, border) : QPen(Qt::NoPen));
        painter.setBrush(toQColor(d.checked ? style.checkedBackground : style.background));
        const QRectF inner = box.adjusted(border / 2, border / 2, -border / 2, -border / 2);
        const qreal radius = std::max<qreal>(0, style.radius - border / 2);
        painter.drawRoundedRect(inner, radius, radius);
        if (!d.checked) {
            continue;
        }
        // The tick: a short stroke down to the lower third, a long one up to the right.
        QPen tick(toQColor(style.check), std::max<qreal>(1.5, border));
        tick.setCapStyle(Qt::RoundCap);
        tick.setJoinStyle(Qt::RoundJoin);
        painter.setPen(tick);
        painter.setBrush(Qt::NoBrush);
        const QPointF points[] = {
            {box.left() + side * 0.25, box.top() + side * 0.52},
            {box.left() + side * 0.43, box.top() + side * 0.70},
            {box.left() + side * 0.76, box.top() + side * 0.32},
        };
        painter.drawPolyline(points, 3);
    }
}

void paintFormulas(QPainter& painter, const QTextBlock& block, const QRectF& rect, const core::LineRender& render,
                   const EditorFormats& formats, std::span<const FormulaHighlight> highlights) {
    const QTextLayout* layout = block.layout();
    if (layout == nullptr || render.math.empty()) {
        return;
    }
    const QString text = block.text();
    const QColor color = toQColor(formats.math().text);
    for (const core::MathSpan& m : render.math) {
        if (!EditorFormats::typeset(render, m) || static_cast<qsizetype>(m.source.end()) > text.size()) {
            continue;
        }
        const QStringView source = QStringView(text).sliced(m.source.start, m.source.length);
        const std::u16string_view view(reinterpret_cast<const char16_t*>(source.utf16()),
                                       static_cast<std::size_t>(source.size()));
        const auto held = formats.mathLayout(view, render.headingLevel, m.display);
        const core::MathLayout& formula = *held;
        if (formula.error) {
            continue;
        }
        const int column = static_cast<int>(m.source.start);
        const QTextLine line = layout->lineForTextPosition(column);
        if (!line.isValid()) {
            continue;
        }
        const QPointF origin(rect.left() + layout->position().x() + line.cursorToX(column),
                             rect.top() + layout->position().y() + line.y() + line.ascent());
        // The formula with its dollars, in document positions.
        const int from = block.position() + column - 1;
        const int to = block.position() + static_cast<int>(m.source.end()) + 1;
        QColor glyphs = color;
        for (const FormulaHighlight& h : highlights) {
            if (h.from < to && h.to > from) {
                const qreal top = std::min(origin.y() - formula.ascent, rect.top() + layout->position().y() + line.y());
                const qreal bottom = std::max(origin.y() + formula.descent,
                                              rect.top() + layout->position().y() + line.y() + line.height());
                painter.fillRect(QRectF(origin.x(), top, formula.width, bottom - top), h.background);
                if (h.text) {
                    glyphs = *h.text;
                }
            }
        }
        paintMathLayout(painter, formula, origin, glyphs);
    }
}

void paintDisplayFormula(QPainter& painter, const QRectF& rect, const core::MathLayout& formula,
                         const core::MathStyle& style) {
    if (formula.error) {
        return;
    }
    const QRectF panel = rect.adjusted(0, style.displayMargin, 0, -style.displayMargin);
    const qreal border = style.displayBorderWidth;
    painter.save();
    painter.setPen(border > 0 ? QPen(toQColor(style.displayBorder), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style.displayBackground));
    const QRectF inner = panel.adjusted(border / 2, border / 2, -border / 2, -border / 2);
    const qreal radius = std::max<qreal>(0, style.displayRadius - border / 2);
    painter.drawRoundedRect(inner, radius, radius);
    painter.restore();
    const QPointF origin(panel.center().x() - formula.width / 2, panel.top() + style.displayPadding + formula.ascent);
    paintMathLayout(painter, formula, origin, toQColor(style.text));
}

void paintFormulaErrorBar(QPainter& painter, const QRectF& rect, const core::MathStyle& style) {
    // Two pixels wide, just left of the text, as wide as a cursor so it reads as a mark, not a border.
    painter.fillRect(QRectF(rect.left() - 6, rect.top(), 2, rect.height()),
                     toQColor(style.error));  // theme-lint: allow
}

}  // namespace hitokoto::ui
