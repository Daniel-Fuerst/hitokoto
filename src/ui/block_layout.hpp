// SPDX-License-Identifier: GPL-3.0-or-later
// S019 / ADR 0003: the editor's document layout. One QTextLayout per block, laid out lazily, with
// geometry in pixels. A block can be hidden or reserve a fixed height for an embedded widget.
#pragma once

#include <QAbstractTextDocumentLayout>

class QTextBlock;
class QTextDocument;

namespace hitokoto::ui {

class BlockLayout : public QAbstractTextDocumentLayout {
    Q_OBJECT
public:
    explicit BlockLayout(QTextDocument* document);

    /// Makes `block` exactly `height` pixels high (its text is not drawn), e.g. for an embedded
    /// table; 0 restores the normal layout.
    void setReservedHeight(const QTextBlock& block, qreal height);
    [[nodiscard]] qreal reservedHeight(const QTextBlock& block) const;

    /// Hides `block` (height 0, the caret skips it) or shows it again.
    void setHidden(const QTextBlock& block, bool hidden);

    // QAbstractTextDocumentLayout
    void draw(QPainter* painter, const PaintContext& context) override;
    [[nodiscard]] int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
    [[nodiscard]] int pageCount() const override;
    [[nodiscard]] QSizeF documentSize() const override;
    [[nodiscard]] QRectF frameBoundingRect(QTextFrame* frame) const override;
    [[nodiscard]] QRectF blockBoundingRect(const QTextBlock& block) const override;

protected:
    void documentChanged(int position, int charsRemoved, int charsAdded) override;
};

}  // namespace hitokoto::ui
