// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. One pass over the document's lines in slices (at most 6 ms in idle
// time, a bounded part on a key) builds the units of the concealed view with the core
// (FindUnitBuilder) and scans them (scanUnits); per line it keeps the entering block state and the
// number of matches that start there, positions only for the lines on screen and the current
// match. An edit restarts the pass a little above the edited line and stops it where the
// state and the results meet the earlier ones again (review O4). Front matter is found and parsed
// once per change of it (code review O6, O12).
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
#include <climits>
#include <functional>
#include <numeric>

namespace hitokoto::ui {
namespace {

constexpr qint64 kSliceNanoseconds = 6'000'000;  ///< idle slices, as the styler's
constexpr qint64 kKeyNanoseconds = 8'000'000;    ///< on a key: typing in the field, next and previous (O9)
constexpr int kWalkLimit = 2000;                 ///< lines a walk up goes before it gives up

std::u16string textOf(const QTextBlock& block) {
    const QString text = block.text();
    return {reinterpret_cast<const char16_t*>(text.utf16()), static_cast<std::size_t>(text.size())};
}

bool sameMatch(const core::FindMatch& a, const core::FindMatch& b) {
    return a.start == b.start && a.end == b.end && a.inDestination == b.inDestination;
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
        view->setFindMarks({}, {});
    }
    editor_->setFocus(Qt::OtherFocusReason);
    if (match) {
        // Decision 9: the caret moves only now, onto the match (for a link destination: its text).
        QTextCursor cursor(editor_->document());
        cursor.setPosition(position(match->start));
        cursor.setPosition(position(match->end), QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);
        // Review O8: a table keeps its source until the caret and the anchor have left it (the
        // embedder holds the whole table, code review O10).
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
    if (pendingSteps_ != 0) {
        pendingSteps_ += direction;  // after the steps still waiting
        takePendingSteps();
        return;
    }
    if (!tryStep(direction, kKeyNanoseconds)) {
        pendingSteps_ += direction;  // code review O9: taken when the pass gets far enough
    }
}

bool FindController::tryStep(int direction, qint64 nanoseconds) {
    QElapsedTimer timer;
    timer.start();
    const auto left = [&] { return std::max<qint64>(0, nanoseconds - timer.nsecsElapsed()); };
    if (current_ && currentOrdinal_ < 0 && !complete_ && nanoseconds > 0) {
        (void)work(left());  // the pass from the start has not reached the current match yet
    }
    if (!current_ || currentOrdinal_ < 0) {
        return complete_ && !current_;  // without any match there is nothing to wait for
    }
    const auto block = static_cast<int>(current_->start.line);
    const BlockMatches here = matchesOfBlock(block);
    const int ordinal = currentOrdinal_ + direction;
    if (ordinal >= 0 && ordinal < static_cast<int>(here.matches.size())) {
        setCurrent(here.matches[static_cast<std::size_t>(ordinal)], ordinal);
        return true;
    }
    if (direction > 0) {
        int b = firstBlockWithMatches(block + 1, scanned_);
        if (b < 0 && !complete_ && nanoseconds > 0) {
            (void)work(left());
            b = firstBlockWithMatches(block + 1, scanned_);
        }
        if (b < 0) {
            if (!complete_) {
                return false;
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
                return false;
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
    return true;
}

void FindController::takePendingSteps() {
    while (pendingSteps_ != 0) {
        const int direction = pendingSteps_ > 0 ? 1 : -1;
        if (!tryStep(direction, 0)) {
            return;
        }
        pendingSteps_ -= direction;
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
    pendingSteps_ = 0;
    needCurrent_ = true;
    announcePending_ = true;
    anchor_ = anchor;
    blockCount_ = editor_->document()->blockCount();
    blocks_.assign(static_cast<std::size_t>(blockCount_), BlockInfo{});
    scanned_ = 0;
    frontMatches_.reset();
    if (front_.known && front_.option != editor_->frontMatterAsProperties()) {
        front_ = FrontMatter{};
    }
    frontMatterClose_ = front_.known && front_.asProperties ? front_.close : -1;
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
    const auto anchorLine = static_cast<int>(anchor_.line);
    BlockStyler& styler = editor_->styler();
    if (anchorLine < kFarEnough || !styler.isComplete()) {
        return;
    }
    const int lowest = styler.frontMatterClose() >= 0 ? styler.frontMatterClose() + 1 : 0;
    if (anchorLine <= lowest) {
        return;
    }
    const QTextDocument* doc = editor_->document();
    const auto entering = [&](int line) { return styler.blockStateBefore(doc->findBlockByNumber(line)); };
    // Code review O7: a match that ends on the anchor's line may start above it.
    const int block = wordWalk(
        anchorLine, lowest, 0, [&](int line) { return hasWord(line, entering(line)); }, [](int) { return false; });
    if (block < 0) {
        return;
    }
    ahead_.active = true;
    ahead_.builder.emplace(registry_);
    ahead_.next = block;
    ahead_.state = entering(block);
    if (isTableInside(ahead_.state)) {
        if (ahead_.builder) {  // always set while the pass runs; explicit for clang-tidy
            ahead_.builder->continueTable(tableColumnsAt(block, entering));
        }
        ahead_.lastWasTable = true;
    }
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
        const std::u16string nextNext = next.find(u'|') != std::u16string::npos && nextBlock.next().isValid()
                                            ? textOf(nextBlock.next())
                                            : std::u16string{};
        const core::LineRender render =
            registry_.render(text, core::LineContext{false, ahead_.state, next, 0, false, nextNext});
        ahead_.state = render.blockState;
        ahead_.next = b + 1;
        const bool skip = ahead_.units.empty() && !render.table && !ahead_.lastWasTable && !mayMatch(text);
        ahead_.lastWasTable = render.table;
        if (!skip) {
            if (ahead_.builder) {  // always set while the pass runs; explicit for clang-tidy
                ahead_.builder->addLine(text, static_cast<std::size_t>(b), render, ahead_.units);
            }
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
    scanned_ = std::min(scanned_, block);
    pass_ = Pass{};
    pass_.active = true;
    pass_.builder.emplace(registry_);
    pass_.next = block;
    if (block == 0) {
        if (front_.known) {
            startAfterFrontMatter();
        } else {
            frontMatterClose_ = -1;
            frontMatches_.reset();
            pass_.frontState = registry_.documentStart();
            pass_.frontMatterPhase = true;
            pass_.frontWalk = 0;
        }
        return;
    }
    pass_.state = blocks_[static_cast<std::size_t>(block)].entering;
    if (isTableInside(pass_.state)) {
        // A row of a table: on with the table's column count, not from its header (code review O8).
        if (pass_.builder) {  // always set while the pass runs; explicit for clang-tidy
            pass_.builder->continueTable(
                tableColumnsAt(block, [this](int line) { return blocks_[static_cast<std::size_t>(line)].entering; }));
        }
        pass_.lastWasTable = true;
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
        if (pass_.frontState.element != start.element) {
            // Line 0 opens no front matter, or line i closes it.
            pass_.frontMatterPhase = false;
            keepFrontMatter(i > 0, i > 0 ? i : -1);
            startAfterFrontMatter();
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
    keepFrontMatter(true, -1);
    startAfterFrontMatter();
    return true;
}

void FindController::keepFrontMatter(bool opens, int close) {
    const QTextDocument* doc = editor_->document();
    front_ = FrontMatter{};
    front_.known = true;
    front_.option = editor_->frontMatterAsProperties();
    front_.opens = opens;
    front_.close = close;
    frontMatches_.reset();
    if (close < 0) {
        front_.textState = core::BlockState{};
        return;
    }
    // Closed: read and parsed once, until it changes (code review O12; as PropertiesEmbedder::read).
    std::vector<std::u16string> yaml;
    yaml.reserve(static_cast<std::size_t>(close));
    for (QTextBlock y = doc->firstBlock().next(); y.isValid() && y.blockNumber() < close; y = y.next()) {
        yaml.push_back(textOf(y));
    }
    ++propertyParses_;
    const core::Properties parsed = core::parseProperties(yaml);
    if (!front_.option || parsed.error) {
        front_.textState = registry_.documentStart();
        return;
    }
    front_.asProperties = true;
    front_.units = core::propertyUnits(yaml, 1);
    // The entering states of the block's lines and of the line after it, for the pass and edits.
    core::BlockState state = registry_.documentStart();
    for (QTextBlock line = doc->firstBlock(); line.isValid() && line.blockNumber() <= close; line = line.next()) {
        front_.entering.push_back(state);
        const QTextBlock after = line.next();
        state = registry_.blockStateAfter(textOf(line), state, after.isValid() ? textOf(after) : std::u16string{});
    }
    front_.entering.push_back(state);
    // Code review O3: the row and the part (key, value or chip) each unit shows in. Units come in
    // document order, a row's key first; list and tags items in the order of their chips.
    std::size_t row = 0;
    int unitsOfRow = 0;
    std::size_t item = 0;
    for (const core::FindUnit& unit : front_.units) {
        const std::size_t yamlLine = unit.segments.empty() ? 0 : unit.segments.front().source.line - 1;
        while (row + 1 < parsed.rows.size() && parsed.rows[row + 1].keyLine <= yamlLine) {
            ++row;
            unitsOfRow = 0;
            item = 0;
        }
        int part = PropertiesView::FindMark::kValue;
        if (row >= parsed.rows.size()) {
            front_.parts.emplace_back(-1, part);
            continue;
        }
        const core::Property& property = parsed.rows[row];
        if (unitsOfRow == 0 && unit.text == property.key) {
            part = PropertiesView::FindMark::kKey;
        } else if (property.type == core::PropertyType::List || property.type == core::PropertyType::Tags) {
            while (item < property.items.size() && property.items[item] != unit.text) {
                ++item;
            }
            if (item < property.items.size()) {
                part = static_cast<int>(item++);
            }
        }
        ++unitsOfRow;
        front_.parts.emplace_back(static_cast<int>(row), part);
    }
}

void FindController::startAfterFrontMatter() {
    if (!front_.asProperties) {
        frontMatterClose_ = -1;
        pass_.state = front_.textState;
        pass_.next = 0;
        return;
    }
    const int close = front_.close;
    frontMatterClose_ = close;
    for (int line = 0; line <= close && line < blockCount_; ++line) {
        BlockInfo& info = blocks_[static_cast<std::size_t>(line)];
        info.entering = front_.entering[static_cast<std::size_t>(line)];
        info.count = 0;
        info.crossed = false;
        info.known = true;
    }
    const std::size_t first = pass_.units.size();
    if (pass_.builder) {  // always set while the pass runs; explicit for clang-tidy
        pass_.builder->addPropertyUnits(front_.units, pass_.units);
    }
    for (std::size_t u = first; u < pass_.units.size(); ++u) {
        pass_.unitBlock.push_back(lineOfUnit(pass_.units[u], 1));
    }
    pass_.state = front_.entering[static_cast<std::size_t>(close) + 1];
    pass_.next = close + 1;
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
            if (pass_.oldComplete) {
                const int oldScanned = pass_.oldScanned;
                finalizeUpTo(b);
                finalizeUpTo(oldScanned);  // the earlier results from here on hold
                pass_.active = false;
                finishPass();
                return true;
            }
            // The earlier pass had not finished, and where it was is not kept: read on.
            pass_.settleAfter = INT_MAX;
            pass_.oldScanned = 0;
        }
        const QTextBlock nextBlock = block.next();
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        const std::u16string nextNext = next.find(u'|') != std::u16string::npos && nextBlock.next().isValid()
                                            ? textOf(nextBlock.next())
                                            : std::u16string{};
        const core::LineRender render =
            registry_.render(text, core::LineContext{false, pass_.state, next, 0, false, nextNext});
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
            if (pass_.builder) {  // always set while the pass runs; explicit for clang-tidy
                pass_.builder->addLine(text, static_cast<std::size_t>(b), render, pass_.units);
            }
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
    int from = scanned_;
    scanned_ = to;
    if (current_ && currentOrdinal_ < 0 && static_cast<int>(current_->start.line) < to) {
        // Found by the pass ahead: its place among the matches of its line.
        const BlockMatches m = matchesOfBlock(static_cast<int>(current_->start.line));
        for (std::size_t i = 0; i < m.matches.size(); ++i) {
            if (sameMatch(m.matches[i], *current_)) {
                currentOrdinal_ = static_cast<int>(i);
            }
        }
        if (currentOrdinal_ < 0) {
            // Code review O7: not a match of the document (a match from above took its text).
            current_.reset();
            needCurrent_ = true;
            from = std::min(from, static_cast<int>(anchor_.line));
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
    if (pendingSteps_ != 0 && current_ && currentOrdinal_ >= 0 && !stepsScheduled_) {
        // Steps asked for before the pass got this far: taken outside the pass.
        stepsScheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            stepsScheduled_ = false;
            takePendingSteps();
        });
    }
}

void FindController::finishPass() {
    complete_ = true;
    sliceTimer_.stop();
    if (needCurrent_) {
        resolveCurrentFromCounts();
    }
    takePendingSteps();
    pendingSteps_ = 0;
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
    if (frontMatterClose_ >= 0 && block <= frontMatterClose_) {
        out.front = true;  // code review O12: scanned once per search, never parsed again
        for (const core::FindMatch& m : frontMatches()) {
            if (static_cast<int>(m.start.line) == block) {
                out.matches.push_back(m);
            }
        }
        return out;
    }
    const QTextDocument* doc = editor_->document();
    core::FindUnitBuilder builder(registry_);
    // A match from above may reach into the line (never out of the properties block: hard edges).
    const int lowest = frontMatterClose_ >= 0 ? frontMatterClose_ + 1 : 0;
    int start = block;
    while (start > lowest && blocks_[static_cast<std::size_t>(start)].crossed) {
        --start;
    }
    core::BlockState state = blocks_[static_cast<std::size_t>(start)].entering;
    if (isTableInside(state)) {
        // A row: on with the table's column count, not from its header (code review O8).
        builder.continueTable(
            tableColumnsAt(start, [this](int line) { return blocks_[static_cast<std::size_t>(line)].entering; }));
    }
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
    for (QTextBlock b = doc->findBlockByNumber(start); b.isValid();) {
        const QTextBlock nextBlock = b.next();
        const std::u16string text = textOf(b);
        const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
        const std::u16string nextNext = next.find(u'|') != std::u16string::npos && nextBlock.next().isValid()
                                            ? textOf(nextBlock.next())
                                            : std::u16string{};
        const core::LineRender render =
            registry_.render(text, core::LineContext{false, state, next, 0, false, nextNext});
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

const std::vector<core::FindMatch>& FindController::frontMatches() const {
    if (!frontMatches_) {
        // The properties block ends at a hard edge: its matches are those of its units alone.
        std::vector<core::FindMatch> matches = core::scanUnits(front_.units, 0, 0, query_, false).matches;
        std::stable_sort(matches.begin(), matches.end(),
                         [](const core::FindMatch& x, const core::FindMatch& y) { return x.start < y.start; });
        frontMatches_ = std::move(matches);
    }
    return *frontMatches_;
}

bool FindController::hasWord(int line, const core::BlockState& entering) const {
    const QTextBlock b = editor_->document()->findBlockByNumber(line);
    if (!b.isValid()) {
        return false;
    }
    const std::u16string text = textOf(b);
    const QTextBlock nextBlock = b.next();
    const std::u16string next = nextBlock.isValid() ? textOf(nextBlock) : std::u16string{};
    const std::u16string nextNext = next.find(u'|') != std::u16string::npos && nextBlock.next().isValid()
                                        ? textOf(nextBlock.next())
                                        : std::u16string{};
    const core::LineRender render =
        registry_.render(text, core::LineContext{false, entering, next, 0, false, nextNext});
    ++lookupRenders_;
    std::vector<core::FindUnit> units;
    if (render.table) {
        if (render.tableDelimiter) {
            return false;
        }
        // Only the first cell: it counts with any column count of the header.
        units = core::tableRowUnits(registry_, text, static_cast<std::size_t>(line), 1);
    } else {
        units = core::lineUnits(text, static_cast<std::size_t>(line), render);
    }
    return std::any_of(units.begin(), units.end(), [](const core::FindUnit& unit) {
        return unit.kind != core::FindUnitKind::Destination &&
               std::any_of(unit.text.begin(), unit.text.end(),
                           [](char16_t c) { return (c >= 0xD800 && c < 0xE000) || !core::isUnicodeWhitespace(c); });
    });
}

int FindController::wordWalk(int line, int lowest, int atLeast, const std::function<bool(int)>& wordAt,
                             const std::function<bool(int)>& holds) const {
    int words = static_cast<int>(query_.words().size()) - 1;
    int walked = 0;
    while (line > lowest && (walked < atLeast || words > 0 || holds(line))) {
        if (++walked > kWalkLimit) {
            return -1;
        }
        --line;
        if (words > 0 && wordAt(line)) {
            --words;
        }
    }
    return line;
}

std::size_t FindController::tableColumnsAt(int line, const std::function<core::BlockState(int)>& entering) const {
    int header = line;
    while (header > 0 && isTableInside(entering(header))) {
        --header;
    }
    const QTextBlock b = editor_->document()->findBlockByNumber(header);
    if (!b.isValid()) {
        return 0;
    }
    const QTextBlock nextBlock = b.next();
    const core::LineRender render = registry_.render(
        textOf(b),
        core::LineContext{false, entering(header), nextBlock.isValid() ? textOf(nextBlock) : std::u16string{}});
    ++lookupRenders_;
    return render.cells.size();
}

void FindController::setCurrent(std::optional<core::FindMatch> match, int ordinal) {
    needCurrent_ = false;
    const bool changed =
        match.has_value() != current_.has_value() || (match && current_ && !sameMatch(*match, *current_));
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
        front_ = FrontMatter{};  // read again with the next search
        return;
    }
    const int newCount = doc->blockCount();
    const int delta = newCount - blockCount_;
    const int first = doc->findBlock(position).blockNumber();
    const int lastNew = std::max(first, doc->findBlock(position + charsAdded).blockNumber());
    const int lastOld = lastNew - delta;
    // Code review O6: the front matter changes with line 0, with a line of the block or the line
    // after it, and, while it is not closed, with any line that could close it.
    bool frontChanged = !front_.known || (first == 0 && (front_.opens || doc->firstBlock().text().startsWith(u"---")));
    if (!frontChanged && front_.opens) {
        if (front_.close >= 0) {
            frontChanged = first <= front_.close + 1;
        } else {
            for (int line = first; line <= lastNew && !frontChanged; ++line) {
                frontChanged = doc->findBlockByNumber(line).text().startsWith(u"---");
            }
        }
    }
    if (frontChanged) {
        front_ = FrontMatter{};
    }
    if (first < 0 || lastOld < first || lastOld >= static_cast<int>(blocks_.size()) || frontChanged ||
        pass_.frontMatterPhase) {
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
    const auto shiftLine = [&](int line) {
        if (line == INT_MAX) {
            return line;
        }
        return line > lastOld ? line + delta : (line > lastNew ? lastNew : line);
    };
    const auto shift = [&](core::DocPos p) {
        p.line = static_cast<std::size_t>(shiftLine(static_cast<int>(p.line)));
        return p;
    };
    // The current match stays above the lines read again; at or below them it stays while it
    // still matches (Firefox).
    const auto keepCurrentAbove = [&](int line) {
        if (current_ && static_cast<int>(current_->end.line) < line) {
            return;
        }
        if (current_) {
            // Until it is found again, a table or the properties block holding it keeps its source
            // where it now is (code review).
            editor_->setFindHold(shiftLine(static_cast<int>(current_->start.line)),
                                 shiftLine(static_cast<int>(current_->end.line)), false);
        }
        anchor_ = shift(current_ ? current_->start : anchor_);
        needCurrent_ = true;
        current_.reset();
        currentOrdinal_ = -1;
    };
    if (pass_.active && first > pass_.next) {
        // Below the line the pass reads next: it reads the edited lines when it gets there. It may
        // not settle on the earlier results before them.
        if (pass_.oldScanned > 0) {
            pass_.oldScanned = shiftLine(pass_.oldScanned);
            pass_.settleAfter = std::max(shiftLine(pass_.settleAfter), lastNew);
        }
        keepCurrentAbove(first - 1);
        return;
    }
    // Restart above the edit: at least on the line above (code review O5: its units depend on the
    // edited line, a table header or a setext heading), up a line with a word per word of the
    // search text but the last (O4), and up over lines a match from above reaches into.
    const int lowest = frontMatterClose_ >= 0 ? frontMatterClose_ + 1 : 0;
    int restart = wordWalk(
        first, lowest, 1,
        [this](int line) {
            const BlockInfo& info = blocks_[static_cast<std::size_t>(line)];
            return info.known && hasWord(line, info.entering);
        },
        [this](int line) {
            const BlockInfo& info = blocks_[static_cast<std::size_t>(line)];
            return info.crossed || !info.known;
        });
    if (restart < 0) {
        restart = lowest;
    }
    // Counts from the restart on are counted again; below the edit the earlier ones may hold. An
    // edit while a pass after an earlier edit still runs keeps what that pass may settle on.
    int oldScanned = scanned_ > lastOld ? scanned_ + delta : std::min(scanned_, first);
    bool oldComplete = complete_;
    int settleAfter = lastNew;
    if (pass_.active) {
        // A pass still running: what it has not scanned yet is counted again from where it is.
        const int pending = pass_.units.empty() ? pass_.next : pass_.unitBlock.front();
        restart = std::min(restart, shiftLine(pending));
        if (pass_.oldScanned > 0) {
            oldScanned = shiftLine(pass_.oldScanned);
            oldComplete = pass_.oldComplete;
            settleAfter = std::max(shiftLine(pass_.settleAfter), lastNew);
        }
    }
    keepCurrentAbove(restart);
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
    if (complete_ && announcePending_) {
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
    // Highlight all in tables shown as cells and in the properties block (review O7): only what is
    // on screen (code review O8), the key, value or chip that holds a match (code review O3).
    std::vector<QPointer<TableModel>> marked;
    std::vector<PropertiesView::FindMark> propertyMarks;
    auto* propertiesView = qobject_cast<PropertiesView*>(editor_->propertiesView());
    if (open_ && highlightAll_) {
        const auto [first, last] = editor_->visibleBlockRange();
        if (propertiesView != nullptr && frontMatterClose_ >= 0 && first <= frontMatterClose_) {
            for (const core::FindMatch& m : frontMatches()) {
                for (std::size_t u = m.unit; u <= m.endUnit && u < front_.parts.size(); ++u) {
                    const auto [row, part] = front_.parts[u];
                    if (row >= 0) {
                        propertyMarks.push_back({row, part});
                    }
                }
            }
        }
        for (int b = std::max({0, first, frontMatterClose_ + 1}); b <= last && b < scanned_; ++b) {
            if (blocks_[static_cast<std::size_t>(b)].count == 0) {
                continue;
            }
            QTableView* view = editor_->tableViewAt(b);
            auto* model = view != nullptr ? qobject_cast<TableModel*>(view->model()) : nullptr;
            if (model == nullptr || std::find(marked.begin(), marked.end(), model) != marked.end()) {
                continue;  // no table view, or marked from an earlier line of the same table
            }
            std::vector<std::pair<int, int>> cells;
            const int from = std::max(model->headerLine().blockNumber(), first);
            const int to = std::min(model->lastLine().blockNumber(), last);
            for (int line = from; line <= to && line < scanned_; ++line) {
                if (blocks_[static_cast<std::size_t>(line)].count == 0) {
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
    if (propertiesView != nullptr) {
        propertiesView->setFindMarks(std::move(propertyMarks), toQColor(style_.matchBackground));
    }
}

void FindController::announce() {
    announcePending_ = false;
    if (query_.findsNothing()) {
        return;
    }
    QString context;
    if (current_) {
        const BlockMatches m = matchesOfBlock(static_cast<int>(current_->start.line));
        const std::vector<core::FindUnit>& units = m.front ? front_.units : m.units;
        for (const core::FindMatch& match : m.matches) {
            if (sameMatch(match, *current_)) {
                context = toQString(core::matchContext(units, match));
                break;
            }
        }
    }
    // Without a match: "No matches" (code review).
    lastAnnouncement_ = context.isEmpty() ? countText() : countText() + QStringLiteral(": ") + context;
    QAccessibleAnnouncementEvent event(bar_->countLabel(), lastAnnouncement_);
    QAccessible::updateAccessibility(&event);
}

}  // namespace hitokoto::ui
