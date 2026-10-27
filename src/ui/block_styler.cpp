// SPDX-License-Identifier: GPL-3.0-or-later
#include "block_styler.hpp"

#include "block_data.hpp"
#include "qt_bridge.hpp"

#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace hitokoto::ui {
namespace {
// QTextBlock::userState() carries two things per block; Qt gives new blocks -1 (nothing known).
// Bits 0-1: what the block was styled for (0 = not styled, 1 = concealed, 2 = revealed).
// Bit 2: the block state after the line is known. Bit 3: that state is active; only then does
// the block carry a BlockData holding it, so plain lines need no allocation.
constexpr unsigned int kStyleMask = 0x3U;
constexpr unsigned int kUnstyled = 0U;
constexpr unsigned int kStyledConcealed = 1U;
constexpr unsigned int kStyledRevealed = 2U;
constexpr unsigned int kStateKnown = 0x4U;
constexpr unsigned int kStateActive = 0x8U;
constexpr unsigned int kTableContentRow = 0x10U;
// Ranges up to this size are styled synchronously; larger ones on screen + in idle slices.
constexpr int kSynchronousLimit = 400;
constexpr qint64 kSliceNanoseconds = 6'000'000;

unsigned int stateBits(const QTextBlock& block) {
    const int bits = block.userState();
    return bits < 0 ? 0U : static_cast<unsigned int>(bits);
}

void setStyled(QTextBlock& block, unsigned int style) {
    block.setUserState(static_cast<int>((stateBits(block) & ~kStyleMask) | style));
}

/// The cached block state after `block`, if known. Joining lines can pair the user state of one
/// line with the BlockData of another; such a mix counts as unknown.
std::optional<core::BlockState> cachedBlockState(const QTextBlock& block) {
    const unsigned int bits = stateBits(block);
    if ((bits & kStateKnown) == 0U) {
        return std::nullopt;
    }
    if ((bits & kStateActive) == 0U) {
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
    const unsigned int preserved = stateBits(block) & (kStyleMask | kTableContentRow);
    if (state.active()) {
        if (data == nullptr) {
            data = new BlockData;
            block.setUserData(data);
        }
        data->blockState = state;
        block.setUserState(static_cast<int>(preserved | kStateKnown | kStateActive));
        return;
    }
    if (data != nullptr) {
        data->blockState = {};
        if (data->empty()) {
            block.setUserData(nullptr);  // deletes it
        }
    }
    block.setUserState(static_cast<int>(preserved | kStateKnown));
}
}  // namespace

BlockStyler::BatchScope::BatchScope(BlockStyler& styler) : styler_(styler) {
    ++styler_.batchNesting_;
}

BlockStyler::BatchScope::~BatchScope() {
    --styler_.batchNesting_;
    if (styler_.batchNesting_ == 0) {
        styler_.invalidateTableMetrics();
        styler_.styleVisible();
    }
}

void BlockStyler::invalidateTableMetrics() {
    tableMetricsCache_.clear();
}

BlockStyler::BlockStyler(QTextDocument* document, const core::ElementRegistry& registry,
                         std::function<std::pair<int, int>()> visibleBlocks, QObject* parent)
    : QObject(parent), document_(document), registry_(registry), visibleBlocks_(std::move(visibleBlocks)) {
    idle_.setInterval(0);
    connect(&idle_, &QTimer::timeout, this, &BlockStyler::idleSlice);
    connect(document_, &QTextDocument::contentsChange, this, &BlockStyler::onContentsChange);
}

void BlockStyler::setFormats(EditorFormats formats) {
    formats_ = std::move(formats);
    invalidateTableMetrics();
    invalidateAll();
}

void BlockStyler::setAvailableWidth(double width) {
    if (std::abs(width - availableWidth_) <= 0.5) {
        return;
    }
    availableWidth_ = width;
    invalidateTableMetrics();
    for (QTextBlock b = document_->begin(); b.isValid(); b = b.next()) {
        if ((stateBits(b) & (kStateActive | kTableContentRow)) != 0U) {
            setStyled(b, kUnstyled);
        }
    }
    styleVisible();
    scheduleFrom(0);
}

void BlockStyler::invalidateAll() {
    invalidateTableMetrics();
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
        const QString lineText = it->text();
        QString nextText;
        if (!state.active() && lineText.contains(QLatin1Char('|'))) {
            const QTextBlock next = it->next();
            if (next.isValid()) {
                nextText = next.text();
            }
        }
        state = registry_.blockStateAfter(view(lineText), state,
                                          nextText.isEmpty() ? std::u16string_view{} : view(nextText));
        cacheBlockState(*it, state);
    }
    return state;
}

core::LineRender BlockStyler::renderFor(const QTextBlock& block) {
    const QString lineText = block.text();
    const auto stateBefore = blockStateBefore(block);
    QString nextText;
    if (!stateBefore.active() && lineText.contains(QLatin1Char('|'))) {
        const QTextBlock next = block.next();
        if (next.isValid()) {
            nextText = next.text();
        }
    }
    return registry_.render(view(lineText), {isRevealed(block.blockNumber()), stateBefore,
                                             nextText.isEmpty() ? std::u16string_view{} : view(nextText)});
}

TableMetrics BlockStyler::tableMetrics(const QTextBlock& block) {
    TableMetrics metrics;
    if (!block.isValid() || !renderFor(block).table) {
        return metrics;
    }
    const int bNum = block.blockNumber();
    for (const auto& cached : tableMetricsCache_) {
        if (bNum >= cached.firstBlock && bNum <= cached.lastBlock) {
            return cached;
        }
    }
    QTextBlock first = block;
    for (int steps = 0; steps < kSynchronousLimit; ++steps) {
        const QTextBlock prev = first.previous();
        if (!prev.isValid() || !renderFor(prev).table) {
            break;
        }
        first = prev;
        if (renderFor(first).tableHeader) {
            break;
        }
    }
    QTextBlock last = first;
    for (int steps = 0; steps < kSynchronousLimit; ++steps) {
        const QTextBlock next = last.next();
        if (!next.isValid()) {
            break;
        }
        const auto nextRender = renderFor(next);
        if (!nextRender.table || nextRender.tableHeader) {
            break;
        }
        last = next;
    }
    metrics.firstBlock = first.blockNumber();
    metrics.lastBlock = last.blockNumber();
    const double minWidth = std::max(24.0, formats_.baseFont().pointSizeF() * 2.0);
    for (QTextBlock b = first; b.isValid() && b.blockNumber() <= metrics.lastBlock; b = b.next()) {
        const auto r = renderFor(b);
        if (r.tableDelimiter) {
            continue;
        }
        if (metrics.columnWidths.size() < r.cells.size()) {
            metrics.columnWidths.resize(r.cells.size(), minWidth);
        }
        const QString text = b.text();
        const auto rowRanges = formats_.ranges(r);
        for (std::size_t c = 0; c < r.cells.size(); ++c) {
            metrics.columnWidths[c] =
                std::max(metrics.columnWidths[c], formats_.cellTextWidth(text, rowRanges, r.cells[c]));
        }
    }
    if (availableWidth_ > 0.0 && !metrics.columnWidths.empty()) {
        const double pad = formats_.table().padding;
        const auto cols = static_cast<double>(metrics.columnWidths.size());
        const double maxInnerWidth = std::max(cols * 8.0, availableWidth_ - cols * 2.0 * pad);
        double naturalSum = 0.0;
        double maxSqrt = 0.0;
        for (double w : metrics.columnWidths) {
            naturalSum += w;
            maxSqrt = std::max(maxSqrt, std::sqrt(w));
        }
        if (naturalSum > maxInnerWidth) {
            const double floorW = std::min(minWidth, maxInnerWidth / cols);
            double lo = 0.0;
            double hi = maxSqrt + 1.0;
            for (int iter = 0; iter < 40; ++iter) {
                const double mid = 0.5 * (lo + hi);
                double sum = 0.0;
                for (double w : metrics.columnWidths) {
                    sum += std::clamp(mid * std::sqrt(w), std::min(w, floorW), w);
                }
                if (sum <= maxInnerWidth) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            for (double& w : metrics.columnWidths) {
                w = std::clamp(lo * std::sqrt(w), std::min(w, floorW), w);
            }
        }
    }
    if (metrics.firstBlock >= 0) {
        tableMetricsCache_.push_back(metrics);
    }
    return metrics;
}

bool BlockStyler::isStyled(const QTextBlock& block) const {
    const unsigned int bits = stateBits(block);
    const unsigned int expected = isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed;
    if ((bits & kStyleMask) != expected) {
        return false;
    }
    if ((bits & kTableContentRow) != 0U) {
        const QTextLayout* layout = block.layout();
        // QPlainTextDocumentLayout::layoutBlock sets leadingIncluded(true); layoutTableRow sets false.
        if (!layout || layout->lineCount() == 0 || layout->lineAt(0).leadingIncluded()) {
            return false;
        }
    }
    return true;
}

void BlockStyler::layoutTableRow(QTextBlock& block, const core::LineRender& render, const TableMetrics& metrics) {
    QTextLayout* layout = block.layout();
    if (!layout || render.cells.empty()) {
        return;
    }
    QTextOption opt = layout->textOption();
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->setTextOption(opt);

    const QString text = block.text();
    const auto lineLen = static_cast<int>(text.size());
    if (lineLen <= 0) {
        return;
    }
    const qreal margin = document_->documentMargin();
    const qreal pad = formats_.table().padding;
    qreal colLeft = 0.0;
    int cursor = 0;

    layout->beginLayout();
    for (std::size_t c = 0; c < render.cells.size(); ++c) {
        const auto& cell = render.cells[c];
        const qreal colW = c < metrics.columnWidths.size() ? metrics.columnWidths[c] : 24.0;
        const int segEnd = (c + 1 < render.cells.size()) ? static_cast<int>(cell.raw.end()) : lineLen;
        const int contentStart = static_cast<int>(cell.content.start);
        const int contentEnd = static_cast<int>(cell.content.end());

        qreal cellY = 0.0;
        while (cursor < segEnd) {
            QTextLine line = layout->createLine();
            if (!line.isValid()) {
                break;
            }
            const int st = line.textStart();
            if (st >= segEnd) {
                break;
            }
            const int rem = segEnd - st;
            line.setNumColumns(rem);

            const int fitVisStart = std::clamp(contentStart, st, st + line.textLength());
            const int fitVisEnd = std::clamp(contentEnd, fitVisStart, st + line.textLength());
            const qreal fitPrefixW = line.cursorToX(fitVisStart) - line.x();
            const qreal fitVisW = line.cursorToX(fitVisEnd) - line.cursorToX(fitVisStart);

            if (fitVisW > colW + 0.5) {
                line.setLineWidth(std::max<qreal>(1.0, colW + fitPrefixW));
                if (line.textLength() <= 0) {
                    line.setNumColumns(1);
                }
                if (line.textStart() + line.textLength() > segEnd ||
                    line.textStart() + line.textLength() >= contentEnd) {
                    line.setNumColumns(rem);
                }
            }

            const int lineEnd = st + line.textLength();
            const int visStart = std::clamp(contentStart, st, lineEnd);
            int visEnd = std::clamp(contentEnd, visStart, lineEnd);
            if (lineEnd < segEnd) {
                while (visEnd > visStart && text[visEnd - 1].isSpace()) {
                    --visEnd;
                }
            }
            const qreal prefixW = line.cursorToX(visStart) - line.x();
            const qreal visW = std::max<qreal>(0.0, line.cursorToX(visEnd) - line.cursorToX(visStart));
            const qreal slack = std::max<qreal>(0.0, colW - visW);
            const qreal alignOffset = cell.align == core::TableAlign::Center  ? slack / 2.0
                                      : cell.align == core::TableAlign::Right ? slack
                                                                              : 0.0;
            line.setLeadingIncluded(false);
            line.setPosition(QPointF(margin + colLeft + pad + alignOffset - prefixW, cellY));
            cellY += line.height();
            cursor = line.textStart() + line.textLength();
        }
        colLeft += colW + 2.0 * pad;
    }
    layout->endLayout();
    block.setLineCount(1);
    if (auto* plainLayout = qobject_cast<QPlainTextDocumentLayout*>(document_->documentLayout())) {
        plainLayout->requestUpdate();
    }
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
    const bool wasTableRow = (stateBits(block) & kTableContentRow) != 0U;
    // Skip the relayout if nothing was and nothing is formatted (most body lines).
    const bool changed = wasTableRow || !(ranges.isEmpty() && layout->formats().isEmpty());
    const bool tableRow = render.table && !render.tableDelimiter && !render.cells.empty();
    const unsigned int style = isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed;
    mutableBlock.setUserState(static_cast<int>((stateBits(mutableBlock) & ~(kStyleMask | kTableContentRow)) | style |
                                               (tableRow ? kTableContentRow : 0U)));
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
    QTextOption opt = layout->textOption();
    const auto desiredWrap =
        (render.table && render.tableDelimiter) ? QTextOption::NoWrap : QTextOption::WrapAtWordBoundaryOrAnywhere;
    if (opt.wrapMode() != desiredWrap) {
        opt.setWrapMode(desiredWrap);
        layout->setTextOption(opt);
    }
    layout->setFormats(ranges);
    document_->markContentsDirty(block.position(), block.length());
    if (render.table && !render.tableDelimiter && !render.cells.empty()) {
        if (const auto* plainLayout = qobject_cast<const QPlainTextDocumentLayout*>(document_->documentLayout())) {
            plainLayout->ensureBlockLayout(block);
        }
        layoutTableRow(mutableBlock, render, tableMetrics(block));
    }
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
    const auto overlaps = [](core::LineRange a, int first, int last) {
        return !a.empty() && first <= last &&
               std::max<std::int64_t>(a.first, first) <= std::min<std::int64_t>(a.last, last);
    };
    bool overlapsTable = false;
    for (const auto& tm : tableMetricsCache_) {
        if (tm.firstBlock >= 0 &&
            (overlaps(range, tm.firstBlock, tm.lastBlock) || overlaps(revealed_, tm.firstBlock, tm.lastBlock))) {
            overlapsTable = true;
            break;
        }
    }
    if (overlapsTable) {
        invalidateTableMetrics();
    }
    const auto changes = core::revealChanges(revealed_, range);
    revealed_ = range;
    for (const auto& c : changes) {
        styleRange(static_cast<int>(c.first), static_cast<int>(c.last));
    }
}

void BlockStyler::onContentsChange(int position, int charsRemoved, int charsAdded) {
    invalidateTableMetrics();
    if (busy_ || batchNesting_ > 0) {
        return;
    }
    const QTextBlock first = document_->findBlock(position);
    QTextBlock last = document_->findBlock(position + charsAdded);
    if (!last.isValid()) {
        last = document_->lastBlock();
    }
    int firstNumber = first.blockNumber();
    if (first.previous().isValid() && first.previous().text().contains(QLatin1Char('|'))) {
        // A 1-line lookahead element (table header) depends on its next line.
        firstNumber = first.previous().blockNumber();
    }
    int lastNumber = std::min(last.blockNumber() + 1, document_->blockCount() - 1);
    for (QTextBlock b = document_->findBlockByNumber(firstNumber);
         b.isValid() && b.blockNumber() <= std::min(lastNumber, firstNumber + kSynchronousLimit); b = b.next()) {
        setStyled(b, kUnstyled);
    }
    if (first.isValid() && (first.text().contains(QLatin1Char('|')) || renderFor(first).table)) {
        const QTextBlock probe = renderFor(first).table ? first : first.previous();
        if (probe.isValid() && renderFor(probe).table) {
            const TableMetrics m = tableMetrics(probe);
            if (m.firstBlock >= 0) {
                firstNumber = std::min(firstNumber, m.firstBlock);
                lastNumber = std::max(lastNumber, m.lastBlock);
                for (QTextBlock b = document_->findBlockByNumber(m.firstBlock);
                     b.isValid() && b.blockNumber() <= m.lastBlock; b = b.next()) {
                    setStyled(b, kUnstyled);
                }
            }
        }
    }
    invalidateTableMetrics();
    styleRange(firstNumber, lastNumber);
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
