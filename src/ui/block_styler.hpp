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

    void setRevealed(core::LineRange range);
    [[nodiscard]] core::LineRange revealed() const noexcept { return revealed_; }
    [[nodiscard]] bool isRevealed(int blockNumber) const noexcept { return revealed_.contains(blockNumber); }

    /// Style the blocks on screen that are not styled yet (call after scrolling/resizing).
    void styleVisible();
    /// Forget all styling (e.g. after a theme change) and restyle lazily.
    void invalidateAll();

    /// True while this class changes formats (document signals caused by it can be ignored).
    [[nodiscard]] bool isBusy() const noexcept { return busy_; }
    /// True when every block is styled for the current reveal state.
    [[nodiscard]] bool isComplete() const noexcept { return !idle_.isActive(); }
    /// Number of blocks styled so far (instrumentation for tests and benchmarks).
    [[nodiscard]] quint64 styledCount() const noexcept { return styledCount_; }

    /// How a block is shown right now (used for drawing decorations and hit testing).
    [[nodiscard]] core::LineRender renderFor(const QTextBlock& block) const;

signals:
    void completed();

private:
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void styleRange(int first, int last);
    void styleBlock(const QTextBlock& block);
    [[nodiscard]] bool isStyled(const QTextBlock& block) const;
    void scheduleFrom(int blockNumber);
    void idleSlice();

    QTextDocument* document_;
    const core::ElementRegistry& registry_;
    std::function<std::pair<int, int>()> visibleBlocks_;
    EditorFormats formats_;
    core::LineRange revealed_{0, 0};
    QTimer idle_;
    int idleNext_ = 0;
    bool busy_ = false;
    quint64 styledCount_ = 0;
};

}  // namespace hitokoto::ui
