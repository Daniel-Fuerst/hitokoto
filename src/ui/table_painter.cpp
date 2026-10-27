// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_painter.hpp"

#include "block_styler.hpp"
#include "qt_bridge.hpp"
#include "table_controller.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

namespace hitokoto::ui {

void TablePainter::drawTableControlButton(QPainter& painter, const QRectF& rect, const core::TableStyle& style,
                                          bool plus) {
    const qreal border = std::max<qreal>(1.0, style.borderWidth);
    painter.setPen(QPen(toQColor(style.border), border));
    painter.setBrush(toQColor(style.headerBackground));
    const qreal radius = std::min<qreal>(style.radius, std::min(rect.width(), rect.height()) / 2.0);
    painter.drawRoundedRect(rect.adjusted(border / 2, border / 2, -border / 2, -border / 2), radius, radius);

    const QPointF c = rect.center();
    const qreal arm = std::min({rect.width(), rect.height(), 12.0}) * 0.28;
    painter.setPen(QPen(toQColor(style.headerText), 1.5));
    painter.drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
    if (plus) {
        painter.drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
    }
}

void TablePainter::paintTableBackgrounds(QPainter& painter, const QTextDocument* doc, BlockStyler* styler,
                                         const TableController& controller, const TableMetrics& metrics) {
    const QRectF tRect = controller.tableRect(metrics.firstBlock);
    if (tRect.isEmpty()) {
        return;
    }
    const core::TableStyle& style = styler->formats().table();
    const qreal bw = style.borderWidth;
    const QRectF inner = tRect.adjusted(bw / 2, bw / 2, -bw / 2, -bw / 2);
    const qreal radius = std::max<qreal>(0, style.radius - bw / 2);
    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(inner, radius, radius);
    painter.setClipPath(clip);
    painter.fillRect(tRect, toQColor(style.background));

    int bodyRowIndex = 0;
    for (QTextBlock b = doc->findBlockByNumber(metrics.firstBlock); b.isValid() && b.blockNumber() <= metrics.lastBlock;
         b = b.next()) {
        const auto r = styler->renderFor(b);
        if (r.tableDelimiter) {
            continue;
        }
        const QRectF cRect = controller.cellRect(b.blockNumber(), 0);
        const QRectF rowBand(tRect.left(), cRect.top(), tRect.width(), cRect.height());
        if (r.tableHeader) {
            painter.fillRect(rowBand, toQColor(style.headerBackground));
        } else {
            if (bodyRowIndex % 2 == 1) {
                painter.fillRect(rowBand, toQColor(style.stripeBackground));
            }
            ++bodyRowIndex;
        }
    }
    painter.restore();
}

void TablePainter::paintTableGridAndControls(QPainter& painter, const QTextDocument* doc, BlockStyler* styler,
                                             const TableController& controller, const TableMetrics& metrics) {
    const QRectF tRect = controller.tableRect(metrics.firstBlock);
    if (tRect.isEmpty()) {
        return;
    }
    const core::TableStyle& style = styler->formats().table();
    const qreal bw = style.borderWidth;
    if (bw > 0) {
        painter.setPen(QPen(toQColor(style.border), bw));
        painter.setBrush(Qt::NoBrush);

        // Vertical column dividers.
        for (std::size_t c = 0; c + 1 < metrics.columnWidths.size(); ++c) {
            const qreal x = controller.cellRect(metrics.firstBlock, static_cast<int>(c)).right();
            painter.drawLine(QPointF(x, tRect.top()), QPointF(x, tRect.bottom()));
        }

        // Horizontal row dividers (below header and between body rows).
        for (QTextBlock b = doc->findBlockByNumber(metrics.firstBlock);
             b.isValid() && b.blockNumber() < metrics.lastBlock; b = b.next()) {
            const auto r = styler->renderFor(b);
            if (r.tableDelimiter) {
                continue;
            }
            const QRectF cRect = controller.cellRect(b.blockNumber(), 0);
            if (cRect.bottom() < tRect.bottom() - 0.5) {
                painter.drawLine(QPointF(tRect.left(), cRect.bottom()), QPointF(tRect.right(), cRect.bottom()));
            }
        }

        // Outer table border.
        const QRectF inner = tRect.adjusted(bw / 2, bw / 2, -bw / 2, -bw / 2);
        const qreal radius = std::max<qreal>(0, style.radius - bw / 2);
        painter.drawRoundedRect(inner, radius, radius);
    }

    if (controller.isAddRowButtonVisible(metrics.firstBlock)) {
        drawTableControlButton(painter, controller.addRowButtonRect(metrics.firstBlock), style, true);
    }
    if (controller.isAddColumnButtonVisible(metrics.firstBlock)) {
        drawTableControlButton(painter, controller.addColumnButtonRect(metrics.firstBlock), style, true);
    }
    if (controller.hoveredDeleteRowBlock() >= metrics.firstBlock + 2 &&
        controller.hoveredDeleteRowBlock() <= metrics.lastBlock &&
        controller.isDeleteRowButtonVisible(controller.hoveredDeleteRowBlock())) {
        drawTableControlButton(painter, controller.deleteRowButtonRect(controller.hoveredDeleteRowBlock()), style,
                               false);
    }
    if (controller.hoveredDeleteColTable() == metrics.firstBlock && controller.hoveredDeleteColIndex() >= 0 &&
        controller.isDeleteColumnButtonVisible(metrics.firstBlock, controller.hoveredDeleteColIndex())) {
        drawTableControlButton(
            painter, controller.deleteColumnButtonRect(metrics.firstBlock, controller.hoveredDeleteColIndex()), style,
            false);
    }
}

}  // namespace hitokoto::ui
