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

void setSharesPrevLine(const QTextBlock& block, bool shares) {
    if (!block.isValid()) {
        return;
    }
    const int state = block.userState() < 0 ? 0 : block.userState();
    const int updated = shares ? (state | kSharesPrevLineBit) : (state & ~kSharesPrevLineBit);
    if (updated != state) {
        const_cast<QTextBlock&>(block).setUserState(updated);
    }
}

/// Where a line of a block starts in its text and in the block (S033: whether laying a block out
/// again changed what the next block continues from).
struct LineShape {
    int textStart = 0;
    int textLength = 0;
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    bool operator==(const LineShape&) const = default;
};

std::vector<LineShape> lineShapes(const QTextBlock& block) {
    const QTextLayout* layout = block.layout();
    std::vector<LineShape> shapes;
    shapes.reserve(static_cast<std::size_t>(layout->lineCount()));
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        shapes.push_back({line.textStart(), line.textLength(), line.x(), line.y(), line.naturalTextWidth()});
    }
    return shapes;
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

void BlockLayout::setWrapIndent(const QTextBlock& block, int columns) {
    if (!block.isValid() || wrapIndent(block) == columns) {
        return;
    }
    const int state = block.userState() < 0 ? 0 : block.userState();
    const int prevBits = (state & kWrapIndentMask) >> kWrapIndentShift;
    const int stateIndent = std::min(columns, kWrapIndentMax);
    const int updated = (state & ~kWrapIndentMask) | ((stateIndent & kWrapIndentMax) << kWrapIndentShift);
    const_cast<QTextBlock&>(block).setUserState(updated);

    if (columns >= kWrapIndentMax || prevBits >= kWrapIndentMax) {
        auto* data = dynamic_cast<BlockData*>(block.userData());
        if (columns >= kWrapIndentMax) {
            if (data == nullptr) {
                data = new BlockData;
                const_cast<QTextBlock&>(block).setUserData(data);
            }
            data->wrapIndent = columns;
        } else if (data != nullptr && data->wrapIndent > 0) {
            data->wrapIndent = 0;
            if (data->empty()) {
                const_cast<QTextBlock&>(block).setUserData(nullptr);
            }
        }
    }

    if (block.layout()->lineCount() <= 0 || reservedHeight(block) > 0) {
        return;
    }
    if (batch_ > 0) {
        dirtyBlocks_.push_back(block.blockNumber());
        batchNeedsRepaint_ = true;
        return;
    }
    layoutParagraphChain(block, true, ChainScope::LaidOutOnly);
    emit update();
}

int BlockLayout::wrapIndent(const QTextBlock& block) const {
    if (!block.isValid()) {
        return 0;
    }
    const int state = block.userState();
    const int bits = state < 0 ? 0 : (state & kWrapIndentMask) >> kWrapIndentShift;
    if (bits < kWrapIndentMax) {
        return bits;
    }
    const BlockData* data = dataOf(block);
    return data != nullptr && data->wrapIndent > 0 ? data->wrapIndent : bits;
}

qreal BlockLayout::bulletIndent() const {
    return std::round(averageCharWidth_ * 2.0);
}

void BlockLayout::setParagraphFlow(const QTextBlock& block, bool flowsToNext, bool hardBreak) {
    if (!block.isValid()) {
        return;
    }
    const int state = block.userState() < 0 ? 0 : block.userState();
    const bool oldFlows = (state & kFlowsToNextBit) != 0;
    const bool oldHard = (state & kHardBreakBit) != 0;
    int updated = state & ~(kFlowsToNextBit | kHardBreakBit);
    if (flowsToNext) {
        updated |= kFlowsToNextBit;
    }
    if (hardBreak) {
        updated |= kHardBreakBit;
    }
    if (updated != state) {
        const_cast<QTextBlock&>(block).setUserState(updated);
    }
    const bool flowChanged = (oldFlows != flowsToNext) || (oldHard != hardBreak);
    const bool inChain =
        flowsToNext || hardBreak || sharesLineWithPrevious(block) || continuesFlowFrom(block.previous(), block);
    if (!flowChanged && !(block.layout()->lineCount() > 0 && inChain)) {
        return;
    }
    const bool anyLaidOut =
        block.layout()->lineCount() > 0 || sharesLineWithPrevious(block) ||
        (block.previous().isValid() && block.previous().layout()->lineCount() > 0) ||
        (block.next().isValid() && (block.next().layout()->lineCount() > 0 || sharesLineWithPrevious(block.next())));
    if (!anyLaidOut) {
        return;
    }
    if (block.layout()->lineCount() > 0) {
        dirtyBlocks_.push_back(block.blockNumber());
        if (flowChanged && block.next().isValid()) {
            dirtyBlocks_.push_back(block.next().blockNumber());
        }
    } else {
        // Nothing of the block is drawn, so it shares no line, and it is laid out when it is needed.
        // Laying it out here would grow the laid-out part of a long paragraph by it on every change
        // of flow. Only laid-out neighbours change: the one before by its advance, the one after
        // when it shared a line with this block.
        if (sharesLineWithPrevious(block)) {
            setSharesPrevLine(block, false);
            if (block.previous().isValid() && block.previous().layout()->lineCount() > 0) {
                dirtyBlocks_.push_back(block.previous().blockNumber());
            }
        }
        const QTextBlock next = block.next();
        if (next.isValid() && next.layout()->lineCount() > 0 && sharesLineWithPrevious(next)) {
            dirtyBlocks_.push_back(next.blockNumber());
        }
    }
    if (batch_ == 0) {
        flushDirtyChains(true);
    }
}

bool BlockLayout::flowsToNext(const QTextBlock& block) const {
    return block.isValid() && block.userState() >= 0 && (block.userState() & kFlowsToNextBit) != 0;
}

bool BlockLayout::hardBreak(const QTextBlock& block) const {
    return block.isValid() && block.userState() >= 0 && (block.userState() & kHardBreakBit) != 0;
}

bool BlockLayout::sharesLineWithPrevious(const QTextBlock& block) const {
    return block.isValid() && block.userState() >= 0 && (block.userState() & kSharesPrevLineBit) != 0;
}

bool BlockLayout::continuesFlowFrom(const QTextBlock& prev, const QTextBlock& cur) const {
    return prev.isValid() && cur.isValid() && prev.isVisible() && cur.isVisible() && reservedHeight(prev) <= 0 &&
           reservedHeight(cur) <= 0 && (flowsToNext(prev) || hardBreak(prev));
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
    layout_->flushDirtyChains(false);
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
    spaceWidth_ = metrics.horizontalAdvance(QLatin1Char(' '));
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
    qreal subLeft = 0;
    qreal subWidth = 0;
    return layoutSingleBlock(block, subLeft, subWidth);
}

qreal BlockLayout::layoutSingleBlock(const QTextBlock& block, qreal& subLeft, qreal& subWidth) const {
    ++blockLayouts_;
    setSharesPrevLine(block, false);
    QTextLayout* layout = block.layout();
    layout->setTextOption(document()->defaultTextOption());
    layout->beginLayout();
    const qreal width = availableWidth();
    const qreal left = margin();
    qreal height = spaceAbove(block);

    const int wrapCols = wrapIndent(block);
    const qreal baseIndent = (wrapCols > 0) ? bulletIndent() : 0.0;
    const qreal line0Width = (baseIndent > 0 && width > baseIndent + 20) ? width - baseIndent : width;
    const qreal line0Left = (line0Width < width) ? left + baseIndent : left;

    QTextLine line0 = layout->createLine();
    if (!line0.isValid()) {
        layout->endLayout();
        layout->setPosition(QPointF(0, 0));
        subLeft = left;
        subWidth = width;
        return height + spaceBelow(block);
    }
    line0.setLeadingIncluded(true);
    line0.setLineWidth(line0Width);
    line0.setPosition(QPointF(line0Left, height));
    height += line0.height();

    qreal indent = 0;
    if (wrapCols > 0 && wrapCols <= line0.textLength()) {
        indent = std::max<qreal>(0, line0.cursorToX(wrapCols) - left);
    }
    subWidth = (indent > 0 && width > indent + 20) ? width - indent : width;
    subLeft = (subWidth < width) ? left + indent : left;

    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLeadingIncluded(true);
        line.setLineWidth(subWidth);
        line.setPosition(QPointF(subLeft, height));
        height += line.height();
    }
    layout->endLayout();
    layout->setPosition(QPointF(0, 0));
    return height + spaceBelow(block);
}

void BlockLayout::layoutContinuationBlock(const QTextBlock& prev, const QTextBlock& cur, qreal subLeft,
                                          qreal subWidth) const {
    ++blockLayouts_;
    QTextLayout* layout = cur.layout();
    const QTextLayout* prevLayout = prev.layout();
    bool placedOnPrev = false;
    if (flowsToNext(prev) && prevLayout->lineCount() > 0) {
        const QTextLine prevLast = prevLayout->lineAt(prevLayout->lineCount() - 1);
        const qreal gap = lineSpacing_ > 0 ? spaceWidth_ * (prevLast.height() / lineSpacing_) : spaceWidth_;
        const qreal nextX = prevLast.x() + prevLast.naturalTextWidth() + gap;
        const qreal remWidth = (subLeft + subWidth) - nextX;
        const QString text = cur.text();
        int firstNonSpace = 0;
        while (firstNonSpace < text.size() &&
               (text[firstNonSpace] == QLatin1Char(' ') || text[firstNonSpace] == QLatin1Char('\t'))) {
            ++firstNonSpace;
        }
        if (remWidth > 1.0 && firstNonSpace < text.size()) {
            QTextOption wordWrap = document()->defaultTextOption();
            wordWrap.setWrapMode(QTextOption::WordWrap);
            layout->setTextOption(wordWrap);
            layout->beginLayout();
            QTextLine line0 = layout->createLine();
            if (line0.isValid()) {
                line0.setLeadingIncluded(true);
                line0.setLineWidth(remWidth);
                if (line0.textStart() + line0.textLength() > firstNonSpace &&
                    line0.naturalTextWidth() <= remWidth + 0.5) {
                    const qreal y0 = spaceAbove(cur);
                    line0.setPosition(QPointF(nextX, y0));
                    qreal y = y0 + std::max(line0.height(), prevLast.height());
                    layout->setTextOption(document()->defaultTextOption());
                    while (true) {
                        QTextLine line = layout->createLine();
                        if (!line.isValid()) {
                            break;
                        }
                        line.setLeadingIncluded(true);
                        line.setLineWidth(subWidth);
                        line.setPosition(QPointF(subLeft, y));
                        y += line.height();
                    }
                    layout->endLayout();
                    layout->setPosition(QPointF(0, 0));
                    setSharesPrevLine(cur, true);
                    placedOnPrev = true;
                } else {
                    layout->endLayout();
                }
            } else {
                layout->endLayout();
            }
        }
    }
    if (!placedOnPrev) {
        setSharesPrevLine(cur, false);
        layout->setTextOption(document()->defaultTextOption());
        layout->beginLayout();
        qreal y = spaceAbove(cur);
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLeadingIncluded(true);
            line.setLineWidth(subWidth);
            line.setPosition(QPointF(subLeft, y));
            y += line.height();
        }
        layout->endLayout();
        layout->setPosition(QPointF(0, 0));
    }
}

int BlockLayout::layoutParagraphChain(const QTextBlock& target, bool emitNow, ChainScope scope) const {
    if (!target.isValid()) {
        return -1;
    }
    if (!target.isVisible() || reservedHeight(target) > 0) {
        setSharesPrevLine(target, false);
        if (!inSync()) {
            deferred_.push_back(target);
        } else {
            setHeight(target.blockNumber(), advanceOf(target), emitNow);
        }
        return target.blockNumber();
    }

    const bool laidOutOnly = scope == ChainScope::LaidOutOnly;
    if (laidOutOnly && target.layout()->lineCount() == 0 && continuesFlowFrom(target.previous(), target) &&
        target.previous().layout()->lineCount() == 0) {
        // The change dropped the block's lines, and those of the block it continues from too: it
        // cannot be placed on that block's last line now, and laying the paragraph out from further
        // back would grow its laid-out part. It is laid out when it is needed; until then it shares
        // no line. Only laid-out neighbours change: the one before by its advance, the one after by
        // no longer sharing a line with it.
        if (sharesLineWithPrevious(target)) {
            setSharesPrevLine(target, false);
            const QTextBlock prev = target.previous();
            if (prev.isValid() && prev.layout()->lineCount() > 0) {
                if (!inSync()) {
                    deferred_.push_back(prev);
                } else {
                    setHeight(prev.blockNumber(), advanceOf(prev), emitNow);
                }
            }
        }
        const QTextBlock next = target.next();
        if (next.isValid() && next.layout()->lineCount() > 0 && sharesLineWithPrevious(next)) {
            return layoutParagraphChain(next, emitNow, scope);
        }
        return target.blockNumber();
    }

    constexpr int kMaxLookback = 64;
    QTextBlock start = target;
    int stepsBack = 0;
    // Extend lays out the blocks before `target` that have no lines yet and goes on from the last one
    // before them that has lines. LaidOutOnly starts at `target`: the lines before it stay as they
    // are (a dirty block before it is laid out first, flushDirtyChains goes in block order).
    while (!laidOutOnly && stepsBack < kMaxLookback && continuesFlowFrom(start.previous(), start) &&
           !hardBreak(start.previous()) && start.previous().layout()->lineCount() == 0) {
        start = start.previous();
        ++stepsBack;
    }

    const qreal width = availableWidth();
    const qreal left = margin();
    qreal subLeft = left;
    qreal subWidth = width;
    const int wrapCols = wrapIndent(start);
    if (wrapCols > 0) {
        QTextBlock head = start;
        int headSteps = 0;
        while (headSteps < kMaxLookback && continuesFlowFrom(head.previous(), head)) {
            head = head.previous();
            ++headSteps;
        }
        if (head != start) {
            if (head.layout()->lineCount() == 0) {
                layoutSingleBlock(head, subLeft, subWidth);
                if (!inSync()) {
                    deferred_.push_back(head);
                } else {
                    setHeight(head.blockNumber(), advanceOf(head), emitNow);
                }
            } else {
                const QTextLine l0 = head.layout()->lineAt(0);
                const qreal indent =
                    wrapCols <= l0.textLength() ? std::max<qreal>(0, l0.cursorToX(wrapCols) - left) : 0;
                subWidth = (indent > 0 && width > indent + 20) ? width - indent : width;
                subLeft = (subWidth < width) ? left + indent : left;
            }
        }
    }

    std::vector<QTextBlock> laidOut;
    if (start.previous().isValid() && start.previous().layout()->lineCount() > 0) {
        laidOut.push_back(start.previous());
    }
    if (!continuesFlowFrom(start.previous(), start)) {
        layoutSingleBlock(start, subLeft, subWidth);
    } else {
        layoutContinuationBlock(start.previous(), start, subLeft, subWidth);
    }
    laidOut.push_back(start);

    const int targetNum = target.blockNumber();
    QTextBlock prev = start;
    while (true) {
        QTextBlock cur = prev.next();
        if (!cur.isValid()) {
            break;
        }
        const bool continues = continuesFlowFrom(prev, cur);
        const bool wasSharing = sharesLineWithPrevious(cur);
        if (!continues) {
            if (wasSharing) {
                setSharesPrevLine(cur, false);
                if (cur.layout()->lineCount() > 0) {
                    qreal sl = 0;
                    qreal sw = 0;
                    layoutSingleBlock(cur, sl, sw);
                    laidOut.push_back(cur);
                }
            }
            break;
        }
        const bool curHadLayout = cur.layout()->lineCount() > 0;
        if (laidOutOnly && !curHadLayout && cur.blockNumber() > targetNum) {
            // Not drawn before, so not now either: it is laid out when it is needed.
            if (wasSharing) {
                setSharesPrevLine(cur, false);
            }
            break;
        }
        const std::vector<LineShape> before = curHadLayout ? lineShapes(cur) : std::vector<LineShape>{};
        layoutContinuationBlock(prev, cur, subLeft, subWidth);
        laidOut.push_back(cur);
        prev = cur;
        if (curHadLayout && cur.blockNumber() > targetNum && sharesLineWithPrevious(cur) == wasSharing &&
            lineShapes(cur) == before) {
            break;  // the same lines as before: the blocks after it keep theirs
        }
        if (cur.blockNumber() > targetNum && !curHadLayout &&
            (!sharesLineWithPrevious(cur) || cur.layout()->lineCount() > 1)) {
            break;
        }
    }

    for (const QTextBlock& b : laidOut) {
        if (!inSync()) {
            deferred_.push_back(b);
        } else {
            setHeight(b.blockNumber(), advanceOf(b), emitNow);
        }
    }
    return prev.blockNumber();
}

void BlockLayout::flushDirtyChains(bool emitNow) const {
    if (dirtyBlocks_.empty() || flushingDirty_) {
        return;
    }
    flushingDirty_ = true;
    std::vector<int> blocks;
    blocks.swap(dirtyBlocks_);
    std::sort(blocks.begin(), blocks.end());
    blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
    int lastLaidOut = -1;
    for (const int num : blocks) {
        const QTextBlock b = document()->findBlockByNumber(num);
        if (!b.isValid() || !b.isVisible() || reservedHeight(b) > 0) {
            continue;
        }
        if (num <= lastLaidOut && continuesFlowFrom(b.previous(), b)) {
            continue;
        }
        lastLaidOut = std::max(lastLaidOut, layoutParagraphChain(b, emitNow, ChainScope::LaidOutOnly));
    }
    flushingDirty_ = false;
    if (batch_ > 0) {
        const_cast<BlockLayout*>(this)->batchNeedsRepaint_ = true;
    } else if (emitNow) {
        emit const_cast<BlockLayout*>(this)->update();
    }
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

qreal BlockLayout::advanceOf(const QTextBlock& block) const {
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
    const QTextBlock next = block.next();
    if (next.isValid() && sharesLineWithPrevious(next)) {
        const QTextLine lastLine = layout->lineAt(layout->lineCount() - 1);
        return lastLine.y();
    }
    return heightOf(block);
}

bool BlockLayout::inSync() const {
    // Qt emits QTextDocument::contentsChange before it calls documentChanged; slots that query the
    // layout in between see the new blocks while heights_ still has the old numbering.
    return heights_.size() == static_cast<std::size_t>(document()->blockCount());
}

void BlockLayout::ensureBlockLayout(const QTextBlock& block) const {
    if (!block.isValid()) {
        return;
    }
    if (!dirtyBlocks_.empty()) {
        flushDirtyChains(batch_ == 0);
    }
    if (block.layout()->lineCount() > 0) {
        return;
    }
    layoutParagraphChain(block, false);
}

void BlockLayout::blockLayoutChanged(const QTextBlock& block) {
    if (!block.isValid()) {
        return;
    }
    if (!inSync()) {
        deferred_.push_back(block);
        return;
    }
    setHeight(block.blockNumber(), advanceOf(block), true);
    emit update();
}

void BlockLayout::storeDeferredHeights() {
    std::vector<QTextBlock> blocks;
    blocks.swap(deferred_);
    for (const QTextBlock& block : blocks) {
        if (block.isValid()) {
            setHeight(block.blockNumber(), advanceOf(block), batch_ == 0);
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

QTextBlock BlockLayout::blockOfTextAt(qreal y) const {
    const std::size_t index = heights_.indexAt(y - margin());
    if (index >= heights_.size()) {
        return {};
    }
    return document()->findBlockByNumber(static_cast<int>(index));
}

QTextBlock BlockLayout::firstBlockOfLineAt(qreal y) const {
    QTextBlock b = blockOfTextAt(y);
    while (b.isValid() && sharesLineWithPrevious(b) && b.previous().isValid()) {
        b = b.previous();
    }
    return b;
}

QRectF BlockLayout::blockBoundingRect(const QTextBlock& block) const {
    if (!block.isValid()) {
        return {};
    }
    if (block.isVisible()) {
        ensureBlockLayout(block);  // a hidden block has height 0; its text is laid out when it shows
    }
    const qreal top = margin() + heights_.sumBefore(static_cast<std::size_t>(block.blockNumber()));
    const qreal width = document()->textWidth() > 0 ? document()->textWidth() : availableWidth() + 2 * margin();
    const qreal height = heightOf(block);
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
    QTextBlock first = firstBlockOfLineAt(point.y());
    if (!first.isValid()) {
        if (accuracy == Qt::ExactHit) {
            return -1;
        }
        QTextBlock block = document()->lastBlock();
        while (block.isValid() && !block.isVisible()) {
            block = block.previous();
        }
        return block.isValid() ? block.position() + block.length() - 1 : 0;
    }
    if (reservedHeight(first) > 0) {
        return first.position();
    }
    struct Candidate {
        QTextBlock block;
        QTextLine line;
        QPointF local;
    };
    std::vector<Candidate> row;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        if (b != first && !sharesLineWithPrevious(b)) {
            break;
        }
        if (!b.isVisible() || reservedHeight(b) > 0) {
            continue;
        }
        const QRectF rect = blockBoundingRect(b);
        const QPointF local = point - rect.topLeft();
        const QTextLayout* layout = b.layout();
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            if (local.y() >= line.y() && local.y() < line.y() + line.height()) {
                row.push_back({b, line, local});
            }
        }
    }
    if (!row.empty()) {
        if (accuracy == Qt::ExactHit) {
            for (const auto& c : row) {
                if (c.local.x() >= c.line.x() && c.local.x() <= c.line.x() + c.line.naturalTextWidth()) {
                    return c.block.position() + c.line.xToCursor(c.local.x());
                }
            }
            return -1;
        }
        std::size_t chosen = 0;
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (row[i].local.x() >= row[i].line.x()) {
                chosen = i;
            }
        }
        const auto& c = row[chosen];
        return c.block.position() + c.line.xToCursor(c.local.x());
    }
    const QRectF rect = blockBoundingRect(first);
    const QTextLayout* layout = first.layout();
    const QPointF local = point - rect.topLeft();
    if (layout->lineCount() == 0) {
        return first.position();
    }
    const int best = local.y() < layout->lineAt(0).y() ? 0 : layout->lineCount() - 1;
    const QTextLine line = layout->lineAt(best);
    if (accuracy == Qt::ExactHit && (local.x() < line.x() || local.x() > line.x() + line.naturalTextWidth())) {
        return -1;
    }
    return first.position() + line.xToCursor(local.x());
}

// --- painting ---------------------------------------------------------------------------------

void BlockLayout::draw(QPainter* painter, const PaintContext& context) {
    const QRectF clip = context.clip.isValid() ? context.clip : QRectF(QPointF(0, 0), documentSize());
    for (QTextBlock block = firstBlockOfLineAt(clip.top()); block.isValid(); block = block.next()) {
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
        dirtyBlocks_.clear();
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            setSharesPrevLine(b, false);
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
        const qreal before = heights_.height(static_cast<std::size_t>(firstNumber));
        first.layout()->clearLayout();
        if (first.isVisible() && reservedHeight(first) <= 0) {
            layoutParagraphChain(first, true);
        }
        const qreal after = heights_.height(static_cast<std::size_t>(firstNumber));
        if (std::abs(after - before) < 1e-6) {
            emit updateBlock(first);
            return;
        }
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
        if (b.layout()->lineCount() > 0 &&
            (flowsToNext(b) || hardBreak(b) || sharesLineWithPrevious(b) || continuesFlowFrom(b.previous(), b))) {
            dirtyBlocks_.push_back(b.blockNumber());
        }
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
