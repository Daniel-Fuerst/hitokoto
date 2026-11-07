// SPDX-License-Identifier: GPL-3.0-or-later
// S028: drives the search for the find bar: the units of the concealed view in slices on the UI
// thread (never blocking), the current match, the count, the highlights, and the source view of
// tables and properties that hold the current match (review O8). It never changes the document.
#pragma once

#include "hitokoto/core/find.hpp"
#include "hitokoto/core/theme.hpp"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>

#include <climits>
#include <functional>
#include <optional>
#include <vector>

namespace hitokoto::ui {

class EditorWidget;
class FindBar;
class TableModel;

class FindController : public QObject {
    Q_OBJECT
public:
    FindController(EditorWidget* editor, FindBar* bar, const core::ElementRegistry& registry,
                   QObject* parent = nullptr);
    ~FindController() override;
    FindController(const FindController&) = delete;
    FindController& operator=(const FindController&) = delete;

    void applyTheme(const core::Theme& theme);

    /// Ctrl+F: opens the bar (or brings the focus back to its field) and selects the search text.
    void open();
    /// Escape: closes the bar; with a current match, selects it in the editor (decision 9).
    void close();
    void next();
    void previous();

    [[nodiscard]] bool isOpen() const noexcept { return open_; }
    /// Whether the whole document has been searched for the current text.
    [[nodiscard]] bool isComplete() const noexcept { return complete_; }
    /// The current match, if any.
    [[nodiscard]] std::optional<core::FindMatch> currentMatch() const { return current_; }
    /// 1-based index of the current match once the count is complete, else 0.
    [[nodiscard]] int currentIndex() const;
    /// Matches found so far (all of them once complete).
    [[nodiscard]] int matchCount() const;
    /// The text of the count: "n of m", "m+ matches" or "No matches" (empty for no search text).
    [[nodiscard]] QString countText() const;
    [[nodiscard]] bool matchCase() const noexcept { return matchCase_; }
    [[nodiscard]] bool highlightAll() const noexcept { return highlightAll_; }
    /// The matches whose highlights are on screen (only the current one unless "Highlight all").
    [[nodiscard]] std::vector<core::FindMatch> visibleMatches() const { return shown_; }
    /// The highlight formats on a block (tests: their text colour must never cover concealed text).
    [[nodiscard]] QList<QTextLayout::FormatRange> highlightFormats(int blockNumber) const;
    /// The accessible announcement made for the current match last (review O11).
    [[nodiscard]] QString lastAnnouncement() const { return lastAnnouncement_; }
    /// Lines rendered to look up the positions of matches (instrumentation, code review O8).
    [[nodiscard]] quint64 lookupRenders() const noexcept { return lookupRenders_; }
    /// Times the front matter was parsed for the search (instrumentation, code review O12).
    [[nodiscard]] quint64 propertyParses() const noexcept { return propertyParses_; }

signals:
    /// The matches, the current match or the count changed.
    void resultsChanged();

private:
    /// What is known about one block (line) of the document for the current search.
    struct BlockInfo {
        core::BlockState entering;  ///< the state entering the line
        std::uint32_t count = 0;    ///< matches that start on the line
        bool crossed = false;       ///< a match starting above reaches into the line
        bool known = false;         ///< `entering` and `count` are from a scan of this search
    };

    /// The matches that start on one block and the units they were found in (for the context).
    struct BlockMatches {
        std::vector<core::FindMatch> matches;
        std::vector<core::FindUnit> units;
        bool front = false;  ///< in the properties block: the units are front_.units
    };

    /// Front matter at the start of the document (S023/S024), found and parsed once per change of
    /// it, not per search (code review O6, O12).
    struct FrontMatter {
        bool known = false;                      ///< the fields below describe the document as it is
        bool option = false;                     ///< frontMatterAsProperties() when it was read
        bool opens = false;                      ///< line 0 opens front matter
        int close = -1;                          ///< its closing line, -1 = not closed
        bool asProperties = false;               ///< closed, valid YAML and the option on: searched as properties
        core::BlockState textState;              ///< otherwise: the state the first line is entered with
        std::vector<core::BlockState> entering;  ///< as properties: lines 0 to close + 1
        std::vector<core::FindUnit> units;       ///< as properties: the units of the block
        /// As properties, per unit: the row and the part of the PropertiesView it shows in.
        std::vector<std::pair<int, int>> parts;
    };

    void startSearch(core::DocPos anchor);
    void restartFrom(int block);
    /// Runs the pass for at most `nanoseconds`; true when it is complete.
    bool work(qint64 nanoseconds);
    void slice();
    /// Front matter at the start of the document: walks until it closes (or the document ends).
    bool frontMatterStep(const QElapsedTimer& timer, qint64 nanoseconds);
    void scanBuffer(bool more);
    void finalizeUpTo(int block);
    void finishPass();
    /// Review O6: a second pass from the caret forward finds the current match before the pass
    /// from the start gets there (with the styler's states, once it has styled everything).
    void startAhead();
    /// Runs the pass ahead for at most `nanoseconds`; true while it goes on.
    bool aheadWork(qint64 nanoseconds);
    /// Front matter walked to its end: keeps what was found (`close` -1: not closed).
    void keepFrontMatter(bool opens, int close);
    /// Starts the pass at line 0 with the front matter known.
    void startAfterFrontMatter();
    /// Whether line `line`, entered with `entering`, has a non-whitespace character in its
    /// concealed text (code review O4: only such a line holds a word of a match).
    [[nodiscard]] bool hasWord(int line, const core::BlockState& entering) const;
    /// The line a pass must start at to find every match that ends on line `line` or later and
    /// starts at or below `lowest`: up over one line with a word per word of the search text but
    /// the last (code review O4, O7). -1 if that takes more than a bounded walk.
    /// `atLeast`: lines to go up in any case; `holds`: a match from above reaches into the line.
    [[nodiscard]] int wordWalk(int line, int lowest, int atLeast, const std::function<bool(int)>& wordAt,
                               const std::function<bool(int)>& holds) const;
    /// The column count of the table that line `line` (entered inside it) belongs to: its header's.
    [[nodiscard]] std::size_t tableColumnsAt(int line, const std::function<core::BlockState(int)>& entering) const;
    /// The matches in the properties block (scanned once per search).
    [[nodiscard]] const std::vector<core::FindMatch>& frontMatches() const;

    [[nodiscard]] BlockMatches matchesOfBlock(int block) const;
    [[nodiscard]] bool isTableInside(const core::BlockState& state) const noexcept;
    /// Whether `text` holds a character of the search text (folded unless "Match case").
    [[nodiscard]] bool mayMatch(std::u16string_view text) const;
    void setCurrent(std::optional<core::FindMatch> match, int ordinal);
    void resolveCurrentFromCounts();
    [[nodiscard]] int firstBlockWithMatches(int from, int to) const;
    [[nodiscard]] int lastBlockWithMatches(int from, int to) const;
    void step(int direction);
    /// One step if the matches it needs are known, working the pass for at most `nanoseconds`
    /// to get them; false if it has to wait for the pass.
    bool tryStep(int direction, qint64 nanoseconds);
    /// Takes the steps asked for before the pass got far enough (code review O9).
    void takePendingSteps();

    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void scheduleRefresh();
    void refreshView();
    void updateHighlights();
    void updateEmbeddedMarks();
    void updateBar();
    void announce();
    void showCurrent();
    [[nodiscard]] core::DocPos caretPos() const;
    [[nodiscard]] int position(core::DocPos p) const;

    EditorWidget* editor_;
    FindBar* bar_;
    const core::ElementRegistry& registry_;
    core::FindStyle style_;

    bool open_ = false;
    bool matchCase_ = false;
    bool highlightAll_ = false;
    QString text_;
    core::FindQuery query_;
    std::vector<char32_t> queryChars_;  ///< the search text's characters, sorted

    std::vector<BlockInfo> blocks_;
    int scanned_ = 0;  ///< blocks below this have their final count
    bool complete_ = true;
    int blockCount_ = 0;  ///< the document's block count the vectors follow

    // The pass over the document.
    struct Pass {
        bool active = false;
        bool frontMatterPhase = false;
        int next = 0;  ///< next block to read
        core::BlockState state;
        std::optional<core::FindUnitBuilder> builder;
        std::vector<core::FindUnit> units;
        std::vector<int> unitBlock;  ///< the block each unit belongs to
        std::size_t resumeUnit = 0;
        std::uint32_t resumeOffset = 0;
        // Front matter at the start (S024): the walk to its closing line.
        core::BlockState frontState;
        int frontWalk = 0;
        // Settling after an edit: blocks from `settleAfter` on may still hold the earlier results.
        int settleAfter = INT_MAX;
        int oldScanned = 0;
        bool oldComplete = false;
        bool lastWasTable = false;
    } pass_;
    Pass ahead_;                               ///< from the caret forward, until the current match is found
    std::vector<core::FindMatch> aheadFound_;  ///< matches the pass ahead found so far
    int frontMatterClose_ = -1;                ///< closing line of front matter searched as properties, else -1
    FrontMatter front_;
    mutable std::optional<std::vector<core::FindMatch>> frontMatches_;
    QTimer sliceTimer_;

    std::optional<core::FindMatch> current_;
    int currentOrdinal_ = -1;
    bool needCurrent_ = false;
    core::DocPos anchor_;
    int pendingSteps_ = 0;  ///< steps asked for before the pass got far enough (+ next, - previous)
    bool announcePending_ = false;
    QString lastAnnouncement_;

    std::vector<core::FindMatch> shown_;
    QList<QTextEdit::ExtraSelection> selections_;
    std::vector<QPointer<TableModel>> markedTables_;
    bool refreshScheduled_ = false;
    bool stepsScheduled_ = false;
    mutable quint64 lookupRenders_ = 0;
    mutable quint64 propertyParses_ = 0;
};

}  // namespace hitokoto::ui
