// SPDX-License-Identifier: GPL-3.0-or-later
// Applies core::LineRender to the editor's blocks, incrementally: edited lines and lines
// entering or leaving the revealed range are styled at once, everything else lazily
// (visible blocks first, the rest in short idle slices). No whole-document pass ever runs on
// the UI thread in one go.
#pragma once

#include "editor_formats.hpp"
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/reveal.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QTextDocument;
class QTextBlock;

namespace hitokoto::ui {

class BlockStyler : public QObject {
    Q_OBJECT
public:
    /// `visibleBlocks` returns the first and last block number currently on screen.
    BlockStyler(QTextDocument* document, const core::ElementRegistry& registry,
                std::function<std::pair<int, int>()> visibleBlocks, QObject* parent = nullptr);

    void setFormats(EditorFormats formats);
    [[nodiscard]] const EditorFormats& formats() const noexcept { return formats_; }

    /// Reveals the lines of `range` (caret or selection) and, for a range in a code block, the
    /// block's fences too (S021): the opening fence of the block that holds the first line and the
    /// closing fence of the block that holds the last one. A range on a line of a setext heading
    /// reveals its underline too (S023): every heading the range touches has its underline in the
    /// range or holds the range's last line.
    void setRevealed(core::LineRange range);
    [[nodiscard]] core::LineRange revealed() const noexcept { return revealed_; }
    [[nodiscard]] bool isRevealed(int blockNumber) const noexcept {
        return revealed_.contains(blockNumber) || blockNumber == revealedOpen_ || blockNumber == revealedClose_ ||
               blockNumber == revealedUnderline_;
    }

    /// Style the blocks on screen that are not styled yet (call after scrolling/resizing).
    void styleVisible();
    /// Forget all styling (e.g. after a theme change) and restyle lazily.
    void invalidateAll();

    /// True while this class changes formats (document signals caused by it can be ignored).
    [[nodiscard]] bool isBusy() const noexcept { return busy_; }
    /// True when every block is styled for the current reveal state.
    [[nodiscard]] bool isComplete() const noexcept {
        return !idle_.isActive() && !fenceSearch_.isActive() && !paragraphSearch_.isActive() &&
               !frontMatterSearch_.isActive() && !underlineSearchTimer_.isActive();
    }
    /// Number of blocks styled so far (instrumentation for tests and benchmarks).
    [[nodiscard]] quint64 styledCount() const noexcept { return styledCount_; }
    /// Number of lines rendered so far (instrumentation for tests, S021).
    [[nodiscard]] quint64 renderCount() const noexcept { return renderCount_; }
    /// Number of lines whose setext level was passed up a paragraph so far (instrumentation for
    /// tests, S023 review O2).
    [[nodiscard]] quint64 levelSteps() const noexcept { return levelSteps_; }

    /// How a block is shown right now (used for drawing decorations and hit testing).
    [[nodiscard]] core::LineRender renderFor(const QTextBlock& block);
    [[nodiscard]] core::BlockState blockStateBefore(const QTextBlock& block);

signals:
    void completed();

private:
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    /// `renderFor`, and the line's link to the paragraph above when `link` is given (S023). Only
    /// styling walks a paragraph to find its setext level; other callers (painting, tables) get the
    /// level the line is shown with.
    [[nodiscard]] core::LineRender renderLine(const QTextBlock& block, core::ParagraphLink* link, bool walk);
    void styleRange(int first, int last);
    void styleBlock(const QTextBlock& block);
    /// Space above and below a block (ADR 0003): a code block panel's margin (S021), a thematic
    /// break's margin (S023), the same concealed and revealed.
    void applyBlockSpacing(const QTextBlock& block, const core::LineRender& render);
    /// The line after `block` when the table element needs it as lookahead, else empty.
    [[nodiscard]] static QString nextLineFor(const QTextBlock& block, core::BlockState state, const QString& text);
    [[nodiscard]] bool isStyled(const QTextBlock& block) const;
    [[nodiscard]] core::BlockState blockStateAfter(const QTextBlock& block);
    /// Opening and closing fence of the code block that holds `block` (S021); open -1 if `block` is
    /// not in a code block, close -1 if the block runs to the end of the document.
    struct CodeExtent {
        int open = -1;
        int close = -1;
    };
    [[nodiscard]] CodeExtent codeExtentOf(const QTextBlock& block);
    /// Walks on towards the fences of the block being searched for, at most `nanoseconds`; true
    /// when both are found.
    bool continueFenceSearch(qint64 nanoseconds);
    void fenceSearchSlice();
    void forgetExtent();
    /// Keeps the cached extent across an edit inside the block, else forgets it.
    void updateExtentAfterEdit(int position, int charsAdded);
    void scheduleFrom(int blockNumber);
    void idleSlice();

    // ---- S023 (ADR 0006): setext headings from below, front matter at the document start -------
    /// The setext level of the paragraph `block` belongs to, given the state after it; not
    /// resolved while the walk over the paragraph goes on in idle slices (then the last known one).
    struct Level {
        std::uint8_t level = 0;
        bool resolved = true;
    };
    [[nodiscard]] Level setextLevel(const QTextBlock& block, core::BlockState after, bool walk = true);
    /// A forward walk over a paragraph, from the line whose level is wanted to the first line that
    /// does not continue the paragraph.
    struct ParagraphSearch {
        core::BlockState inside;       ///< the state after every line of the paragraph
        int from = -1;                 ///< the line the walk started from
        int next = -1;                 ///< the next line to look at
        bool previousChanged = false;  ///< the walk changed the cached state of the line before `next`
        int lastAnswered = -1;         ///< the last line shown with the level it had while the walk went on
        int end = -1;                  ///< once found: the first line after the paragraph
        std::uint8_t level = 0;        ///< once found: the paragraph's level
        int applyNext = -1;            ///< next line to give the level to
        int restyle = -1;              ///< first line to restyle once the level is given
        /// A line at or below `from` with the paragraph's state waits for this walk; one past the
        /// paragraph's end is restyled when the walk ends.
        [[nodiscard]] bool covers(int number, core::BlockState after) const noexcept {
            return number >= from && after == inside;
        }
    };
    /// Walks on for at most `nanoseconds`; true when the paragraph's level is found and applied.
    bool continueParagraphSearch(ParagraphSearch& search, qint64 nanoseconds);
    /// One line of a walk, entered with the paragraph's state: its level if the walk ends there.
    std::optional<std::uint8_t> paragraphStep(QTextBlock& block, ParagraphSearch& search);
    /// Marks a styled block for restyling in idle slices.
    void restyleLater(QTextBlock& block);
    /// `level` for the lines from `first` to before `end` (invalid: the end of the document), and
    /// up the paragraph above.
    void giveLevel(const QTextBlock& first, const QTextBlock& end, std::uint8_t level);
    void paragraphSearchSlice();
    /// Gives the level found to the lines from `search.from` to before `search.end`, at most
    /// `nanoseconds`; restyles those that showed another one and passes it up to the lines above
    /// that continue into them. True when done.
    bool applyParagraphLevel(ParagraphSearch& search, const QElapsedTimer& timer, qint64 nanoseconds);
    /// `block` and the lines above it that continue into it get `level`; changed lines are restyled.
    void propagateLevelUp(const QTextBlock& block, std::uint8_t level);
    /// Work left over from a key (review O2): forgetting the level of the lines further up a
    /// paragraph (`end` < 0, from `line` upwards), or restyling the lines `line` to before `end`.
    struct LevelJob {
        int line = 0;
        int end = -1;
    };
    /// Goes on with `job` for at most `nanoseconds`; true when done.
    bool continueLevelJob(LevelJob& job, qint64 nanoseconds);
    /// After styling `block`: how it relates to the paragraph above, and the level that follows for
    /// the line above.
    void updateParagraphAbove(const QTextBlock& block, const core::LineRender& render, core::ParagraphLink link);
    /// The setext underline of the heading that holds `block`, or -1 (also while it is searched for
    /// in idle slices; setRevealed() is called again when it is found).
    [[nodiscard]] int underlineOf(const QTextBlock& block);
    /// Walks on towards the underline, at most `nanoseconds`; true when the walk has ended.
    bool continueUnderlineSearch(qint64 nanoseconds);
    void underlineSearchSlice();
    /// After an edit of lines `first` to `last`: keeps the underline found last if the edit leaves
    /// its heading's lines continuing the paragraph.
    void updateUnderlineAfterEdit(int first, int last, int delta);
    /// After an edit: searches that walked over the edited lines start again.
    void restartParagraphSearchesAfterEdit(int firstEdited, int delta);

    /// Confirms whether the document starts with closed front matter (the state the first line is
    /// entered with), within the synchronous budget and then in idle slices.
    void checkFrontMatter();
    bool continueFrontMatterSearch(qint64 nanoseconds);
    void frontMatterSlice();
    /// After an edit of lines `first` to `last`: whether the front matter must be confirmed again.
    void frontMatterAfterEdit(int first, int last);

    QTextDocument* document_;
    const core::ElementRegistry& registry_;
    std::function<std::pair<int, int>()> visibleBlocks_;
    EditorFormats formats_;
    core::LineRange revealed_{0, 0};
    int revealedOpen_ = -1;       ///< fence revealed besides revealed_ (S021), -1 = none
    int revealedClose_ = -1;      ///< likewise
    int revealedUnderline_ = -1;  ///< setext underline revealed besides revealed_ (S023), -1 = none
    // The extent of the code block last looked up. A caret step inside a block, and typing in it,
    // reuse it, so their cost does not depend on the size of the block (review O2).
    // The code block searched last, and while a search runs, how far it got. A caret step inside
    // that block, and typing in it, reuse it, so their cost does not depend on the size of the
    // block (review O2). A search that does not finish at once (a large block, or one whose states
    // are not cached yet) goes on in idle slices; the fences it finds are revealed then.
    struct FenceSearch {
        core::BlockState inside;  ///< the state every line of the block carries
        int from = -1;            ///< a line known to be inside (where the search started)
        int open = -1;            ///< opening fence, once known
        int close = -1;           ///< closing fence once known; -1 after the search: runs to the end
        int back = -1;            ///< next line to look at above; -1 = the opening fence is known
        int forward = -1;         ///< next line to look at below; -1 = the end is known
        bool valid = false;
        [[nodiscard]] bool contains(int number) const noexcept;
        [[nodiscard]] CodeExtent known() const noexcept;
    };
    FenceSearch search_;
    QTimer fenceSearch_;
    std::vector<ParagraphSearch> paragraphSearches_;  ///< walks going on in idle slices, oldest first
    std::vector<LevelJob> levelJobs_;                 ///< done before the walks, oldest first
    QTimer paragraphSearch_;
    /// The heading whose underline was found last (S023): a caret step inside it costs nothing.
    struct UnderlineCache {
        core::BlockState inside;  ///< the state after every line of the heading
        int first = -1;           ///< a line of the heading
        int underline = -1;       ///< its underline
    };
    UnderlineCache underline_;
    struct UnderlineSearch {
        core::BlockState inside;
        int from = -1;  ///< the line the search started from, -1 = none
        int next = -1;
    };
    UnderlineSearch underlineSearch_;
    QTimer underlineSearchTimer_;
    core::BlockState startState_;  ///< the state the first line is entered with (front matter)
    int frontMatterClose_ = -1;    ///< closing line of the front matter confirmed last, -1 = none
    struct FrontMatterSearch {
        core::BlockState state;  ///< state after the line before `next`
        int next = -1;           ///< the next line to look at, -1 = no search
    };
    FrontMatterSearch frontMatter_;
    QTimer frontMatterSearch_;
    int blockCount_ = 1;  ///< block count after the last change seen
    QTimer idle_;
    int idleNext_ = 0;
    bool busy_ = false;
    quint64 styledCount_ = 0;
    quint64 renderCount_ = 0;
    quint64 levelSteps_ = 0;
};

}  // namespace hitokoto::ui
