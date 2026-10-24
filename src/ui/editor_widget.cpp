// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "qt_bridge.hpp"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

EditorWidget::EditorWidget(const core::ElementRegistry& registry, QWidget* parent) : QPlainTextEdit(parent) {
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
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

void EditorWidget::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    updateMargins();
    styler_->styleVisible();
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QPlainTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
}

void EditorWidget::paintEvent(QPaintEvent* event) {
    QPlainTextEdit::paintEvent(event);
    paintDecorations();
    emit painted();
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
        if (styler_->isRevealed(block.blockNumber())) {
            continue;
        }
        const auto render = styler_->renderFor(block);
        if (render.decorations.empty()) {
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

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    // S010: a click into a line whose markers are collapsed hits the concealed layout. The
    // character index is right, except left of the first visible character, where the hit
    // can land inside the collapsed marker; move it behind the marker.
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    const bool wasConcealed = !styler_->isRevealed(block.blockNumber());
    const auto render = wasConcealed ? styler_->renderFor(block) : core::LineRender{};

    QPlainTextEdit::mousePressEvent(event);

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

}  // namespace hitokoto::ui
