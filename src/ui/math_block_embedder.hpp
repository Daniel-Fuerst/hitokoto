// SPDX-License-Identifier: GPL-3.0-or-later
// S047: display math blocks shown typeset. A closed `$$` block off the caret and the selection
// reserves the panel's height on its opening line and hides its other lines (BlockLayout); the
// editor paints the panel and the formula there. No widget: the source is edited as text.
#pragma once

#include "block_layout.hpp"

#include <QObject>
#include <QTextCursor>

#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hitokoto::ui {

class BlockStyler;
class EditorFormats;

class MathBlockEmbedder final : public QObject, public BlockLayout::Claimant {
    Q_OBJECT
public:
    /// `visibleBlocks` gives the first and last block on screen.
    MathBlockEmbedder(QTextDocument* document, BlockStyler* styler, BlockLayout* layout,
                      std::function<std::pair<int, int>()> visibleBlocks, QObject* parent = nullptr);
    ~MathBlockEmbedder() override;
    MathBlockEmbedder(const MathBlockEmbedder&) = delete;
    MathBlockEmbedder& operator=(const MathBlockEmbedder&) = delete;

    [[nodiscard]] bool claims(const QTextBlock& block) const override;

    /// A block shown typeset: its opening and last line (block numbers at the last refresh) and
    /// the formula's source.
    struct Typeset {
        int open = 0;
        int last = 0;
        std::u16string source;
    };
    /// A closed block the engine could not lay out: it shows its source, with the reason.
    struct Failed {
        int open = 0;
        int last = 0;
        std::string reason;
    };
    [[nodiscard]] const std::vector<Typeset>& typeset() const noexcept { return typeset_; }
    [[nodiscard]] const std::vector<Failed>& failed() const noexcept { return failed_; }
    /// The panel's height for a formula: its ascent and descent, the padding and the margins.
    [[nodiscard]] double panelHeight(const std::u16string& source) const;

    void setFormats(const EditorFormats* formats);

public slots:
    /// Finds the blocks on and near the screen and claims or releases their lines.
    void refresh();
    /// refresh() once the event loop runs, however often asked before.
    void scheduleRefresh();
    /// The document's blocks went without notice (a new document): forget them, touch nothing.
    void forgetAll();

private:
    struct Found {
        std::vector<Typeset> typeset;
        std::vector<Failed> failed;
    };
    [[nodiscard]] Found find(int from, int to) const;
    void release(const QTextBlock& block);
    /// Brings claimedNumbers_ in line with where the claimed cursors are now: an edit moves block
    /// numbers before refresh() runs again (S044 code review O5).
    void syncNumbers() const;
    /// What claimedNumbers_ was made from: the document's shape and its edits at that time.
    struct NumbersKey {
        int blockCount = -1;
        int revision = -1;
        quint64 edits = 0;
        friend bool operator==(const NumbersKey&, const NumbersKey&) = default;
    };
    [[nodiscard]] NumbersKey numbersKey() const;

    QTextDocument* document_;
    BlockStyler* styler_;
    BlockLayout* layout_;
    std::function<std::pair<int, int>()> visibleBlocks_;
    const EditorFormats* formats_ = nullptr;
    std::vector<Typeset> typeset_;
    std::vector<Failed> failed_;
    std::vector<QTextCursor> claimed_;  ///< follow their blocks through edits elsewhere
    mutable std::unordered_set<int> claimedNumbers_;
    mutable NumbersKey numbersKey_;
    quint64 edits_ = 0;  ///< contents changes seen
    bool refreshPending_ = false;
    bool refreshing_ = false;
};

}  // namespace hitokoto::ui
