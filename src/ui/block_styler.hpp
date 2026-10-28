// SPDX-License-Identifier: GPL-3.0-or-later
// Applies core::LineRender to the editor's blocks, incrementally: edited lines and lines
// entering or leaving the revealed range are styled at once, everything else lazily
// (visible blocks first, the rest in short idle slices). No whole-document pass ever runs on
// the UI thread in one go.
#pragma once

#include "editor_formats.hpp"
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/reveal.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <utility>

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
    /// closing fence of the block that holds the last one.
    void setRevealed(core::LineRange range);
    [[nodiscard]] core::LineRange revealed() const noexcept { return revealed_; }
    [[nodiscard]] bool isRevealed(int blockNumber) const noexcept {
        return revealed_.contains(blockNumber) || blockNumber == revealedOpen_ || blockNumber == revealedClose_;
    }

    /// Style the blocks on screen that are not styled yet (call after scrolling/resizing).
    void styleVisible();
    /// Forget all styling (e.g. after a theme change) and restyle lazily.
    void invalidateAll();

    /// True while this class changes formats (document signals caused by it can be ignored).
    [[nodiscard]] bool isBusy() const noexcept { return busy_; }
    /// True when every block is styled for the current reveal state.
    [[nodiscard]] bool isComplete() const noexcept { return !idle_.isActive() && !fenceSearch_.isActive(); }
    /// Number of blocks styled so far (instrumentation for tests and benchmarks).
    [[nodiscard]] quint64 styledCount() const noexcept { return styledCount_; }
    /// Number of lines rendered so far (instrumentation for tests, S021).
    [[nodiscard]] quint64 renderCount() const noexcept { return renderCount_; }

    /// How a block is shown right now (used for drawing decorations and hit testing).
    [[nodiscard]] core::LineRender renderFor(const QTextBlock& block);
    [[nodiscard]] core::BlockState blockStateBefore(const QTextBlock& block);

signals:
    void completed();

private:
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void styleRange(int first, int last);
    void styleBlock(const QTextBlock& block);
    void applyCodeBlockMargin(const QTextBlock& block, const core::LineRender& render);
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

    QTextDocument* document_;
    const core::ElementRegistry& registry_;
    std::function<std::pair<int, int>()> visibleBlocks_;
    EditorFormats formats_;
    core::LineRange revealed_{0, 0};
    int revealedOpen_ = -1;   ///< fence revealed besides revealed_ (S021), -1 = none
    int revealedClose_ = -1;  ///< likewise
    // The extent of the code block last looked up. A caret step inside a block, and typing in it,
    // reuse it, so their cost does not depend on the size of the block (review O2).
    CodeExtent extent_;
    bool extentValid_ = false;
    // A search for the fences that does not finish at once (a large block whose states are not
    // cached yet) goes on in idle slices; the fences it finds are revealed then.
    QTimer fenceSearch_;
    core::BlockState searchInside_;
    CodeExtent search_;
    int searchBack_ = -1;     ///< next line to look at above; -1 = the opening fence is known
    int searchForward_ = -1;  ///< next line to look at below; -1 = the closing fence is known
    int blockCount_ = 1;      ///< block count after the last change seen
    QTimer idle_;
    int idleNext_ = 0;
    bool busy_ = false;
    quint64 styledCount_ = 0;
    quint64 renderCount_ = 0;
};

}  // namespace hitokoto::ui
