// SPDX-License-Identifier: GPL-3.0-or-later
// S019 / ADR 0003: the editor's document layout. One QTextLayout per block, laid out lazily, with
// geometry in pixels. A block can be hidden or reserve a fixed height for an embedded widget.
#pragma once

#include "hitokoto/core/height_index.hpp"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>

#include <vector>

class QTextDocument;

namespace hitokoto::ui {

class BlockLayout : public QAbstractTextDocumentLayout {
    Q_OBJECT
    // QWidgetTextControl sets the caret width through this property (as on Qt's own layouts).
    Q_PROPERTY(int cursorWidth READ cursorWidth WRITE setCursorWidth)
public:
    explicit BlockLayout(QTextDocument* document);

    /// Makes `block` exactly `height` pixels high (its text is not drawn), e.g. for an embedded
    /// table; 0 restores the normal layout.
    void setReservedHeight(const QTextBlock& block, qreal height);
    [[nodiscard]] qreal reservedHeight(const QTextBlock& block) const;

    /// Hides `block` (height 0, the caret skips it) or shows it again.
    void setHidden(const QTextBlock& block, bool hidden);

    /// The visible block at document y, or an invalid block below the last one.
    [[nodiscard]] QTextBlock blockAt(qreal y) const;
    /// Lays out `block` now unless it already has lines.
    void ensureBlockLayout(const QTextBlock& block) const;
    /// Takes the block's height from the lines it has now; for callers that set the lines of a
    /// block's QTextLayout themselves (table rows, S018).
    void blockLayoutChanged(const QTextBlock& block);

    /// Collects size changes while many blocks are restyled (idle slices) and reports them once
    /// at the end, instead of after every block. Nests.
    class Batch {
    public:
        explicit Batch(BlockLayout* layout);
        ~Batch();
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;

    private:
        BlockLayout* layout_;
    };
    [[nodiscard]] bool inBatch() const noexcept { return batch_ > 0; }

    [[nodiscard]] int cursorWidth() const noexcept { return cursorWidth_; }
    void setCursorWidth(int width);

    // QAbstractTextDocumentLayout
    void draw(QPainter* painter, const PaintContext& context) override;
    [[nodiscard]] int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
    [[nodiscard]] int pageCount() const override;
    [[nodiscard]] QSizeF documentSize() const override;
    [[nodiscard]] QRectF frameBoundingRect(QTextFrame* frame) const override;
    [[nodiscard]] QRectF blockBoundingRect(const QTextBlock& block) const override;

signals:
    /// A block at document y `top` changed from `oldHeight` to `newHeight`. Lets the view keep its
    /// content still when blocks above the viewport change height.
    void blockHeightChanged(qreal top, qreal oldHeight, qreal newHeight);
    /// A batch ended after the document size was reported.
    void batchFinished();

protected:
    void documentChanged(int position, int charsRemoved, int charsAdded) override;

private:
    [[nodiscard]] qreal availableWidth() const;
    [[nodiscard]] qreal margin() const;
    void refreshMetrics();
    [[nodiscard]] bool inSync() const;
    void applyChange(int position, int charsRemoved, int charsAdded);
    void storeDeferredHeights();
    [[nodiscard]] qreal estimatedHeight(const QTextBlock& block) const;
    /// Lays out the lines of `block` (wrapping at the available width) and returns its height.
    qreal layoutBlock(const QTextBlock& block) const;
    [[nodiscard]] qreal heightOf(const QTextBlock& block) const;
    /// Stores `height` for block `number`; emits the size and height signals when it changed.
    void setHeight(int number, qreal height, bool emitNow) const;
    /// Estimated heights for every block (loading, a new width).
    void estimateAll();
    void scheduleSizeChange() const;

    // Heights per block number with their prefix sums: block <-> y in O(log n), new lines too.
    // Mutable: blocks are laid out lazily from const queries (blockBoundingRect, hitTest), like
    // Qt's layouts.
    mutable core::HeightIndex heights_;
    mutable bool sizeChangePending_ = false;
    mutable std::vector<QTextBlock> deferred_;  ///< laid out while heights_ was stale
    qreal laidOutWidth_ = -1;
    qreal lineSpacing_ = 0;       ///< of the default font, for estimates
    qreal averageCharWidth_ = 0;  ///< of the default font, for estimates
    int cursorWidth_ = 1;
    int batch_ = 0;
    mutable bool batchChanged_ = false;
    bool batchNeedsRepaint_ = false;
};

}  // namespace hitokoto::ui
