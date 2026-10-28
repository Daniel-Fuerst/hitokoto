// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_styler.hpp"

#include "block_data.hpp"
#include "block_layout.hpp"
#include "qt_bridge.hpp"

#include <QElapsedTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <optional>
#include <vector>

namespace hitokoto::ui {
namespace {
// QTextBlock::userState() carries two things per block; Qt gives new blocks -1 (nothing known).
// Bits 0-1: what the block was styled for (0 = not styled, 1 = concealed, 2 = revealed).
// Bit 2: the block state after the line is known. Bit 3: that state is active; only then does
// the block carry a BlockData holding it, so plain lines need no allocation.
constexpr int kStyleMask = 0x3;
constexpr int kUnstyled = 0;
constexpr int kStyledConcealed = 1;
constexpr int kStyledRevealed = 2;
constexpr int kStateKnown = 0x4;
constexpr int kStateActive = 0x8;
// Ranges up to this size are styled synchronously; larger ones on screen + in idle slices.
constexpr int kSynchronousLimit = 400;
constexpr qint64 kSliceNanoseconds = 6'000'000;

int stateBits(const QTextBlock& block) {
    const int bits = block.userState();
    return bits < 0 ? 0 : bits;
}

void setStyled(QTextBlock& block, int style) {
    block.setUserState((stateBits(block) & ~kStyleMask) | style);
}

/// The cached block state after `block`, if known. Joining lines can pair the user state of one
/// line with the BlockData of another; such a mix counts as unknown.
std::optional<core::BlockState> cachedBlockState(const QTextBlock& block) {
    const int bits = stateBits(block);
    if ((bits & kStateKnown) == 0) {
        return std::nullopt;
    }
    if ((bits & kStateActive) == 0) {
        return core::BlockState{};
    }
    const auto* data = dynamic_cast<const BlockData*>(block.userData());
    if (data != nullptr && data->blockState.active()) {
        return data->blockState;
    }
    return std::nullopt;
}

void cacheBlockState(QTextBlock& block, core::BlockState state) {
    auto* data = dynamic_cast<BlockData*>(block.userData());
    const int style = stateBits(block) & kStyleMask;
    if (state.active()) {
        if (data == nullptr) {
            data = new BlockData;
            block.setUserData(data);
        }
        data->blockState = state;
        block.setUserState(style | kStateKnown | kStateActive);
        return;
    }
    if (data != nullptr) {
        data->blockState = {};
        if (data->empty()) {
            block.setUserData(nullptr);  // deletes it
        }
    }
    block.setUserState(style | kStateKnown);
}
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
        setStyled(b, kUnstyled);
    }
    styleVisible();
    scheduleFrom(0);
}

core::BlockState BlockStyler::blockStateBefore(const QTextBlock& block) {
    std::vector<QTextBlock> uncached;
    core::BlockState state;
    for (QTextBlock previous = block.previous(); previous.isValid(); previous = previous.previous()) {
        if (const auto cached = cachedBlockState(previous)) {
            state = *cached;
            break;
        }
        uncached.push_back(previous);
    }
    for (auto it = uncached.rbegin(); it != uncached.rend(); ++it) {
        const QString text = it->text();
        const QString next = nextLineFor(*it, state, text);
        state = registry_.blockStateAfter(view(text), state, next.isEmpty() ? std::u16string_view{} : view(next));
        cacheBlockState(*it, state);
    }
    return state;
}

QString BlockStyler::nextLineFor(const QTextBlock& block, core::BlockState state, const QString& text) {
    // A table header is recognised by the delimiter row below it (S018): only a line that could
    // be one, outside an open block, needs the next line.
    if (state.active() || !text.contains(QLatin1Char('|'))) {
        return {};
    }
    const QTextBlock next = block.next();
    return next.isValid() ? next.text() : QString();
}

core::LineRender BlockStyler::renderFor(const QTextBlock& block) {
    const QString text = block.text();
    const core::BlockState state = blockStateBefore(block);
    const QString next = nextLineFor(block, state, text);
    return registry_.render(
        view(text), {isRevealed(block.blockNumber()), state, next.isEmpty() ? std::u16string_view{} : view(next)});
}

bool BlockStyler::isStyled(const QTextBlock& block) const {
    const int expected = isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed;
    return (stateBits(block) & kStyleMask) == expected;
}

void BlockStyler::styleBlock(const QTextBlock& block) {
    QTextLayout* layout = block.layout();
    if (!layout) {
        return;
    }
    const auto render = renderFor(block);
    QTextBlock mutableBlock = block;
    const auto previousState = cachedBlockState(block);
    const bool blockStateChanged = previousState && *previousState != render.blockState;
    cacheBlockState(mutableBlock, render.blockState);
    const auto ranges = formats_.ranges(render);
    // Skip the relayout if nothing was and nothing is formatted (most body lines).
    const bool changed = !(ranges.isEmpty() && layout->formats().isEmpty());
    setStyled(mutableBlock, isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed);
    ++styledCount_;
    if (blockStateChanged) {
        // The next line's block membership may change too. Restyle onward lazily; each restyled
        // line continues the chain only while its own block state changes. This must not depend on
        // the edit changing the text length (e.g. one backtick replaced by a letter).
        QTextBlock next = block.next();
        if (next.isValid()) {
            setStyled(next, kUnstyled);
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
        const BlockLayout::Batch batch(qobject_cast<BlockLayout*>(document_->documentLayout()));
        for (QTextBlock b = document_->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
            styleBlock(b);
        }
        return;
    }
    styleVisible();
    scheduleFrom(first);
}

void BlockStyler::styleVisible() {
    const BlockLayout::Batch batch(qobject_cast<BlockLayout*>(document_->documentLayout()));
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

core::LineRange BlockStyler::expandCodeBlocks(core::LineRange range) {
    if (range.empty() || document_->isEmpty()) {
        return range;
    }
    int first = static_cast<int>(range.first);
    int last = static_cast<int>(range.last);

    QTextBlock bFirst = document_->findBlockByNumber(first);
    if (bFirst.isValid()) {
        const auto renderFirst = renderFor(bFirst);
        if (renderFirst.codeBlock) {
            if (!(renderFirst.fenceLine && !blockStateBefore(bFirst).active())) {
                for (QTextBlock prev = bFirst.previous(); prev.isValid(); prev = prev.previous()) {
                    if (!blockStateBefore(prev).active()) {
                        first = prev.blockNumber();
                        break;
                    }
                }
            }
        }
    }

    QTextBlock bLast = document_->findBlockByNumber(last);
    if (bLast.isValid()) {
        const auto renderLast = renderFor(bLast);
        if (renderLast.codeBlock) {
            if (!(renderLast.fenceLine && blockStateBefore(bLast).active())) {
                for (QTextBlock next = bLast.next(); next.isValid(); next = next.next()) {
                    const auto renderNext = renderFor(next);
                    if (!renderNext.codeBlock) {
                        break;
                    }
                    last = next.blockNumber();
                    if (renderNext.fenceLine) {
                        break;
                    }
                }
            }
        }
    }

    return {first, last};
}

void BlockStyler::setRevealed(core::LineRange range) {
    range = expandCodeBlocks(range);
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
    QTextBlock first = document_->findBlock(position);
    // A table header depends on the line below it (S018): restyle the line above the change too.
    if (first.isValid() && first.previous().isValid() && first.previous().text().contains(QLatin1Char('|'))) {
        first = first.previous();
    }
    QTextBlock last = document_->findBlock(position + charsAdded);
    if (!last.isValid()) {
        last = document_->lastBlock();
    }
    const int firstNumber = first.isValid() ? first.blockNumber() : 0;
    const int lastNumber = last.blockNumber();
    // Edited blocks keep their user state but their formats are stale: force a restyle.
    for (QTextBlock b = first; b.isValid() && b.blockNumber() <= std::min(lastNumber, firstNumber + kSynchronousLimit);
         b = b.next()) {
        setStyled(b, kUnstyled);
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
    const BlockLayout::Batch batch(qobject_cast<BlockLayout*>(document_->documentLayout()));
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
