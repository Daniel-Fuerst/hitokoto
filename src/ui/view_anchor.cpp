// SPDX-License-Identifier: GPL-3.0-or-later
#include "view_anchor.hpp"

#include "block_layout.hpp"
#include "smooth_scroller.hpp"

#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

ViewAnchor::ViewAnchor(QTextEdit* editor, BlockLayout* layout, SmoothScroller* scroller, const bool& insideEdit)
    : QObject(editor), editor_(editor), layout_(layout), scroller_(scroller), insideEdit_(insideEdit) {
    connect(layout_, &BlockLayout::blockHeightChanged, this, &ViewAnchor::keepContentStill);
    connect(layout_, &BlockLayout::batchFinished, this, &ViewAnchor::applyPendingShift);
    // A scroll of the reader's lets go of the reading position held across resizes (S033); the
    // scrolls of a resize or theme change and those that keep the content still do not. The view
    // follows the caret while the caret is on screen after a scroll; one that leaves it off screen
    // lets it go.
    connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        if (resizing_ || applyingShift_) {
            return;
        }
        if (rangeClamps_) {
            rangeClamps_ = false;  // the end of the document cut the value short, no scroll
            return;
        }
        heldAnchor_ = {};
        if (!followingCaret_ && !typing_ && followCaret_ && !caretOnScreen()) {
            followCaret_ = false;
        }
    });
    // A new scroll range that no longer holds the value clamps it right after this signal. While a
    // reading position is held or the view follows the caret, that is no scroll of the reader's; the
    // reading position is shown again once the range allows it.
    connect(editor_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int min, int max) {
        if (heldAnchor_.text.isNull() && !followCaret_) {
            return;
        }
        const int value = editor_->verticalScrollBar()->value();
        rangeClamps_ = value > max || value < min;
        if (!resizing_ && !heldAnchor_.text.isNull()) {
            QMetaObject::invokeMethod(this, &ViewAnchor::settleAnchor, Qt::QueuedConnection);
        }
        scheduleCaretCheck();
    });
    // S033: a caret that moved, or text typed at it, is followed while blocks are laid out and
    // restyled in idle slices; their heights change the place of the caret on screen.
    connect(editor_, &QTextEdit::cursorPositionChanged, this, [this] {
        // The caret is followed after typing and after a move of its own (keys, a click, the
        // program placing it); not when an edit through another cursor carried it along off screen
        // (opening a file, a program's insert).
        followCaret_ = typing_ || !insideEdit_ || caretOnScreen();
        scheduleCaretCheck();
    });
    connect(layout_, &BlockLayout::batchFinished, this, &ViewAnchor::scheduleCaretCheck);
    connect(layout_, &BlockLayout::documentSizeChanged, this, &ViewAnchor::scheduleCaretCheck);
}

void ViewAnchor::keepContentStill(qreal top, qreal oldHeight, qreal newHeight) {
    // A block wholly above the viewport changed height (laid out lazily or restyled): scroll by
    // the same amount so the text on screen does not move. Blocks on screen are left alone; they
    // are what the reader sees. Inside a layout batch the scroll range is not updated yet; the
    // shift is applied when the batch ends.
    const QScrollBar* bar = editor_->verticalScrollBar();
    const qreal visualBottom = oldHeight > 0 ? top + oldHeight : top + newHeight;
    if (bar->value() <= 0 || visualBottom > bar->value() + 0.5) {
        scheduleCaretCheck();  // a block on screen or below moves the caret, if it is below
        return;
    }
    pendingShift_ += newHeight - oldHeight;
    if (!layout_->inBatch()) {
        applyPendingShift();
    }
}

void ViewAnchor::applyPendingShift() {
    if (applyingShift_) {
        return;  // scrolling restyles; a shift that causes reaches us again
    }
    const int shift = static_cast<int>(std::lround(pendingShift_));
    pendingShift_ -= shift;  // keep the fraction: lazily laid-out blocks change by parts of a pixel
    if (shift != 0) {
        applyingShift_ = true;
        QScrollBar* bar = editor_->verticalScrollBar();
        bar->setMaximum(std::max(bar->maximum(), bar->value() + shift));
        scroller_->shift(shift);  // moves the bar, and a running wheel animation with it
        applyingShift_ = false;
    }
}

bool ViewAnchor::caretOnScreen() const {
    const QRect caret = editor_->cursorRect();
    return caret.top() >= 0 && caret.bottom() <= editor_->viewport()->height();
}

void ViewAnchor::scheduleCaretCheck() {
    // Queued and once per turn of the event loop: heights change inside layout queries, often
    // while painting, where the view must not scroll.
    if (!followCaret_ || caretCheckQueued_) {
        return;
    }
    caretCheckQueued_ = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            caretCheckQueued_ = false;
            keepCaretOnScreen();
        },
        Qt::QueuedConnection);
}

void ViewAnchor::keepCaretOnScreen() {
    if (!followCaret_ || resizing_ || followingCaret_ || caretOnScreen()) {
        return;
    }
    const QScopedValueRollback<bool> guard(followingCaret_, true);
    editor_->ensureCursorVisible();
}

ViewAnchor::ScrollAnchor ViewAnchor::captureAnchor() const {
    const qreal y = editor_->verticalScrollBar()->value();
    if (y <= 0) {
        return {};
    }
    // The text of the top line itself, not a share of a block's height: a block that shares its first
    // line with the blocks before it has more or fewer lines at another width, and the same share
    // would land on another of its lines (S033).
    QTextBlock block = layout_->blockOfTextAt(y);
    if (!block.isValid()) {
        block = editor_->document()->lastBlock();
    }
    const QRectF rect = layout_->blockBoundingRect(block);  // lays the block out
    const QTextLayout* text = block.layout();
    if (layout_->reservedHeight(block) > 0 || text->lineCount() == 0) {
        QTextCursor cursor(block);
        return {cursor, y - rect.top()};
    }
    int index = 0;
    while (index + 1 < text->lineCount() && text->lineAt(index + 1).y() <= y - rect.top()) {
        ++index;
    }
    const qreal lineTop = rect.top() + text->lineAt(index).y();
    // A first line that goes on in the line before: the line starts in an earlier block.
    while (index == 0 && layout_->sharesLineWithPrevious(block) && block.previous().isValid() &&
           block.previous().layout()->lineCount() > 0) {
        block = block.previous();
        index = block.layout()->lineCount() - 1;
    }
    QTextCursor cursor(block);
    cursor.setPosition(block.position() + block.layout()->lineAt(index).textStart());
    return {cursor, y - lineTop};
}

ViewAnchor::ScrollAnchor ViewAnchor::readingAnchor() {
    if (heldAnchor_.text.isNull()) {
        heldAnchor_ = captureAnchor();
    }
    return heldAnchor_;
}

int ViewAnchor::anchorValue(const ScrollAnchor& anchor) const {
    const QTextBlock block = anchor.text.block();
    qreal top = layout_->blockBoundingRect(block).top();  // lays the block out
    if (const QTextLayout* text = block.layout(); layout_->reservedHeight(block) <= 0 && text->lineCount() > 0) {
        const QTextLine line = text->lineForTextPosition(anchor.text.positionInBlock());
        if (line.isValid()) {
            top += line.y();
        }
    }
    return static_cast<int>(std::lround(top + anchor.offset));
}

void ViewAnchor::restore(const ScrollAnchor& anchor) {
    if (!anchor.text.isNull()) {
        ++restores_;
    }
    settlePasses_ = 0;
    restoreAnchor(anchor);
}

void ViewAnchor::settleLater() {
    QMetaObject::invokeMethod(this, &ViewAnchor::settleAnchor, Qt::QueuedConnection);
}

void ViewAnchor::restoreAnchor(const ScrollAnchor& anchor) {
    // A new width or font re-estimates every block, so the same scroll value would show other
    // text; scroll back to the line that was at the top.
    if (anchor.text.isNull()) {
        return;
    }
    pendingShift_ = 0;
    // Laying blocks out changes heights that the scroll range takes in only once the event loop
    // runs, and scrolling lays out the blocks that come into view: near the end of the document
    // either can cut the value short while the estimates below are still too small. Set it again
    // once the blocks on screen have their lines; the range follows the document size later.
    // A range that does not scroll stays as it is: the document fits the view. A larger range
    // would show the scroll bar; the narrower text column gives the document its own range again,
    // which hides the scroll bar at once, and the restore of that resize would make the range
    // larger again, without end. Should the laid-out document not fit after all, its range comes
    // with the next size report, and settleAnchor shows the reading position then.
    QScrollBar* bar = editor_->verticalScrollBar();
    for (int pass = 0; pass < 2; ++pass) {
        const int value = anchorValue(anchor);
        if (bar->maximum() > bar->minimum()) {
            bar->setMaximum(std::max(bar->maximum(), value));
        }
        bar->setValue(value);
        if (bar->value() == value) {
            break;
        }
    }
}

void ViewAnchor::settleAnchor() {
    // Heights above the reading position can still change after it was restored, and the end of the
    // document can cut the value short until the blocks below are laid out (S033). While the reader
    // has not scrolled, show the reading position again, as far as the scroll range allows.
    if (heldAnchor_.text.isNull() || resizing_ || settlePasses_ >= kMaxSettlePasses) {
        return;
    }
    const QScopedValueRollback<bool> guard(resizing_, true);
    const int value = anchorValue(heldAnchor_);
    if (value != editor_->verticalScrollBar()->value()) {
        ++settlePasses_;
        pendingShift_ = 0;
        editor_->verticalScrollBar()->setValue(value);
    }
}

}  // namespace hitokoto::ui
