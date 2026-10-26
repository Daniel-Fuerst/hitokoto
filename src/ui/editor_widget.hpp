// SPDX-License-Identifier: GPL-3.0-or-later
// The editing surface: a QPlainTextEdit whose document is the only copy of the text
// (ADR 0002). Rendering comes from the core through BlockStyler; this class adds the
// cursor-aware reveal, bullet drawing and click handling.
#pragma once

#include "block_styler.hpp"
#include "hitokoto/core/theme.hpp"

#include <QPlainTextEdit>
#include <QPointF>

#include <optional>

class QPainter;
class QTextBlock;

namespace hitokoto::ui {

class EditorWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(const core::ElementRegistry& registry, QWidget* parent = nullptr);

    void applyTheme(const core::Theme& theme);

    [[nodiscard]] BlockStyler& styler() noexcept { return *styler_; }
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
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void updateReveal();
    void updateMargins();
    [[nodiscard]] double availableTableWidth(double basePointSize) const;
    [[nodiscard]] QRectF addRowHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF addColumnHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteRowHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteColumnHoverRect(int blockNumber, int column) const;
    void updateHoverPos(std::optional<QPointF> pos);
    bool refreshHoverState();
    [[nodiscard]] int tableHitPosition(int blockNumber, std::size_t colIdx, QPointF viewportPos) const;
    void paintCodeBlockPanels();
    void paintTableBackgrounds(QPainter& painter, const TableMetrics& metrics);
    void paintTableGridAndControls(QPainter& painter, const TableMetrics& metrics);
    void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                              QPointF origin, const core::CodeStyle& style);
    void paintDecorations();
    void placeCaretInCell(int blockNumber, std::size_t column, bool atEnd = false);
    void appendRowToTable(int blockNumber, std::size_t targetColumn = 0);
    void appendColumnToTable(int blockNumber);
    void deleteTableRow(int rowBlockNumber, std::size_t targetColumn = 0, bool preferPreviousRow = false);
    void deleteTableColumn(int blockNumber, std::size_t column);
    void exitTableBelow(int blockNumber, bool removeEmptyLastRow = false);
    void exitTableAbove(int blockNumber);

    BlockStyler* styler_ = nullptr;
    double padding_ = 0;
    double maxWidth_ = 0;
    quint64 editCount_ = 0;
    std::optional<QPointF> hoverPos_;
    int hoveredAddRowBlock_ = -1;
    int hoveredAddColBlock_ = -1;
    int hoveredDeleteRowBlock_ = -1;
    int hoveredDeleteColTable_ = -1;
    int hoveredDeleteColIndex_ = -1;
    bool overTableButton_ = false;
};

}  // namespace hitokoto::ui
