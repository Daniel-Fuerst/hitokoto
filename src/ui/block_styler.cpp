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
#include <limits>
#include <optional>
#include <vector>

namespace hitokoto::ui {
namespace {
// QTextBlock::userState() carries what is known about a block; Qt gives new blocks -1 (nothing).
// Bits 0-1: what the block was styled for (0 = not styled, 1 = concealed, 2 = revealed).
// Bit 2: the block state after the line is known. Bit 3: that state is active; only then does
// the block carry a BlockData holding it, so plain lines need no allocation.
// Bits 4-19: the paragraph of that state (BlockState::paragraph, S023, ADR 0006).
// Bit 20: the setext level of the line's paragraph is known; bits 21-22: that level, or the one
// the line was last shown with. Bit 23: the line's link to the paragraph above is known; bits
// 24-25: that link (none, continues, underline of level 1 or 2).
constexpr int kStyleMask = 0x3;
constexpr int kUnstyled = 0;
constexpr int kStyledConcealed = 1;
constexpr int kStyledRevealed = 2;
constexpr int kStateKnown = 0x4;
constexpr int kStateActive = 0x8;
constexpr int kParagraphShift = 4;
constexpr int kParagraphMask = 0xFFFF << kParagraphShift;
constexpr int kLevelKnown = 1 << 20;
constexpr int kLevelShift = 21;
constexpr int kLevelMask = 0x3 << kLevelShift;
constexpr int kLinkKnown = 1 << 23;
constexpr int kLinkShift = 24;
constexpr int kLinkMask = 0x3 << kLinkShift;
constexpr int kLinkContinues = 1;
// The walk over a paragraph (S023) runs synchronously for this long, then in idle slices.
constexpr qint64 kParagraphSearchNanoseconds = 2'000'000;
// Ranges up to this size are styled synchronously; larger ones on screen + in idle slices.
constexpr int kSynchronousLimit = 400;
constexpr qint64 kSliceNanoseconds = 6'000'000;
// Searching the fences of a code block (S021) runs synchronously for this long, then in slices.
constexpr qint64 kFenceSearchNanoseconds = 4'000'000;

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
        return core::BlockState{0, 0, static_cast<std::uint16_t>((bits & kParagraphMask) >> kParagraphShift)};
    }
    const auto* data = dynamic_cast<const BlockData*>(block.userData());
    if (data != nullptr && data->blockState.active()) {
        return data->blockState;
    }
    return std::nullopt;
}

/// Caches the state after `block`. A changed state makes the line's setext level unknown.
void cacheBlockState(QTextBlock& block, core::BlockState state) {
    auto* data = dynamic_cast<BlockData*>(block.userData());
    const auto previous = cachedBlockState(block);
    int keep = stateBits(block) & (kStyleMask | kLevelKnown | kLevelMask | kLinkKnown | kLinkMask);
    if (previous && *previous != state) {
        keep &= ~kLevelKnown;  // found for another state
    }
    if (state.active()) {
        if (data == nullptr) {
            data = new BlockData;
            block.setUserData(data);
        }
        data->blockState = state;
        block.setUserState(keep | kStateKnown | kStateActive);
        return;
    }
    if (data != nullptr) {
        data->blockState = {};
        if (data->empty()) {
            block.setUserData(nullptr);  // deletes it
        }
    }
    block.setUserState(keep | kStateKnown | (static_cast<int>(state.paragraph) << kParagraphShift));
}

bool levelKnown(const QTextBlock& block) {
    return (stateBits(block) & kLevelKnown) != 0;
}

/// The setext level the line was last computed or shown with.
std::uint8_t levelOf(const QTextBlock& block) {
    return static_cast<std::uint8_t>((stateBits(block) & kLevelMask) >> kLevelShift);
}

void setLevel(QTextBlock& block, std::uint8_t level, bool known) {
    const int bits = stateBits(block) & ~(kLevelKnown | kLevelMask);
    block.setUserState(bits | (known ? kLevelKnown : 0) | (static_cast<int>(level) << kLevelShift));
}

void forgetLevel(QTextBlock& block) {
    block.setUserState(stateBits(block) & ~kLevelKnown);
}

int linkCode(core::ParagraphLink link) {
    return link.underline != 0 ? 1 + link.underline : link.continues ? kLinkContinues : 0;
}

void storeLink(QTextBlock& block, core::ParagraphLink link) {
    const int bits = stateBits(block) & ~kLinkMask;
    block.setUserState(bits | kLinkKnown | (linkCode(link) << kLinkShift));
}

/// The stored link of `block` to the paragraph above, if known.
std::optional<int> storedLink(const QTextBlock& block) {
    const int bits = stateBits(block);
    if ((bits & kLinkKnown) == 0) {
        return std::nullopt;
    }
    return (bits & kLinkMask) >> kLinkShift;
}

std::u16string_view viewOrEmpty(const QString& text) {
    return text.isEmpty() ? std::u16string_view{} : view(text);
}
}  // namespace

BlockStyler::BlockStyler(QTextDocument* document, const core::ElementRegistry& registry,
                         std::function<std::pair<int, int>()> visibleBlocks, QObject* parent)
    : QObject(parent), document_(document), registry_(registry), visibleBlocks_(std::move(visibleBlocks)) {
    idle_.setInterval(0);
    connect(&idle_, &QTimer::timeout, this, &BlockStyler::idleSlice);
    fenceSearch_.setInterval(0);
    connect(&fenceSearch_, &QTimer::timeout, this, &BlockStyler::fenceSearchSlice);
    paragraphSearch_.setInterval(0);
    connect(&paragraphSearch_, &QTimer::timeout, this, &BlockStyler::paragraphSearchSlice);
    underlineSearchTimer_.setInterval(0);
    connect(&underlineSearchTimer_, &QTimer::timeout, this, &BlockStyler::underlineSearchSlice);
    frontMatterSearch_.setInterval(0);
    connect(&frontMatterSearch_, &QTimer::timeout, this, &BlockStyler::frontMatterSlice);
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
    core::BlockState state = startState_;
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
    return renderLine(block, nullptr, false);
}

core::LineRender BlockStyler::renderLine(const QTextBlock& block, core::ParagraphLink* link, bool walk) {
    ++renderCount_;
    const QString text = block.text();
    const core::BlockState state = blockStateBefore(block);
    const QString next = nextLineFor(block, state, text);
    core::LineContext context{isRevealed(block.blockNumber()), state, viewOrEmpty(next)};
    core::ParagraphLink ignored;
    auto render = registry_.render(view(text), context, link != nullptr ? *link : ignored);
    if (render.blockState.underlinable()) {
        // S023: a line of paragraph text is a setext heading line if its paragraph ends in an
        // underline below (ADR 0006); most paragraphs do not, and the level is cached.
        context.setextLevel = setextLevel(block, render.blockState, walk).level;
        if (context.setextLevel != 0) {
            render = registry_.render(view(text), context);
        }
    }
    return render;
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
    core::ParagraphLink link;
    const auto render = renderLine(block, &link, true);
    QTextBlock mutableBlock = block;
    const auto previousState = cachedBlockState(block);
    const bool blockStateChanged = previousState && *previousState != render.blockState;
    const bool levelWasKnown = levelKnown(block);
    cacheBlockState(mutableBlock, render.blockState);
    if (levelWasKnown && render.blockState.underlinable()) {
        // The level was found for this very state while rendering.
        setLevel(mutableBlock, levelOf(block), true);
    }
    updateParagraphAbove(block, render, link);
    applyBlockSpacing(block, render);
    const auto ranges = formats_.ranges(render);
    // Skip the relayout if nothing was and nothing is formatted (most body lines).
    const bool changed = !(ranges.isEmpty() && layout->formats().isEmpty());
    setStyled(mutableBlock, isRevealed(block.blockNumber()) ? kStyledRevealed : kStyledConcealed);
    ++styledCount_;
    if (blockStateChanged) {
        forgetExtent();  // a fence appeared or went away
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

void BlockStyler::applyBlockSpacing(const QTextBlock& block, const core::LineRender& render) {
    // S021: the panel's margin is part of the geometry of its first and last line (ADR 0003),
    // never applied while painting (review O3). S023: a thematic break's margin likewise.
    auto* layout = qobject_cast<BlockLayout*>(document_->documentLayout());
    if (layout == nullptr) {
        return;
    }
    qreal above = 0;
    qreal below = 0;
    if (render.thematicBreak) {
        above = formats_.thematicBreak().margin;
        below = above;
    } else if (render.codeBlock) {
        const qreal margin = formats_.codeBlock().margin;
        // An opening fence leaves the block open, a closing one closes it; the line before an
        // opening fence may belong to another block (a table).
        const bool opening = render.fenceLine && render.blockState.active();
        above = opening ? margin : 0;
        below = (render.fenceLine && !opening) || !block.next().isValid() ? margin : 0;
    }
    layout->setBlockSpacing(block, above, below);
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

core::BlockState BlockStyler::blockStateAfter(const QTextBlock& block) {
    if (const auto cached = cachedBlockState(block)) {
        return *cached;
    }
    const QTextBlock next = block.next();
    if (next.isValid()) {
        return blockStateBefore(next);  // computes and caches the states up to `block`
    }
    const QString text = block.text();
    const core::BlockState before = blockStateBefore(block);
    const QString lookahead = nextLineFor(block, before, text);
    return registry_.blockStateAfter(view(text), before, lookahead.isEmpty() ? std::u16string_view{} : view(lookahead));
}

bool BlockStyler::FenceSearch::contains(int number) const noexcept {
    const int low = back >= 0 ? back + 1 : open;
    const int high = forward >= 0 ? std::max(forward - 1, from) : close;
    return valid && number >= low && (high < 0 || number <= high);
}

BlockStyler::CodeExtent BlockStyler::FenceSearch::known() const noexcept {
    return {back < 0 ? open : -1, forward < 0 ? close : -1};
}

BlockStyler::CodeExtent BlockStyler::codeExtentOf(const QTextBlock& block) {
    const int number = block.blockNumber();
    if (search_.contains(number)) {
        return search_.known();  // the block searched last: a caret step inside costs nothing
    }
    const auto render = renderFor(block);
    if (!render.codeBlock) {
        return {};  // a search still running goes on, for when the caret comes back
    }
    // Every line of one code block carries the same block state; the line before the opening
    // fence and the closing fence carry another one. The search reads cached states and renders
    // nothing; it runs once per block, not once per caret step (review O2).
    const core::BlockState before = blockStateBefore(block);
    const bool opening = render.fenceLine && render.blockState.active();
    const bool closing = render.fenceLine && !render.blockState.active();
    fenceSearch_.stop();
    search_ = {.inside = opening ? render.blockState : before,
               .from = number,
               .open = opening ? number : -1,
               .close = closing ? number : -1,
               .back = opening ? -1 : number - 1,
               .forward = closing ? -1 : number,
               .valid = true};
    if (!continueFenceSearch(kFenceSearchNanoseconds)) {
        // A large block (a warm walk takes about 150 ns per line) or one whose states are not
        // cached yet (just opened): find the rest in idle slices.
        fenceSearch_.start();
    }
    return search_.known();
}

bool BlockStyler::continueFenceSearch(qint64 nanoseconds) {
    QElapsedTimer timer;
    timer.start();
    int steps = 0;
    const auto overBudget = [&] { return ++steps % 256 == 0 && timer.nsecsElapsed() > nanoseconds; };
    if (search_.back >= 0) {
        // The states above the line the search started from are cached: rendering it computed them.
        QTextBlock b = document_->findBlockByNumber(search_.back);
        while (b.isValid() && blockStateAfter(b) == search_.inside) {
            b = b.previous();
            if (b.isValid() && overBudget()) {
                search_.back = b.blockNumber();
                return false;
            }
        }
        search_.open = b.isValid() ? b.blockNumber() + 1 : 0;
        search_.back = -1;
    }
    if (search_.forward >= 0) {
        QTextBlock b = document_->findBlockByNumber(search_.forward);
        while (b.isValid() && blockStateAfter(b) == search_.inside) {
            b = b.next();
            if (b.isValid() && overBudget()) {
                search_.forward = b.blockNumber();
                return false;
            }
        }
        search_.close = b.isValid() ? b.blockNumber() : -1;  // -1: the block runs to the end
        search_.forward = -1;
    }
    return true;
}

void BlockStyler::fenceSearchSlice() {
    if (!continueFenceSearch(kSliceNanoseconds)) {
        return;
    }
    fenceSearch_.stop();
    setRevealed(revealed_);  // reveals the fences just found
}

void BlockStyler::forgetExtent() {
    search_.valid = false;
    fenceSearch_.stop();
}

void BlockStyler::updateExtentAfterEdit(int position, int charsAdded) {
    const int delta = document_->blockCount() - blockCount_;
    blockCount_ = document_->blockCount();
    if (!search_.valid) {
        return;
    }
    const QTextBlock first = document_->findBlock(position);
    QTextBlock last = document_->findBlock(position + charsAdded);
    if (!last.isValid()) {
        last = document_->lastBlock();
    }
    const int f = first.isValid() ? first.blockNumber() : 0;
    const int l = last.blockNumber();
    // Lines up to `top` are not known to be inside the block: an edit there changes what is known.
    const int top = search_.back >= 0 ? search_.back : search_.open;
    if (f <= top) {
        forgetExtent();
        return;
    }
    // Below the closing fence, or below the part a running search has looked at: nothing known
    // changes.
    if ((search_.forward < 0 && search_.close >= 0 && f > search_.close) ||
        (search_.forward >= 0 && f >= search_.forward)) {
        return;
    }
    // An edit between the fences that adds no fence keeps what is known; the closing fence moves
    // by the number of lines added or removed (typing in a block never searches again).
    const int close = search_.close >= 0 ? search_.close + delta : -1;
    bool keep = l - f <= kSynchronousLimit && (close < 0 || l < close);
    for (QTextBlock b = first; keep && b.isValid() && b.blockNumber() <= l; b = b.next()) {
        keep = !renderFor(b).fenceLine;
    }
    if (keep && close >= 0) {
        const auto render = renderFor(document_->findBlockByNumber(close));
        keep = render.codeBlock && render.fenceLine;
    }
    if (!keep) {
        forgetExtent();
        return;
    }
    if (revealedClose_ >= 0 && revealedClose_ == search_.close) {
        revealedClose_ = close;
    }
    search_.close = close;
    if (search_.forward >= 0) {
        search_.forward = std::min(search_.forward + delta, f);  // looks at the edited lines again
    }
    search_.from = std::min(search_.from, f);
}

void BlockStyler::setRevealed(core::LineRange range) {
    int open = -1;
    int close = -1;
    int underline = -1;
    if (!range.empty()) {
        const QTextBlock first = document_->findBlockByNumber(static_cast<int>(range.first));
        const QTextBlock last = document_->findBlockByNumber(static_cast<int>(range.last));
        if (first.isValid()) {
            open = codeExtentOf(first).open;
        }
        if (last.isValid()) {
            close = codeExtentOf(last).close;
            underline = underlineOf(last);
        }
    }
    if (range == revealed_ && open == revealedOpen_ && close == revealedClose_ && underline == revealedUnderline_) {
        return;
    }
    const auto changes = core::revealChanges(revealed_, range);
    const int fences[] = {revealedOpen_, revealedClose_, revealedUnderline_, open, close, underline};
    revealed_ = range;
    revealedOpen_ = open;
    revealedClose_ = close;
    revealedUnderline_ = underline;
    for (const auto& c : changes) {
        styleRange(static_cast<int>(c.first), static_cast<int>(c.last));
    }
    for (const int fence : fences) {
        if (fence >= 0) {
            styleRange(fence, fence);
        }
    }
}

void BlockStyler::onContentsChange(int position, int charsRemoved, int charsAdded) {
    if (busy_) {
        return;
    }
    const int countBefore = blockCount_;
    updateExtentAfterEdit(position, charsAdded);
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
    // Edited blocks keep their user state but their formats are stale: force a restyle. Their
    // setext level is computed again from the line below (S023).
    for (QTextBlock b = first; b.isValid() && b.blockNumber() <= std::min(lastNumber, firstNumber + kSynchronousLimit);
         b = b.next()) {
        setStyled(b, kUnstyled);
        forgetLevel(b);
    }
    restartParagraphSearchesAfterEdit(firstNumber, document_->blockCount() - countBefore);
    updateUnderlineAfterEdit(firstNumber, lastNumber, document_->blockCount() - countBefore);
    frontMatterAfterEdit(firstNumber, lastNumber);
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
    // Styling a line can schedule lines above it (a setext level passed up a paragraph, S023):
    // scheduleFrom() lowers this while the slice runs.
    idleNext_ = std::numeric_limits<int>::max();
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
    idleNext_ = std::min(idleNext_, b.isValid() ? b.blockNumber() : document_->blockCount());
    if (idleNext_ < document_->blockCount()) {
        return;
    }
    idle_.stop();
    // Block states below an edit settle lazily; the fences and the underline to reveal for the
    // caret may have changed with them (S021, S023).
    setRevealed(revealed_);
    if (!idle_.isActive()) {
        emit completed();
    }
}

// ---- S023 (ADR 0006): setext headings from below ---------------------------------------------

BlockStyler::Level BlockStyler::setextLevel(const QTextBlock& block, core::BlockState after, bool walk) {
    if (!after.underlinable()) {
        return {0, true};
    }
    if (levelKnown(block)) {
        return {levelOf(block), true};
    }
    if (!paragraphSearches_.empty()) {
        const int number = block.blockNumber();
        for (auto& search : paragraphSearches_) {
            if (search.covers(number, after)) {
                // A walk over this paragraph goes on in idle slices: show the level the line had.
                search.lastAnswered = std::max(search.lastAnswered, number);
                return {levelOf(block), false};
            }
        }
    }
    if (!walk) {
        return {levelOf(block), false};  // painting or tables: the level the line is shown with
    }
    // Most paragraphs are short: walk down from the line without numbering lines.
    ParagraphSearch search{.inside = after};
    QElapsedTimer timer;
    timer.start();
    int steps = 0;
    QTextBlock b = block.next();
    for (; b.isValid(); b = b.next()) {
        if (const auto level = paragraphStep(b, search)) {
            giveLevel(block, b, *level);
            return {*level, true};
        }
        if (++steps % 64 == 0 && timer.nsecsElapsed() > kParagraphSearchNanoseconds) {
            // A long paragraph (a cold walk takes about 0.3 us per line): walk on in idle slices
            // and show the level the line had until then.
            search.from = block.blockNumber();
            search.next = b.next().isValid() ? b.next().blockNumber() : document_->blockCount();
            search.lastAnswered = search.from;
            paragraphSearches_.push_back(search);
            paragraphSearch_.start();
            return {levelOf(block), false};
        }
    }
    giveLevel(block, b, 0);  // the paragraph ends the document
    return {0, true};
}

std::optional<std::uint8_t> BlockStyler::paragraphStep(QTextBlock& block, ParagraphSearch& search) {
    const QString text = block.text();
    const QString next = nextLineFor(block, search.inside, text);
    const core::ParagraphLink link = registry_.paragraphLink(view(text), search.inside, viewOrEmpty(next));
    storeLink(block, link);
    if (!link.continues) {
        if (search.previousChanged) {
            restyleLater(block);  // entered with a new state
        }
        return link.underline;
    }
    // The line continues the paragraph, so the state after it is the paragraph's. A line whose
    // cached state differs (typing "- " before a long paragraph changes its content column) is
    // cached anew and restyled lazily; the next one is entered with a new state and restyled too.
    const auto cached = cachedBlockState(block);
    const bool changed = !cached || *cached != search.inside;
    if (changed) {
        cacheBlockState(block, search.inside);
    }
    if (changed || search.previousChanged) {
        restyleLater(block);
    } else if (levelKnown(block)) {
        return levelOf(block);  // the rest of the paragraph is known
    }
    search.previousChanged = changed;
    return std::nullopt;
}

void BlockStyler::restyleLater(QTextBlock& block) {
    if ((stateBits(block) & kStyleMask) != kUnstyled) {
        setStyled(block, kUnstyled);
        scheduleFrom(block.blockNumber());
    }
}

void BlockStyler::giveLevel(const QTextBlock& first, const QTextBlock& end, std::uint8_t level) {
    for (QTextBlock b = first; b.isValid() && b != end; b = b.next()) {
        if (levelOf(b) != level) {
            restyleLater(b);  // shown with another level
        }
        setLevel(b, level, true);
    }
    // The lines above that continue into the paragraph's first walked line share its level.
    if (storedLink(first) == kLinkContinues && first.previous().isValid()) {
        propagateLevelUp(first.previous(), level);
    }
}

bool BlockStyler::continueParagraphSearch(ParagraphSearch& search, qint64 nanoseconds) {
    QElapsedTimer timer;
    timer.start();
    if (search.end >= 0) {
        return applyParagraphLevel(search, timer, nanoseconds);
    }
    int steps = 0;
    const auto found = [&](int end, std::uint8_t level) {
        search.end = end;
        search.level = level;
        search.applyNext = search.from;
        return applyParagraphLevel(search, timer, nanoseconds);
    };
    for (QTextBlock b = document_->findBlockByNumber(search.next); b.isValid(); b = b.next()) {
        if (const auto level = paragraphStep(b, search)) {
            return found(b.blockNumber(), *level);
        }
        if (++steps % 64 == 0 && timer.nsecsElapsed() > nanoseconds) {
            search.next = b.next().isValid() ? b.next().blockNumber() : document_->blockCount();
            return false;
        }
    }
    return found(document_->blockCount(), 0);  // the paragraph ends the document
}

bool BlockStyler::applyParagraphLevel(ParagraphSearch& search, const QElapsedTimer& timer, qint64 nanoseconds) {
    int steps = 0;
    const int last = std::max(search.end - 1, search.lastAnswered);
    QTextBlock b = document_->findBlockByNumber(search.applyNext);
    for (int number = search.applyNext; b.isValid() && number <= last; b = b.next(), ++number) {
        // Past the paragraph, a line that waited for the walk starts a walk of its own; inside it,
        // a line shown with another level is restyled.
        const bool past = number >= search.end;
        if (past || levelOf(b) != search.level) {
            setStyled(b, kUnstyled);
            search.restyle = search.restyle < 0 ? number : search.restyle;
        }
        if (!past) {
            setLevel(b, search.level, true);
        }
        if (++steps % 256 == 0 && timer.nsecsElapsed() > nanoseconds) {
            search.applyNext = number + 1;
            return false;
        }
    }
    if (search.restyle >= 0) {
        scheduleFrom(search.restyle);
    }
    // The lines above that continue into the paragraph's first walked line share its level.
    const QTextBlock from = document_->findBlockByNumber(search.from);
    if (from.isValid() && storedLink(from) == kLinkContinues && from.previous().isValid()) {
        propagateLevelUp(from.previous(), search.level);
    }
    return true;
}

void BlockStyler::propagateLevelUp(QTextBlock block, std::uint8_t level) {
    int top = -1;
    for (QTextBlock b = block; b.isValid(); b = b.previous()) {
        const auto state = cachedBlockState(b);
        if (!state || !state->underlinable() || (levelKnown(b) && levelOf(b) == level)) {
            break;
        }
        if (levelOf(b) != level) {
            setStyled(b, kUnstyled);
            top = b.blockNumber();
        }
        setLevel(b, level, true);
        if (storedLink(b) != kLinkContinues) {
            break;  // the first line of the paragraph
        }
    }
    if (top >= 0) {
        scheduleFrom(top);
    }
}

void BlockStyler::updateParagraphAbove(const QTextBlock& block, const core::LineRender& render,
                                       core::ParagraphLink link) {
    // An underline typed or removed below a paragraph, or a line that joins or leaves it, changes
    // the level of the lines above; they are restyled (ADR 0006).
    QTextBlock mutableBlock = block;
    storeLink(mutableBlock, link);
    const QTextBlock above = block.previous();
    const auto aboveState = above.isValid() ? cachedBlockState(above) : std::nullopt;
    if (!aboveState || !aboveState->underlinable()) {
        return;  // the line above is no paragraph text that could be underlined
    }
    std::uint8_t level = link.underline;
    if (link.continues) {
        const Level own = setextLevel(block, render.blockState);
        if (!own.resolved) {
            return;  // the walk passes the level up when it ends
        }
        level = own.level;
    }
    propagateLevelUp(above, level);
}

void BlockStyler::paragraphSearchSlice() {
    QElapsedTimer timer;
    timer.start();
    while (!paragraphSearches_.empty() && timer.nsecsElapsed() < kSliceNanoseconds) {
        if (!continueParagraphSearch(paragraphSearches_.front(), kSliceNanoseconds - timer.nsecsElapsed())) {
            return;
        }
        paragraphSearches_.erase(paragraphSearches_.begin());
    }
    if (paragraphSearches_.empty()) {
        paragraphSearch_.stop();
        setRevealed(revealed_);  // an underline found now may need revealing
    }
}

void BlockStyler::restartParagraphSearchesAfterEdit(int firstEdited, int delta) {
    // Lines a walk has passed may have changed: drop the walks and restyle the lines they covered,
    // which start new ones. Only long paragraphs have walks in progress.
    if (paragraphSearches_.empty()) {
        return;
    }
    int restyle = -1;
    for (const auto& search : paragraphSearches_) {
        if (firstEdited > std::max(search.next, search.lastAnswered)) {
            continue;  // below what the walk has seen and what waits for it
        }
        const int seen = std::max(search.next, search.lastAnswered + 1);
        const int low = std::min(search.from, firstEdited);
        const int high = std::min(std::max(seen, seen + delta), document_->blockCount());
        for (QTextBlock b = document_->findBlockByNumber(low); b.isValid() && b.blockNumber() < high; b = b.next()) {
            setStyled(b, kUnstyled);
        }
        restyle = restyle < 0 ? low : std::min(restyle, low);
    }
    paragraphSearches_.erase(
        std::remove_if(paragraphSearches_.begin(), paragraphSearches_.end(),
                       [&](const ParagraphSearch& s) { return firstEdited <= std::max(s.next, s.lastAnswered); }),
        paragraphSearches_.end());
    if (paragraphSearches_.empty()) {
        paragraphSearch_.stop();
    }
    if (restyle >= 0) {
        scheduleFrom(restyle);
    }
}

int BlockStyler::underlineOf(const QTextBlock& block) {
    const int number = block.blockNumber();
    if (underline_.underline >= 0 && number >= underline_.first && number < underline_.underline) {
        return underline_.underline;  // a caret step inside the heading found last
    }
    const core::BlockState after = blockStateAfter(block);
    const Level level = setextLevel(block, after);
    if (!level.resolved || level.level == 0) {
        return -1;
    }
    if (underlineSearch_.from == number && underlineSearch_.inside == after) {
        return -1;  // searched for in idle slices
    }
    underlineSearchTimer_.stop();
    underlineSearch_ = {after, number, number + 1};
    if (continueUnderlineSearch(kParagraphSearchNanoseconds)) {
        return underline_.first == number ? underline_.underline : -1;
    }
    underlineSearchTimer_.start();  // a long heading
    return -1;
}

bool BlockStyler::continueUnderlineSearch(qint64 nanoseconds) {
    // The lines below continue the paragraph down to its underline (its level says it is there).
    QElapsedTimer timer;
    timer.start();
    int steps = 0;
    for (QTextBlock b = document_->findBlockByNumber(underlineSearch_.next); b.isValid(); b = b.next()) {
        const QString text = b.text();
        const QString next = nextLineFor(b, underlineSearch_.inside, text);
        const core::ParagraphLink link =
            registry_.paragraphLink(view(text), underlineSearch_.inside, viewOrEmpty(next));
        if (link.underline != 0) {
            underline_ = {underlineSearch_.inside, underlineSearch_.from, b.blockNumber()};
            break;
        }
        if (!link.continues) {
            break;
        }
        if (++steps % 64 == 0 && timer.nsecsElapsed() > nanoseconds) {
            underlineSearch_.next = b.blockNumber() + 1;
            return false;
        }
    }
    underlineSearch_ = {};
    return true;
}

void BlockStyler::underlineSearchSlice() {
    if (continueUnderlineSearch(kSliceNanoseconds)) {
        underlineSearchTimer_.stop();
        setRevealed(revealed_);  // reveals the underline just found
    }
}

void BlockStyler::updateUnderlineAfterEdit(int first, int last, int delta) {
    underlineSearchTimer_.stop();
    underlineSearch_ = {};  // setRevealed() searches again if the caret still needs it
    if (underline_.underline < 0 || first > underline_.underline) {
        return;  // nothing found, or edited below the underline
    }
    // An edit between the first known line and the underline that leaves every edited line
    // continuing the paragraph keeps the underline; it moves by the lines added or removed.
    bool keep = first > underline_.first && last - first <= kSynchronousLimit && last < underline_.underline + delta;
    for (QTextBlock b = document_->findBlockByNumber(first); keep && b.isValid() && b.blockNumber() <= last;
         b = b.next()) {
        const QString text = b.text();
        const QString next = nextLineFor(b, underline_.inside, text);
        keep = registry_.paragraphLink(view(text), underline_.inside, viewOrEmpty(next)).continues;
    }
    if (keep) {
        const QTextBlock underline = document_->findBlockByNumber(underline_.underline + delta);
        const QString text = underline.text();
        keep =
            underline.isValid() && registry_
                                           .paragraphLink(view(text), underline_.inside,
                                                          viewOrEmpty(nextLineFor(underline, underline_.inside, text)))
                                           .underline != 0;
    }
    if (keep) {
        underline_.underline += delta;
    } else {
        underline_ = {};
    }
}

// ---- S023: front matter at the start of the document -----------------------------------------

void BlockStyler::checkFrontMatter() {
    frontMatterSearch_.stop();
    const core::BlockState candidate = registry_.documentStart();
    if (!candidate.active()) {
        frontMatter_ = {};
        return;
    }
    frontMatter_ = {candidate, 0};
    if (!continueFrontMatterSearch(kFenceSearchNanoseconds)) {
        frontMatterSearch_.start();  // a long or unclosed front matter: confirm in idle slices
    }
}

bool BlockStyler::continueFrontMatterSearch(qint64 nanoseconds) {
    const core::BlockState candidate = registry_.documentStart();
    QElapsedTimer timer;
    timer.start();
    int steps = 0;
    int close = -1;
    QTextBlock b = document_->findBlockByNumber(frontMatter_.next);
    for (; b.isValid(); b = b.next()) {
        const QString text = b.text();
        frontMatter_.state = registry_.blockStateAfter(view(text), frontMatter_.state);
        if (frontMatter_.state.element != candidate.element) {
            close = b.blockNumber() > 0 ? b.blockNumber() : -1;  // closed, or no front matter at all
            break;
        }
        if (++steps % 256 == 0 && timer.nsecsElapsed() > nanoseconds) {
            frontMatter_.next = b.blockNumber() + 1;
            return false;
        }
    }
    frontMatter_ = {};
    const core::BlockState start = close >= 0 ? candidate : core::BlockState{};
    frontMatterClose_ = close;
    if (start != startState_) {
        // The first line is entered with another state: restyle it; the chain of changed block
        // states restyles the lines below as far as they change.
        startState_ = start;
        QTextBlock first = document_->firstBlock();
        setStyled(first, kUnstyled);
        styleRange(0, 0);
    }
    return true;
}

void BlockStyler::frontMatterSlice() {
    if (continueFrontMatterSearch(kSliceNanoseconds)) {
        frontMatterSearch_.stop();
    }
}

void BlockStyler::frontMatterAfterEdit(int first, int last) {
    // Front matter depends on its first line and its closing line: an edit of the first line, at
    // or above the closing line, or of a line that could close it while it is not closed, confirms
    // it again.
    const core::BlockState candidate = registry_.documentStart();
    if (!candidate.active()) {
        return;
    }
    if (first == 0 || frontMatter_.next >= 0 || (frontMatterClose_ >= 0 && first <= frontMatterClose_ + 1)) {
        checkFrontMatter();
        return;
    }
    if (frontMatterClose_ >= 0) {
        return;  // edited below the front matter
    }
    const QString firstLine = document_->firstBlock().text();
    const core::BlockState inside = registry_.blockStateAfter(view(firstLine), candidate);
    if (inside.element != candidate.element) {
        return;  // the first line opens no front matter
    }
    for (QTextBlock b = document_->findBlockByNumber(first);
         b.isValid() && b.blockNumber() <= std::min(last, first + kSynchronousLimit); b = b.next()) {
        const QString text = b.text();
        if (registry_.blockStateAfter(view(text), inside).element != candidate.element) {
            checkFrontMatter();  // this line could close it
            return;
        }
    }
}

}  // namespace hitokoto::ui
