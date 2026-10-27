// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "qt_bridge.hpp"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
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
}  // namespace

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
    struct PendingBadge {
        QRectF rect;
        QString text;
    };
    std::vector<PendingBadge> badges;

    for (QTextBlock b = document()->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
        const auto render = styler_->renderFor(b);
        const QRectF line = blockBoundingGeometry(b).translated(offset);
        QTextLayout* layout = b.layout();
        if (layout != nullptr) {
            const qreal targetY = render.codeBlock ? 3.0 : 0.0;
            qreal curY = targetY;
            for (int i = 0; i < layout->lineCount(); ++i) {
                QTextLine l = layout->lineAt(i);
                l.setPosition(QPointF(l.x(), curY));
                curY += l.height();
            }
        }
        if (render.codeBlock) {
            if (!inPanel) {
                panel = {outer, line.top(), width, line.height()};
                inPanel = true;
            } else {
                panel.setBottom(line.bottom());
            }
            if (render.fenceLine && !styler_->isRevealed(b.blockNumber()) && !styler_->blockStateBefore(b).active()) {
                const QString text = b.text();
                int idx = 0;
                while (idx < text.size() && (text[idx] == QLatin1Char(' ') || text[idx] == QLatin1Char('\t'))) {
                    ++idx;
                }
                if (idx < text.size() && (text[idx] == QLatin1Char('`') || text[idx] == QLatin1Char('~'))) {
                    const QChar fenceChar = text[idx];
                    while (idx < text.size() && text[idx] == fenceChar) {
                        ++idx;
                    }
                    QString info = text.mid(idx).trimmed();
                    if (!info.isEmpty()) {
                        info[0] = info[0].toUpper();
                        badges.push_back({line, info});
                    }
                }
            }
            continue;
        }
        if (inPanel) {
            drawBox(painter, panel.adjusted(-padding, 3, padding, 3), block);
            inPanel = false;
        }
        paintInlineCodeBoxes(painter, b, render, line.topLeft(), inlineCode);
    }
    if (inPanel) {
        drawBox(painter, panel.adjusted(-padding, 3, padding, 3), block);
    }

    if (!badges.empty()) {
        painter.setFont(styler_->formats().baseFont());
        painter.setPen(toQColor(block.marker));
        for (const auto& badge : badges) {
            const QRectF badgeRect(outer, badge.rect.top() + 3, width, badge.rect.height());
            painter.drawText(badgeRect, Qt::AlignRight | Qt::AlignVCenter, badge.text);
        }
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
