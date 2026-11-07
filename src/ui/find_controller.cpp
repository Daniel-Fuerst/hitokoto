// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. One pass over the document's lines in slices (at most 6 ms in idle
// time, a bounded part on a key) builds the units of the concealed view with the core
// (FindUnitBuilder) and scans them (scanUnits); per line it keeps the entering block state and the
// number of matches that start there, positions only for the lines on screen and the current
// match. An edit restarts the pass a little above the edited line and stops it where the
// state and the results meet the earlier ones again (review O4).
#include "find_controller.hpp"

#include "editor_widget.hpp"
#include "find_bar.hpp"
#include "properties_view.hpp"
#include "qt_bridge.hpp"
#include "table_model.hpp"

#include "hitokoto/core/properties.hpp"

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QTableView>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>

#include <algorithm>
#include <numeric>

namespace hitokoto::ui {
namespace {

constexpr qint64 kSliceNanoseconds = 6'000'000;  ///< idle slices, as the styler's
constexpr qint64 kKeyNanoseconds = 8'000'000;    ///< on a key in the field (decision 8: search while typing)
constexpr qint64 kStepNanoseconds = 30'000'000;  ///< next/previous before the count is complete

std::u16string textOf(const QTextBlock& block) {
    const QString text = block.text();
    return {reinterpret_cast<const char16_t*>(text.utf16()), static_cast<std::size_t>(text.size())};
}

int lineOfUnit(const core::FindUnit& unit, int fallback) {
    if (unit.kind == core::FindUnitKind::Destination) {
        return static_cast<int>(unit.linkStart.line);
    }
    return unit.segments.empty() ? fallback : static_cast<int>(unit.segments.front().source.line);
}

}  // namespace

FindController::FindController(EditorWidget* editor, FindBar* bar, const core::ElementRegistry& registry,
                               QObject* parent)
    : QObject(parent),
      editor_(editor),
      bar_(bar),
      registry_(registry),
      style_(core::findStyle(core::builtinTheme(core::ColorScheme::Light))) {
    sliceTimer_.setSingleShot(true);
    sliceTimer_.setInterval(0);
    connect(&sliceTimer_, &QTimer::timeout, this, &FindController::slice);
    connect(bar_, &FindBar::textChanged, this, [this](const QString& text) {
        text_ = text;
        startSearch(current_ ? current_->start : caretPos());
    });
    connect(bar_, &FindBar::nextRequested, this, &FindController::next);
    connect(bar_, &FindBar::previousRequested, this, &FindController::previous);
    connect(bar_, &FindBar::closeRequested, this, &FindController::close);
    connect(bar_, &FindBar::matchCaseChanged, this, [this](bool on) {
        matchCase_ = on;
        if (open_) {
            startSearch(current_ ? current_->start : caretPos());
        }
    });
    connect(bar_, &FindBar::highlightAllChanged, this, [this](bool on) {
        highlightAll_ = on;
        refreshView();
    });
    connect(editor_->document(), &QTextDocument::contentsChange, this, &FindController::onContentsChange);
    connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged, this, &FindController::scheduleRefresh);
    connect(editor_->verticalScrollBar(), &QScrollBar::rangeChanged, this, &FindController::scheduleRefresh);
    connect(editor_, &QTextEdit::cursorPositionChanged, this, &FindController::scheduleRefresh);
}

FindController::~FindController() = default;

void FindController::applyTheme(const core::Theme& theme) {
    style_ = core::findStyle(theme);
    bar_->applyStyle(style_);
    refreshView();
}

// ---- opening, closing, stepping ------------------------------------------------------------

void FindController::open() {
    QLineEdit* field = bar_->searchField();
    if (!open_) {
        open_ = true;
        bar_->show();
        text_ = field->text();
        startSearch(caretPos());
    }
    field->setFocus(Qt::ShortcutFocusReason);
    field->selectAll();
}

void FindController::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    sliceTimer_.stop();
    pass_ = Pass{};
    ahead_ = Pass{};
    const std::optional<core::FindMatch> match = current_;
    bar_->hide();
    editor_->setExtraSelections({});
    selections_.clear();
    shown_.clear();
    for (const auto& model : markedTables_) {
        if (model) {
            model->setFindCells({}, {});
        }
    }
    markedTables_.clear();
    if (auto* view = qobject_cast<PropertiesView*>(editor_->propertiesView())) {
        view->setFindRows({}, {});
    }
    editor_->setFocus(Qt::OtherFocusReason);
    if (match) {
        // Decision 9: the caret moves only now, onto the match (for a link destination: its text).
        QTextCursor cursor(editor_->document());
        cursor.setPosition(position(match->start));
        cursor.setPosition(position(match->end), QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);
        // Review O8: a table keeps its lines until the caret and the anchor have left it.
        editor_->setFindHold(static_cast<int>(match->start.line), static_cast<int>(match->end.line), true);
    } else {
        editor_->setFindHold(-1, -1, false);
    }
    current_.reset();
    blocks_.clear();
    complete_ = true;
}

void FindController::next() {
    if (!open_) {
        open();
        return;
    }
    step(+1);
}

void FindController::previous() {
    if (!open_) {
        open();
        return;
    }
    step(-1);
}

void FindController::step(int direction) {
    if (current_ && currentOrdinal_ < 0 && !complete_) {
        (void)work(kStepNanoseconds);  // the pass from the start has not reached the current match yet
    }
    if (!current_ || currentOrdinal_ < 0) {
        pendingStep_ = current_ ? direction : 0;
        return;
    }
    const auto block = static_cast<int>(current_->start.line);
    const BlockMatches here = matchesOfBlock(block);
    const int ordinal = currentOrdinal_ + direction;
    if (ordinal >= 0 && ordinal < static_cast<int>(here.matches.size())) {
        setCurrent(here.matches[static_cast<std::size_t>(ordinal)], ordinal);
        return;
    }
    if (!complete_) {
        (void)work(kStepNanoseconds);
    }
    if (direction > 0) {
        int b = firstBlockWithMatches(block + 1, scanned_);
        if (b < 0) {
            if (!complete_) {
                pendingStep_ = +1;  // done when the pass finds one
                return;
            }
            b = firstBlockWithMatches(0, scanned_);  // wrap at the end of the document
        }
        if (b >= 0) {
            const BlockMatches m = matchesOfBlock(b);
            if (!m.matches.empty()) {
                setCurrent(m.matches.front(), 0);
            }
        }
    } else {
        int b = lastBlockWithMatches(0, block);
        if (b < 0) {
            if (!complete_) {
                pendingStep_ = -1;
                return;
            }
            b = lastBlockWithMatches(0, scanned_);  // wrap at the start of the document
        }
        if (b >= 0) {
            const BlockMatches m = matchesOfBlock(b);
            if (!m.matches.empty()) {
                setCurrent(m.matches.back(), static_cast<int>(m.matches.size()) - 1);
            }
        }
    }
}

int FindController::firstBlockWithMatches(int from, int to) const {
    for (int b = std::max(0, from); b < to && b < static_cast<int>(blocks_.size()); ++b) {
        if (blocks_[static_cast<std::size_t>(b)].count > 0) {
            return b;
        }
    }
    return -1;
}

int FindController::lastBlockWithMatches(int from, int to) const {
    for (int b = std::min(to, static_cast<int>(blocks_.size())) - 1; b >= from; --b) {
        if (blocks_[static_cast<std::size_t>(b)].count > 0) {
            return b;
        }
    }
    return -1;
}

// ---- the pass --------------------------------------------------------------------------------

void FindController::startSearch(core::DocPos anchor) {
    if (!open_) {
        return;
    }
    query_ = core::FindQuery(text_.toStdU16String(), {.matchCase = matchCase_});
    queryChars_.clear();
    for (const std::u32string& word : query_.words()) {
        queryChars_.insert(queryChars_.end(), word.begin(), word.end());
    }
    std::sort(queryChars_.begin(), queryChars_.end());
    queryChars_.erase(std::unique(queryChars_.begin(), queryChars_.end()), queryChars_.end());
    sliceTimer_.stop();
    current_.reset();
    currentOrdinal_ = -1;
    pendingStep_ = 0;
    needCurrent_ = true;
    announcePending_ = true;
    anchor_ = anchor;
    blockCount_ = editor_->document()->blockCount();
    blocks_.assign(static_cast<std::size_t>(blockCount_), BlockInfo{});
    scanned_ = 0;
    frontMatterClose_ = -1;
    if (query_.findsNothing()) {
        pass_ = Pass{};
        complete_ = true;
        needCurrent_ = false;
        editor_->setFindHold(-1, -1, false);
        refreshView();
        return;
    }
    complete_ = false;
    restartFrom(0);
    startAhead();
    if (!aheadWork(kKeyNanoseconds / 2) || ahead_.active) {
        (void)work(kKeyNanoseconds / 2);
    } else {
        (void)work(kKeyNanoseconds);
    }
    if (!complete_) {
        sliceTimer_.start();
    }
    refreshView();
}

void FindController::startAhead() {
    ahead_ = Pass{};
    aheadFound_.clear();
    // Only far into a long document, and only with states the styler has already: computing them
    // is the pass from the start.
    constexpr int kFarEnough = 2000;
    int block = static_cast<int>(anchor_.line);
    if (block < kFarEnough || !editor_->styler().isComplete() || frontMatterClose_ >= 0) {
        return;
    }
    QTextBlock b = editor_->document()->findBlockByNumber(block);
    core::BlockState state = editor_->styler().blockStateBefore(b);
    while (b.isValid() && block > 0 && isTableInside(state)) {
        b = b.previous();
        --block;
        state = editor_->styler().blockStateBefore(b);
    }
    if (!b.isValid()) {
        return;
    }
    ahead_.active = true;
    ahead_.builder.emplace(registry_);
    ahead_.next = block;
    ahead_.state = state;
}

bool FindController::aheadWork(qint64 nanoseconds) {
    if (!ahead_.active || !needCurrent_) {
        ahead_.active = false;
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    QTextBlock block = editor_->document()->findBlockByNumber(ahead_.next);
    std::u16string text = block.isValid() ? textOf(block) : std::u16string{};
    int n = 0;
    while (block.isValid()) {
        const int b = block.blockNumber();
        const QTextBlock nextBlock = block.next();
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        const core::LineRender render = registry_.render(text, core::LineContext{false, ahead_.state, next});
        ahead_.state = render.blockState;
        ahead_.next = b + 1;
        const bool skip = ahead_.units.empty() && !render.table && !ahead_.lastWasTable && !mayMatch(text);
        ahead_.lastWasTable = render.table;
        if (!skip) {
            ahead_.builder->addLine(text, static_cast<std::size_t>(b), render, ahead_.units);
            ahead_.unitBlock.resize(ahead_.units.size(), b);
            const core::FindScan scan =
                core::scanUnits(ahead_.units, ahead_.resumeUnit, ahead_.resumeOffset, query_, nextBlock.isValid());
            aheadFound_.insert(aheadFound_.end(), scan.matches.begin(), scan.matches.end());
            const std::size_t keep = std::min(scan.resumeUnit, ahead_.units.size());
            ahead_.units.erase(ahead_.units.begin(), ahead_.units.begin() + static_cast<std::ptrdiff_t>(keep));
            ahead_.unitBlock.erase(ahead_.unitBlock.begin(),
                                   ahead_.unitBlock.begin() + static_cast<std::ptrdiff_t>(keep));
            ahead_.resumeUnit = 0;
            ahead_.resumeOffset = ahead_.units.empty() ? 0 : scan.resumeOffset;
        }
        // Matches that start above the first unit still pending are all known.
        const int known = ahead_.units.empty() ? ahead_.next : ahead_.unitBlock.front();
        const core::FindMatch* best = nullptr;
        for (const core::FindMatch& m : aheadFound_) {
            if (static_cast<int>(m.start.line) < known && anchor_ <= m.start &&
                (best == nullptr || m.start < best->start)) {
                best = &m;
            }
        }
        if (best != nullptr && static_cast<int>(best->start.line) < known) {
            const core::FindMatch found = *best;
            ahead_.active = false;
            aheadFound_.clear();
            setCurrent(found, -1);  // its number is known once the pass from the start gets there
            return false;
        }
        block = nextBlock;
        text = next;
        if ((++n & 31) == 0 && timer.nsecsElapsed() > nanoseconds) {
            return true;
        }
    }
    ahead_.active = false;  // nothing after the caret: the pass from the start wraps around
    return false;
}

void FindController::restartFrom(int block) {
    // A table is read from its header line (the builder needs its column count).
    while (block > 0 && isTableInside(blocks_[static_cast<std::size_t>(block)].entering)) {
        --block;
    }
    scanned_ = std::min(scanned_, block);
    pass_ = Pass{};
    pass_.active = true;
    pass_.builder.emplace(registry_);
    pass_.next = block;
    if (block == 0) {
        pass_.frontState = registry_.documentStart();
        pass_.frontMatterPhase = pass_.frontState.active();
        frontMatterClose_ = -1;
        frontMatterYaml_.clear();
    } else {
        pass_.state = blocks_[static_cast<std::size_t>(block)].entering;
    }
}

bool FindController::frontMatterStep(const QElapsedTimer& timer, qint64 nanoseconds) {
    // As renderDocument: the block counts only if it closes before the document ends.
    const QTextDocument* doc = editor_->document();
    QTextBlock b = doc->findBlockByNumber(pass_.frontWalk);
    const core::BlockState start = registry_.documentStart();
    int n = 0;
    while (b.isValid()) {
        const QTextBlock nextBlock = b.next();
        const std::u16string text = textOf(b);
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        pass_.frontState = registry_.blockStateAfter(text, pass_.frontState, next);
        const int i = b.blockNumber();
        if (pass_.frontState.element != start.element && i == 0) {
            break;  // the first line opens no front matter
        }
        if (pass_.frontState.element != start.element) {
            // Closed on line i.
            std::vector<std::u16string> yaml;
            for (QTextBlock y = doc->firstBlock().next(); y.isValid() && y.blockNumber() < i; y = y.next()) {
                yaml.push_back(textOf(y));
            }
            pass_.frontMatterPhase = false;
            ++propertyParses_;
            if (editor_->frontMatterAsProperties() && !core::parseProperties(yaml).error) {
                frontMatterClose_ = i;
                frontMatterYaml_ = std::move(yaml);
                // The entering states of the block's lines, for the pass and for edits.
                core::BlockState state = start;
                for (QTextBlock line = doc->firstBlock(); line.isValid() && line.blockNumber() <= i;
                     line = line.next()) {
                    blocks_[static_cast<std::size_t>(line.blockNumber())].entering = state;
                    const QTextBlock after = line.next();
                    state = registry_.blockStateAfter(textOf(line), state,
                                                      after.isValid() ? textOf(after) : std::u16string{});
                }
                const std::size_t first = pass_.units.size();
                ++propertyParses_;
                pass_.builder->addProperties(frontMatterYaml_, 1, pass_.units);
                for (std::size_t u = first; u < pass_.units.size(); ++u) {
                    pass_.unitBlock.push_back(lineOfUnit(pass_.units[u], 1));
                }
                for (int line = 0; line <= i; ++line) {
                    BlockInfo& info = blocks_[static_cast<std::size_t>(line)];
                    info.count = 0;
                    info.crossed = false;
                    info.known = true;
                }
                pass_.state = state;
                pass_.next = i + 1;
            } else {
                pass_.state = start;
                pass_.next = 0;
            }
            return true;
        }
        b = nextBlock;
        if ((++n & 63) == 0 && timer.nsecsElapsed() > nanoseconds) {
            pass_.frontWalk = b.isValid() ? b.blockNumber() : blockCount_;
            return false;
        }
    }
    // Not closed: the first line is entered with no block open.
    pass_.frontMatterPhase = false;
    pass_.state = core::BlockState{};
    pass_.next = 0;
    return true;
}

bool FindController::work(qint64 nanoseconds) {
    if (!pass_.active) {
        return complete_;
    }
    QElapsedTimer timer;
    timer.start();
    if (pass_.frontMatterPhase && !frontMatterStep(timer, nanoseconds)) {
        return false;
    }
    const QTextDocument* doc = editor_->document();
    QTextBlock block = doc->findBlockByNumber(pass_.next);
    std::u16string text = block.isValid() ? textOf(block) : std::u16string{};
    int n = 0;
    while (block.isValid()) {
        const int b = block.blockNumber();
        BlockInfo& info = blocks_[static_cast<std::size_t>(b)];
        // Review O4: past an edit, stop where the state and the results meet the earlier ones.
        if (b > pass_.settleAfter && b < pass_.oldScanned && info.known && info.entering == pass_.state &&
            !info.crossed && !pass_.lastWasTable && pass_.units.empty()) {
            const int oldScanned = pass_.oldScanned;
            const bool wasComplete = pass_.oldComplete;
            finalizeUpTo(b);
            finalizeUpTo(oldScanned);  // the earlier results from here on hold
            pass_.active = false;
            if (wasComplete) {
                finishPass();
                return true;
            }
            // The earlier pass had not finished: go on from where it stopped.
            restartFrom(oldScanned);
            return work(std::max<qint64>(0, nanoseconds - timer.nsecsElapsed()));
        }
        const QTextBlock nextBlock = block.next();
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        const core::LineRender render = registry_.render(text, core::LineContext{false, pass_.state, next});
        info.entering = pass_.state;
        info.count = 0;
        info.crossed = false;
        info.known = true;
        pass_.state = render.blockState;
        pass_.next = b + 1;
        // A line without any character of the search text holds no match, and with nothing
        // pending no match goes through it: its units need not be built (most lines of a long
        // document). Tables keep the builder's state.
        const bool skip = pass_.units.empty() && !render.table && !pass_.lastWasTable && !mayMatch(text);
        pass_.lastWasTable = render.table;
        if (skip) {
            finalizeUpTo(pass_.next);
        } else {
            pass_.builder->addLine(text, static_cast<std::size_t>(b), render, pass_.units);
            pass_.unitBlock.resize(pass_.units.size(), b);
            scanBuffer(nextBlock.isValid());
        }
        block = nextBlock;
        text = next;
        if ((++n & 31) == 0 && timer.nsecsElapsed() > nanoseconds) {
            return false;
        }
    }
    scanBuffer(false);
    finalizeUpTo(blockCount_);
    pass_.active = false;
    finishPass();
    return true;
}

void FindController::scanBuffer(bool more) {
    const core::FindScan scan = core::scanUnits(pass_.units, pass_.resumeUnit, pass_.resumeOffset, query_, more);
    for (const core::FindMatch& m : scan.matches) {
        const auto start = static_cast<std::size_t>(m.start.line);
        if (start < blocks_.size()) {
            ++blocks_[start].count;
        }
        for (std::size_t line = start + 1; line <= m.end.line && line < blocks_.size(); ++line) {
            blocks_[line].crossed = true;
        }
    }
    // Keep only the units from the resume point on: a match may start there once more units follow.
    const std::size_t keep = std::min(scan.resumeUnit, pass_.units.size());
    pass_.units.erase(pass_.units.begin(), pass_.units.begin() + static_cast<std::ptrdiff_t>(keep));
    pass_.unitBlock.erase(pass_.unitBlock.begin(), pass_.unitBlock.begin() + static_cast<std::ptrdiff_t>(keep));
    pass_.resumeUnit = 0;
    pass_.resumeOffset = pass_.units.empty() ? 0 : scan.resumeOffset;
    finalizeUpTo(pass_.units.empty() ? pass_.next : pass_.unitBlock.front());
}

void FindController::finalizeUpTo(int block) {
    const int to = std::min(block, blockCount_);
    if (to <= scanned_) {
        return;
    }
    const int from = scanned_;
    scanned_ = to;
    if (current_ && currentOrdinal_ < 0 && static_cast<int>(current_->start.line) < to) {
        // Found by the pass ahead: its place among the matches of its line.
        const BlockMatches m = matchesOfBlock(static_cast<int>(current_->start.line));
        for (std::size_t i = 0; i < m.matches.size(); ++i) {
            if (m.matches[i].start == current_->start && m.matches[i].end == current_->end) {
                currentOrdinal_ = static_cast<int>(i);
            }
        }
    }
    if (needCurrent_) {
        for (int b = std::max(from, static_cast<int>(anchor_.line)); b < to && needCurrent_; ++b) {
            if (blocks_[static_cast<std::size_t>(b)].count == 0) {
                continue;
            }
            const BlockMatches m = matchesOfBlock(b);
            for (std::size_t i = 0; i < m.matches.size(); ++i) {
                if (anchor_ <= m.matches[i].start) {
                    setCurrent(m.matches[i], static_cast<int>(i));
                    break;
                }
            }
        }
    }
    if (pendingStep_ != 0 && current_ && currentOrdinal_ >= 0 &&
        (pendingStep_ < 0 || firstBlockWithMatches(static_cast<int>(current_->start.line), to) >= 0)) {
        // A step asked for before the pass got this far: take it now, outside the pass.
        const int direction = pendingStep_;
        pendingStep_ = 0;
        QTimer::singleShot(0, this, [this, direction] { step(direction); });
    }
}

void FindController::finishPass() {
    complete_ = true;
    sliceTimer_.stop();
    if (needCurrent_) {
        resolveCurrentFromCounts();
    }
    if (pendingStep_ != 0 && current_) {
        const int direction = pendingStep_;
        pendingStep_ = 0;
        step(direction);
    }
    pendingStep_ = 0;
    refreshView();
}

void FindController::resolveCurrentFromCounts() {
    // The first match at or after the anchor, else the first in the document (wrapping).
    for (int b = static_cast<int>(anchor_.line); b < scanned_; ++b) {
        if (blocks_[static_cast<std::size_t>(b)].count == 0) {
            continue;
        }
        const BlockMatches m = matchesOfBlock(b);
        for (std::size_t i = 0; i < m.matches.size(); ++i) {
            if (anchor_ <= m.matches[i].start) {
                setCurrent(m.matches[i], static_cast<int>(i));
                return;
            }
        }
    }
    const int first = firstBlockWithMatches(0, scanned_);
    if (first >= 0) {
        const BlockMatches m = matchesOfBlock(first);
        if (!m.matches.empty()) {
            setCurrent(m.matches.front(), 0);
            return;
        }
    }
    needCurrent_ = false;
    setCurrent(std::nullopt, -1);
}

void FindController::slice() {
    if (ahead_.active && aheadWork(kSliceNanoseconds / 2)) {
        (void)work(kSliceNanoseconds / 2);
        sliceTimer_.start();
    } else if (!work(kSliceNanoseconds)) {
        sliceTimer_.start();
    }
    refreshView();
}

bool FindController::mayMatch(std::u16string_view text) const {
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char16_t c = text[i];
        if (c >= 0xD800 && c < 0xE000) {
            return true;  // outside the BMP: no quick answer
        }
        const char32_t folded = matchCase_ ? c : core::foldCaseSimple(c);
        if (std::binary_search(queryChars_.begin(), queryChars_.end(), folded)) {
            return true;
        }
    }
    return false;
}

bool FindController::isTableInside(const core::BlockState& state) const noexcept {
    return state.active() && state.element == registry_.blockElement("table");
}

FindController::BlockMatches FindController::matchesOfBlock(int block) const {
    BlockMatches out;
    if (block < 0 || block >= static_cast<int>(blocks_.size()) || !blocks_[static_cast<std::size_t>(block)].known) {
        return out;
    }
    const QTextDocument* doc = editor_->document();
    core::FindUnitBuilder builder(registry_);
    int start = block;
    if (frontMatterClose_ >= 0 && block <= frontMatterClose_) {
        ++propertyParses_;
        builder.addProperties(frontMatterYaml_, 1, out.units);
        start = frontMatterClose_ + 1;
    } else {
        // A match from above may reach into the line, and a table is read from its header.
        while (start > 0 && (blocks_[static_cast<std::size_t>(start)].crossed ||
                             isTableInside(blocks_[static_cast<std::size_t>(start)].entering))) {
            --start;
        }
        if (frontMatterClose_ >= 0 && start <= frontMatterClose_) {
            start = frontMatterClose_ + 1;
        }
    }
    core::BlockState state = start < static_cast<int>(blocks_.size())
                                 ? blocks_[static_cast<std::size_t>(start)].entering
                                 : core::BlockState{};
    std::size_t resumeUnit = 0;
    std::uint32_t resumeOffset = 0;
    const auto take = [&](const core::FindScan& scan) {
        for (const core::FindMatch& m : scan.matches) {
            if (static_cast<int>(m.start.line) == block) {
                out.matches.push_back(m);
            }
        }
        resumeUnit = scan.resumeUnit;
        resumeOffset = scan.resumeOffset;
    };
    QTextBlock b = doc->findBlockByNumber(start);
    if (start > block) {
        // The properties block: a line after it is needed only while a match goes on.
        take(core::scanUnits(out.units, 0, 0, query_, b.isValid()));
        if (resumeUnit >= out.units.size()) {
            b = QTextBlock();
        }
    }
    while (b.isValid()) {
        const QTextBlock nextBlock = b.next();
        const std::u16string text = textOf(b);
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        const core::LineRender render = registry_.render(text, core::LineContext{false, state, next});
        ++lookupRenders_;
        builder.addLine(text, static_cast<std::size_t>(b.blockNumber()), render, out.units);
        state = render.blockState;
        take(core::scanUnits(out.units, resumeUnit, resumeOffset, query_, nextBlock.isValid()));
        const bool pending = resumeUnit < out.units.size();
        if (b.blockNumber() >= block && !pending) {
            break;
        }
        if (b.blockNumber() >= block && pending && lineOfUnit(out.units[resumeUnit], b.blockNumber()) > block) {
            break;  // what is pending starts below the line
        }
        b = nextBlock;
    }
    std::stable_sort(out.matches.begin(), out.matches.end(),
                     [](const core::FindMatch& x, const core::FindMatch& y) { return x.start < y.start; });
    return out;
}

void FindController::setCurrent(std::optional<core::FindMatch> match, int ordinal) {
    needCurrent_ = false;
    const bool changed = match.has_value() != current_.has_value() ||
                         (match && current_ && (match->start != current_->start || match->end != current_->end));
    current_ = match;
    currentOrdinal_ = match ? ordinal : -1;
    if (current_) {
        editor_->setFindHold(static_cast<int>(current_->start.line), static_cast<int>(current_->end.line), false);
    } else {
        editor_->setFindHold(-1, -1, false);
    }
    if (changed) {
        announcePending_ = true;
        showCurrent();
    }
    refreshView();
}

// ---- edits -----------------------------------------------------------------------------------

void FindController::onContentsChange(int position, int charsRemoved, int charsAdded) {
    (void)charsRemoved;
    QTextDocument* doc = editor_->document();
    if (!open_ || query_.findsNothing() || blocks_.empty()) {
        blocks_.clear();
        blockCount_ = doc->blockCount();
        return;
    }
    const int newCount = doc->blockCount();
    const int delta = newCount - blockCount_;
    const int first = doc->findBlock(position).blockNumber();
    const int lastNew = std::max(first, doc->findBlock(position + charsAdded).blockNumber());
    const int lastOld = lastNew - delta;
    if (first < 0 || lastOld < first || lastOld >= static_cast<int>(blocks_.size())) {
        startSearch(current_ ? current_->start : anchor_);
        return;
    }
    // The vectors follow the document: the edited lines get fresh entries; the first keeps its
    // entering state, which only the lines above decide.
    const BlockInfo firstInfo = blocks_[static_cast<std::size_t>(first)];
    // Only the difference moves the entries below (a key within a line moves none).
    if (delta > 0) {
        blocks_.insert(blocks_.begin() + lastOld + 1, static_cast<std::size_t>(delta), BlockInfo{});
    } else if (delta < 0) {
        blocks_.erase(blocks_.begin() + lastNew + 1, blocks_.begin() + lastOld + 1);
    }
    std::fill(blocks_.begin() + first, blocks_.begin() + lastNew + 1, BlockInfo{});
    blocks_[static_cast<std::size_t>(first)].entering = firstInfo.entering;
    blocks_[static_cast<std::size_t>(first)].known = firstInfo.known;
    blocks_[static_cast<std::size_t>(first)].crossed = firstInfo.crossed;
    blockCount_ = newCount;
    const auto shift = [&](core::DocPos p) {
        if (static_cast<int>(p.line) > lastOld) {
            p.line = static_cast<std::size_t>(static_cast<int>(p.line) + delta);
        } else if (static_cast<int>(p.line) > lastNew) {
            p.line = static_cast<std::size_t>(lastNew);
        }
        return p;
    };
    if (current_ && static_cast<int>(current_->end.line) < first) {
        // Above the edit: unchanged.
    } else {
        // At or below the edit: the current match stays while it still matches (Firefox).
        anchor_ = shift(current_ ? current_->start : anchor_);
        needCurrent_ = true;
        current_.reset();
        currentOrdinal_ = -1;
    }
    // Restart a little above the edit: a match ending in it may start a word per whitespace run
    // above, and a match from further up may reach into it. A pass after an earlier edit that has
    // not come this far goes on from where it is (with what it has pending).
    const auto shiftLine = [&](int line) { return line > lastOld ? line + delta : (line > lastNew ? lastNew : line); };
    int restart = first;
    const bool chained = pass_.active && pass_.oldScanned > 0;
    if (chained) {
        const int pending = pass_.units.empty() ? pass_.next : pass_.unitBlock.front();
        restart = std::min(restart, shiftLine(pending));
    }
    int words = static_cast<int>(query_.words().size()) - 1;
    while (restart > 0) {
        const BlockInfo& info = blocks_[static_cast<std::size_t>(restart)];
        if (!info.known) {
            restart = 0;
            break;
        }
        if (!info.crossed && !isTableInside(info.entering) && words <= 0) {
            break;
        }
        --restart;
        if (!doc->findBlockByNumber(restart).text().trimmed().isEmpty()) {
            --words;
        }
    }
    if (restart == 0 || (frontMatterClose_ >= 0 && restart <= frontMatterClose_ + 1)) {
        startSearch(anchor_);
        return;
    }
    // Counts from the restart on are counted again; below the edit the earlier ones may hold. An
    // edit while a pass after an earlier edit still runs keeps what that pass may settle on.
    int oldScanned = scanned_ > lastOld ? scanned_ + delta : std::min(scanned_, first);
    bool oldComplete = complete_;
    int settleAfter = lastNew;
    if (chained) {
        oldScanned = shiftLine(pass_.oldScanned);
        oldComplete = pass_.oldComplete;
        settleAfter = std::max(shiftLine(pass_.settleAfter), lastNew);
    }
    complete_ = false;
    sliceTimer_.stop();
    ahead_ = Pass{};
    restartFrom(restart);
    pass_.settleAfter = settleAfter;
    pass_.oldScanned = oldScanned;
    pass_.oldComplete = oldComplete;
    sliceTimer_.start();
}

// ---- what is shown ---------------------------------------------------------------------------

int FindController::matchCount() const {
    const auto to = static_cast<std::size_t>(std::min(scanned_, static_cast<int>(blocks_.size())));
    return std::accumulate(blocks_.begin(), blocks_.begin() + static_cast<std::ptrdiff_t>(to), 0,
                           [](int sum, const BlockInfo& info) { return sum + static_cast<int>(info.count); });
}

int FindController::currentIndex() const {
    if (!complete_ || !current_ || currentOrdinal_ < 0) {
        return 0;
    }
    const auto block = static_cast<std::ptrdiff_t>(current_->start.line);
    return std::accumulate(blocks_.begin(), blocks_.begin() + block, 0,
                           [](int sum, const BlockInfo& info) { return sum + static_cast<int>(info.count); }) +
           currentOrdinal_ + 1;
}

QString FindController::countText() const {
    if (query_.findsNothing()) {
        return {};
    }
    if (!complete_) {
        return tr("%1+ matches").arg(matchCount());
    }
    const int count = matchCount();
    if (count == 0) {
        return tr("No matches");
    }
    return tr("%1 of %2").arg(currentIndex()).arg(count);
}

QList<QTextLayout::FormatRange> FindController::highlightFormats(int blockNumber) const {
    QList<QTextLayout::FormatRange> out;
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return out;
    }
    const int start = block.position();
    const int end = start + block.length();
    for (const QTextEdit::ExtraSelection& s : selections_) {
        const int from = std::max(s.cursor.selectionStart(), start);
        const int to = std::min(s.cursor.selectionEnd(), end);
        if (to > from) {
            QTextLayout::FormatRange r;
            r.start = from - start;
            r.length = to - from;
            r.format = s.format;
            out.append(r);
        }
    }
    return out;
}

core::DocPos FindController::caretPos() const {
    const QTextCursor c = editor_->textCursor();
    return {static_cast<std::size_t>(c.blockNumber()), static_cast<std::uint32_t>(c.positionInBlock())};
}

int FindController::position(core::DocPos p) const {
    const QTextBlock b = editor_->document()->findBlockByNumber(static_cast<int>(p.line));
    if (!b.isValid()) {
        return std::max(0, editor_->document()->characterCount() - 1);
    }
    return b.position() + std::min(static_cast<int>(p.column), b.length() - 1);
}

void FindController::showCurrent() {
    if (!current_) {
        return;
    }
    editor_->showPosition(position(current_->start));
    // A table or the properties block shows its source only after the layout has followed.
    QTimer::singleShot(0, this, [this] {
        if (current_ && open_) {
            editor_->showPosition(position(current_->start));
        }
    });
}

void FindController::scheduleRefresh() {
    if (refreshScheduled_ || !open_) {
        return;
    }
    refreshScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        refreshScheduled_ = false;
        refreshView();
    });
}

void FindController::refreshView() {
    updateHighlights();
    updateEmbeddedMarks();
    updateBar();
    if (complete_ && announcePending_ && current_) {
        announce();
    }
    emit resultsChanged();
}

void FindController::updateBar() {
    bar_->countLabel()->setText(countText());
    bar_->setNoMatch(open_ && complete_ && !query_.findsNothing() && matchCount() == 0);
}

void FindController::updateHighlights() {
    shown_.clear();
    selections_.clear();
    if (!open_) {
        editor_->setExtraSelections({});
        return;
    }
    if (current_) {
        shown_.push_back(*current_);
    }
    if (highlightAll_) {
        const auto [first, last] = editor_->visibleBlockRange();
        for (int b = std::max(0, first); b <= last && b < scanned_; ++b) {
            if (blocks_[static_cast<std::size_t>(b)].count == 0) {
                continue;
            }
            for (const core::FindMatch& m : matchesOfBlock(b).matches) {
                if (!current_ || m.start != current_->start || m.end != current_->end) {
                    shown_.push_back(m);
                }
            }
        }
    }
    const QTextDocument* doc = editor_->document();
    for (std::size_t i = 0; i < shown_.size(); ++i) {
        const core::FindMatch& m = shown_[i];
        const bool isCurrent = i == 0 && current_.has_value();
        const QColor background = toQColor(isCurrent ? style_.currentBackground : style_.matchBackground);
        const QColor foreground = toQColor(isCurrent ? style_.currentText : style_.matchText);
        for (std::size_t line = m.start.line; line <= m.end.line; ++line) {
            const QTextBlock block = doc->findBlockByNumber(static_cast<int>(line));
            if (!block.isValid()) {
                break;
            }
            const int from = line == m.start.line ? static_cast<int>(m.start.column) : 0;
            const int to = line == m.end.line ? static_cast<int>(m.end.column) : block.length() - 1;
            if (to <= from) {
                continue;
            }
            QTextEdit::ExtraSelection whole;
            whole.cursor = QTextCursor(block);
            whole.cursor.setPosition(block.position() + from);
            whole.cursor.setPosition(block.position() + to, QTextCursor::KeepAnchor);
            whole.format.setBackground(background);
            selections_.append(whole);
            // Review O9: the text colour only on what is visible, never on concealed markers.
            std::vector<bool> hidden(static_cast<std::size_t>(to), false);
            for (const core::HiddenRange& h : editor_->styler().renderFor(block).hidden) {
                for (auto c = static_cast<int>(h.span.start); c < static_cast<int>(h.span.end()) && c < to; ++c) {
                    hidden[static_cast<std::size_t>(c)] = true;
                }
            }
            for (int c = from; c < to;) {
                if (hidden[static_cast<std::size_t>(c)]) {
                    ++c;
                    continue;
                }
                int e = c;
                while (e < to && !hidden[static_cast<std::size_t>(e)]) {
                    ++e;
                }
                QTextEdit::ExtraSelection piece;
                piece.cursor = QTextCursor(block);
                piece.cursor.setPosition(block.position() + c);
                piece.cursor.setPosition(block.position() + e, QTextCursor::KeepAnchor);
                piece.format.setForeground(foreground);
                selections_.append(piece);
                c = e;
            }
        }
    }
    editor_->setExtraSelections(selections_);
}

void FindController::updateEmbeddedMarks() {
    // Highlight all in tables shown as cells and in the properties block (review O7).
    std::vector<QPointer<TableModel>> marked;
    std::vector<int> propertyRows;
    if (open_ && highlightAll_) {
        const auto [first, last] = editor_->visibleBlockRange();
        const int from = std::max(0, first - 200);
        for (int b = from; b <= last && b < scanned_; ++b) {
            if (blocks_[static_cast<std::size_t>(b)].count == 0) {
                continue;
            }
            if (frontMatterClose_ >= 0 && b <= frontMatterClose_) {
                auto* view = qobject_cast<PropertiesView*>(editor_->propertiesView());
                if (view == nullptr) {
                    continue;
                }
                for (const core::FindMatch& m : matchesOfBlock(b).matches) {
                    const auto yamlLine = static_cast<std::size_t>(m.start.line) - 1;
                    int row = -1;
                    for (std::size_t r = 0; r < view->rowCount(); ++r) {
                        if (view->property(r).keyLine <= yamlLine) {
                            row = static_cast<int>(r);
                        }
                    }
                    if (row >= 0) {
                        propertyRows.push_back(row);
                    }
                }
                continue;
            }
            QTableView* view = editor_->tableViewAt(b);
            auto* model = view != nullptr ? qobject_cast<TableModel*>(view->model()) : nullptr;
            if (model == nullptr) {
                continue;
            }
            if (std::find(marked.begin(), marked.end(), model) != marked.end()) {
                continue;  // marked from an earlier line of the same table
            }
            std::vector<std::pair<int, int>> cells;
            const int header = model->headerLine().blockNumber();
            const int lastLine = model->lastLine().blockNumber();
            for (int line = header; line <= lastLine; ++line) {
                if (line >= scanned_ || blocks_[static_cast<std::size_t>(line)].count == 0) {
                    continue;
                }
                const int row = model->rowOfBlock(line);
                if (row < 0) {
                    continue;
                }
                for (const core::FindMatch& m : matchesOfBlock(line).matches) {
                    cells.emplace_back(row, model->columnAt(row, static_cast<int>(m.start.column)));
                }
            }
            model->setFindCells(std::move(cells), toQColor(style_.matchBackground));
            marked.emplace_back(model);
        }
    }
    for (const QPointer<TableModel>& old : markedTables_) {
        if (old && std::find(marked.begin(), marked.end(), old) == marked.end()) {
            old->setFindCells({}, {});
        }
    }
    markedTables_ = std::move(marked);
    if (auto* view = qobject_cast<PropertiesView*>(editor_->propertiesView())) {
        view->setFindRows(std::move(propertyRows), toQColor(style_.matchBackground));
    }
}

void FindController::announce() {
    announcePending_ = false;
    if (!current_) {
        return;
    }
    const BlockMatches m = matchesOfBlock(static_cast<int>(current_->start.line));
    QString context;
    for (const core::FindMatch& match : m.matches) {
        if (match.start == current_->start && match.end == current_->end) {
            context = toQString(core::matchContext(m.units, match));
            break;
        }
    }
    lastAnnouncement_ = context.isEmpty() ? countText() : countText() + QStringLiteral(": ") + context;
    QAccessibleAnnouncementEvent event(bar_->countLabel(), lastAnnouncement_);
    QAccessible::updateAccessibility(&event);
}

}  // namespace hitokoto::ui
