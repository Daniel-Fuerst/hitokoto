// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_layout.hpp"

#include <QTextBlock>
#include <QTextDocument>

namespace hitokoto::ui {

// Interface only (S019, test first); the layout follows in the next commit.

BlockLayout::BlockLayout(QTextDocument* document) : QAbstractTextDocumentLayout(document) {}

void BlockLayout::setReservedHeight(const QTextBlock& /*block*/, qreal /*height*/) {}

qreal BlockLayout::reservedHeight(const QTextBlock& /*block*/) const {
    return 0;
}

void BlockLayout::setHidden(const QTextBlock& /*block*/, bool /*hidden*/) {}

void BlockLayout::draw(QPainter* /*painter*/, const PaintContext& /*context*/) {}

int BlockLayout::hitTest(const QPointF& /*point*/, Qt::HitTestAccuracy /*accuracy*/) const {
    return -1;
}

int BlockLayout::pageCount() const {
    return 1;
}

QSizeF BlockLayout::documentSize() const {
    return {};
}

QRectF BlockLayout::frameBoundingRect(QTextFrame* /*frame*/) const {
    return {};
}

QRectF BlockLayout::blockBoundingRect(const QTextBlock& /*block*/) const {
    return {};
}

void BlockLayout::documentChanged(int /*position*/, int /*charsRemoved*/, int /*charsAdded*/) {}

}  // namespace hitokoto::ui
