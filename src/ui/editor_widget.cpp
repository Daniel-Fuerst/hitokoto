// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "floating_toolbar.hpp"
#include "properties_embedder.hpp"
#include "qt_bridge.hpp"
#include "smooth_scroller.hpp"
#include "table_embedder.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QInputMethodEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTableView>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QToolTip>

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
    properties_ = std::make_unique<PropertiesEmbedder>(this, styler_, layout_);
    connect(layout_, &BlockLayout::blockHeightChanged, this, &EditorWidget::keepContentStill);
    connect(layout_, &BlockLayout::batchFinished, this, &EditorWidget::applyPendingShift);

    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setCursor(Qt::IBeamCursor);

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
    connect(layout_, &BlockLayout::blocksGoneUnreported, tables_.get(), &TableEmbedder::forgetAll);
    connect(this, &QTextEdit::selectionChanged, this, &EditorWidget::updateReveal);
    // S024: front matter as properties.
    connect(this, &QTextEdit::cursorPositionChanged, properties_.get(), &PropertiesEmbedder::caretMoved);
    connect(this, &QTextEdit::selectionChanged, properties_.get(), &PropertiesEmbedder::caretMoved);
    connect(document(), &QTextDocument::contentsChange, properties_.get(), &PropertiesEmbedder::documentChanged);
    connect(styler_, &BlockStyler::frontMatterConfirmed, properties_.get(), &PropertiesEmbedder::schedule);
    connect(styler_, &BlockStyler::completed, properties_.get(), &PropertiesEmbedder::schedule);
    connect(layout_, &BlockLayout::documentSizeChanged, properties_.get(), &PropertiesEmbedder::reposition);
    connect(layout_, &BlockLayout::blocksGoneUnreported, properties_.get(), &PropertiesEmbedder::forgetAll);

    // S026: floating toolbar on the right-hand side.
    toolbar_ = new FloatingToolbar(this);
    connect(toolbar_, &FloatingToolbar::createTableRequested, this, &EditorWidget::insertTable);
    connect(toolbar_, &FloatingToolbar::createInlineCodeRequested, this, &EditorWidget::insertInlineCode);
    positionToolbar();

    // S027: smooth scrolling on mouse wheel events.
    verticalScroller_ = std::make_unique<SmoothScroller>(verticalScrollBar(), this);
}

EditorWidget::~EditorWidget() = default;

void EditorWidget::setSmoothScrolling(bool on) noexcept {
    smoothScrolling_ = on;
    if (!smoothScrolling_ && verticalScroller_) {
        verticalScroller_->stop();
    }
}

SmoothScroller* EditorWidget::smoothScroller() const noexcept {
    return verticalScroller_.get();
}

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
        if (verticalScroller_) {
            verticalScroller_->shift(shift);
        }
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
    properties_->setAvailableWidth(availableTableWidth());
    properties_->setLook(styler_->formats().properties(), styler_->formats().propertiesFont(),
                         styler_->formats().propertiesBadgeFont());
    toolbar_->applyStyle(core::toolbarStyle(theme));
    positionToolbar();
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

void EditorWidget::positionToolbar() {
    if (toolbar_ == nullptr) {
        return;
    }
    const QScrollBar* bar = verticalScrollBar();
    const bool barTakesSpace = verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff &&
                               style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) == 0;
    const int barWidth = barTakesSpace ? bar->sizeHint().width() : 0;
    const int margin = std::max(0, static_cast<int>(std::lround(toolbar_->toolbarStyle().margin)));
    const int x = std::max(0, width() - barWidth - margin - toolbar_->width());
    const int y = std::max(0, (height() - toolbar_->height()) / 2);
    toolbar_->move(x, y);
    toolbar_->raise();
}

void EditorWidget::resizeEvent(QResizeEvent* event) {
    if (verticalScroller_) {
        verticalScroller_->stop();
    }
    const ScrollAnchor anchor = captureAnchor();
    QTextEdit::resizeEvent(event);
    updateMargins();
    positionToolbar();
    tables_->setAvailableWidth(availableTableWidth());
    properties_->setAvailableWidth(availableTableWidth());
    restoreAnchor(anchor);
    styler_->styleVisible();
    // The widgets follow at once; finding tables waits for the event loop, so it never runs in
    // the middle of a document change (opening a file scrolls while it replaces the text).
    tables_->reposition();
    properties_->reposition();
    tables_->scheduleRefresh();
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
    tables_->reposition();
    properties_->reposition();
    tables_->scheduleRefresh();
}

namespace {
bool isBrowserUrl(const QString& href) {
    const QUrl url(href);
    if (!url.isValid()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}
}  // namespace

void EditorWidget::paintEvent(QPaintEvent* event) {
    paintCodeBlockPanels();
    QTextEdit::paintEvent(event);
    paintDecorations();
    emit painted();
}

void EditorWidget::mouseReleaseEvent(QMouseEvent* event) {
    QTextEdit::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        properties_->setMouseHeld(false);  // S024 review O6: the block may switch now
    }
}

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    if (verticalScroller_) {
        verticalScroller_->stop();
    }
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    const bool wasConcealed = block.isValid() && !styler_->isRevealed(block.blockNumber());

    if (event->button() == Qt::LeftButton) {
        const bool wantsOpen =
            (wasConcealed && event->modifiers() == Qt::NoModifier) || (event->modifiers() & Qt::ControlModifier);
        if (wantsOpen) {
            const QString href = anchorAt(event->position().toPoint());
            if (!href.isEmpty() && isBrowserUrl(href)) {
                QDesktopServices::openUrl(QUrl(href));
                event->accept();
                return;
            }
        }
        properties_->setMouseHeld(true);  // S024 review O6
    }
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier &&
        tables_->handleEditorClick(event->position().toPoint())) {
        event->accept();  // S018: a line above or below a table at the document's edge
        return;
    }
    // S010: a click into a line whose markers are collapsed hits the concealed layout. The
    // character index is right, except left of the first visible character, where the hit
    // can land inside the collapsed marker; move it behind the marker.
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

void EditorWidget::mouseMoveEvent(QMouseEvent* event) {
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    const bool isConcealed = block.isValid() && !styler_->isRevealed(block.blockNumber());
    const QString href = anchorAt(event->position().toPoint());
    const bool canOpen = isBrowserUrl(href) && (isConcealed || (event->modifiers() & Qt::ControlModifier));
    if (canOpen) {
        viewport()->setCursor(Qt::PointingHandCursor);
    } else {
        viewport()->setCursor(Qt::IBeamCursor);
    }
    QTextEdit::mouseMoveEvent(event);
}

void EditorWidget::wheelEvent(QWheelEvent* event) {
    if (!smoothScrolling_) {
        QTextEdit::wheelEvent(event);
        return;
    }

    if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) != 0) {
        if (verticalScroller_) {
            verticalScroller_->stop();
        }
        QTextEdit::wheelEvent(event);
        return;
    }

    const int angleY = event->angleDelta().y();
    double delta = 0.0;
    if (angleY != 0) {
        const int lineSpacing =
            fontMetrics().lineSpacing() > 0 ? fontMetrics().lineSpacing() : verticalScrollBar()->singleStep();
        const int lines = QApplication::wheelScrollLines() > 0 ? QApplication::wheelScrollLines() : 3;
        const double distancePerNotch = static_cast<double>(lines * (lineSpacing > 0 ? lineSpacing : 20));
        delta = -(static_cast<double>(angleY) / 120.0) * distancePerNotch;
    } else if (!event->pixelDelta().isNull() && event->pixelDelta().y() != 0) {
        delta = -static_cast<double>(event->pixelDelta().y());
    }

    if (std::abs(delta) > 0.001 && verticalScrollBar() != nullptr &&
        verticalScrollBar()->maximum() > verticalScrollBar()->minimum()) {
        if (verticalScroller_) {
            verticalScroller_->scroll(delta);
        }
        event->accept();
        return;
    }

    QTextEdit::wheelEvent(event);
}

void EditorWidget::leaveEvent(QEvent* event) {
    viewport()->setCursor(Qt::IBeamCursor);
    QTextEdit::leaveEvent(event);
}

bool EditorWidget::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        wheelEvent(static_cast<QWheelEvent*>(event));
        return event->isAccepted();
    }
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const QTextCharFormat fmt = formatAt(help->pos());
        if (!fmt.toolTip().isEmpty()) {
            QToolTip::showText(help->globalPos(), fmt.toolTip(), viewport());
            return true;
        }
        QToolTip::hideText();
    }
    return QTextEdit::viewportEvent(event);
}

QTextCharFormat EditorWidget::formatAt(const QPoint& pos) const {
    const QPointF docPos(pos.x() + horizontalScrollBar()->value(), pos.y() + verticalScrollBar()->value());
    const QTextBlock block = layout_->blockAt(docPos.y());
    if (!block.isValid() || layout_->reservedHeight(block) > 0) {
        return {};
    }
    layout_->ensureBlockLayout(block);
    const QRectF rect = layout_->blockBoundingRect(block);
    const QPointF local = docPos - rect.topLeft();
    const QTextLayout* layout = block.layout();
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        if (local.y() >= line.y() && local.y() < line.y() + line.height()) {
            if (local.x() < line.x() || local.x() >= line.x() + line.naturalTextWidth()) {
                return {};
            }
            const int col = line.xToCursor(local.x(), QTextLine::CursorOnCharacter);
            if (col < 0 || col >= block.length()) {
                return {};
            }
            QTextCharFormat merged;
            for (const auto& range : layout->formats()) {
                if (col >= range.start && col < range.start + range.length) {
                    merged.merge(range.format);
                }
            }
            return merged;
        }
    }
    return {};
}

QString EditorWidget::anchorAt(const QPoint& pos) const {
    const QTextCharFormat fmt = formatAt(pos);
    return fmt.isAnchor() ? fmt.anchorHref() : QString{};
}

void EditorWidget::keyPressEvent(QKeyEvent* event) {
    if (verticalScroller_) {
        verticalScroller_->stop();
    }
    if (tables_->handleEditorKey(event) || properties_->handleEditorKey(event)) {
        return;
    }
    if (handleBulletKey(event)) {
        return;
    }
    const TableEmbedder::Typing typing(tables_.get());
    const QString typed = event->text();
    if (!typed.isEmpty() && typed.at(0).isPrint() &&
        (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) == 0) {
        tables_->beforeWritingBelow();
    }
    QTextEdit::keyPressEvent(event);
}

void EditorWidget::inputMethodEvent(QInputMethodEvent* event) {
    const TableEmbedder::Typing typing(tables_.get());
    if (!event->commitString().isEmpty()) {
        tables_->beforeWritingBelow();
    }
    QTextEdit::inputMethodEvent(event);
}

void EditorWidget::insertFromMimeData(const QMimeData* source) {
    const TableEmbedder::Typing typing(tables_.get());
    tables_->beforeWritingBelow();
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
    struct Badge {
        QRectF rect;
        QString text;
    };
    std::vector<Badge> badges;

    // The panel spans its lines from the opening to the closing fence; the margin above and below
    // is part of the first and last line's geometry (BlockLayout, S021), so it stays outside.
    for (QTextBlock b = document()->findBlockByNumber(first); b.isValid() && b.blockNumber() <= last; b = b.next()) {
        if (!b.isVisible()) {
            continue;  // takes no space (an embedded element's line): nothing to paint, nothing to render
        }
        const auto render = styler_->renderFor(b);
        const QRectF rect = blockBoundingGeometry(b).translated(offset);
        if (render.codeBlock) {
            const qreal top = rect.top() + layout_->spaceAbove(b);
            const qreal bottom = rect.bottom() - layout_->spaceBelow(b);
            if (!inPanel) {
                panel = {outer, top, width, bottom - top};
                inPanel = true;
            } else {
                panel.setBottom(bottom);
            }
            if (const QString badge = codeBlockBadge(b); !badge.isEmpty()) {
                const QTextLine line = b.layout()->lineAt(0);
                badges.push_back({QRectF(outer, rect.top() + line.y(), width, line.height()), badge});
            }
            continue;
        }
        if (inPanel) {
            drawBox(painter, panel.adjusted(-padding, 0, padding, 0), block);
            inPanel = false;
        }
        paintInlineCodeBoxes(painter, b, render, rect.topLeft(), inlineCode);
    }
    if (inPanel) {
        drawBox(painter, panel.adjusted(-padding, 0, padding, 0), block);
    }

    if (!badges.empty()) {
        painter.setFont(styler_->formats().badgeFont());
        painter.setPen(toQColor(block.badgeText));
        for (const auto& badge : badges) {
            painter.drawText(badge.rect, Qt::AlignRight | Qt::AlignVCenter, badge.text);
        }
    }
}

QString EditorWidget::codeBlockBadge(const QTextBlock& block) {
    // The language comes from core, which recognises the fences (review O7).
    const auto render = styler_->renderFor(block);
    if (render.codeLanguage.empty() || styler_->isRevealed(block.blockNumber())) {
        return {};
    }
    QString language = toQString(render.codeLanguage);
    language[0] = language[0].toUpper();
    return language;
}

QWidget* EditorWidget::propertiesView() const {
    return properties_->view();
}

QString EditorWidget::frontMatterBadge() const {
    return properties_->badge();
}

void EditorWidget::setFrontMatterAsProperties(bool on) {
    properties_->setEnabled(on);
}

void EditorWidget::placeCaretAfterOpening(bool loaded) {
    properties_->placeCaretAfterOpening(loaded);
}

void EditorWidget::insertTable() {
    if (isReadOnly()) {
        return;
    }
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    bool needsBlankBefore = false;
    if (const QTextBlock tableLast = tables_->lastBlockOfTableAt(block.blockNumber()); tableLast.isValid()) {
        const QTextBlock sep = tableLast.next();
        if (sep.isValid() && sep.text().trimmed().isEmpty()) {
            block = sep;
        } else {
            block = tableLast;
            needsBlankBefore = true;
        }
    }

    const QString tableMarkdown = toQString(core::makeEmptyTable(2, 1));
    const bool replaceEmptyBlock = block.text().trimmed().isEmpty() && !tables_->isSeparatorBlock(block) &&
                                   !styler_->renderFor(block).codeBlock && !styler_->renderFor(block).frontMatter;
    const QTextBlock next = block.next();
    const bool needsSeparatorAfter = next.isValid() && !next.text().trimmed().isEmpty();

    cursor.beginEditBlock();
    int headerBlockNumber = block.blockNumber();
    if (replaceEmptyBlock) {
        cursor.setPosition(block.position());
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.insertText(needsSeparatorAfter ? tableMarkdown + QLatin1Char('\n') : tableMarkdown);
    } else {
        cursor.setPosition(block.position() + block.length() - 1);
        const QString prefix = needsBlankBefore ? QStringLiteral("\n\n") : QStringLiteral("\n");
        cursor.insertText(prefix + (needsSeparatorAfter ? tableMarkdown + QLatin1Char('\n') : tableMarkdown));
        headerBlockNumber = block.blockNumber() + (needsBlankBefore ? 2 : 1);
    }
    cursor.endEditBlock();

    const QTextBlock headerBlock = document()->findBlockByNumber(headerBlockNumber);
    if (headerBlock.isValid()) {
        QTextCursor headerCursor(headerBlock);
        setTextCursor(headerCursor);
    }
    styler_->styleVisible();
    tables_->refresh();
    tables_->focusCellAt(headerBlockNumber, 0, 0);
}

void EditorWidget::insertInlineCode() {
    if (isReadOnly()) {
        return;
    }
    if (auto* cell = qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
        cell != nullptr && viewport()->isAncestorOf(cell)) {
        QTextCursor c = cell->textCursor();
        c.beginEditBlock();
        if (c.hasSelection()) {
            const QString selected = c.selectedText();
            c.insertText(QLatin1Char('`') + selected + QLatin1Char('`'));
        } else {
            c.insertText(QStringLiteral("``"));
            c.movePosition(QTextCursor::Left);
        }
        c.endEditBlock();
        cell->setTextCursor(c);
        cell->setFocus();
        return;
    }

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        cursor.insertText(QLatin1Char('`') + selected + QLatin1Char('`'));
    } else {
        cursor.insertText(QStringLiteral("``"));
        cursor.movePosition(QTextCursor::Left);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    setFocus();
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

void EditorWidget::paintThematicBreak(QPainter& painter, const QTextBlock& block, const QRectF& rect) {
    // S023: a concealed break is a line across the text column, centred vertically in its block
    // (margins included), max(1, round(border-width x device pixel ratio)) device pixels thick and
    // on whole device pixel rows, so it stays crisp at fractional scaling.
    const core::ThematicBreakStyle& style = styler_->formats().thematicBreak();
    const QTextLayout* layout = block.layout();
    if (style.borderWidth <= 0 || style.border.a == 0 || layout == nullptr || layout->lineCount() == 0) {
        return;
    }
    const qreal dpr = viewport()->devicePixelRatioF();
    const int thickness = std::max(1, static_cast<int>(std::lround(style.borderWidth * dpr)));
    const QTextLine line = layout->lineAt(0);
    const qreal left = rect.left() + layout->position().x() + line.x();
    const qreal right = std::min(left + line.width(), static_cast<qreal>(viewport()->width()));
    const int top = static_cast<int>(std::lround(rect.center().y() * dpr - thickness / 2.0));
    const bool antialiased = painter.testRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(QRectF(left, top / dpr, right - left, thickness / dpr), toQColor(style.border));
    painter.setRenderHint(QPainter::Antialiasing, antialiased);
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
        if (!block.isVisible()) {
            continue;
        }
        const auto render = styler_->renderFor(block);
        if (styler_->isRevealed(block.blockNumber())) {
            continue;
        }
        if (render.thematicBreak) {
            paintThematicBreak(painter, block, rect);
        }
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

bool EditorWidget::handleBulletKey(QKeyEvent* event) {
    if (isReadOnly()) {
        return false;
    }
    const int key = event->key();
    const auto mods = event->modifiers();
    const bool isEnter =
        (key == Qt::Key_Return || key == Qt::Key_Enter) && (mods == Qt::NoModifier || mods == Qt::KeypadModifier);
    const bool isTab = (key == Qt::Key_Tab && mods == Qt::NoModifier);
    const bool isBacktab = (key == Qt::Key_Backtab) || (key == Qt::Key_Tab && (mods & Qt::ShiftModifier));

    if (!isEnter && !isTab && !isBacktab) {
        return false;
    }

    QTextCursor cursor = textCursor();
    if (isEnter) {
        return handleBulletEnter(cursor);
    }
    return handleBulletTab(cursor, isBacktab);
}

bool EditorWidget::handleBulletEnter(QTextCursor& cursor) {
    QTextBlock block = cursor.block();
    const auto info = core::parseBulletLine(view(block.text()));
    if (!info.has_value() || styler_->renderFor(block).codeBlock) {
        return false;
    }

    if (cursor.hasSelection()) {
        cursor.removeSelectedText();
        block = cursor.block();
    }

    const int posInBlock = cursor.positionInBlock();

    if (info->isEmpty) {
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        if (info->markerChar > 0) {
            const QString outdented = toQString(core::outdentBulletLine(view(block.text())));
            cursor.insertText(outdented);
        } else {
            cursor.removeSelectedText();
        }
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
        return true;
    }

    if (posInBlock >= static_cast<int>(info->prefixLength)) {
        cursor.beginEditBlock();
        const QString prefix = block.text().left(static_cast<qsizetype>(info->prefixLength));
        cursor.insertText(QLatin1Char('\n') + prefix);
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
        return true;
    }

    return false;
}

bool EditorWidget::handleBulletTab(QTextCursor& cursor, bool outdent) {
    const int startPos = cursor.selectionStart();
    const int endPos = cursor.selectionEnd();
    QTextBlock startBlock = document()->findBlock(startPos);
    QTextBlock endBlock = document()->findBlock(endPos);
    if (endPos > startPos && endBlock.position() == endPos) {
        endBlock = endBlock.previous();
    }

    if (!startBlock.isValid() || !endBlock.isValid()) {
        return false;
    }

    if (startBlock == endBlock) {
        const auto info = core::parseBulletLine(view(startBlock.text()));
        if (!info.has_value() || styler_->renderFor(startBlock).codeBlock) {
            return false;
        }

        cursor.beginEditBlock();
        const int oldPos = cursor.position();
        const int blockPos = startBlock.position();
        if (!outdent) {
            QTextCursor lineStart(startBlock);
            lineStart.insertText(QStringLiteral("  "));
            cursor.setPosition(oldPos + 2);
        } else {
            const QString text = startBlock.text();
            int toRemove = 0;
            if (text.startsWith(QStringLiteral("  "))) {
                toRemove = 2;
            } else if (text.startsWith(QLatin1Char(' ')) || text.startsWith(QLatin1Char('\t'))) {
                toRemove = 1;
            }
            if (toRemove > 0) {
                QTextCursor remover(startBlock);
                remover.setPosition(blockPos + toRemove, QTextCursor::KeepAnchor);
                remover.removeSelectedText();
                cursor.setPosition(std::max(blockPos, oldPos - toRemove));
            }
        }
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
        return true;
    }

    bool anyBullet = false;
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (core::parseBulletLine(view(b.text())).has_value() && !styler_->renderFor(b).codeBlock) {
            anyBullet = true;
            break;
        }
    }
    if (!anyBullet) {
        return false;
    }

    cursor.beginEditBlock();
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (!core::parseBulletLine(view(b.text())).has_value() || styler_->renderFor(b).codeBlock) {
            continue;
        }
        const int blockPos = b.position();
        if (!outdent) {
            QTextCursor lineStart(b);
            lineStart.insertText(QStringLiteral("  "));
        } else {
            const QString text = b.text();
            int toRemove = 0;
            if (text.startsWith(QStringLiteral("  "))) {
                toRemove = 2;
            } else if (text.startsWith(QLatin1Char(' ')) || text.startsWith(QLatin1Char('\t'))) {
                toRemove = 1;
            }
            if (toRemove > 0) {
                QTextCursor remover(b);
                remover.setPosition(blockPos + toRemove, QTextCursor::KeepAnchor);
                remover.removeSelectedText();
            }
        }
    }
    cursor.endEditBlock();
    ensureCursorVisible();
    return true;
}

}  // namespace hitokoto::ui
