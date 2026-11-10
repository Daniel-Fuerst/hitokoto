// SPDX-License-Identifier: GPL-3.0-or-later
// The editing surface: a QTextEdit whose document is the only copy of the text (ADR 0002) and is
// laid out by BlockLayout (ADR 0003). Rendering comes from the core through BlockStyler; this
// class adds the cursor-aware reveal, bullet drawing and click handling.
#pragma once

#include "block_layout.hpp"
#include "block_styler.hpp"
#include "hitokoto/core/theme.hpp"

#include <QPointF>
#include <QTextBlock>
#include <QTextEdit>

#include <memory>
#include <optional>

class QMimeData;
class QPainter;

class QTableView;

namespace hitokoto::ui {

class FloatingToolbar;
class PropertiesEmbedder;
class SmoothScroller;
class TableEmbedder;

class EditorWidget : public QTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(const core::ElementRegistry& registry, QWidget* parent = nullptr);
    ~EditorWidget() override;

    void applyTheme(const core::Theme& theme);

    [[nodiscard]] BlockStyler& styler() noexcept { return *styler_; }
    /// The table view showing the table whose lines include `blockNumber`; null if the line is not
    /// in a table or the table shows its source (S018).
    [[nodiscard]] QTableView* tableViewAt(int blockNumber) const;
    /// Whether the table whose lines include `blockNumber` shows its Markdown lines (S018).
    [[nodiscard]] bool isTableSourceShown(int blockNumber) const;
    /// The document layout (ADR 0003).
    [[nodiscard]] BlockLayout* blockLayout() const noexcept { return layout_; }
    /// Counts real text edits (not formatting); used to decide whether a save is current.
    [[nodiscard]] quint64 editCount() const noexcept { return editCount_; }
    /// First and last block number on screen.
    [[nodiscard]] std::pair<int, int> visibleBlockRange() const;
    /// The language badge painted on `block` (an opening fence while concealed), else empty (S021).
    [[nodiscard]] QString codeBlockBadge(const QTextBlock& block);
    /// S024: the properties block shown in place of the front matter; null while there is none, the
    /// YAML shows, or the toggle is off.
    [[nodiscard]] QWidget* propertiesView() const;
    /// S024: the badge after the first line of front matter that is not shown as properties
    /// ("Invalid YAML (line n): reason"), else empty.
    [[nodiscard]] QString frontMatterBadge() const;
    /// S024 review O2: front matter as a properties block (on) or as plain text, as in S023 (off).
    void setFrontMatterAsProperties(bool on);
    [[nodiscard]] bool frontMatterAsProperties() const noexcept { return frontMatterAsProperties_; }
    /// S028 review O8: a table or the front matter with lines in [first, last] shows its source
    /// while the find bar's current match is there; `untilCaretLeaves` (after Escape, tables): until
    /// the caret and the anchor have left it. first < 0 releases it.
    void setFindHold(int first, int last, bool untilCaretLeaves);
    /// S028: scrolls so that document position `position` is on screen, without moving the caret.
    void showPosition(int position);
    /// After opening a file (`loaded`: all of it is in): the caret below closed front matter, else at
    /// the start of the document (S024).
    void placeCaretAfterOpening(bool loaded);
    /// The anchor URL at viewport point `pos`, or empty (S022).
    [[nodiscard]] QString anchorAt(const QPoint& pos) const;
    /// Merged character format at viewport point `pos` (S022).
    [[nodiscard]] QTextCharFormat formatAt(const QPoint& pos) const;
    /// S026: the floating toolbar on the right-hand side of the editor.
    [[nodiscard]] FloatingToolbar* floatingToolbar() const noexcept { return toolbar_; }
    /// S026: inserts a 2-column × 1-body-row table and focuses its first header cell.
    void insertTable();
    /// S026: wraps the selection in single backticks, or inserts `` `` `` with the caret inside.
    void insertInlineCode();
    /// S027: whether smooth scrolling on mouse wheel events is enabled.
    [[nodiscard]] bool isSmoothScrollingEnabled() const noexcept { return smoothScrolling_; }
    /// S027: enable or disable smooth scrolling on mouse wheel events.
    void setSmoothScrolling(bool on);
    /// S027: the vertical smooth scroller.
    [[nodiscard]] SmoothScroller* smoothScroller() const noexcept { return verticalScroller_; }
    /// S031: pastes the clipboard's plain text without converting tables (Paste as Plain Text).
    void pasteAsPlainText();
    /// S031: whether the clipboard holds plain text to paste.
    [[nodiscard]] static bool canPastePlainText();
    /// S031 review (code) O9: the format that marks a copy made in hitokoto, whose paste skips the
    /// HTML (it holds Markdown source, never a table).
    [[nodiscard]] static QString ownCopyFormat();

signals:
    void painted();

protected:
    bool viewportEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    friend class TableEmbedder;
    friend class PropertiesEmbedder;

    // Block geometry as QPlainTextEdit offered it: document coordinates, and the offset that maps
    // them into the viewport.
    [[nodiscard]] QRectF blockBoundingGeometry(const QTextBlock& block) const;
    [[nodiscard]] QPointF contentOffset() const;
    [[nodiscard]] QTextBlock firstVisibleBlock() const;
    /// The line at the top of the viewport and how far into it the view starts.
    struct ScrollAnchor {
        QTextBlock block;
        qreal fraction = 0;
    };
    [[nodiscard]] ScrollAnchor captureAnchor() const;
    void restoreAnchor(const ScrollAnchor& anchor);
    void keepContentStill(qreal top, qreal oldHeight, qreal newHeight);
    void applyPendingShift();

    void updateReveal();
    void updateMargins();
    void positionToolbar();
    [[nodiscard]] int availableTableWidth() const;
    void paintCodeBlockPanels();
    void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                              QPointF origin, const core::CodeStyle& style);
    void paintDecorations();
    /// S023: the line of a concealed thematic break (`rect`: the block in viewport coordinates).
    void paintThematicBreak(QPainter& painter, const QTextBlock& block, const QRectF& rect);
    bool handleBulletKey(QKeyEvent* event);
    bool handleBulletEnter(QTextCursor& cursor);
    bool handleBulletTab(QTextCursor& cursor, bool outdent);
    /// S031: pastes the tables of `source` as Markdown tables, if the paste converts; returns
    /// false if it stays plain text (no table that counts, or the caret is in a code block, front
    /// matter or a table's lines).
    bool pasteTables(const QMimeData* source);

    BlockLayout* layout_ = nullptr;
    qreal pendingShift_ = 0;  ///< height change above the viewport not yet scrolled away
    bool applyingShift_ = false;
    BlockStyler* styler_ = nullptr;
    std::unique_ptr<TableEmbedder> tables_;
    std::unique_ptr<PropertiesEmbedder> properties_;
    FloatingToolbar* toolbar_ = nullptr;
    SmoothScroller* verticalScroller_ = nullptr;
    bool smoothScrolling_ = true;
    bool frontMatterAsProperties_ = true;
    double padding_ = 0;
    double maxWidth_ = 0;
    quint64 editCount_ = 0;
    bool plainPaste_ = false;  ///< S031: inside a drop or Paste as Plain Text: no conversion
};

}  // namespace hitokoto::ui
