// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_controller.hpp"

#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "qt_bridge.hpp"

#include <QAction>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>

namespace hitokoto::ui {

namespace {

std::size_t findCellIndex(const core::LineRender& render, std::uint32_t columnInLine) {
    if (render.cells.empty()) {
        return 0;
    }
    for (std::size_t i = 0; i < render.cells.size(); ++i) {
        if (columnInLine >= render.cells[i].content.start && columnInLine <= render.cells[i].content.end()) {
            return i;
        }
    }
    for (std::size_t i = 0; i < render.cells.size(); ++i) {
        if (columnInLine >= render.cells[i].raw.start && columnInLine <= render.cells[i].raw.end()) {
            return i;
        }
    }
    for (std::size_t i = 0; i < render.cells.size(); ++i) {
        if (columnInLine < render.cells[i].content.start) {
            return i;
        }
    }
    return render.cells.size() - 1;
}

std::pair<int, int> cellSegmentRange(const core::LineRender& render, std::size_t colIdx, int lineLen) {
    const int segStart = colIdx == 0 ? 0 : static_cast<int>(render.cells[colIdx - 1].raw.end());
    const int segEnd = colIdx + 1 < render.cells.size() ? static_cast<int>(render.cells[colIdx].raw.end()) : lineLen;
    return {segStart, segEnd};
}

bool isEmptyTableBodyRow(const core::LineRender& render) {
    if (!render.table || render.tableHeader || render.tableDelimiter || render.cells.empty()) {
        return false;
    }
    return std::all_of(render.cells.begin(), render.cells.end(),
                       [](const core::TableCell& c) { return c.content.length == 0; });
}

bool isEmptyTableColumn(const QTextDocument* doc, BlockStyler* styler, const TableMetrics& metrics,
                        std::size_t colIdx) {
    if (metrics.firstBlock < 0 || metrics.columnWidths.size() <= 1 || colIdx >= metrics.columnWidths.size()) {
        return false;
    }
    for (int bNum = metrics.firstBlock; bNum <= metrics.lastBlock; ++bNum) {
        const QTextBlock b = doc->findBlockByNumber(bNum);
        if (!b.isValid()) {
            continue;
        }
        const auto r = styler->renderFor(b);
        if (r.tableDelimiter) {
            continue;
        }
        if (colIdx < r.cells.size() && r.cells[colIdx].content.length > 0) {
            return false;
        }
    }
    return true;
}

std::pair<qreal, qreal> blockTextSpanY(const QRectF& blockGeom, const QTextBlock& block) {
    const QTextLayout* layout = block.layout();
    if (!layout) {
        return {blockGeom.top(), blockGeom.bottom()};
    }
    const qreal top = blockGeom.top() + layout->position().y();
    return {top, top + layout->boundingRect().height()};
}

}  // namespace

TableController::TableController(EditorWidget* editor, BlockStyler* styler) : editor_(editor), styler_(styler) {}

QRectF TableController::tableRect(int blockNumber) const {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return {};
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.empty()) {
        return {};
    }
    const QTextBlock first = editor_->document()->findBlockByNumber(metrics.firstBlock);
    const QTextBlock last = editor_->document()->findBlockByNumber(metrics.lastBlock);
    if (!first.isValid() || !last.isValid() || !first.layout()) {
        return {};
    }
    const QPointF offset = editor_->contentOffset();
    const QRectF firstGeom = editor_->blockBoundingGeometry(first).translated(offset);
    const QRectF lastGeom = editor_->blockBoundingGeometry(last).translated(offset);
    const double pad = styler_->formats().table().padding;
    double totalWidth = 0.0;
    for (double w : metrics.columnWidths) {
        totalWidth += w + 2.0 * pad;
    }
    const qreal left = firstGeom.left() + first.layout()->position().x() + editor_->document()->documentMargin();
    const qreal top = blockTextSpanY(firstGeom, first).first;
    const qreal bottom = blockTextSpanY(lastGeom, last).second;
    return {left, top, totalWidth, bottom - top};
}

QRectF TableController::cellRect(int blockNumber, int column) const {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid() || column < 0) {
        return {};
    }
    const auto render = styler_->renderFor(block);
    if (!render.table || render.tableDelimiter) {
        return {};
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    const auto colIdx = static_cast<std::size_t>(column);
    if (metrics.firstBlock < 0 || colIdx >= metrics.columnWidths.size()) {
        return {};
    }
    const QTextBlock first = editor_->document()->findBlockByNumber(metrics.firstBlock);
    if (!first.isValid() || !first.layout()) {
        return {};
    }
    const QPointF offset = editor_->contentOffset();
    const QRectF firstGeom = editor_->blockBoundingGeometry(first).translated(offset);
    const QRectF rowGeom = editor_->blockBoundingGeometry(block).translated(offset);
    const double pad = styler_->formats().table().padding;
    qreal cellLeft = firstGeom.left() + first.layout()->position().x() + editor_->document()->documentMargin();
    for (std::size_t c = 0; c < colIdx; ++c) {
        cellLeft += metrics.columnWidths[c] + 2.0 * pad;
    }
    const qreal cellW = metrics.columnWidths[colIdx] + 2.0 * pad;
    auto [top, bottom] = blockTextSpanY(rowGeom, block);
    if (render.tableHeader) {
        const QTextBlock delim = block.next();
        if (delim.isValid() && styler_->renderFor(delim).tableDelimiter) {
            bottom = blockTextSpanY(editor_->blockBoundingGeometry(delim).translated(offset), delim).second;
        }
    }
    return {cellLeft, top, cellW, bottom - top};
}

QRectF TableController::addRowButtonRect(int blockNumber) const {
    const QRectF tRect = tableRect(blockNumber);
    if (tRect.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    const qreal top = tRect.bottom() + 2.0;
    return {tRect.left(), top, tRect.width(), thickness};
}

QRectF TableController::addColumnButtonRect(int blockNumber) const {
    const QRectF tRect = tableRect(blockNumber);
    if (tRect.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    const qreal left = tRect.right() + 2.0;
    return {left, tRect.top(), thickness, tRect.height()};
}

QRectF TableController::deleteRowButtonRect(int blockNumber) const {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return {};
    }
    const auto render = styler_->renderFor(block);
    if (!render.table || render.tableHeader || render.tableDelimiter) {
        return {};
    }
    const QRectF c0 = cellRect(blockNumber, 0);
    if (c0.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    const qreal left = std::max<qreal>(1.0, c0.left() - thickness - 2.0);
    const qreal width = std::max<qreal>(10.0, c0.left() - 2.0 - left);
    return {left, c0.top(), width, c0.height()};
}

QRectF TableController::deleteColumnButtonRect(int blockNumber, int column) const {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid() || column < 0) {
        return {};
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    const auto colIdx = static_cast<std::size_t>(column);
    if (metrics.firstBlock < 0 || metrics.columnWidths.size() <= 1 || colIdx >= metrics.columnWidths.size()) {
        return {};
    }
    const QRectF cHeader = cellRect(metrics.firstBlock, column);
    if (cHeader.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    const qreal top = std::max<qreal>(1.0, cHeader.top() - thickness - 2.0);
    const qreal height = std::max<qreal>(10.0, cHeader.top() - 2.0 - top);
    return {cHeader.left(), top, cHeader.width(), height};
}

QRectF TableController::addRowHoverRect(int blockNumber) const {
    const QRectF btn = addRowButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    return btn.adjusted(-8.0, -10.0, 8.0, 4.0);
}

QRectF TableController::addColumnHoverRect(int blockNumber) const {
    const QRectF btn = addColumnButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    return btn.adjusted(-10.0, -8.0, 12.0, 8.0);
}

QRectF TableController::deleteRowHoverRect(int blockNumber) const {
    const QRectF btn = deleteRowButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    const QRectF c0 = cellRect(blockNumber, 0);
    const qreal left = std::max<qreal>(0.0, btn.left() - 8.0);
    const qreal right = c0.left() + std::min<qreal>(24.0, c0.width() * 0.5);
    return {left, btn.top(), right - left, btn.height()};
}

QRectF TableController::deleteColumnHoverRect(int blockNumber, int column) const {
    const QRectF btn = deleteColumnButtonRect(blockNumber, column);
    if (btn.isEmpty()) {
        return {};
    }
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    const TableMetrics metrics = styler_->tableMetrics(block);
    const QRectF cHeader = cellRect(metrics.firstBlock, column);
    const qreal top = std::max<qreal>(0.0, btn.top() - 4.0);
    const qreal bottom = cHeader.top() + std::min<qreal>(14.0, cHeader.height() * 0.45);
    return {btn.left(), top, btn.width(), bottom - top};
}

bool TableController::isAddRowButtonVisible(int blockNumber) const {
    if (!hoverPos_.has_value()) {
        return false;
    }
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredAddRowBlock_ == metrics.firstBlock;
}

bool TableController::isAddColumnButtonVisible(int blockNumber) const {
    if (!hoverPos_.has_value()) {
        return false;
    }
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredAddColBlock_ == metrics.firstBlock;
}

bool TableController::isDeleteRowButtonVisible(int blockNumber) const {
    return hoverPos_.has_value() && blockNumber >= 0 && hoveredDeleteRowBlock_ == blockNumber;
}

bool TableController::isDeleteColumnButtonVisible(int blockNumber, int column) const {
    if (!hoverPos_.has_value() || column < 0) {
        return false;
    }
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredDeleteColTable_ == metrics.firstBlock && hoveredDeleteColIndex_ == column;
}

void TableController::updateHoverPos(std::optional<QPointF> pos) {
    hoverPos_ = pos;
    if (refreshHoverState()) {
        editor_->viewport()->update();
    }
}

bool TableController::refreshHoverState() {
    int nextAddRow = -1;
    int nextAddCol = -1;
    int nextDelRow = -1;
    int nextDelColTable = -1;
    int nextDelColIdx = -1;

    if (hoverPos_.has_value()) {
        const QPointF pos = *hoverPos_;
        const auto [first, last] = editor_->visibleBlockRange();
        int checkedTableEnd = -1;
        for (QTextBlock b = editor_->document()->findBlockByNumber(std::max(0, first - 1));
             b.isValid() && b.blockNumber() <= last; b = b.next()) {
            if (b.blockNumber() <= checkedTableEnd || !styler_->renderFor(b).table) {
                continue;
            }
            const TableMetrics metrics = styler_->tableMetrics(b);
            if (metrics.firstBlock < 0) {
                continue;
            }
            checkedTableEnd = metrics.lastBlock;

            if (addRowHoverRect(metrics.firstBlock).contains(pos)) {
                nextAddRow = metrics.firstBlock;
            }
            if (addColumnHoverRect(metrics.firstBlock).contains(pos)) {
                nextAddCol = metrics.firstBlock;
            }

            for (int rNum = metrics.firstBlock + 2; rNum <= metrics.lastBlock; ++rNum) {
                const QTextBlock rowB = editor_->document()->findBlockByNumber(rNum);
                if (rowB.isValid() && !styler_->renderFor(rowB).tableDelimiter &&
                    deleteRowHoverRect(rNum).contains(pos)) {
                    nextDelRow = rNum;
                    break;
                }
            }

            if (metrics.columnWidths.size() > 1) {
                for (std::size_t c = 0; c < metrics.columnWidths.size(); ++c) {
                    if (deleteColumnHoverRect(metrics.firstBlock, static_cast<int>(c)).contains(pos)) {
                        nextDelColTable = metrics.firstBlock;
                        nextDelColIdx = static_cast<int>(c);
                        break;
                    }
                }
            }
        }
    }

    const bool changed = (nextAddRow != hoveredAddRowBlock_) || (nextAddCol != hoveredAddColBlock_) ||
                         (nextDelRow != hoveredDeleteRowBlock_) || (nextDelColTable != hoveredDeleteColTable_) ||
                         (nextDelColIdx != hoveredDeleteColIndex_);
    hoveredAddRowBlock_ = nextAddRow;
    hoveredAddColBlock_ = nextAddCol;
    hoveredDeleteRowBlock_ = nextDelRow;
    hoveredDeleteColTable_ = nextDelColTable;
    hoveredDeleteColIndex_ = nextDelColIdx;
    return changed;
}

void TableController::placeCaretInCell(int blockNumber, std::size_t column, bool atEnd) {
    QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    auto render = styler_->renderFor(block);
    if (render.table && render.tableDelimiter) {
        const QTextBlock next = block.next();
        block = (next.isValid() && styler_->renderFor(next).table) ? next : block.previous();
        if (!block.isValid()) {
            return;
        }
        render = styler_->renderFor(block);
    }
    if (!render.table) {
        return;
    }
    if (column >= render.cells.size()) {
        const QString text = block.text();
        const std::u16string padded = core::padTableRowCells(view(text), column + 1);
        QTextCursor edit(block);
        edit.beginEditBlock();
        edit.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        edit.insertText(toQString(padded));
        edit.endEditBlock();
        block = editor_->document()->findBlockByNumber(blockNumber);
        render = styler_->renderFor(block);
    }
    if (render.cells.empty()) {
        return;
    }
    const std::size_t idx = std::min(column, render.cells.size() - 1);
    const auto& cell = render.cells[idx];
    const std::uint32_t offset = atEnd ? cell.content.end() : cell.content.start;
    QTextCursor c(block);
    c.setPosition(block.position() + static_cast<int>(offset));
    editor_->setTextCursor(c);
}

void TableController::appendRowToTable(int blockNumber, std::size_t targetColumn) {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.lastBlock < 0 || metrics.columnWidths.empty()) {
        return;
    }
    const QTextBlock last = editor_->document()->findBlockByNumber(metrics.lastBlock);
    if (!last.isValid()) {
        return;
    }
    const std::u16string emptyRow = core::makeEmptyTableRow(metrics.columnWidths.size());
    QTextCursor c(last);
    c.beginEditBlock();
    c.movePosition(QTextCursor::EndOfBlock);
    c.insertText(QStringLiteral("\n") + toQString(emptyRow));
    c.endEditBlock();
    styler_->styleVisible();
    placeCaretInCell(metrics.lastBlock + 1, targetColumn, false);
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

void TableController::appendColumnToTable(int blockNumber) {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.empty()) {
        return;
    }
    const std::size_t newColumnIndex = metrics.columnWidths.size();
    {
        BlockStyler::BatchScope batch(*styler_);
        QTextCursor c(editor_->document());
        c.beginEditBlock();
        for (int bNum = metrics.firstBlock; bNum <= metrics.lastBlock; ++bNum) {
            const QTextBlock b = editor_->document()->findBlockByNumber(bNum);
            if (!b.isValid()) {
                continue;
            }
            const auto r = styler_->renderFor(b);
            const QString lineText = b.text();
            const std::u16string updated = core::appendTableColumn(view(lineText), r.tableDelimiter);
            c.setPosition(b.position());
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            c.insertText(toQString(updated));
        }
        c.endEditBlock();
    }
    placeCaretInCell(metrics.firstBlock, newColumnIndex, false);
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

void TableController::deleteTableRow(int rowBlockNumber, std::size_t targetColumn, bool preferPreviousRow) {
    const QTextBlock rowBlock = editor_->document()->findBlockByNumber(rowBlockNumber);
    if (!rowBlock.isValid()) {
        return;
    }
    const auto render = styler_->renderFor(rowBlock);
    if (!render.table || render.tableHeader || render.tableDelimiter) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(rowBlock);
    if (metrics.firstBlock < 0 || rowBlockNumber <= metrics.firstBlock + 1) {
        return;
    }
    const int newLastBlock = metrics.lastBlock - 1;
    const int prevContentBlock =
        (rowBlockNumber - 1 > metrics.firstBlock + 1) ? (rowBlockNumber - 1) : metrics.firstBlock;

    QTextCursor c(editor_->document());
    c.beginEditBlock();
    c.setPosition(rowBlock.position() - 1);
    c.setPosition(rowBlock.position() + static_cast<int>(rowBlock.text().size()), QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.endEditBlock();

    styler_->styleVisible();
    if (!preferPreviousRow && rowBlockNumber <= newLastBlock) {
        placeCaretInCell(rowBlockNumber, targetColumn, false);
    } else {
        placeCaretInCell(prevContentBlock, targetColumn, preferPreviousRow);
    }
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

void TableController::deleteTableColumn(int blockNumber, std::size_t column) {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.size() <= 1 || column >= metrics.columnWidths.size()) {
        return;
    }
    int caretRow = metrics.firstBlock;
    const QTextBlock curBlock = editor_->textCursor().block();
    if (curBlock.isValid() && curBlock.blockNumber() >= metrics.firstBlock &&
        curBlock.blockNumber() <= metrics.lastBlock && !styler_->renderFor(curBlock).tableDelimiter) {
        caretRow = curBlock.blockNumber();
    }
    const std::size_t newColCount = metrics.columnWidths.size() - 1;
    const std::size_t nextCol = std::min(column, newColCount - 1);

    {
        BlockStyler::BatchScope batch(*styler_);
        QTextCursor c(editor_->document());
        c.beginEditBlock();
        for (int bNum = metrics.firstBlock; bNum <= metrics.lastBlock; ++bNum) {
            const QTextBlock b = editor_->document()->findBlockByNumber(bNum);
            if (!b.isValid()) {
                continue;
            }
            const QString lineText = b.text();
            const std::u16string updated = core::removeTableColumn(view(lineText), column);
            c.setPosition(b.position());
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            c.insertText(toQString(updated));
        }
        c.endEditBlock();
    }

    placeCaretInCell(caretRow, nextCol, false);
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

void TableController::exitTableBelow(int blockNumber, bool removeEmptyLastRow) {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.lastBlock < 0) {
        return;
    }
    const QTextBlock last = editor_->document()->findBlockByNumber(metrics.lastBlock);
    if (!last.isValid()) {
        return;
    }

    int tableEndBlock = metrics.lastBlock;
    const bool deletingRow = removeEmptyLastRow && metrics.lastBlock > metrics.firstBlock + 1;
    const QTextBlock probeEnd = deletingRow ? editor_->document()->findBlockByNumber(metrics.lastBlock - 1) : last;
    const QTextBlock existingNext = last.next();

    if (!deletingRow && existingNext.isValid() && existingNext.next().isValid() && existingNext.text().isEmpty()) {
        QTextCursor target(existingNext.next());
        editor_->setTextCursor(target);
        return;
    }

    QTextCursor c(editor_->document());
    c.beginEditBlock();
    if (deletingRow) {
        c.setPosition(last.position() - 1);
        c.setPosition(last.position() + static_cast<int>(last.text().size()), QTextCursor::KeepAnchor);
        c.removeSelectedText();
        tableEndBlock = probeEnd.blockNumber();
    }
    const QTextBlock endBlock = editor_->document()->findBlockByNumber(tableEndBlock);
    const QTextBlock b1 = endBlock.next();
    const QTextBlock b2 = b1.isValid() ? b1.next() : QTextBlock{};
    if (!b1.isValid() || !b1.text().isEmpty()) {
        c.setPosition(endBlock.position() + static_cast<int>(endBlock.text().size()));
        c.insertText(QStringLiteral("\n\n"));
    } else if (!b2.isValid()) {
        c.setPosition(b1.position());
        c.insertText(QStringLiteral("\n"));
    }
    c.endEditBlock();

    styler_->styleVisible();
    const QTextBlock targetBlock = editor_->document()->findBlockByNumber(tableEndBlock + 2);
    if (targetBlock.isValid()) {
        QTextCursor target(targetBlock);
        editor_->setTextCursor(target);
    }
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

void TableController::exitTableAbove(int blockNumber) {
    const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0) {
        return;
    }
    if (metrics.firstBlock > 0) {
        QTextCursor above(editor_->document()->findBlockByNumber(metrics.firstBlock - 1));
        editor_->setTextCursor(above);
        return;
    }
    QTextCursor c(editor_->document());
    c.beginEditBlock();
    c.setPosition(0);
    c.insertText(QStringLiteral("\n\n"));
    c.endEditBlock();
    styler_->styleVisible();
    QTextCursor top(editor_->document()->firstBlock());
    editor_->setTextCursor(top);
    updateHoverPos(hoverPos_);
    editor_->viewport()->update();
}

int TableController::tableHitPosition(int blockNumber, std::size_t colIdx, QPointF viewportPos) const {
    const QTextBlock rowBlock = editor_->document()->findBlockByNumber(blockNumber);
    if (!rowBlock.isValid() || !rowBlock.layout()) {
        return -1;
    }
    const auto rowRender = styler_->renderFor(rowBlock);
    if (!rowRender.table || rowRender.tableDelimiter || colIdx >= rowRender.cells.size()) {
        return -1;
    }
    const auto& cell = rowRender.cells[colIdx];
    const auto [segStart, segEnd] = cellSegmentRange(rowRender, colIdx, static_cast<int>(rowBlock.text().size()));
    const QRectF rowGeom = editor_->blockBoundingGeometry(rowBlock).translated(editor_->contentOffset());
    const QTextLayout* layout = rowBlock.layout();
    const qreal relX = viewportPos.x() - rowGeom.left() - layout->position().x();
    const qreal relY = viewportPos.y() - rowGeom.top() - layout->position().y();

    QTextLine bestLine;
    qreal bestDist = 1e18;
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        if (line.textStart() < segStart || line.textStart() >= segEnd) {
            continue;
        }
        const qreal top = line.y();
        const qreal bottom = top + line.height();
        const qreal dist = relY < top ? (top - relY) : (relY > bottom ? (relY - bottom) : 0.0);
        if (!bestLine.isValid() || dist < bestDist) {
            bestLine = line;
            bestDist = dist;
            if (dist == 0.0) {
                break;
            }
        }
    }
    if (!bestLine.isValid()) {
        return rowBlock.position() + static_cast<int>(cell.content.start);
    }
    const int hitCol = bestLine.xToCursor(relX);
    const int clamped = std::clamp(hitCol, static_cast<int>(cell.content.start), static_cast<int>(cell.content.end()));
    return rowBlock.position() + clamped;
}

bool TableController::handleMousePress(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (event->button() != Qt::LeftButton || (event->modifiers() & Qt::ShiftModifier)) {
        return false;
    }

    updateHoverPos(pos);
    const auto [first, last] = editor_->visibleBlockRange();
    int checkedTableEnd = -1;
    for (QTextBlock b = editor_->document()->findBlockByNumber(std::max(0, first - 1));
         b.isValid() && b.blockNumber() <= last; b = b.next()) {
        if (b.blockNumber() <= checkedTableEnd || !styler_->renderFor(b).table) {
            continue;
        }
        const TableMetrics tm = styler_->tableMetrics(b);
        if (tm.firstBlock < 0) {
            continue;
        }
        checkedTableEnd = tm.lastBlock;
        if (isAddRowButtonVisible(tm.firstBlock) && addRowButtonRect(tm.firstBlock).contains(pos)) {
            appendRowToTable(tm.firstBlock, 0);
            return true;
        }
        if (isAddColumnButtonVisible(tm.firstBlock) && addColumnButtonRect(tm.firstBlock).contains(pos)) {
            appendColumnToTable(tm.firstBlock);
            return true;
        }
        if (hoveredDeleteRowBlock_ >= tm.firstBlock + 2 && hoveredDeleteRowBlock_ <= tm.lastBlock &&
            isDeleteRowButtonVisible(hoveredDeleteRowBlock_) &&
            deleteRowButtonRect(hoveredDeleteRowBlock_).contains(pos)) {
            deleteTableRow(hoveredDeleteRowBlock_, 0, false);
            return true;
        }
        if (hoveredDeleteColTable_ == tm.firstBlock && hoveredDeleteColIndex_ >= 0 &&
            isDeleteColumnButtonVisible(tm.firstBlock, hoveredDeleteColIndex_) &&
            deleteColumnButtonRect(tm.firstBlock, hoveredDeleteColIndex_).contains(pos)) {
            deleteTableColumn(tm.firstBlock, static_cast<std::size_t>(hoveredDeleteColIndex_));
            return true;
        }
        if (tableRect(tm.firstBlock).contains(pos)) {
            // Find the clicked row and column.
            int targetBlock = tm.firstBlock;
            for (int rNum = tm.firstBlock; rNum <= tm.lastBlock; ++rNum) {
                const QTextBlock rowB = editor_->document()->findBlockByNumber(rNum);
                if (!rowB.isValid() || styler_->renderFor(rowB).tableDelimiter) {
                    continue;
                }
                const QRectF c0 = cellRect(rNum, 0);
                if (pos.y() >= c0.top() && pos.y() <= c0.bottom()) {
                    targetBlock = rNum;
                    break;
                }
            }
            std::size_t targetCol = tm.columnWidths.size() - 1;
            for (std::size_t c = 0; c < tm.columnWidths.size(); ++c) {
                const QRectF cr = cellRect(targetBlock, static_cast<int>(c));
                if (pos.x() <= cr.right()) {
                    targetCol = c;
                    break;
                }
            }
            editor_->setFocus();
            const int docPos = tableHitPosition(targetBlock, targetCol, pos);
            if (docPos >= 0) {
                QTextCursor cur = editor_->textCursor();
                cur.setPosition(docPos);
                editor_->setTextCursor(cur);
                cellDragActive_ = true;
                cellDragAnchor_ = docPos;
                dragBlock_ = targetBlock;
                dragCol_ = targetCol;
            } else {
                placeCaretInCell(targetBlock, targetCol, false);
                cellDragActive_ = false;
            }
            return true;
        }
    }

    // Clicking above a table at block 0 or below a table at the end of the document creates a plain line outside.
    const QTextBlock firstDocBlock = editor_->document()->firstBlock();
    if (firstDocBlock.isValid() && styler_->renderFor(firstDocBlock).table) {
        const QRectF firstTable = tableRect(0);
        if (!firstTable.isEmpty() && pos.y() < firstTable.top() - 4.0) {
            editor_->setFocus();
            exitTableAbove(0);
            return true;
        }
    }
    const QTextBlock lastDocBlock = editor_->document()->lastBlock();
    if (lastDocBlock.isValid() && styler_->renderFor(lastDocBlock).table) {
        const QRectF lastTable = tableRect(lastDocBlock.blockNumber());
        if (!lastTable.isEmpty() && pos.y() > lastTable.bottom()) {
            editor_->setFocus();
            exitTableBelow(lastDocBlock.blockNumber(), false);
            return true;
        }
    }

    return false;
}

bool TableController::handleMouseMove(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) != 0 && cellDragActive_) {
        const int hitPos = tableHitPosition(dragBlock_, dragCol_, event->position());
        if (hitPos >= 0) {
            QTextCursor cur = editor_->textCursor();
            cur.setPosition(cellDragAnchor_);
            cur.setPosition(hitPos, QTextCursor::KeepAnchor);
            editor_->setTextCursor(cur);
        }
        updateHoverPos(std::nullopt);
        return true;
    }
    if (event->buttons() != Qt::NoButton) {
        updateHoverPos(std::nullopt);
        return false;
    }
    updateHoverPos(event->position());
    return false;
}

void TableController::handleMouseRelease(QMouseEvent* /*event*/) {
    cellDragActive_ = false;
}

bool TableController::handleContextMenu(QContextMenuEvent* event) {
    const QPointF pos = editor_->viewport()->mapFromGlobal(event->globalPos());
    const auto [first, last] = editor_->visibleBlockRange();
    int tableFirstBlock = -1;
    int tableRowBlock = -1;
    std::size_t targetCol = 0;
    int checkedTableEnd = -1;
    for (QTextBlock b = editor_->document()->findBlockByNumber(std::max(0, first - 1));
         b.isValid() && b.blockNumber() <= last; b = b.next()) {
        if (b.blockNumber() <= checkedTableEnd || !styler_->renderFor(b).table) {
            continue;
        }
        const TableMetrics tm = styler_->tableMetrics(b);
        if (tm.firstBlock < 0) {
            continue;
        }
        checkedTableEnd = tm.lastBlock;
        if (tableRect(tm.firstBlock).contains(pos)) {
            tableFirstBlock = tm.firstBlock;
            tableRowBlock = tm.firstBlock;
            for (int rNum = tm.firstBlock; rNum <= tm.lastBlock; ++rNum) {
                const QTextBlock rowB = editor_->document()->findBlockByNumber(rNum);
                if (!rowB.isValid() || styler_->renderFor(rowB).tableDelimiter) {
                    continue;
                }
                const QRectF c0 = cellRect(rNum, 0);
                if (pos.y() >= c0.top() && pos.y() <= c0.bottom()) {
                    tableRowBlock = rNum;
                    break;
                }
            }
            for (std::size_t c = 0; c < tm.columnWidths.size(); ++c) {
                if (pos.x() <= cellRect(tableRowBlock, static_cast<int>(c)).right()) {
                    targetCol = c;
                    break;
                }
            }
            break;
        }
    }

    if (tableFirstBlock < 0) {
        return false;
    }

    auto* menu = editor_->createStandardContextMenu();
    menu->addSeparator();
    auto* addRowAct = menu->addAction(QStringLiteral("Add Row"));
    auto* delRowAct = menu->addAction(QStringLiteral("Delete Row"));
    auto* addColAct = menu->addAction(QStringLiteral("Add Column"));
    auto* delColAct = menu->addAction(QStringLiteral("Delete Column"));

    const auto rowRender = styler_->renderFor(editor_->document()->findBlockByNumber(tableRowBlock));
    if (rowRender.tableHeader || rowRender.tableDelimiter) {
        delRowAct->setEnabled(false);
    }
    const TableMetrics tm = styler_->tableMetrics(editor_->document()->findBlockByNumber(tableFirstBlock));
    if (tm.columnWidths.size() <= 1) {
        delColAct->setEnabled(false);
    }

    QObject::connect(addRowAct, &QAction::triggered, editor_,
                     [this, tableFirstBlock, targetCol] { appendRowToTable(tableFirstBlock, targetCol); });
    QObject::connect(delRowAct, &QAction::triggered, editor_,
                     [this, tableRowBlock, targetCol] { deleteTableRow(tableRowBlock, targetCol, false); });
    QObject::connect(addColAct, &QAction::triggered, editor_,
                     [this, tableFirstBlock] { appendColumnToTable(tableFirstBlock); });
    QObject::connect(delColAct, &QAction::triggered, editor_,
                     [this, tableFirstBlock, targetCol] { deleteTableColumn(tableFirstBlock, targetCol); });
    menu->exec(event->globalPos());
    delete menu;
    return true;
}

bool TableController::handleKeyPress(QKeyEvent* event) {
    QTextCursor cur = editor_->textCursor();
    QTextBlock block = cur.block();
    auto render = styler_->renderFor(block);
    if (!render.table) {
        return false;
    }

    if (render.tableDelimiter) {
        placeCaretInCell(block.blockNumber(), 0, false);
        cur = editor_->textCursor();
        block = cur.block();
        render = styler_->renderFor(block);
    }

    const TableMetrics metrics = styler_->tableMetrics(block);
    const std::size_t totalCols = std::max<std::size_t>(1, metrics.columnWidths.size());
    const auto posInLine = static_cast<std::uint32_t>(cur.positionInBlock());
    const std::size_t cellIdx = findCellIndex(render, posInLine);
    const core::TableCell cell =
        cellIdx < render.cells.size() ? render.cells[cellIdx] : core::TableCell{{posInLine, 0}, {posInLine, 0}};

    const auto nextContentRow = [&](int fromBlock) -> int {
        for (int b = fromBlock + 1; b <= metrics.lastBlock; ++b) {
            const QTextBlock candidate = editor_->document()->findBlockByNumber(b);
            if (candidate.isValid() && !styler_->renderFor(candidate).tableDelimiter) {
                return b;
            }
        }
        return -1;
    };
    const auto prevContentRow = [&](int fromBlock) -> int {
        for (int b = fromBlock - 1; b >= metrics.firstBlock; --b) {
            const QTextBlock candidate = editor_->document()->findBlockByNumber(b);
            if (candidate.isValid() && !styler_->renderFor(candidate).tableDelimiter) {
                return b;
            }
        }
        return -1;
    };

    const int key = event->key();
    const bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;
    const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
    const bool alt = (event->modifiers() & Qt::AltModifier) != 0;

    if (key == Qt::Key_Escape) {
        exitTableBelow(metrics.firstBlock, false);
        return true;
    }

    if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
        if (key == Qt::Key_Backtab || shift) {
            if (cellIdx > 0) {
                placeCaretInCell(block.blockNumber(), cellIdx - 1, false);
            } else if (const int prevRow = prevContentRow(block.blockNumber()); prevRow >= 0) {
                placeCaretInCell(prevRow, totalCols - 1, false);
            }
        } else {
            if (cellIdx + 1 < totalCols) {
                placeCaretInCell(block.blockNumber(), cellIdx + 1, false);
            } else if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
                placeCaretInCell(nextRow, 0, false);
            } else {
                appendRowToTable(metrics.firstBlock, 0);
            }
        }
        return true;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (ctrl) {
            exitTableBelow(metrics.firstBlock, false);
            return true;
        }
        const int nextRow = nextContentRow(block.blockNumber());
        if (nextRow >= 0) {
            placeCaretInCell(nextRow, cellIdx, false);
        } else {
            if (isEmptyTableBodyRow(render)) {
                exitTableBelow(metrics.firstBlock, true);
            } else {
                appendRowToTable(metrics.firstBlock, cellIdx);
            }
        }
        return true;
    }

    if (key == Qt::Key_Up && !shift) {
        if (const QTextLayout* layout = block.layout(); layout && cellIdx < render.cells.size()) {
            const auto [segStart, segEnd] = cellSegmentRange(render, cellIdx, static_cast<int>(block.text().size()));
            const QTextLine curLine = layout->lineForTextPosition(static_cast<int>(posInLine));
            if (curLine.isValid() && curLine.lineNumber() > 0) {
                const QTextLine prevLine = layout->lineAt(curLine.lineNumber() - 1);
                if (prevLine.isValid() && prevLine.textStart() >= segStart && prevLine.textStart() < segEnd) {
                    const qreal x = curLine.cursorToX(static_cast<int>(posInLine));
                    const int nextCol = std::clamp(prevLine.xToCursor(x), static_cast<int>(cell.content.start),
                                                   static_cast<int>(cell.content.end()));
                    cur.setPosition(block.position() + nextCol);
                    editor_->setTextCursor(cur);
                    return true;
                }
            }
        }
        if (const int prevRow = prevContentRow(block.blockNumber()); prevRow >= 0) {
            placeCaretInCell(prevRow, cellIdx, false);
        } else if (metrics.firstBlock > 0) {
            QTextCursor above(editor_->document()->findBlockByNumber(metrics.firstBlock - 1));
            editor_->setTextCursor(above);
        }
        return true;
    }

    if (key == Qt::Key_Down && !shift) {
        if (const QTextLayout* layout = block.layout(); layout && cellIdx < render.cells.size()) {
            const auto [segStart, segEnd] = cellSegmentRange(render, cellIdx, static_cast<int>(block.text().size()));
            const QTextLine curLine = layout->lineForTextPosition(static_cast<int>(posInLine));
            if (curLine.isValid() && curLine.lineNumber() + 1 < layout->lineCount()) {
                const QTextLine nextLine = layout->lineAt(curLine.lineNumber() + 1);
                if (nextLine.isValid() && nextLine.textStart() >= segStart && nextLine.textStart() < segEnd) {
                    const qreal x = curLine.cursorToX(static_cast<int>(posInLine));
                    const int nextCol = std::clamp(nextLine.xToCursor(x), static_cast<int>(cell.content.start),
                                                   static_cast<int>(cell.content.end()));
                    cur.setPosition(block.position() + nextCol);
                    editor_->setTextCursor(cur);
                    return true;
                }
            }
        }
        if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
            placeCaretInCell(nextRow, cellIdx, false);
        } else if (metrics.lastBlock + 1 < editor_->document()->blockCount()) {
            QTextCursor below(editor_->document()->findBlockByNumber(metrics.lastBlock + 1));
            editor_->setTextCursor(below);
        }
        return true;
    }

    if (key == Qt::Key_Left && !shift) {
        if (posInLine > cell.content.start) {
            std::uint32_t nextPos = posInLine - 1;
            if (nextPos > cell.content.start &&
                block.text().mid(static_cast<int>(nextPos - 1), 2) == QStringLiteral("\\|")) {
                --nextPos;
            }
            cur.setPosition(block.position() + static_cast<int>(nextPos));
            editor_->setTextCursor(cur);
        } else if (cellIdx > 0) {
            placeCaretInCell(block.blockNumber(), cellIdx - 1, true);
        } else if (const int prevRow = prevContentRow(block.blockNumber()); prevRow >= 0) {
            placeCaretInCell(prevRow, totalCols - 1, true);
        } else if (metrics.firstBlock > 0) {
            QTextCursor above(editor_->document()->findBlockByNumber(metrics.firstBlock - 1));
            above.movePosition(QTextCursor::EndOfBlock);
            editor_->setTextCursor(above);
        }
        return true;
    }

    if (key == Qt::Key_Right && !shift) {
        if (posInLine < cell.content.end()) {
            std::uint32_t nextPos = posInLine + 1;
            if (nextPos < cell.content.end() &&
                block.text().mid(static_cast<int>(posInLine), 2) == QStringLiteral("\\|")) {
                ++nextPos;
            }
            cur.setPosition(block.position() + static_cast<int>(nextPos));
            editor_->setTextCursor(cur);
        } else if (cellIdx + 1 < totalCols) {
            placeCaretInCell(block.blockNumber(), cellIdx + 1, false);
        } else if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
            placeCaretInCell(nextRow, 0, false);
        } else if (metrics.lastBlock + 1 < editor_->document()->blockCount()) {
            QTextCursor below(editor_->document()->findBlockByNumber(metrics.lastBlock + 1));
            editor_->setTextCursor(below);
        }
        return true;
    }

    if (key == Qt::Key_Home) {
        cur.setPosition(block.position() + static_cast<int>(cell.content.start),
                        shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
        editor_->setTextCursor(cur);
        return true;
    }

    if (key == Qt::Key_End) {
        cur.setPosition(block.position() + static_cast<int>(cell.content.end()),
                        shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
        editor_->setTextCursor(cur);
        return true;
    }

    if (key == Qt::Key_Backspace) {
        if (ctrl && alt) {
            deleteTableColumn(metrics.firstBlock, cellIdx);
            return true;
        }
        if (ctrl && shift) {
            if (render.tableHeader) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
            } else {
                deleteTableRow(block.blockNumber(), cellIdx, true);
            }
            return true;
        }
        if (!cur.hasSelection()) {
            if (isEmptyTableBodyRow(render)) {
                deleteTableRow(block.blockNumber(), cellIdx, true);
                return true;
            }
            if (isEmptyTableColumn(editor_->document(), styler_, metrics, cellIdx)) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
                return true;
            }
            if (posInLine <= cell.content.start) {
                return true;
            }
            if (posInLine >= cell.content.start + 2 &&
                block.text().mid(static_cast<int>(posInLine - 2), 2) == QStringLiteral("\\|")) {
                cur.setPosition(block.position() + static_cast<int>(posInLine - 2), QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                return true;
            }
        } else {
            const int selStart = std::min(cur.position(), cur.anchor());
            const int selEnd = std::max(cur.position(), cur.anchor());
            if (cur.block() != editor_->document()->findBlock(cur.anchor()) || selStart < block.position() ||
                selEnd > block.position() + block.length()) {
                // Multi-block selection across table boundary (e.g. Ctrl+A): fall back to default handler.
                return false;
            }
            const int selStartInBlock = selStart - block.position();
            const int selEndInBlock = selEnd - block.position();
            if (selStartInBlock < static_cast<int>(cell.content.start) ||
                selEndInBlock > static_cast<int>(cell.content.end())) {
                const int clampedStart = std::clamp(selStartInBlock, static_cast<int>(cell.content.start),
                                                    static_cast<int>(cell.content.end()));
                const int clampedEnd = std::clamp(selEndInBlock, clampedStart, static_cast<int>(cell.content.end()));
                if (clampedStart < clampedEnd) {
                    cur.setPosition(block.position() + clampedStart);
                    cur.setPosition(block.position() + clampedEnd, QTextCursor::KeepAnchor);
                    cur.removeSelectedText();
                }
                return true;
            }
        }
    }

    if (key == Qt::Key_Delete) {
        if (ctrl && alt) {
            deleteTableColumn(metrics.firstBlock, cellIdx);
            return true;
        }
        if (ctrl && shift) {
            if (render.tableHeader) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
            } else {
                deleteTableRow(block.blockNumber(), cellIdx, false);
            }
            return true;
        }
        if (!cur.hasSelection()) {
            if (isEmptyTableBodyRow(render)) {
                deleteTableRow(block.blockNumber(), cellIdx, false);
                return true;
            }
            if (isEmptyTableColumn(editor_->document(), styler_, metrics, cellIdx)) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
                return true;
            }
            if (posInLine >= cell.content.end()) {
                return true;
            }
            if (posInLine + 2 <= cell.content.end() &&
                block.text().mid(static_cast<int>(posInLine), 2) == QStringLiteral("\\|")) {
                cur.setPosition(block.position() + static_cast<int>(posInLine + 2), QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                return true;
            }
        } else {
            const int selStart = std::min(cur.position(), cur.anchor());
            const int selEnd = std::max(cur.position(), cur.anchor());
            if (cur.block() != editor_->document()->findBlock(cur.anchor()) || selStart < block.position() ||
                selEnd > block.position() + block.length()) {
                // Multi-block selection across table boundary (e.g. Ctrl+A): fall back to default handler.
                return false;
            }
            const int selStartInBlock = selStart - block.position();
            const int selEndInBlock = selEnd - block.position();
            if (selStartInBlock < static_cast<int>(cell.content.start) ||
                selEndInBlock > static_cast<int>(cell.content.end())) {
                const int clampedStart = std::clamp(selStartInBlock, static_cast<int>(cell.content.start),
                                                    static_cast<int>(cell.content.end()));
                const int clampedEnd = std::clamp(selEndInBlock, clampedStart, static_cast<int>(cell.content.end()));
                if (clampedStart < clampedEnd) {
                    cur.setPosition(block.position() + clampedStart);
                    cur.setPosition(block.position() + clampedEnd, QTextCursor::KeepAnchor);
                    cur.removeSelectedText();
                }
                return true;
            }
        }
    }

    if (event->text() == QStringLiteral("|")) {
        if (posInLine < cell.content.start) {
            cur.setPosition(block.position() + static_cast<int>(cell.content.start));
            editor_->setTextCursor(cur);
        }
        editor_->textCursor().insertText(QStringLiteral("\\|"));
        return true;
    }

    return false;
}

void TableController::handlePostNavigation(QKeyEvent* event) {
    QTextCursor after = editor_->textCursor();
    if (!after.hasSelection()) {
        const auto afterRender = styler_->renderFor(after.block());
        if (afterRender.table) {
            if (afterRender.tableDelimiter) {
                const int dir = (event->key() == Qt::Key_Up || event->key() == Qt::Key_Left) ? -1 : 1;
                placeCaretInCell(after.blockNumber() + dir, 0, dir < 0);
            } else if (!afterRender.cells.empty()) {
                const std::size_t cellIdx =
                    findCellIndex(afterRender, static_cast<std::uint32_t>(after.positionInBlock()));
                const auto& cell = afterRender.cells[cellIdx];
                const auto col = static_cast<std::uint32_t>(after.positionInBlock());
                if (col < cell.content.start || col > cell.content.end()) {
                    placeCaretInCell(after.blockNumber(), cellIdx, col > cell.content.end());
                }
            }
        }
    }
}

}  // namespace hitokoto::ui
