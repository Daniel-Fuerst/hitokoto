// SPDX-License-Identifier: GPL-3.0-or-later
// Encapsulates Markdown table geometry, hover detection, mouse interaction, cell navigation,
// and table mutations.
#pragma once

#include "block_styler.hpp"

#include <QPointF>
#include <QRectF>

#include <cstddef>
#include <optional>

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;

namespace hitokoto::ui {

class EditorWidget;

class TableController {
public:
    explicit TableController(EditorWidget* editor, BlockStyler* styler);

    // Bounding rectangles and controls
    [[nodiscard]] QRectF tableRect(int blockNumber) const;
    [[nodiscard]] QRectF cellRect(int blockNumber, int column) const;
    [[nodiscard]] QRectF addRowButtonRect(int blockNumber) const;
    [[nodiscard]] QRectF addColumnButtonRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteRowButtonRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteColumnButtonRect(int blockNumber, int column) const;
    [[nodiscard]] bool isAddRowButtonVisible(int blockNumber) const;
    [[nodiscard]] bool isAddColumnButtonVisible(int blockNumber) const;
    [[nodiscard]] bool isDeleteRowButtonVisible(int blockNumber) const;
    [[nodiscard]] bool isDeleteColumnButtonVisible(int blockNumber, int column) const;

    [[nodiscard]] int hoveredDeleteRowBlock() const noexcept { return hoveredDeleteRowBlock_; }
    [[nodiscard]] int hoveredDeleteColTable() const noexcept { return hoveredDeleteColTable_; }
    [[nodiscard]] int hoveredDeleteColIndex() const noexcept { return hoveredDeleteColIndex_; }

    // Hover state management
    void updateHoverPos(std::optional<QPointF> pos);
    bool refreshHoverState();

    // Event handling (returns true if event was handled by table logic)
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    void handleMouseRelease(QMouseEvent* event);
    bool handleContextMenu(QContextMenuEvent* event);
    bool handleKeyPress(QKeyEvent* event);
    void handlePostNavigation(QKeyEvent* event);

    // Caret and table mutation operations
    void placeCaretInCell(int blockNumber, std::size_t column, bool atEnd = false);
    void appendRowToTable(int blockNumber, std::size_t targetColumn = 0);
    void appendColumnToTable(int blockNumber);
    void deleteTableRow(int rowBlockNumber, std::size_t targetColumn = 0, bool preferPreviousRow = false);
    void deleteTableColumn(int blockNumber, std::size_t column);
    void exitTableBelow(int blockNumber, bool removeEmptyLastRow = false);
    void exitTableAbove(int blockNumber);

    [[nodiscard]] int tableHitPosition(int blockNumber, std::size_t colIdx, QPointF viewportPos) const;

private:
    [[nodiscard]] QRectF addRowHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF addColumnHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteRowHoverRect(int blockNumber) const;
    [[nodiscard]] QRectF deleteColumnHoverRect(int blockNumber, int column) const;

    EditorWidget* editor_;
    BlockStyler* styler_;
    std::optional<QPointF> hoverPos_;
    int hoveredAddRowBlock_ = -1;
    int hoveredAddColBlock_ = -1;
    int hoveredDeleteRowBlock_ = -1;
    int hoveredDeleteColTable_ = -1;
    int hoveredDeleteColIndex_ = -1;

    // Mouse drag selection inside a cell
    bool cellDragActive_ = false;
    int cellDragAnchor_ = -1;
    int dragBlock_ = -1;
    std::size_t dragCol_ = 0;
};

}  // namespace hitokoto::ui
