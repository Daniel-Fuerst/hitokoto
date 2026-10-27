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

class QPainter;

namespace hitokoto::ui {

class TableController;

class EditorWidget : public QTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(const core::ElementRegistry& registry, QWidget* parent = nullptr);
    ~EditorWidget() override;

    void applyTheme(const core::Theme& theme);

    [[nodiscard]] BlockStyler& styler() noexcept { return *styler_; }
    /// The document layout (ADR 0003).
    [[nodiscard]] BlockLayout* blockLayout() const noexcept { return layout_; }
    [[nodiscard]] TableController* tableController() const noexcept { return tableController_.get(); }
    /// Counts real text edits (not formatting); used to decide whether a save is current.
    [[nodiscard]] quint64 editCount() const noexcept { return editCount_; }
    /// First and last block number on screen.
    [[nodiscard]] std::pair<int, int> visibleBlockRange() const;

    /// Outer bounding rectangle (viewport coordinates) of the table containing `blockNumber`.
    [[nodiscard]] QRectF tableRect(int blockNumber) const;
    /// Bounding rectangle (viewport coordinates) of `column` on table row `blockNumber`.
    [[nodiscard]] QRectF cellRect(int blockNumber, int column) const;
    /// Graphical '+' button rectangle below the table containing `blockNumber`.
    [[nodiscard]] QRectF addRowButtonRect(int blockNumber) const;
    /// Graphical '+' button rectangle to the right of the table containing `blockNumber`.
    [[nodiscard]] QRectF addColumnButtonRect(int blockNumber) const;
    /// Graphical '-' button rectangle to the left of table body row `blockNumber`.
    [[nodiscard]] QRectF deleteRowButtonRect(int blockNumber) const;
    /// Graphical '-' button rectangle above `column` of the table containing `blockNumber`.
    [[nodiscard]] QRectF deleteColumnButtonRect(int blockNumber, int column) const;
    /// True when the graphical '+' add-row button for `blockNumber`'s table is revealed by hover.
    [[nodiscard]] bool isAddRowButtonVisible(int blockNumber) const;
    /// True when the graphical '+' add-column button for `blockNumber`'s table is revealed by hover.
    [[nodiscard]] bool isAddColumnButtonVisible(int blockNumber) const;
    /// True when the graphical '-' delete-row button for body row `blockNumber` is revealed by hover.
    [[nodiscard]] bool isDeleteRowButtonVisible(int blockNumber) const;
    /// True when the graphical '-' delete-column button for `column` on `blockNumber`'s table is revealed by hover.
    [[nodiscard]] bool isDeleteColumnButtonVisible(int blockNumber, int column) const;

signals:
    void painted();

protected:
    bool viewportEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    friend class TableController;

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
    [[nodiscard]] double availableTableWidth(double basePointSize) const;
    void paintCodeBlockPanels();
    void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                              QPointF origin, const core::CodeStyle& style);
    void paintDecorations();

    BlockLayout* layout_ = nullptr;
    qreal pendingShift_ = 0;
    bool applyingShift_ = false;  ///< height change above the viewport not yet scrolled away
    BlockStyler* styler_ = nullptr;
    std::unique_ptr<TableController> tableController_;
    double padding_ = 0;
    double maxWidth_ = 0;
    quint64 editCount_ = 0;
};

}  // namespace hitokoto::ui
