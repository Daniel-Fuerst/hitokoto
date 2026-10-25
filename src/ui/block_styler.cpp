// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_styler.hpp"

#include "block_data.hpp"
#include "qt_bridge.hpp"

#include <QElapsedTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <vector>

namespace hitokoto::ui {
namespace {
// Block user state encodes what a block was styled for: -1 = never, 0 = concealed, 1 = revealed.
constexpr int kUnstyled = -1;
constexpr int kStyledConcealed = 0;
constexpr int kStyledRevealed = 1;
// Ranges up to this size are styled synchronously; larger ones on screen + in idle slices.
constexpr int kSynchronousLimit = 400;
constexpr qint64 kSliceNanoseconds = 6'000'000;
}  // namespace

BlockStyler::BlockStyler(QTextDocument* document, const core::ElementRegistry& registry,
                         std::function<std::pair<int, int>()> visibleBlocks, QObject* parent)
    : QObject(parent), document_(document), registry_(registry), visibleBlocks_(std::move(visibleBlocks)) {
    idle_.setInterval(0);
    connect(&idle_, &QTimer::timeout, this, &BlockStyler::idleSlice);
    connect(document_, &QTextDocument::contentsChange, this, &BlockStyler::onContentsChange);
}

void BlockStyler::setFormats(EditorFormats formats) {
    formats_ = std::move(formats);
    invalidateAll();
}

void BlockStyler::invalidateAll() {
    for (QTextBlock b = document_->begin(); b.isValid(); b = b.next()) {
        b.setUserState(kUnstyled);
    }
    styleVisible();
    scheduleFrom(0);
}

core::BlockState BlockStyler::blockStateBefore(const QTextBlock& block) {
    std::vector<QTextBlock> uncached;
    QTextBlock previous = block.previous();
    while (previous.isValid()) {
        auto* data = dynamic_cast<BlockData*>(previous.userData());
        if (data != nullptr && data->blockStateValid) {
            auto state = data->blockState;
            for (auto it = uncached.rbegin(); it != uncached.rend(); ++it) {
                state = registry_.blockStateAfter(view(it->text()), state);
                auto* current = dynamic_cast<BlockData*>(it->userData());
                if (current == nullptr) {
                    current = new BlockData;
                    it->setUserData(current);
                }
                current->blockState = state;
                current->blockStateValid = true;
            }
            return state;
        }
        uncached.push_back(previous);
        previous = previous.previous();
    }

    core::BlockState state;
    for (auto it = uncached.rbegin(); it != uncached.rend(); ++it) {
        state = registry_.blockStateAfter(view(it->text()), state);
        auto* current = dynamic_cast<BlockData*>(it->userData());
        if (current == nullptr) {
            current = new BlockData;
            it->setUserData(current);
        }
        current->blockState = state;
        current->blockStateValid = true;
    }
    return state;
}

core::LineRender BlockStyler::renderFor(const QTextBlock& block) {
    return registry_.render(view(block.text()), {isRevealed(block.blockNumber()), blockStateBefore(block)});
}

bool BlockStyler::isStyled(const QTextBlock& block) const {
    const int expected = isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed;
    return block.userState() == expected;
}

void BlockStyler::styleBlock(const QTextBlock& block) {
    QTextLayout* layout = block.layout();
    if (!layout) {
        return;
    }
    const auto render = renderFor(block);
    auto* data = dynamic_cast<BlockData*>(block.userData());
    if (data == nullptr) {
        data = new BlockData;
        QTextBlock mutableBlock = block;
        mutableBlock.setUserData(data);
    }
    const bool blockStateChanged = data->blockStateValid && data->blockState != render.blockState;
    data->blockState = render.blockState;
    data->blockStateValid = true;
    const auto ranges = formats_.ranges(render);
    // Skip the relayout if nothing was and nothing is formatted (most body lines).
    const bool changed = !(ranges.isEmpty() && layout->formats().isEmpty());
    QTextBlock mutableBlock = block;
    mutableBlock.setUserState(isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed);
    ++styledCount_;
    if (blockStateChanged) {
        // The next line's block membership may change too. Restyle onward lazily; each restyled
        // line continues the chain only while its own block state changes. This must not depend on
        // the edit changing the text length (e.g. one backtick replaced by a letter).
        QTextBlock next = block.next();
        if (next.isValid()) {
            next.setUserState(kUnstyled);
            scheduleFrom(next.blockNumber());
        }
    }
    if (!changed) {
        return;
    }
    busy_ = true;
    layout->setFormats(ranges);
    document_->markContentsDirty(block.position(), block.length());
    busy_ = false;
}

void BlockStyler::styleRange(int first, int last) {
    first = std::max(first, 0);
    last = std::min(last, document_->blockCount() - 1);
    if (first > last) {
        return;
    }
    if (last - first + 1 <= kSynchronousLimit) {
        for (QTextBlock b = document_->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
            styleBlock(b);
        }
        return;
    }
    styleVisible();
    scheduleFrom(first);
}

void BlockStyler::styleVisible() {
    // Styling can change line heights and therefore what is visible; settle in a few rounds.
    for (int round = 0; round < 3; ++round) {
        const auto [first, last] = visibleBlocks_();
        bool any = false;
        for (QTextBlock b = document_->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
            if (!isStyled(b)) {
                styleBlock(b);
                any = true;
            }
        }
        if (!any) {
            break;
        }
    }
}

void BlockStyler::setRevealed(core::LineRange range) {
    if (range == revealed_) {
        return;
    }
    const auto changes = core::revealChanges(revealed_, range);
    revealed_ = range;
    for (const auto& c : changes) {
        styleRange(static_cast<int>(c.first), static_cast<int>(c.last));
    }
}

void BlockStyler::onContentsChange(int position, int charsRemoved, int charsAdded) {
    if (busy_) {
        return;
    }
    const QTextBlock first = document_->findBlock(position);
    QTextBlock last = document_->findBlock(position + charsAdded);
    if (!last.isValid()) {
        last = document_->lastBlock();
    }
    const int firstNumber = first.isValid() ? first.blockNumber() : 0;
    const int lastNumber = last.blockNumber();
    // Edited blocks keep their user state but their formats are stale: force a restyle.
    for (QTextBlock b = first; b.isValid() && b.blockNumber() <= std::min(lastNumber, firstNumber + kSynchronousLimit);
         b = b.next()) {
        b.setUserState(kUnstyled);
    }
    styleRange(firstNumber, std::min(lastNumber + 1, document_->blockCount() - 1));
    if (charsRemoved != charsAdded || lastNumber - firstNumber > kSynchronousLimit) {
        // Line numbers below may have shifted relative to the revealed range: re-check lazily.
        scheduleFrom(firstNumber);
    }
}

void BlockStyler::scheduleFrom(int blockNumber) {
    idleNext_ = idle_.isActive() ? std::min(idleNext_, blockNumber) : blockNumber;
    if (!idle_.isActive()) {
        idle_.start();
    }
}

void BlockStyler::idleSlice() {
    QElapsedTimer timer;
    timer.start();
    QTextBlock b = document_->findBlockByNumber(idleNext_);
    int checked = 0;
    while (b.isValid()) {
        if (!isStyled(b)) {
            styleBlock(b);
        }
        b = b.next();
        if (++checked % 64 == 0 && timer.nsecsElapsed() > kSliceNanoseconds) {
            break;
        }
    }
    if (b.isValid()) {
        idleNext_ = b.blockNumber();
        return;
    }
    idle_.stop();
    emit completed();
}

}  // namespace hitokoto::ui
