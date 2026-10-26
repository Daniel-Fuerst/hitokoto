// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "qt_bridge.hpp"

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <memory>

namespace hitokoto::ui {

EditorWidget::EditorWidget(const core::ElementRegistry& registry, QWidget* parent) : QPlainTextEdit(parent) {
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    styler_ = new BlockStyler(document(), registry, [this] { return visibleBlockRange(); }, this);

    connect(document(), &QTextDocument::contentsChange, this, [this](int, int, int) {
        if (!styler_->isBusy()) {
            ++editCount_;
        }
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &EditorWidget::updateReveal);
    connect(this, &QPlainTextEdit::selectionChanged, this, &EditorWidget::updateReveal);
}

void EditorWidget::applyTheme(const core::Theme& theme) {
    EditorFormats formats(theme);
    // Changing fonts and margins marks the document as modified in Qt; a theme is not an edit.
    const bool wasModified = document()->isModified();
    document()->setDefaultFont(formats.baseFont());
    setFont(formats.baseFont());
    padding_ = theme.editorPadding;
    maxWidth_ = theme.editorMaxWidth;
    document()->setDocumentMargin(theme.editorPadding);
    updateMargins();
    styler_->setAvailableWidth(availableTableWidth(formats.baseFont().pointSizeF()));
    styler_->setFormats(std::move(formats));
    if (!wasModified) {
        document()->setModified(false);
    }
    viewport()->update();
}

std::pair<int, int> EditorWidget::visibleBlockRange() const {
    QTextBlock block = firstVisibleBlock();
    if (!block.isValid()) {
        return {0, -1};
    }
    const int first = block.blockNumber();
    int last = first;
    const qreal bottom = viewport()->height();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    while (block.isValid() && top <= bottom) {
        last = block.blockNumber();
        top += blockBoundingRect(block).height();
        block = block.next();
    }
    return {first, last};
}

void EditorWidget::updateReveal() {
    const QTextCursor c = textCursor();
    const auto anchorLine = document()->findBlock(c.anchor()).blockNumber();
    const auto caretLine = c.block().blockNumber();
    styler_->setRevealed(core::revealedLines(anchorLine, caretLine));
}

void EditorWidget::updateMargins() {
    // Centre the text column when the window is wider than editor-max-width.
    int side = 0;
    if (maxWidth_ > 0) {
        side = std::max(0, static_cast<int>((width() - maxWidth_) / 2.0));
    }
    setViewportMargins(side, 0, side, 0);
}

double EditorWidget::availableTableWidth(double basePointSize) const {
    int side = 0;
    if (maxWidth_ > 0) {
        side = std::max(0, static_cast<int>((width() - maxWidth_) / 2.0));
    }
    const int vpWidth = isVisible() ? viewport()->width() : std::max(viewport()->width(), width() - 2 * side);
    const double thickness = std::clamp(basePointSize * 1.3, 14.0, 22.0);
    const double margin = document()->documentMargin();
    const double rightMargin = std::max(margin, thickness + 6.0);
    return std::max(60.0, static_cast<double>(vpWidth) - margin - rightMargin);
}

bool EditorWidget::viewportEvent(QEvent* event) {
    const bool handled = QPlainTextEdit::viewportEvent(event);
    if (event->type() == QEvent::Leave) {
        updateHoverPos(std::nullopt);
    } else if (event->type() == QEvent::Resize && styler_ != nullptr) {
        styler_->setAvailableWidth(availableTableWidth(styler_->formats().baseFont().pointSizeF()));
        styler_->styleVisible();
        updateHoverPos(hoverPos_);
    }
    return handled;
}

void EditorWidget::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    updateMargins();
    styler_->setAvailableWidth(availableTableWidth(styler_->formats().baseFont().pointSizeF()));
    styler_->styleVisible();
    updateHoverPos(hoverPos_);
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QPlainTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
    updateHoverPos(hoverPos_);
}

void EditorWidget::paintEvent(QPaintEvent* event) {
    if (!styler_->isBusy()) {
        styler_->styleVisible();
    }
    (void)refreshHoverState();
    paintCodeBlockPanels();
    QPlainTextEdit::paintEvent(event);
    paintDecorations();
    emit painted();
}

namespace {
/// Fills `rect` with the element's background and draws its border inside the rect.
void drawBox(QPainter& painter, const QRectF& rect, const core::CodeStyle& style) {
    const qreal border = style.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(style.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style.background));
    const QRectF inner = rect.adjusted(border / 2, border / 2, -border / 2, -border / 2);
    const qreal radius = std::max<qreal>(0, style.radius - border / 2);
    painter.drawRoundedRect(inner, radius, radius);
}

void drawTableControlButton(QPainter& painter, const QRectF& rect, const core::TableStyle& style, bool plus) {
    const qreal border = std::max<qreal>(1.0, style.borderWidth);
    painter.setPen(QPen(toQColor(style.border), border));
    painter.setBrush(toQColor(style.headerBackground));
    const qreal radius = std::min<qreal>(style.radius, std::min(rect.width(), rect.height()) / 2.0);
    painter.drawRoundedRect(rect.adjusted(border / 2, border / 2, -border / 2, -border / 2), radius, radius);

    const QPointF c = rect.center();
    const qreal arm = std::min({rect.width(), rect.height(), 12.0}) * 0.28;
    painter.setPen(QPen(toQColor(style.headerText), 1.5));
    painter.drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
    if (plus) {
        painter.drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
    }
}

/// Finds which cell index `columnInLine` belongs to on `render`.
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

/// Vertical span `[top, bottom]` of `block`'s laid-out text, excluding QPlainTextDocumentLayout's
/// document-margin padding on the very first and very last block of the document.
std::pair<qreal, qreal> blockTextSpanY(const QRectF& blockGeom, const QTextBlock& block) {
    const QTextLayout* layout = block.layout();
    if (!layout) {
        return {blockGeom.top(), blockGeom.bottom()};
    }
    const qreal top = blockGeom.top() + layout->position().y();
    return {top, top + layout->boundingRect().height()};
}
}  // namespace

QRectF EditorWidget::tableRect(int blockNumber) const {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return {};
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.empty()) {
        return {};
    }
    const QTextBlock first = document()->findBlockByNumber(metrics.firstBlock);
    const QTextBlock last = document()->findBlockByNumber(metrics.lastBlock);
    if (!first.isValid() || !last.isValid() || !first.layout()) {
        return {};
    }
    const QPointF offset = contentOffset();
    const QRectF firstGeom = blockBoundingGeometry(first).translated(offset);
    const QRectF lastGeom = blockBoundingGeometry(last).translated(offset);
    const double pad = styler_->formats().table().padding;
    double totalWidth = 0.0;
    for (double w : metrics.columnWidths) {
        totalWidth += w + 2.0 * pad;
    }
    const qreal left = firstGeom.left() + first.layout()->position().x() + document()->documentMargin();
    const qreal top = blockTextSpanY(firstGeom, first).first;
    const qreal bottom = blockTextSpanY(lastGeom, last).second;
    return {left, top, totalWidth, bottom - top};
}

QRectF EditorWidget::cellRect(int blockNumber, int column) const {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
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
    const QTextBlock first = document()->findBlockByNumber(metrics.firstBlock);
    if (!first.isValid() || !first.layout()) {
        return {};
    }
    const QPointF offset = contentOffset();
    const QRectF firstGeom = blockBoundingGeometry(first).translated(offset);
    const QRectF rowGeom = blockBoundingGeometry(block).translated(offset);
    const double pad = styler_->formats().table().padding;
    qreal cellLeft = firstGeom.left() + first.layout()->position().x() + document()->documentMargin();
    for (std::size_t c = 0; c < colIdx; ++c) {
        cellLeft += metrics.columnWidths[c] + 2.0 * pad;
    }
    const qreal cellW = metrics.columnWidths[colIdx] + 2.0 * pad;
    auto [top, bottom] = blockTextSpanY(rowGeom, block);
    if (render.tableHeader) {
        const QTextBlock delim = block.next();
        if (delim.isValid() && styler_->renderFor(delim).tableDelimiter) {
            bottom = blockTextSpanY(blockBoundingGeometry(delim).translated(offset), delim).second;
        }
    }
    return {cellLeft, top, cellW, bottom - top};
}

QRectF EditorWidget::addRowButtonRect(int blockNumber) const {
    const QRectF tRect = tableRect(blockNumber);
    if (tRect.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    return {tRect.left(), tRect.bottom() + 2.0, tRect.width(), thickness};
}

QRectF EditorWidget::addColumnButtonRect(int blockNumber) const {
    const QRectF tRect = tableRect(blockNumber);
    if (tRect.isEmpty()) {
        return {};
    }
    const qreal thickness = std::clamp(styler_->formats().baseFont().pointSizeF() * 1.3, 14.0, 22.0);
    return {tRect.right() + 2.0, tRect.top(), thickness, tRect.height()};
}

QRectF EditorWidget::deleteRowButtonRect(int blockNumber) const {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
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

QRectF EditorWidget::deleteColumnButtonRect(int blockNumber, int column) const {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
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

QRectF EditorWidget::addRowHoverRect(int blockNumber) const {
    const QRectF btn = addRowButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    return btn.adjusted(-8.0, -10.0, 8.0, 4.0);
}

QRectF EditorWidget::addColumnHoverRect(int blockNumber) const {
    const QRectF btn = addColumnButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    return btn.adjusted(-10.0, -8.0, 12.0, 8.0);
}

QRectF EditorWidget::deleteRowHoverRect(int blockNumber) const {
    const QRectF btn = deleteRowButtonRect(blockNumber);
    if (btn.isEmpty()) {
        return {};
    }
    const QRectF c0 = cellRect(blockNumber, 0);
    const qreal left = std::max<qreal>(0.0, btn.left() - 8.0);
    const qreal right = c0.left() + std::min<qreal>(24.0, c0.width() * 0.5);
    return {left, btn.top(), right - left, btn.height()};
}

QRectF EditorWidget::deleteColumnHoverRect(int blockNumber, int column) const {
    const QRectF btn = deleteColumnButtonRect(blockNumber, column);
    if (btn.isEmpty()) {
        return {};
    }
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    const TableMetrics metrics = styler_->tableMetrics(block);
    const QRectF cHeader = cellRect(metrics.firstBlock, column);
    const qreal top = std::max<qreal>(0.0, btn.top() - 4.0);
    const qreal bottom = cHeader.top() + std::min<qreal>(14.0, cHeader.height() * 0.45);
    return {btn.left(), top, btn.width(), bottom - top};
}

bool EditorWidget::isAddRowButtonVisible(int blockNumber) const {
    if (!hoverPos_.has_value()) {
        return false;
    }
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredAddRowBlock_ == metrics.firstBlock;
}

bool EditorWidget::isAddColumnButtonVisible(int blockNumber) const {
    if (!hoverPos_.has_value()) {
        return false;
    }
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredAddColBlock_ == metrics.firstBlock;
}

bool EditorWidget::isDeleteRowButtonVisible(int blockNumber) const {
    return hoverPos_.has_value() && blockNumber >= 0 && hoveredDeleteRowBlock_ == blockNumber;
}

bool EditorWidget::isDeleteColumnButtonVisible(int blockNumber, int column) const {
    if (!hoverPos_.has_value() || column < 0) {
        return false;
    }
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return false;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    return metrics.firstBlock >= 0 && hoveredDeleteColTable_ == metrics.firstBlock && hoveredDeleteColIndex_ == column;
}

void EditorWidget::updateHoverPos(std::optional<QPointF> pos) {
    hoverPos_ = pos;
    if (refreshHoverState()) {
        viewport()->update();
    }
}

bool EditorWidget::refreshHoverState() {
    int nextAddRow = -1;
    int nextAddCol = -1;
    int nextDelRow = -1;
    int nextDelColTable = -1;
    int nextDelColIdx = -1;
    bool nextOverButton = false;

    if (hoverPos_.has_value() && styler_ != nullptr) {
        const QPointF pos = *hoverPos_;
        qreal bestRowDist = 1e18;
        qreal bestColDist = 1e18;
        const auto [first, last] = visibleBlockRange();
        int checkedTableEnd = -1;
        for (QTextBlock b = document()->findBlockByNumber(std::max(0, first - 1));
             b.isValid() && b.blockNumber() <= last; b = b.next()) {
            if (b.blockNumber() <= checkedTableEnd || !styler_->renderFor(b).table) {
                continue;
            }
            const TableMetrics tm = styler_->tableMetrics(b);
            if (tm.firstBlock < 0) {
                continue;
            }
            checkedTableEnd = tm.lastBlock;

            if (addRowHoverRect(tm.firstBlock).contains(pos)) {
                const QRectF btn = addRowButtonRect(tm.firstBlock);
                const qreal dist = pos.y() < btn.top() ? (btn.top() - pos.y())
                                                       : (pos.y() > btn.bottom() ? (pos.y() - btn.bottom()) : 0.0);
                if (dist < bestRowDist) {
                    bestRowDist = dist;
                    nextAddRow = tm.firstBlock;
                }
            }
            if (addColumnHoverRect(tm.firstBlock).contains(pos)) {
                const QRectF btn = addColumnButtonRect(tm.firstBlock);
                const qreal dist = pos.x() < btn.left() ? (btn.left() - pos.x())
                                                        : (pos.x() > btn.right() ? (pos.x() - btn.right()) : 0.0);
                if (dist < bestColDist) {
                    bestColDist = dist;
                    nextAddCol = tm.firstBlock;
                }
            }
            for (int rNum = tm.firstBlock + 2; rNum <= std::min(tm.lastBlock, last); ++rNum) {
                if (deleteRowHoverRect(rNum).contains(pos)) {
                    nextDelRow = rNum;
                    break;
                }
            }
            if (tm.columnWidths.size() >= 2) {
                for (std::size_t c = 0; c < tm.columnWidths.size(); ++c) {
                    const auto colInt = static_cast<int>(c);
                    if (deleteColumnHoverRect(tm.firstBlock, colInt).contains(pos)) {
                        nextDelColTable = tm.firstBlock;
                        nextDelColIdx = colInt;
                        break;
                    }
                }
            }
        }
        if ((nextAddRow >= 0 && addRowButtonRect(nextAddRow).contains(pos)) ||
            (nextAddCol >= 0 && addColumnButtonRect(nextAddCol).contains(pos)) ||
            (nextDelRow >= 0 && deleteRowButtonRect(nextDelRow).contains(pos)) ||
            (nextDelColTable >= 0 && nextDelColIdx >= 0 &&
             deleteColumnButtonRect(nextDelColTable, nextDelColIdx).contains(pos))) {
            nextOverButton = true;
        }
    }

    if (nextOverButton != overTableButton_) {
        overTableButton_ = nextOverButton;
        viewport()->setCursor(overTableButton_ ? Qt::PointingHandCursor : Qt::IBeamCursor);
    } else if (overTableButton_) {
        viewport()->setCursor(Qt::PointingHandCursor);
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

void EditorWidget::paintTableBackgrounds(QPainter& painter, const TableMetrics& metrics) {
    const QRectF tRect = tableRect(metrics.firstBlock);
    if (tRect.isEmpty()) {
        return;
    }
    const core::TableStyle& style = styler_->formats().table();
    const qreal bw = style.borderWidth;
    const QRectF inner = tRect.adjusted(bw / 2, bw / 2, -bw / 2, -bw / 2);
    const qreal radius = std::max<qreal>(0, style.radius - bw / 2);
    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(inner, radius, radius);
    painter.setClipPath(clip);
    painter.fillRect(tRect, toQColor(style.background));

    int bodyRowIndex = 0;
    for (QTextBlock b = document()->findBlockByNumber(metrics.firstBlock);
         b.isValid() && b.blockNumber() <= metrics.lastBlock; b = b.next()) {
        const auto r = styler_->renderFor(b);
        if (r.tableDelimiter) {
            continue;
        }
        const QRectF cRect = cellRect(b.blockNumber(), 0);
        const QRectF rowBand(tRect.left(), cRect.top(), tRect.width(), cRect.height());
        if (r.tableHeader) {
            painter.fillRect(rowBand, toQColor(style.headerBackground));
        } else {
            if (bodyRowIndex % 2 == 1) {
                painter.fillRect(rowBand, toQColor(style.stripeBackground));
            }
            ++bodyRowIndex;
        }
    }
    painter.restore();
}

void EditorWidget::paintTableGridAndControls(QPainter& painter, const TableMetrics& metrics) {
    const QRectF tRect = tableRect(metrics.firstBlock);
    if (tRect.isEmpty()) {
        return;
    }
    const core::TableStyle& style = styler_->formats().table();
    const qreal bw = style.borderWidth;
    if (bw > 0) {
        painter.setPen(QPen(toQColor(style.border), bw));
        painter.setBrush(Qt::NoBrush);

        // Vertical column dividers.
        for (std::size_t c = 0; c + 1 < metrics.columnWidths.size(); ++c) {
            const qreal x = cellRect(metrics.firstBlock, static_cast<int>(c)).right();
            painter.drawLine(QPointF(x, tRect.top()), QPointF(x, tRect.bottom()));
        }

        // Horizontal row dividers (below header and between body rows).
        for (QTextBlock b = document()->findBlockByNumber(metrics.firstBlock);
             b.isValid() && b.blockNumber() < metrics.lastBlock; b = b.next()) {
            const auto r = styler_->renderFor(b);
            if (r.tableDelimiter) {
                continue;
            }
            const QRectF cRect = cellRect(b.blockNumber(), 0);
            if (cRect.bottom() < tRect.bottom() - 0.5) {
                painter.drawLine(QPointF(tRect.left(), cRect.bottom()), QPointF(tRect.right(), cRect.bottom()));
            }
        }

        // Outer table border.
        const QRectF inner = tRect.adjusted(bw / 2, bw / 2, -bw / 2, -bw / 2);
        const qreal radius = std::max<qreal>(0, style.radius - bw / 2);
        painter.drawRoundedRect(inner, radius, radius);
    }

    if (isAddRowButtonVisible(metrics.firstBlock)) {
        drawTableControlButton(painter, addRowButtonRect(metrics.firstBlock), style, true);
    }
    if (isAddColumnButtonVisible(metrics.firstBlock)) {
        drawTableControlButton(painter, addColumnButtonRect(metrics.firstBlock), style, true);
    }
    if (hoveredDeleteRowBlock_ >= metrics.firstBlock + 2 && hoveredDeleteRowBlock_ <= metrics.lastBlock &&
        isDeleteRowButtonVisible(hoveredDeleteRowBlock_)) {
        drawTableControlButton(painter, deleteRowButtonRect(hoveredDeleteRowBlock_), style, false);
    }
    if (hoveredDeleteColTable_ == metrics.firstBlock && hoveredDeleteColIndex_ >= 0 &&
        isDeleteColumnButtonVisible(metrics.firstBlock, hoveredDeleteColIndex_)) {
        drawTableControlButton(painter, deleteColumnButtonRect(metrics.firstBlock, hoveredDeleteColIndex_), style,
                               false);
    }
}

void EditorWidget::paintCodeBlockPanels() {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const auto [first, last] = visibleBlockRange();
    const core::CodeStyle& block = styler_->formats().codeBlock();
    const core::CodeStyle& inlineCode = styler_->formats().inlineCode();
    const qreal padding = block.padding;
    const qreal outer = document()->documentMargin() / 2.0;
    const qreal width = viewport()->width() - outer * 2;
    const QPointF offset = contentOffset();
    QRectF panel;
    bool inPanel = false;
    int lastPaintedTableEnd = -1;
    for (QTextBlock b = document()->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
        const auto render = styler_->renderFor(b);
        const QRectF line = blockBoundingGeometry(b).translated(offset);
        if (render.codeBlock && !render.fenceLine) {
            if (!inPanel) {
                panel = {outer, line.top(), width, line.height()};
                inPanel = true;
            } else {
                panel.setBottom(line.bottom());
            }
            continue;
        }
        if (inPanel) {
            drawBox(painter, panel.adjusted(-padding, -padding, padding, padding), block);
            inPanel = false;
        }
        if (render.table && b.blockNumber() > lastPaintedTableEnd) {
            const TableMetrics metrics = styler_->tableMetrics(b);
            if (metrics.firstBlock >= 0) {
                paintTableBackgrounds(painter, metrics);
                lastPaintedTableEnd = metrics.lastBlock;
            }
        }
        paintInlineCodeBoxes(painter, b, render, line.topLeft(), inlineCode);
    }
    if (inPanel) {
        drawBox(painter, panel.adjusted(-padding, -padding, padding, padding), block);
    }
}

void EditorWidget::paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                                        QPointF origin, const core::CodeStyle& style) {
    const QTextLayout* layout = block.layout();
    origin += layout->position();
    for (const auto& run : render.runs) {
        if (run.role != core::Role::InlineCode) {
            continue;
        }
        // The box covers the backticks around the code (the concealed ones reserve the padding).
        const int from = static_cast<int>(run.span.start) - 1;
        const int to = static_cast<int>(run.span.end()) + 1;
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            const int a = std::max(from, line.textStart());
            const int z = std::min(to, line.textStart() + line.textLength());
            if (a >= z) {
                continue;
            }
            const qreal x1 = line.cursorToX(a);
            const qreal x2 = line.cursorToX(z);
            drawBox(painter, QRectF(origin.x() + x1, origin.y() + line.y(), x2 - x1, line.height()), style);
        }
    }
}

void EditorWidget::paintDecorations() {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = styler_->formats().bulletColor();
    const QFontMetricsF metrics(document()->defaultFont());
    const qreal radius = std::max<qreal>(1.5, metrics.xHeight() * 0.32);
    const QPointF offset = contentOffset();
    const qreal bottom = viewport()->height();

    int lastPaintedTableEnd = -1;
    const QTextBlock firstVis = firstVisibleBlock();
    const QTextBlock startBlock = (firstVis.isValid() && firstVis.blockNumber() > 0) ? firstVis.previous() : firstVis;
    for (QTextBlock block = startBlock; block.isValid(); block = block.next()) {
        const QRectF rect = blockBoundingGeometry(block).translated(offset);
        if (rect.top() > bottom) {
            break;
        }
        const auto render = styler_->renderFor(block);
        if (render.table && block.blockNumber() > lastPaintedTableEnd) {
            const TableMetrics tm = styler_->tableMetrics(block);
            if (tm.firstBlock >= 0) {
                paintTableGridAndControls(painter, tm);
                lastPaintedTableEnd = tm.lastBlock;
            }
        }
        if (styler_->isRevealed(block.blockNumber()) || render.decorations.empty()) {
            continue;
        }
        const QTextLayout* layout = block.layout();
        for (const auto& d : render.decorations) {
            const int column = static_cast<int>(d.column);
            const QTextLine line = layout->lineForTextPosition(column);
            if (!line.isValid()) {
                continue;
            }
            const qreal x1 = line.cursorToX(column);
            const qreal x2 = line.cursorToX(column + 1);
            const QPointF centre(
                rect.left() + layout->position().x() + (x1 + x2) / 2.0,
                rect.top() + layout->position().y() + line.y() + line.ascent() - metrics.xHeight() / 2.0);
            if (d.depth % 2 == 0) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
            } else {
                painter.setPen(QPen(color, std::max<qreal>(1.0, radius / 2.5)));
                painter.setBrush(Qt::NoBrush);
            }
            painter.drawEllipse(centre, radius, radius);
        }
    }
}

void EditorWidget::placeCaretInCell(int blockNumber, std::size_t column, bool atEnd) {
    QTextBlock block = document()->findBlockByNumber(blockNumber);
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
        edit.insertText(QString::fromStdU16String(padded));
        edit.endEditBlock();
        block = document()->findBlockByNumber(blockNumber);
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
    setTextCursor(c);
}

void EditorWidget::appendRowToTable(int blockNumber, std::size_t targetColumn) {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.lastBlock < 0 || metrics.columnWidths.empty()) {
        return;
    }
    const QTextBlock last = document()->findBlockByNumber(metrics.lastBlock);
    if (!last.isValid()) {
        return;
    }
    const std::u16string emptyRow = core::makeEmptyTableRow(metrics.columnWidths.size());
    QTextCursor c(last);
    c.beginEditBlock();
    c.movePosition(QTextCursor::EndOfBlock);
    c.insertText(QStringLiteral("\n") + QString::fromStdU16String(emptyRow));
    c.endEditBlock();
    styler_->styleVisible();
    placeCaretInCell(metrics.lastBlock + 1, targetColumn, false);
    updateHoverPos(hoverPos_);
    viewport()->update();
}

void EditorWidget::appendColumnToTable(int blockNumber) {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.empty()) {
        return;
    }
    const std::size_t newColumnIndex = metrics.columnWidths.size();
    QTextCursor c(document());
    c.beginEditBlock();
    for (int bNum = metrics.firstBlock; bNum <= metrics.lastBlock; ++bNum) {
        const QTextBlock b = document()->findBlockByNumber(bNum);
        if (!b.isValid()) {
            continue;
        }
        const auto r = styler_->renderFor(b);
        const QString lineText = b.text();
        const std::u16string updated = core::appendTableColumn(view(lineText), r.tableDelimiter);
        c.setPosition(b.position());
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.insertText(QString::fromStdU16String(updated));
    }
    c.endEditBlock();
    styler_->styleVisible();
    placeCaretInCell(metrics.firstBlock, newColumnIndex, false);
    updateHoverPos(hoverPos_);
    viewport()->update();
}

void EditorWidget::deleteTableRow(int rowBlockNumber, std::size_t targetColumn, bool preferPreviousRow) {
    const QTextBlock rowBlock = document()->findBlockByNumber(rowBlockNumber);
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

    QTextCursor c(document());
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
    viewport()->update();
}

void EditorWidget::deleteTableColumn(int blockNumber, std::size_t column) {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0 || metrics.columnWidths.size() <= 1 || column >= metrics.columnWidths.size()) {
        return;
    }
    int caretRow = metrics.firstBlock;
    const QTextBlock curBlock = textCursor().block();
    if (curBlock.isValid() && curBlock.blockNumber() >= metrics.firstBlock &&
        curBlock.blockNumber() <= metrics.lastBlock && !styler_->renderFor(curBlock).tableDelimiter) {
        caretRow = curBlock.blockNumber();
    }
    const std::size_t newColCount = metrics.columnWidths.size() - 1;
    const std::size_t nextCol = std::min(column, newColCount - 1);

    QTextCursor c(document());
    c.beginEditBlock();
    for (int bNum = metrics.firstBlock; bNum <= metrics.lastBlock; ++bNum) {
        const QTextBlock b = document()->findBlockByNumber(bNum);
        if (!b.isValid()) {
            continue;
        }
        const QString lineText = b.text();
        const std::u16string updated = core::removeTableColumn(view(lineText), column);
        c.setPosition(b.position());
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.insertText(QString::fromStdU16String(updated));
    }
    c.endEditBlock();

    styler_->styleVisible();
    placeCaretInCell(caretRow, nextCol, false);
    updateHoverPos(hoverPos_);
    viewport()->update();
}

void EditorWidget::exitTableBelow(int blockNumber, bool removeEmptyLastRow) {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.lastBlock < 0) {
        return;
    }
    const QTextBlock last = document()->findBlockByNumber(metrics.lastBlock);
    if (!last.isValid()) {
        return;
    }

    int tableEndBlock = metrics.lastBlock;
    const bool deletingRow = removeEmptyLastRow && metrics.lastBlock > metrics.firstBlock + 1;
    const QTextBlock probeEnd = deletingRow ? document()->findBlockByNumber(metrics.lastBlock - 1) : last;
    const QTextBlock existingNext = last.next();

    if (!deletingRow && existingNext.isValid() && existingNext.next().isValid() && existingNext.text().isEmpty()) {
        QTextCursor target(existingNext.next());
        setTextCursor(target);
        return;
    }

    QTextCursor c(document());
    c.beginEditBlock();
    if (deletingRow) {
        c.setPosition(last.position() - 1);
        c.setPosition(last.position() + static_cast<int>(last.text().size()), QTextCursor::KeepAnchor);
        c.removeSelectedText();
        tableEndBlock = probeEnd.blockNumber();
    }
    const QTextBlock endBlock = document()->findBlockByNumber(tableEndBlock);
    const QTextBlock b1 = endBlock.next();
    const QTextBlock b2 = b1.isValid() ? b1.next() : QTextBlock{};
    if (!b1.isValid()) {
        c.setPosition(endBlock.position() + static_cast<int>(endBlock.text().size()));
        c.insertText(QStringLiteral("\n\n"));
    } else if (b1.text().isEmpty() && !b2.isValid()) {
        c.setPosition(b1.position());
        c.insertText(QStringLiteral("\n"));
    } else if (!b1.text().isEmpty()) {
        c.setPosition(endBlock.position() + static_cast<int>(endBlock.text().size()));
        c.insertText(QStringLiteral("\n\n"));
    }
    c.endEditBlock();

    styler_->styleVisible();
    const QTextBlock targetBlock = document()->findBlockByNumber(tableEndBlock + 2);
    if (targetBlock.isValid()) {
        QTextCursor target(targetBlock);
        setTextCursor(target);
    }
    updateHoverPos(hoverPos_);
    viewport()->update();
}

void EditorWidget::exitTableAbove(int blockNumber) {
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }
    const TableMetrics metrics = styler_->tableMetrics(block);
    if (metrics.firstBlock < 0) {
        return;
    }
    if (metrics.firstBlock > 0) {
        QTextCursor above(document()->findBlockByNumber(metrics.firstBlock - 1));
        setTextCursor(above);
        return;
    }
    QTextCursor c(document());
    c.beginEditBlock();
    c.setPosition(0);
    c.insertText(QStringLiteral("\n\n"));
    c.endEditBlock();
    styler_->styleVisible();
    QTextCursor top(document()->firstBlock());
    setTextCursor(top);
    updateHoverPos(hoverPos_);
    viewport()->update();
}

int EditorWidget::tableHitPosition(int blockNumber, std::size_t colIdx, QPointF viewportPos) const {
    const QTextBlock rowBlock = document()->findBlockByNumber(blockNumber);
    if (!rowBlock.isValid() || !rowBlock.layout()) {
        return -1;
    }
    const auto rowRender = styler_->renderFor(rowBlock);
    if (!rowRender.table || rowRender.tableDelimiter || colIdx >= rowRender.cells.size()) {
        return -1;
    }
    const auto& cell = rowRender.cells[colIdx];
    const auto [segStart, segEnd] = cellSegmentRange(rowRender, colIdx, static_cast<int>(rowBlock.text().size()));
    const QRectF rowGeom = blockBoundingGeometry(rowBlock).translated(contentOffset());
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

void EditorWidget::mouseMoveEvent(QMouseEvent* event) {
    QPlainTextEdit::mouseMoveEvent(event);
    if (event->buttons() != Qt::NoButton) {
        updateHoverPos(std::nullopt);
        return;
    }
    updateHoverPos(event->position());
}

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {
        updateHoverPos(pos);
        const auto [first, last] = visibleBlockRange();
        int checkedTableEnd = -1;
        for (QTextBlock b = document()->findBlockByNumber(std::max(0, first - 1));
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
                return;
            }
            if (isAddColumnButtonVisible(tm.firstBlock) && addColumnButtonRect(tm.firstBlock).contains(pos)) {
                appendColumnToTable(tm.firstBlock);
                return;
            }
            if (hoveredDeleteRowBlock_ >= tm.firstBlock + 2 && hoveredDeleteRowBlock_ <= tm.lastBlock &&
                isDeleteRowButtonVisible(hoveredDeleteRowBlock_) &&
                deleteRowButtonRect(hoveredDeleteRowBlock_).contains(pos)) {
                deleteTableRow(hoveredDeleteRowBlock_, 0, false);
                return;
            }
            if (hoveredDeleteColTable_ == tm.firstBlock && hoveredDeleteColIndex_ >= 0 &&
                isDeleteColumnButtonVisible(tm.firstBlock, hoveredDeleteColIndex_) &&
                deleteColumnButtonRect(tm.firstBlock, hoveredDeleteColIndex_).contains(pos)) {
                deleteTableColumn(tm.firstBlock, static_cast<std::size_t>(hoveredDeleteColIndex_));
                return;
            }
            if (tableRect(tm.firstBlock).contains(pos)) {
                // Find the clicked row and column.
                int targetBlock = tm.firstBlock;
                for (int rNum = tm.firstBlock; rNum <= tm.lastBlock; ++rNum) {
                    const QTextBlock rowB = document()->findBlockByNumber(rNum);
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
                setFocus();
                if (const int docPos = tableHitPosition(targetBlock, targetCol, pos); docPos >= 0) {
                    QTextCursor cur = textCursor();
                    cur.setPosition(docPos);
                    setTextCursor(cur);
                } else {
                    placeCaretInCell(targetBlock, targetCol, false);
                }
                return;
            }
        }

        // Clicking above a table at block 0 or below a table at the end of the document creates a plain line outside.
        const QTextBlock firstDocBlock = document()->firstBlock();
        if (firstDocBlock.isValid() && styler_->renderFor(firstDocBlock).table) {
            const QRectF firstTable = tableRect(0);
            if (!firstTable.isEmpty() && pos.y() < firstTable.top() - 4.0) {
                setFocus();
                exitTableAbove(0);
                return;
            }
        }
        const QTextBlock lastDocBlock = document()->lastBlock();
        if (lastDocBlock.isValid() && styler_->renderFor(lastDocBlock).table) {
            const QRectF lastTable = tableRect(lastDocBlock.blockNumber());
            if (!lastTable.isEmpty() && pos.y() > lastTable.bottom()) {
                setFocus();
                exitTableBelow(lastDocBlock.blockNumber(), false);
                return;
            }
        }
    }

    // S010: a click into a line whose markers are collapsed hits the concealed layout. The
    // character index is right, except left of the first visible character, where the hit
    // can land inside the collapsed marker; move it behind the marker.
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    const bool wasConcealed = !styler_->isRevealed(block.blockNumber());
    const auto render = wasConcealed ? styler_->renderFor(block) : core::LineRender{};

    QPlainTextEdit::mousePressEvent(event);

    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {
        QTextCursor after = textCursor();
        if (!after.hasSelection()) {
            const auto afterRender = styler_->renderFor(after.block());
            if (afterRender.table) {
                if (afterRender.tableDelimiter) {
                    placeCaretInCell(std::max(0, after.blockNumber() - 1), 0, false);
                } else if (!afterRender.cells.empty()) {
                    const auto col = static_cast<std::uint32_t>(after.positionInBlock());
                    const std::size_t cellIdx = findCellIndex(afterRender, col);
                    const auto& cell = afterRender.cells[cellIdx];
                    if (col < cell.content.start || col > cell.content.end()) {
                        placeCaretInCell(after.blockNumber(), cellIdx, col > cell.content.end());
                    }
                }
                return;
            }
        }
    }

    if (!wasConcealed || event->button() != Qt::LeftButton || (event->modifiers() & Qt::ShiftModifier)) {
        return;
    }
    QTextCursor c = textCursor();
    if (c.block() != block || c.hasSelection()) {
        return;
    }
    const auto column = static_cast<std::uint32_t>(c.positionInBlock());
    for (const auto& h : render.hidden) {
        if (h.mode == core::HideMode::Collapse && column >= h.span.start && column < h.span.end()) {
            c.setPosition(block.position() + static_cast<int>(h.span.end()));
            setTextCursor(c);
            break;
        }
    }
}

void EditorWidget::contextMenuEvent(QContextMenuEvent* event) {
    const QPointF pos = viewport()->mapFromGlobal(event->globalPos());
    const auto [first, last] = visibleBlockRange();
    int tableFirstBlock = -1;
    int tableRowBlock = -1;
    std::size_t targetCol = 0;
    int checkedTableEnd = -1;
    for (QTextBlock b = document()->findBlockByNumber(std::max(0, first - 1)); b.isValid() && b.blockNumber() <= last;
         b = b.next()) {
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
                const QTextBlock rowB = document()->findBlockByNumber(rNum);
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
        QPlainTextEdit::contextMenuEvent(event);
        return;
    }

    std::unique_ptr<QMenu> menu(createStandardContextMenu());
    menu->addSeparator();
    QAction* addRowAct = menu->addAction(tr("Add Row"));
    QAction* delRowAct = menu->addAction(tr("Delete Row"));
    QAction* addColAct = menu->addAction(tr("Add Column"));
    QAction* delColAct = menu->addAction(tr("Delete Column"));
    const QTextBlock rowB = document()->findBlockByNumber(tableRowBlock);
    const auto rowRender = rowB.isValid() ? styler_->renderFor(rowB) : core::LineRender{};
    const TableMetrics tableMetrics = rowB.isValid() ? styler_->tableMetrics(rowB) : TableMetrics{};
    delRowAct->setEnabled(rowRender.table && !rowRender.tableHeader && !rowRender.tableDelimiter);
    delColAct->setEnabled(tableMetrics.columnWidths.size() >= 2);
    connect(addRowAct, &QAction::triggered, this,
            [this, tableFirstBlock, targetCol] { appendRowToTable(tableFirstBlock, targetCol); });
    connect(delRowAct, &QAction::triggered, this,
            [this, tableRowBlock, targetCol] { deleteTableRow(tableRowBlock, targetCol, false); });
    connect(addColAct, &QAction::triggered, this, [this, tableFirstBlock] { appendColumnToTable(tableFirstBlock); });
    connect(delColAct, &QAction::triggered, this,
            [this, tableFirstBlock, targetCol] { deleteTableColumn(tableFirstBlock, targetCol); });
    menu->exec(event->globalPos());
}

void EditorWidget::keyPressEvent(QKeyEvent* event) {
    QTextCursor cur = textCursor();
    QTextBlock block = cur.block();
    auto render = styler_->renderFor(block);
    if (!render.table) {
        QPlainTextEdit::keyPressEvent(event);
        // If an arrow/page navigation outside a table landed on a table delimiter or concealed pipe, clamp into a cell.
        QTextCursor after = textCursor();
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
        return;
    }

    if (render.tableDelimiter) {
        placeCaretInCell(block.blockNumber(), 0, false);
        cur = textCursor();
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
            const QTextBlock candidate = document()->findBlockByNumber(b);
            if (candidate.isValid() && !styler_->renderFor(candidate).tableDelimiter) {
                return b;
            }
        }
        return -1;
    };
    const auto prevContentRow = [&](int fromBlock) -> int {
        for (int b = fromBlock - 1; b >= metrics.firstBlock; --b) {
            const QTextBlock candidate = document()->findBlockByNumber(b);
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
        return;
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
        return;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (ctrl) {
            if (shift) {
                exitTableAbove(metrics.firstBlock);
            } else {
                exitTableBelow(metrics.firstBlock, false);
            }
            return;
        }
        if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
            placeCaretInCell(nextRow, cellIdx, false);
        } else if (isEmptyTableBodyRow(render)) {
            exitTableBelow(metrics.firstBlock, true);
        } else {
            appendRowToTable(metrics.firstBlock, cellIdx);
        }
        return;
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
                    setTextCursor(cur);
                    return;
                }
            }
        }
        if (const int prevRow = prevContentRow(block.blockNumber()); prevRow >= 0) {
            placeCaretInCell(prevRow, cellIdx, false);
        } else if (metrics.firstBlock > 0) {
            QTextCursor above(document()->findBlockByNumber(metrics.firstBlock - 1));
            setTextCursor(above);
        }
        return;
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
                    setTextCursor(cur);
                    return;
                }
            }
        }
        if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
            placeCaretInCell(nextRow, cellIdx, false);
        } else if (metrics.lastBlock + 1 < document()->blockCount()) {
            QTextCursor below(document()->findBlockByNumber(metrics.lastBlock + 1));
            setTextCursor(below);
        }
        return;
    }

    if (key == Qt::Key_Left && !shift) {
        if (posInLine > cell.content.start) {
            std::uint32_t nextPos = posInLine - 1;
            if (nextPos > cell.content.start &&
                block.text().mid(static_cast<int>(nextPos - 1), 2) == QStringLiteral("\\|")) {
                --nextPos;
            }
            cur.setPosition(block.position() + static_cast<int>(nextPos));
            setTextCursor(cur);
        } else if (cellIdx > 0) {
            placeCaretInCell(block.blockNumber(), cellIdx - 1, true);
        } else if (const int prevRow = prevContentRow(block.blockNumber()); prevRow >= 0) {
            placeCaretInCell(prevRow, totalCols - 1, true);
        } else if (metrics.firstBlock > 0) {
            QTextCursor above(document()->findBlockByNumber(metrics.firstBlock - 1));
            above.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(above);
        }
        return;
    }

    if (key == Qt::Key_Right && !shift) {
        if (posInLine < cell.content.end()) {
            std::uint32_t nextPos = posInLine + 1;
            if (nextPos < cell.content.end() &&
                block.text().mid(static_cast<int>(posInLine), 2) == QStringLiteral("\\|")) {
                ++nextPos;
            }
            cur.setPosition(block.position() + static_cast<int>(nextPos));
            setTextCursor(cur);
        } else if (cellIdx + 1 < totalCols) {
            placeCaretInCell(block.blockNumber(), cellIdx + 1, false);
        } else if (const int nextRow = nextContentRow(block.blockNumber()); nextRow >= 0) {
            placeCaretInCell(nextRow, 0, false);
        } else if (metrics.lastBlock + 1 < document()->blockCount()) {
            QTextCursor below(document()->findBlockByNumber(metrics.lastBlock + 1));
            setTextCursor(below);
        }
        return;
    }

    if (key == Qt::Key_Home) {
        cur.setPosition(block.position() + static_cast<int>(cell.content.start),
                        shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
        setTextCursor(cur);
        return;
    }

    if (key == Qt::Key_End) {
        cur.setPosition(block.position() + static_cast<int>(cell.content.end()),
                        shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
        setTextCursor(cur);
        return;
    }

    if (key == Qt::Key_Backspace) {
        if (ctrl && alt) {
            deleteTableColumn(metrics.firstBlock, cellIdx);
            return;
        }
        if (ctrl && shift) {
            if (render.tableHeader) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
            } else {
                deleteTableRow(block.blockNumber(), cellIdx, true);
            }
            return;
        }
        if (!cur.hasSelection()) {
            if (isEmptyTableBodyRow(render)) {
                deleteTableRow(block.blockNumber(), cellIdx, true);
                return;
            }
            if (isEmptyTableColumn(document(), styler_, metrics, cellIdx)) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
                return;
            }
            if (posInLine <= cell.content.start) {
                return;
            }
            if (posInLine >= cell.content.start + 2 &&
                block.text().mid(static_cast<int>(posInLine - 2), 2) == QStringLiteral("\\|")) {
                cur.setPosition(block.position() + static_cast<int>(posInLine - 2), QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                return;
            }
        } else {
            const int selStart = std::min(cur.position(), cur.anchor()) - block.position();
            const int selEnd = std::max(cur.position(), cur.anchor()) - block.position();
            if (selStart < static_cast<int>(cell.content.start) || selEnd > static_cast<int>(cell.content.end())) {
                return;
            }
        }
    }

    if (key == Qt::Key_Delete) {
        if (ctrl && alt) {
            deleteTableColumn(metrics.firstBlock, cellIdx);
            return;
        }
        if (ctrl && shift) {
            if (render.tableHeader) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
            } else {
                deleteTableRow(block.blockNumber(), cellIdx, false);
            }
            return;
        }
        if (!cur.hasSelection()) {
            if (isEmptyTableBodyRow(render)) {
                deleteTableRow(block.blockNumber(), cellIdx, false);
                return;
            }
            if (isEmptyTableColumn(document(), styler_, metrics, cellIdx)) {
                deleteTableColumn(metrics.firstBlock, cellIdx);
                return;
            }
            if (posInLine >= cell.content.end()) {
                return;
            }
            if (posInLine + 2 <= cell.content.end() &&
                block.text().mid(static_cast<int>(posInLine), 2) == QStringLiteral("\\|")) {
                cur.setPosition(block.position() + static_cast<int>(posInLine + 2), QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                return;
            }
        } else {
            const int selStart = std::min(cur.position(), cur.anchor()) - block.position();
            const int selEnd = std::max(cur.position(), cur.anchor()) - block.position();
            if (selStart < static_cast<int>(cell.content.start) || selEnd > static_cast<int>(cell.content.end())) {
                return;
            }
        }
    }

    if (event->text() == QStringLiteral("|")) {
        if (posInLine < cell.content.start) {
            cur.setPosition(block.position() + static_cast<int>(cell.content.start));
            setTextCursor(cur);
        }
        textCursor().insertText(QStringLiteral("\\|"));
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

}  // namespace hitokoto::ui
