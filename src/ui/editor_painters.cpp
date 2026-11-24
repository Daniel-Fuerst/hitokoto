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
        const core::MathLayout& formula = formats.mathLayout(view, render.headingLevel, m.display);
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

}  // namespace hitokoto::ui
