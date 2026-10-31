// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_layout.hpp"

#include "block_data.hpp"

#include <QFontMetricsF>
#include <QMetaObject>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hitokoto::ui {
namespace {

const BlockData* dataOf(const QTextBlock& block) {
    return dynamic_cast<const BlockData*>(block.userData());
}

}  // namespace

BlockLayout::BlockLayout(QTextDocument* document) : QAbstractTextDocumentLayout(document) {
    refreshMetrics();
    estimateAll();
}

// --- block attributes -------------------------------------------------------------------------

void BlockLayout::addClaimant(const Claimant* claimant) {
    if (std::find(claimants_.begin(), claimants_.end(), claimant) == claimants_.end()) {
        claimants_.push_back(claimant);
    }
}

void BlockLayout::removeClaimant(const Claimant* claimant) {
    std::erase(claimants_, claimant);
}

bool BlockLayout::claimedByOther(const QTextBlock& block, const Claimant* asker) const {
    return std::any_of(claimants_.begin(), claimants_.end(),
                       [&](const Claimant* c) { return c != asker && c->claims(block); });
}

void BlockLayout::setReservedHeight(const QTextBlock& block, qreal height) {
    if (!block.isValid()) {
        return;
    }
    auto* data = dynamic_cast<BlockData*>(block.userData());
    if (height > 0) {
        if (data == nullptr) {
            data = new BlockData;
            const_cast<QTextBlock&>(block).setUserData(data);
        }
        data->reservedHeight = height;
    } else if (data != nullptr) {
        data->reservedHeight = 0;
        if (data->empty()) {
            const_cast<QTextBlock&>(block).setUserData(nullptr);  // deletes it
        }
    }
    setHeight(block.blockNumber(), heightOf(block), true);
    emit update();
}

qreal BlockLayout::reservedHeight(const QTextBlock& block) const {
    const BlockData* data = dataOf(block);
    return data != nullptr ? data->reservedHeight : 0.0;
}

void BlockLayout::setHidden(const QTextBlock& block, bool hidden) {
    if (!block.isValid() || block.isVisible() == !hidden) {
        return;
    }
    const_cast<QTextBlock&>(block).setVisible(!hidden);
    setHeight(block.blockNumber(), heightOf(block), true);
    emit update();
}

void BlockLayout::setBlockSpacing(const QTextBlock& block, qreal above, qreal below) {
    if (!block.isValid() ||
        (std::abs(spaceAbove(block) - above) < 1e-6 && std::abs(spaceBelow(block) - below) < 1e-6)) {
        return;
    }
    auto* data = dynamic_cast<BlockData*>(block.userData());
    if (data == nullptr) {
        data = new BlockData;
        const_cast<QTextBlock&>(block).setUserData(data);
    }
    data->spaceAbove = above;
    data->spaceBelow = below;
    if (data->empty()) {
        const_cast<QTextBlock&>(block).setUserData(nullptr);  // deletes it
    }
    if (block.layout()->lineCount() > 0 && reservedHeight(block) <= 0) {
        layoutBlock(block);  // the lines move down by the space above
    }
    if (!inSync()) {
        deferred_.push_back(block);
        return;
    }
    setHeight(block.blockNumber(), heightOf(block), true);
    if (batch_ > 0) {
        batchNeedsRepaint_ = true;
    } else {
        emit update();
    }
}

qreal BlockLayout::spaceAbove(const QTextBlock& block) const {
    const BlockData* data = dataOf(block);
    return data != nullptr ? data->spaceAbove : 0.0;
}

qreal BlockLayout::spaceBelow(const QTextBlock& block) const {
    const BlockData* data = dataOf(block);
    return data != nullptr ? data->spaceBelow : 0.0;
}

BlockLayout::Batch::Batch(BlockLayout* layout) : layout_(layout) {
    if (layout_ != nullptr) {
        ++layout_->batch_;
    }
}

BlockLayout::Batch::~Batch() {
    if (layout_ == nullptr || --layout_->batch_ > 0) {
        return;
    }
    if (layout_->batchChanged_) {
        // Blocks report their own repaints (updateBlock); only the size is still owed.
        layout_->batchChanged_ = false;
        emit layout_->documentSizeChanged(layout_->documentSize());
    }
    if (layout_->batchNeedsRepaint_) {
        layout_->batchNeedsRepaint_ = false;
        emit layout_->update();
    }
    emit layout_->batchFinished();
}

void BlockLayout::setCursorWidth(int width) {
    cursorWidth_ = width;
}

// --- geometry ---------------------------------------------------------------------------------

qreal BlockLayout::margin() const {
    return document()->documentMargin();
}

qreal BlockLayout::availableWidth() const {
    const qreal width = document()->textWidth();
    const qreal usable = width > 0 ? width : qreal(std::numeric_limits<int>::max());
    return std::max<qreal>(1.0, usable - 2 * margin());
}

void BlockLayout::refreshMetrics() {
    const QFontMetricsF metrics(document()->defaultFont());
    lineSpacing_ = metrics.lineSpacing();
    averageCharWidth_ = metrics.averageCharWidth();
}

qreal BlockLayout::estimatedHeight(const QTextBlock& block) const {
    if (!block.isVisible()) {
        return 0;
    }
    if (const qreal reserved = reservedHeight(block); reserved > 0) {
        return reserved;
    }
    const qreal textWidth = averageCharWidth_ * block.length();
    const qreal lines = std::max<qreal>(1.0, std::ceil(textWidth / availableWidth()));
    return lines * lineSpacing_ + spaceAbove(block) + spaceBelow(block);
}

qreal BlockLayout::layoutBlock(const QTextBlock& block) const {
    QTextLayout* layout = block.layout();
    layout->setTextOption(document()->defaultTextOption());
    layout->beginLayout();
    const qreal width = availableWidth();
    const qreal left = margin();
    qreal height = spaceAbove(block);
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLeadingIncluded(true);
        line.setLineWidth(width);
        line.setPosition(QPointF(left, height));
        height += line.height();
    }
    layout->endLayout();
    layout->setPosition(QPointF(0, 0));
    return height + spaceBelow(block);
}

qreal BlockLayout::heightOf(const QTextBlock& block) const {
    if (!block.isVisible()) {
        return 0;
    }
    if (const qreal reserved = reservedHeight(block); reserved > 0) {
        return reserved;
    }
    const QTextLayout* layout = block.layout();
    if (layout->lineCount() == 0) {
        return estimatedHeight(block);
    }
    // Lines may sit side by side (table cells, S018); the block ends below the lowest one, plus the
    // space below it (S021).
    qreal bottom = 0;
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        bottom = std::max(bottom, line.y() + line.height());
    }
    return bottom + spaceBelow(block);
}

bool BlockLayout::inSync() const {
    // Qt emits QTextDocument::contentsChange before it calls documentChanged; slots that query the
    // layout in between see the new blocks while heights_ still has the old numbering.
    return heights_.size() == static_cast<std::size_t>(document()->blockCount());
}

void BlockLayout::ensureBlockLayout(const QTextBlock& block) const {
    if (!block.isValid() || block.layout()->lineCount() > 0) {
        return;
    }
    layoutBlock(block);
    if (!inSync()) {
        deferred_.push_back(block);  // stored once documentChanged has renumbered
        return;
    }
    setHeight(block.blockNumber(), heightOf(block), false);
}

void BlockLayout::blockLayoutChanged(const QTextBlock& block) {
    if (!block.isValid()) {
        return;
    }
    if (!inSync()) {
        deferred_.push_back(block);
        return;
    }
    setHeight(block.blockNumber(), heightOf(block), true);
    emit update();
}

void BlockLayout::storeDeferredHeights() {
    std::vector<QTextBlock> blocks;
    blocks.swap(deferred_);
    for (const QTextBlock& block : blocks) {
        if (block.isValid()) {
            setHeight(block.blockNumber(), heightOf(block), batch_ == 0);
        }
    }
}

void BlockLayout::setHeight(int number, qreal height, bool emitNow) const {
    if (number < 0 || static_cast<std::size_t>(number) >= heights_.size() || !inSync()) {
        return;
    }
    const auto index = static_cast<std::size_t>(number);
    const qreal delta = height - heights_.height(index);
    if (std::abs(delta) < 1e-6) {
        return;
    }
    heights_.set(index, height);
    const qreal top = margin() + heights_.sumBefore(index);
    auto* self = const_cast<BlockLayout*>(this);
    if (batch_ > 0) {
        batchChanged_ = true;
    } else if (emitNow) {
        emit self->documentSizeChanged(documentSize());
    } else {
        scheduleSizeChange();
    }
    emit self->blockHeightChanged(top, height - delta, height);
}

void BlockLayout::scheduleSizeChange() const {
    // Lazily laid-out blocks change height inside const queries (often while painting); report the
    // new size once the event loop runs again.
    if (sizeChangePending_) {
        return;
    }
    sizeChangePending_ = true;
    auto* self = const_cast<BlockLayout*>(this);
    QMetaObject::invokeMethod(
        self,
        [self] {
            self->sizeChangePending_ = false;
            emit self->documentSizeChanged(self->documentSize());
            emit self->update();
        },
        Qt::QueuedConnection);
}

void BlockLayout::estimateAll() {
    std::vector<double> heights(static_cast<std::size_t>(document()->blockCount()), 0.0);
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        heights[static_cast<std::size_t>(b.blockNumber())] = estimatedHeight(b);
    }
    heights_.assign(heights);
}

QTextBlock BlockLayout::blockAt(qreal y) const {
    const std::size_t index = heights_.indexAt(y - margin());
    return index < heights_.size() ? document()->findBlockByNumber(static_cast<int>(index)) : QTextBlock{};
}

QRectF BlockLayout::blockBoundingRect(const QTextBlock& block) const {
    if (!block.isValid()) {
        return {};
    }
    ensureBlockLayout(block);
    const qreal top = margin() + heights_.sumBefore(static_cast<std::size_t>(block.blockNumber()));
    const qreal width = document()->textWidth() > 0 ? document()->textWidth() : availableWidth() + 2 * margin();
    const auto number = static_cast<std::size_t>(block.blockNumber());
    const qreal height = inSync() && number < heights_.size() ? heights_.height(number) : heightOf(block);
    return {0, top, width, height};
}

QSizeF BlockLayout::documentSize() const {
    const qreal width = document()->textWidth() > 0 ? document()->textWidth() : availableWidth() + 2 * margin();
    return {width, heights_.total() + 2 * margin()};
}

QRectF BlockLayout::frameBoundingRect(QTextFrame* frame) const {
    if (frame != document()->rootFrame()) {
        return {};
    }
    return {QPointF(0, 0), documentSize()};
}

int BlockLayout::pageCount() const {
    return 1;
}

int BlockLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const {
    QTextBlock block = blockAt(point.y());
    if (!block.isValid()) {
        if (accuracy == Qt::ExactHit) {
            return -1;
        }
        block = document()->lastBlock();
        while (block.isValid() && !block.isVisible()) {
            block = block.previous();
        }
        return block.isValid() ? block.position() + block.length() - 1 : 0;
    }
    const QRectF rect = blockBoundingRect(block);
    if (reservedHeight(block) > 0) {
        return block.position();
    }
    const QTextLayout* layout = block.layout();
    const QPointF local = point - rect.topLeft();
    // Lines may sit side by side (table cells): prefer the line whose box holds the point, then
    // the line at that height, then the nearest line above or below.
    int best = -1;
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        if (local.y() >= line.y() && local.y() < line.y() + line.height()) {
            const bool inside = local.x() >= line.x() && local.x() < line.x() + line.width();
            if (inside || best < 0) {
                best = i;
            }
            if (inside) {
                break;
            }
        }
    }
    if (best < 0 && layout->lineCount() > 0) {
        best = local.y() < layout->lineAt(0).y() ? 0 : layout->lineCount() - 1;
    }
    if (best < 0) {
        return block.position();
    }
    const QTextLine line = layout->lineAt(best);
    if (accuracy == Qt::ExactHit && (local.x() < line.x() || local.x() > line.x() + line.naturalTextWidth())) {
        return -1;
    }
    return block.position() + line.xToCursor(local.x());
}

// --- painting ---------------------------------------------------------------------------------

void BlockLayout::draw(QPainter* painter, const PaintContext& context) {
    const QRectF clip = context.clip.isValid() ? context.clip : QRectF(QPointF(0, 0), documentSize());
    for (QTextBlock block = blockAt(clip.top()); block.isValid(); block = block.next()) {
        if (!block.isVisible()) {
            continue;
        }
        const QRectF rect = blockBoundingRect(block);
        if (rect.top() > clip.bottom()) {
            break;
        }
        if (reservedHeight(block) > 0) {
            continue;  // an embedded widget covers it
        }
        QTextLayout* layout = block.layout();
        const int start = block.position();
        const int length = block.length();

        QList<QTextLayout::FormatRange> selections;
        for (const Selection& selection : context.selections) {
            const int from = selection.cursor.selectionStart() - start;
            const int to = selection.cursor.selectionEnd() - start;
            if (from < length && to > 0 && to > from) {
                QTextLayout::FormatRange range;
                range.start = std::max(from, 0);
                range.length = std::min(to, length) - range.start;
                range.format = selection.format;
                selections.append(range);
            } else if (!selection.cursor.hasSelection() &&
                       selection.format.hasProperty(QTextFormat::FullWidthSelection) &&
                       block.contains(selection.cursor.position())) {
                QTextLayout::FormatRange range;
                const QTextLine line = layout->lineForTextPosition(selection.cursor.position() - start);
                range.start = line.isValid() ? line.textStart() : 0;
                range.length = line.isValid() ? std::max(line.textLength(), 1) : length;
                range.format = selection.format;
                selections.append(range);
            }
        }
        layout->draw(painter, rect.topLeft(), selections, clip);

        const int cursor = context.cursorPosition;
        const bool cursorHere = cursor >= start && cursor < start + length;
        const bool preeditCursor = cursor < -1 && !layout->preeditAreaText().isEmpty();
        if (cursorHere || preeditCursor) {
            const int position = cursor < -1 ? layout->preeditAreaPosition() - (cursor + 2) : cursor - start;
            painter->setPen(context.palette.color(QPalette::Text));
            layout->drawCursor(painter, rect.topLeft(), position, cursorWidth_);
        }
    }
}

// --- document changes -------------------------------------------------------------------------

void BlockLayout::documentChanged(int position, int charsRemoved, int charsAdded) {
    applyChange(position, charsRemoved, charsAdded);
    storeDeferredHeights();
}

void BlockLayout::applyChange(int position, int charsRemoved, int charsAdded) {
    QTextDocument* doc = document();
    const int newCount = doc->blockCount();
    const int oldCount = static_cast<int>(heights_.size());
    if (newCount < oldCount && charsRemoved == 0) {
        emit blocksGoneUnreported();
    }
    const qreal available = availableWidth();
    const bool wholeDocument = position == 0 && charsAdded >= doc->characterCount() - 1;
    if (wholeDocument) {
        refreshMetrics();  // the default font or margin may have changed
    }

    if (std::abs(available - laidOutWidth_) > 0.5 || oldCount == 0 || (wholeDocument && charsRemoved == 0)) {
        // Everything wraps anew: estimate all blocks, lay them out lazily.
        laidOutWidth_ = available;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            b.layout()->clearLayout();
        }
        estimateAll();
        emit documentSizeChanged(documentSize());
        emit update();
        return;
    }

    QTextBlock first = doc->findBlock(position);
    if (!first.isValid()) {
        first = doc->lastBlock();
    }
    // As in Qt's QPlainTextDocumentLayout: a change that stays inside one block is laid out now;
    // anything wider, including QTextDocument::markContentsDirty (which reports the range as
    // removed and added), only drops the layouts. They are laid out again when drawn or queried,
    // so restyling a whole file in idle slices costs no text shaping for lines nobody sees.
    QTextBlock last = doc->findBlock(position + std::max(charsRemoved + charsAdded - 1, 0));
    if (!last.isValid()) {
        last = doc->lastBlock();
    }
    const int firstNumber = first.blockNumber();
    const int delta = newCount - oldCount;

    if (delta == 0 && first == last) {
        first.layout()->clearLayout();
        if (first.isVisible() && reservedHeight(first) <= 0) {
            layoutBlock(first);
        }
        const qreal before = heights_.height(static_cast<std::size_t>(firstNumber));
        const qreal after = heightOf(first);
        if (std::abs(after - before) < 1e-6) {
            emit updateBlock(first);
            return;
        }
        setHeight(firstNumber, after, true);
        if (batch_ == 0) {
            const qreal top = margin() + heights_.sumBefore(static_cast<std::size_t>(firstNumber));
            emit update(QRectF(0, top, documentSize().width(), documentSize().height() - top));
        } else {
            batchNeedsRepaint_ = true;  // blocks below moved
        }
        return;
    }

    if (delta != 0) {
        // Blocks [firstNumber, newLast - delta] became [firstNumber, newLast]; the new ones get
        // estimated heights until they are laid out.
        QTextBlock end = doc->findBlock(position + charsAdded);
        if (!end.isValid()) {
            end = doc->lastBlock();
        }
        const int newLast = end.blockNumber();
        const int oldLast = std::clamp(newLast - delta, firstNumber - 1, oldCount - 1);
        const int eraseFrom = std::clamp(firstNumber, 0, oldCount);
        const int eraseTo = std::clamp(oldLast + 1, firstNumber, oldCount);
        heights_.erase(static_cast<std::size_t>(eraseFrom), static_cast<std::size_t>(eraseTo - eraseFrom));
        const int inserted = newLast - firstNumber + 1;
        heights_.insert(static_cast<std::size_t>(std::clamp(firstNumber, 0, static_cast<int>(heights_.size()))),
                        static_cast<std::size_t>(inserted), 0.0);
        QTextBlock b = first;
        for (int n = firstNumber; n <= newLast && b.isValid(); ++n, b = b.next()) {
            heights_.set(static_cast<std::size_t>(n), estimatedHeight(b));  // O(log n) each
        }
        if (last.blockNumber() < newLast) {
            last = end;
        }
    }
    // Keep the heights of blocks whose layouts are dropped: they are right unless the text
    // changed, and laying the block out again corrects them.
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        b.layout()->clearLayout();
        if (b == last) {
            break;
        }
    }
    if (batch_ > 0) {
        batchChanged_ = batchChanged_ || delta != 0;
        batchNeedsRepaint_ = true;
        return;
    }
    if (delta != 0) {
        emit documentSizeChanged(documentSize());
    }
    const qreal top = margin() + heights_.sumBefore(static_cast<std::size_t>(firstNumber));
    emit update(QRectF(0, top, documentSize().width(), documentSize().height() - top));
}

}  // namespace hitokoto::ui
