// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "qt_bridge.hpp"
#include "table_embedder.hpp"

#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
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

EditorWidget::EditorWidget(const core::ElementRegistry& registry, QWidget* parent) : QTextEdit(parent) {
    // ADR 0003: our own layout on a plain document; QTextEdit connects to the layout of the
    // document it is given, so the layout is set first.
    auto* doc = new QTextDocument(this);
    layout_ = new BlockLayout(doc);
    doc->setDocumentLayout(layout_);
    setDocument(doc);
    setAcceptRichText(false);
    styler_ = new BlockStyler(document(), registry, [this] { return visibleBlockRange(); }, this);
    tables_ = std::make_unique<TableEmbedder>(this, styler_, layout_);
    connect(layout_, &BlockLayout::blockHeightChanged, this, &EditorWidget::keepContentStill);
    connect(layout_, &BlockLayout::batchFinished, this, &EditorWidget::applyPendingShift);

    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setMouseTracking(true);

    connect(document(), &QTextDocument::contentsChange, this, [this](int, int, int) {
        if (!styler_->isBusy()) {
            ++editCount_;
        }
    });
    connect(this, &QTextEdit::cursorPositionChanged, this, &EditorWidget::updateReveal);
    connect(this, &QTextEdit::cursorPositionChanged, tables_.get(), &TableEmbedder::caretMoved);
    connect(document(), &QTextDocument::contentsChange, tables_.get(), &TableEmbedder::documentChanged);
    connect(styler_, &BlockStyler::completed, tables_.get(), &TableEmbedder::scheduleRefresh);
    connect(layout_, &BlockLayout::documentSizeChanged, tables_.get(), &TableEmbedder::reposition);
    connect(this, &QTextEdit::selectionChanged, this, &EditorWidget::updateReveal);
}

EditorWidget::~EditorWidget() = default;

QTableView* EditorWidget::tableViewAt(int blockNumber) const {
    return tables_->viewAt(blockNumber);
}

bool EditorWidget::isTableSourceShown(int blockNumber) const {
    return tables_->isSourceShown(blockNumber);
}

QRectF EditorWidget::blockBoundingGeometry(const QTextBlock& block) const {
    return layout_->blockBoundingRect(block);
}

QPointF EditorWidget::contentOffset() const {
    return {-static_cast<qreal>(horizontalScrollBar()->value()), -static_cast<qreal>(verticalScrollBar()->value())};
}

QTextBlock EditorWidget::firstVisibleBlock() const {
    QTextBlock block = layout_->blockAt(verticalScrollBar()->value());
    return block.isValid() ? block : document()->lastBlock();
}

void EditorWidget::keepContentStill(qreal top, qreal oldHeight, qreal newHeight) {
    // A block wholly above the viewport changed height (laid out lazily or restyled): scroll by
    // the same amount so the text on screen does not move. Blocks on screen are left alone; they
    // are what the reader sees. Inside a layout batch the scroll range is not updated yet; the
    // shift is applied when the batch ends.
    const QScrollBar* bar = verticalScrollBar();
    if (bar->value() <= 0 || top + oldHeight > bar->value() + 0.5) {
        return;
    }
    pendingShift_ += newHeight - oldHeight;
    if (!layout_->inBatch()) {
        applyPendingShift();
    }
}

void EditorWidget::applyPendingShift() {
    if (applyingShift_) {
        return;  // scrolling restyles; a shift that causes reaches us again
    }
    const int shift = static_cast<int>(std::lround(pendingShift_));
    pendingShift_ -= shift;  // keep the fraction: lazily laid-out blocks change by parts of a pixel
    if (shift != 0) {
        applyingShift_ = true;
        QScrollBar* bar = verticalScrollBar();
        bar->setMaximum(std::max(bar->maximum(), bar->value() + shift));
        bar->setValue(bar->value() + shift);
        applyingShift_ = false;
    }
}

EditorWidget::ScrollAnchor EditorWidget::captureAnchor() const {
    if (verticalScrollBar()->value() <= 0) {
        return {};
    }
    const QTextBlock block = firstVisibleBlock();
    const QRectF rect = blockBoundingGeometry(block);
    const qreal into = verticalScrollBar()->value() - rect.top();
    return {block, rect.height() > 0 ? into / rect.height() : 0.0};
}

void EditorWidget::restoreAnchor(const ScrollAnchor& anchor) {
    // A new width or font re-estimates every block, so the same scroll value would show other
    // text; scroll back to the line that was at the top.
    if (!anchor.block.isValid()) {
        return;
    }
    const QRectF rect = blockBoundingGeometry(anchor.block);
    pendingShift_ = 0;
    verticalScrollBar()->setValue(static_cast<int>(std::lround(rect.top() + anchor.fraction * rect.height())));
}

void EditorWidget::applyTheme(const core::Theme& theme) {
    const ScrollAnchor anchor = captureAnchor();
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
    tables_->setFormats(&styler_->formats());
    tables_->setAvailableWidth(availableTableWidth());
    if (!wasModified) {
        document()->setModified(false);
    }
    restoreAnchor(anchor);
    viewport()->update();
}

int EditorWidget::availableTableWidth() const {
    // The text column, with the vertical scroll bar reserved even while it is hidden: a width that
    // changed with the bar would change the table's wrapping and height, and so the bar again
    // (review objection O2).
    double width = static_cast<double>(viewport()->width());
    const QScrollBar* bar = verticalScrollBar();
    const bool barTakesSpace = verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff &&
                               style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) == 0;
    if (barTakesSpace && !bar->isVisibleTo(this)) {
        width -= static_cast<double>(bar->sizeHint().width());
    }
    width -= 2.0 * document()->documentMargin();
    return std::max(120, static_cast<int>(width));
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
        top += blockBoundingGeometry(block).height();
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

void EditorWidget::resizeEvent(QResizeEvent* event) {
    const ScrollAnchor anchor = captureAnchor();
    QTextEdit::resizeEvent(event);
    updateMargins();
    tables_->setAvailableWidth(availableTableWidth());
    restoreAnchor(anchor);
    styler_->styleVisible();
    // The widgets follow at once; finding tables waits for the event loop, so it never runs in
    // the middle of a document change (opening a file scrolls while it replaces the text).
    tables_->reposition();
    tables_->scheduleRefresh();
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
    tables_->reposition();
    tables_->scheduleRefresh();
}

void EditorWidget::paintEvent(QPaintEvent* event) {
    paintCodeBlockPanels();
    QTextEdit::paintEvent(event);
    paintDecorations();
    emit painted();
}

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier &&
        tables_->handleEditorClick(event->position().toPoint())) {
        event->accept();  // S018: a line above or below a table at the document's edge
        return;
    }
    // S010: a click into a line whose markers are collapsed hits the concealed layout. The
    // character index is right, except left of the first visible character, where the hit
    // can land inside the collapsed marker; move it behind the marker.
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    const bool wasConcealed = !styler_->isRevealed(block.blockNumber());
    const auto render = wasConcealed ? styler_->renderFor(block) : core::LineRender{};

    QTextEdit::mousePressEvent(event);

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

void EditorWidget::keyPressEvent(QKeyEvent* event) {
    if (tables_->handleEditorKey(event)) {
        return;
    }
    const TableEmbedder::Typing typing(tables_.get());
    QTextEdit::keyPressEvent(event);
}

void EditorWidget::inputMethodEvent(QInputMethodEvent* event) {
    const TableEmbedder::Typing typing(tables_.get());
    QTextEdit::inputMethodEvent(event);
}

void EditorWidget::insertFromMimeData(const QMimeData* source) {
    const TableEmbedder::Typing typing(tables_.get());
    QTextEdit::insertFromMimeData(source);
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

    for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next()) {
        const QRectF rect = blockBoundingGeometry(block).translated(offset);
        if (rect.top() > bottom) {
            break;
        }
        const auto render = styler_->renderFor(block);
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
