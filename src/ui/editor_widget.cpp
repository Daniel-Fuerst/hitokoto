// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "qt_bridge.hpp"
#include "table_controller.hpp"
#include "table_painter.hpp"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

namespace {

void drawBox(QPainter& painter, const QRectF& rect, const core::CodeStyle& style) {
    const qreal border = style.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(style.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style.background));
    const QRectF inner = rect.adjusted(border / 2, border / 2, -border / 2, -border / 2);
    const qreal radius = std::max<qreal>(0, style.radius - border / 2);
    painter.drawRoundedRect(inner, radius, radius);
}

}  // namespace

EditorWidget::EditorWidget(const core::ElementRegistry& registry, QWidget* parent)
    : QPlainTextEdit(parent),
      styler_(new BlockStyler(
          document(), registry, [this] { return visibleBlockRange(); }, this)),
      tableController_(std::make_unique<TableController>(this, styler_)) {
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setMouseTracking(true);

    connect(document(), &QTextDocument::contentsChange, this, [this](int, int, int) {
        if (!styler_->isBusy()) {
            ++editCount_;
        }
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &EditorWidget::updateReveal);
    connect(this, &QPlainTextEdit::selectionChanged, this, &EditorWidget::updateReveal);
}

EditorWidget::~EditorWidget() = default;

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
    styler_->setFormats(formats);
    styler_->setAvailableWidth(availableTableWidth(formats.baseFont().pointSizeF()));
    if (!wasModified) {
        document()->setModified(false);
    }
    viewport()->update();
}

double EditorWidget::availableTableWidth(double basePointSize) const {
    const double viewW = static_cast<double>(viewport()->width());
    const double gutterPad = std::clamp(basePointSize * 1.3, 14.0, 22.0) + 6.0;
    const double contentW = maxWidth_ > 0 ? std::min(viewW - padding_ * 2.0, maxWidth_) : viewW - padding_ * 2.0;
    return std::max(120.0, contentW - gutterPad * 2.0);
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

QRectF EditorWidget::tableRect(int blockNumber) const {
    return tableController_->tableRect(blockNumber);
}

QRectF EditorWidget::cellRect(int blockNumber, int column) const {
    return tableController_->cellRect(blockNumber, column);
}

QRectF EditorWidget::addRowButtonRect(int blockNumber) const {
    return tableController_->addRowButtonRect(blockNumber);
}

QRectF EditorWidget::addColumnButtonRect(int blockNumber) const {
    return tableController_->addColumnButtonRect(blockNumber);
}

QRectF EditorWidget::deleteRowButtonRect(int blockNumber) const {
    return tableController_->deleteRowButtonRect(blockNumber);
}

QRectF EditorWidget::deleteColumnButtonRect(int blockNumber, int column) const {
    return tableController_->deleteColumnButtonRect(blockNumber, column);
}

bool EditorWidget::isAddRowButtonVisible(int blockNumber) const {
    return tableController_->isAddRowButtonVisible(blockNumber);
}

bool EditorWidget::isAddColumnButtonVisible(int blockNumber) const {
    return tableController_->isAddColumnButtonVisible(blockNumber);
}

bool EditorWidget::isDeleteRowButtonVisible(int blockNumber) const {
    return tableController_->isDeleteRowButtonVisible(blockNumber);
}

bool EditorWidget::isDeleteColumnButtonVisible(int blockNumber, int column) const {
    return tableController_->isDeleteColumnButtonVisible(blockNumber, column);
}

bool EditorWidget::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::Leave) {
        tableController_->updateHoverPos(std::nullopt);
    }
    return QPlainTextEdit::viewportEvent(event);
}

void EditorWidget::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    updateMargins();
    styler_->setAvailableWidth(availableTableWidth(styler_->formats().baseFont().pointSizeF()));
    styler_->styleVisible();
    tableController_->refreshHoverState();
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QPlainTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
    tableController_->refreshHoverState();
}

void EditorWidget::paintEvent(QPaintEvent* event) {
    paintCodeBlockPanels();
    QPlainTextEdit::paintEvent(event);
    paintDecorations();
    emit painted();
}

void EditorWidget::mouseMoveEvent(QMouseEvent* event) {
    if (tableController_->handleMouseMove(event)) {
        return;
    }
    QPlainTextEdit::mouseMoveEvent(event);
}

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    if (tableController_->handleMousePress(event)) {
        return;
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
        tableController_->handlePostNavigation(nullptr);
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

void EditorWidget::mouseReleaseEvent(QMouseEvent* event) {
    tableController_->handleMouseRelease(event);
    QPlainTextEdit::mouseReleaseEvent(event);
}

void EditorWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (tableController_->handleContextMenu(event)) {
        return;
    }
    QPlainTextEdit::contextMenuEvent(event);
}

void EditorWidget::keyPressEvent(QKeyEvent* event) {
    if (tableController_->handleKeyPress(event)) {
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
    tableController_->handlePostNavigation(event);
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
                TablePainter::paintTableBackgrounds(painter, document(), styler_, *tableController_, metrics);
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
    for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next()) {
        const QRectF rect = blockBoundingGeometry(block).translated(offset);
        if (rect.top() > bottom) {
            break;
        }
        const auto render = styler_->renderFor(block);
        if (render.table && block.blockNumber() > lastPaintedTableEnd) {
            const TableMetrics tm = styler_->tableMetrics(block);
            if (tm.firstBlock >= 0) {
                TablePainter::paintTableGridAndControls(painter, document(), styler_, *tableController_, tm);
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

}  // namespace hitokoto::ui
