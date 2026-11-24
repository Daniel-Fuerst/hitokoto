// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_widget.hpp"

#include "editor_painters.hpp"
#include "floating_toolbar.hpp"
#include "hitokoto/core/table.hpp"
#include "math_block_embedder.hpp"
#include "properties_embedder.hpp"
#include "qt_bridge.hpp"
#include "settings_menu.hpp"
#include "smooth_scroller.hpp"
#include "table_embedder.hpp"
#include "table_paste.hpp"
#include "text_input.hpp"
#include "theme_applier.hpp"
#include "view_anchor.hpp"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QInputMethodEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QStyle>
#include <QTableView>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextDocumentWriter>
#include <QTextLayout>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <utility>

namespace hitokoto::ui {

EditorWidget::EditorWidget(const core::ElementRegistry& registry, QWidget* parent) : QTextEdit(parent) {
    // ADR 0003: our own layout on a plain document; QTextEdit connects to the layout of the
    // document it is given, so the layout is set first.
    auto* doc = new QTextDocument(this);
    layout_ = new BlockLayout(doc);
    doc->setDocumentLayout(layout_);
    setDocument(doc);
    setAcceptRichText(false);
    // S027: smooth scrolling on mouse wheel events; created first, before anything that scrolls
    // (applyPendingShift, resize, mouse and key events) can reach it (review O5).
    verticalScroller_ = new SmoothScroller(verticalScrollBar(), this);
    styler_ = new BlockStyler(document(), registry, [this] { return visibleBlockRange(); }, this);
    tables_ = std::make_unique<TableEmbedder>(this, styler_, layout_);
    properties_ = std::make_unique<PropertiesEmbedder>(this, styler_, layout_);
    mathBlocks_ =
        std::make_unique<MathBlockEmbedder>(document(), styler_, layout_, [this] { return visibleBlockRange(); });
    // S033: the reading position across resizes and theme changes, the text on screen while
    // blocks above it change height, and the caret on screen while it is followed.
    view_ = new ViewAnchor(this, layout_, verticalScroller_, insideEdit_);

    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setCursor(Qt::IBeamCursor);

    connect(document(), &QTextDocument::contentsChange, this, [this](int, int, int) { insideEdit_ = true; });
    connect(document(), &QTextDocument::contentsChanged, this, [this] { insideEdit_ = false; });
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
    // S047: display formulas follow the caret, the selection, edits and the styler.
    connect(this, &QTextEdit::cursorPositionChanged, mathBlocks_.get(), &MathBlockEmbedder::scheduleRefresh);
    connect(this, &QTextEdit::selectionChanged, mathBlocks_.get(), &MathBlockEmbedder::scheduleRefresh);
    connect(document(), &QTextDocument::contentsChange, mathBlocks_.get(), &MathBlockEmbedder::scheduleRefresh);
    connect(styler_, &BlockStyler::completed, mathBlocks_.get(), &MathBlockEmbedder::scheduleRefresh);
    connect(layout_, &BlockLayout::blocksGoneUnreported, mathBlocks_.get(), &MathBlockEmbedder::forgetAll);

    // S026: floating toolbar on the right-hand side and its settings flyout.
    themeConfigDir_ = ThemeManager::defaultConfigDir();
    toolbar_ = new FloatingToolbar(this);
    settingsMenu_ = new SettingsMenu(this);
    connect(toolbar_, &FloatingToolbar::createTableRequested, this, &EditorWidget::insertTable);
    connect(toolbar_, &FloatingToolbar::createInlineCodeRequested, this, &EditorWidget::insertInlineCode);
    connect(toolbar_, &FloatingToolbar::openSettingsRequested, this, &EditorWidget::toggleSettingsMenu);
    connect(settingsMenu_, &SettingsMenu::themeEntrySelected, this, [this](const ThemeEntry& entry) {
        activeThemePath_ = entry.builtinScheme.has_value() ? QString() : QFileInfo(entry.path).absoluteFilePath();
        emit themeEntrySelected(entry);
    });
    // S043: the switch "Smooth scrolling" of the flyout; MainWindow saves the choice.
    connect(settingsMenu_, &SettingsMenu::smoothScrollingToggled, this, [this](bool on) {
        setSmoothScrolling(on);
        emit smoothScrollingChosen(on);
    });
    settingsMenu_->setSmoothScrolling(smoothScrolling_);
    positionToolbar();
}

EditorWidget::~EditorWidget() = default;

void EditorWidget::setSmoothScrolling(bool on) {
    smoothScrolling_ = on;
    if (!smoothScrolling_) {
        verticalScroller_->stop();
    }
    if (settingsMenu_ != nullptr) {
        settingsMenu_->setSmoothScrolling(on);
    }
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
    QTextBlock block = layout_->firstBlockOfLineAt(verticalScrollBar()->value());
    return block.isValid() ? block : document()->lastBlock();
}

quint64 EditorWidget::anchorRestores() const noexcept {
    return view_->restores();
}

void EditorWidget::applyTheme(const core::Theme& theme) {
    const ViewAnchor::Reshape reshape(*view_);
    const ViewAnchor::ScrollAnchor anchor = view_->readingAnchor();
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
    mathBlocks_->setFormats(&styler_->formats());
    tables_->setAvailableWidth(availableTableWidth());
    properties_->setAvailableWidth(availableTableWidth());
    properties_->setLook(styler_->formats().properties(), styler_->formats().propertiesFont(),
                         styler_->formats().propertiesBadgeFont());
    currentTheme_ = theme;
    toolbar_->applyStyle(core::toolbarStyle(theme));
    settingsMenu_->applyTheme(theme);
    if (settingsMenu_->isVisible()) {
        refreshAvailableThemes();
    }
    positionToolbar();
    if (!wasModified) {
        document()->setModified(false);
    }
    view_->restore(anchor);
    viewport()->update();
    view_->settleLater();
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
    // Add up the blocks' own advances from the first block down. Tops from the height index would
    // lag behind while Qt reports an edit before the layout knows the new blocks: every new block
    // would then seem to be on screen, and all of them were styled and laid out at once (S033).
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    while (block.isValid() && top <= bottom) {
        last = block.blockNumber();
        if (block.isVisible()) {
            layout_->ensureBlockLayout(block);
        }
        top += layout_->advanceOf(block);
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
    const QScrollBar* bar = verticalScrollBar();
    const bool barTakesSpace = verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff &&
                               style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) == 0;
    const int barWidth = barTakesSpace ? bar->sizeHint().width() : 0;
    const int margin = std::max(0, static_cast<int>(std::lround(toolbar_->toolbarStyle().margin)));
    const int x = std::max(0, width() - barWidth - margin - toolbar_->width());
    const int y = std::max(0, (height() - toolbar_->height()) / 2);
    toolbar_->move(x, y);
    toolbar_->raise();
    if (settingsMenu_ != nullptr) {
        const int gap = std::max(4, static_cast<int>(std::lround(toolbar_->toolbarStyle().spacing)) * 2);
        const int menuX = std::max(0, x - gap - settingsMenu_->width());
        const int menuY = std::clamp(y + (toolbar_->height() - settingsMenu_->height()) / 2, 0,
                                     std::max(0, height() - settingsMenu_->height()));
        settingsMenu_->move(menuX, menuY);
        settingsMenu_->raise();
    }
}

void EditorWidget::toggleSettingsMenu() {
    setSettingsMenuOpen(!settingsMenu_->isVisible());
}

void EditorWidget::setSettingsMenuOpen(bool open) {
    toolbar_->setSettingsOpen(open);
    if (open) {
        refreshAvailableThemes();
        settingsMenu_->show();
        positionToolbar();
    } else {
        settingsMenu_->hide();
    }
}

void EditorWidget::setThemeConfigDir(const QString& dir) {
    themeConfigDir_ = dir;
    const QString cfgPath = ThemeManager::configFilePath(themeConfigDir_);
    if (!cfgPath.isEmpty() && QFileInfo::exists(cfgPath)) {
        const core::SettingsLoadResult loaded = core::loadSettingsFile(std::filesystem::path(cfgPath.toStdU16String()));
        setFrontMatterAsProperties(loaded.settings.frontMatterAsProperties);
        setSmoothScrolling(loaded.settings.smoothScrolling);
        emit settingsLoaded(loaded.settings);
        if (!loaded.diagnostics.empty()) {
            emit settingsProblemsFound(cfgPath, ThemeManager::formatSettingsDiagnostics(loaded));
        }
    }
    if (const auto saved = ThemeManager::loadSavedThemeEntry(themeConfigDir_)) {
        const auto scheme = ThemeManager::systemScheme();
        core::ThemeLoadResult result{core::builtinTheme(saved->builtinScheme.value_or(scheme)), false, {}};
        if (!saved->builtinScheme.has_value()) {
            result = ThemeManager::loadThemeFile(saved->path, scheme);
        }
        if (!result.usedFallback) {
            activeThemePath_ = saved->builtinScheme.has_value() ? QString() : QFileInfo(saved->path).absoluteFilePath();
            if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
                applyThemeToApplication(*app, result.theme);
            }
            emit savedThemeLoaded(result.theme);
        }
    }
    if (settingsMenu_->isVisible()) {
        refreshAvailableThemes();
    }
}

void EditorWidget::setActiveThemePath(const QString& path) {
    activeThemePath_ = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
    if (settingsMenu_->isVisible()) {
        refreshAvailableThemes();
    }
}

void EditorWidget::refreshAvailableThemes() {
    settingsMenu_->setAvailableThemes(ThemeManager::availableThemes(themeConfigDir_), activeThemePath_, currentTheme_);
    positionToolbar();
}

void EditorWidget::resizeEvent(QResizeEvent* event) {
    if (view_->reshaping()) {
        QTextEdit::resizeEvent(event);
        return;
    }
    const ViewAnchor::Reshape reshape(*view_);
    verticalScroller_->stop();
    const ViewAnchor::ScrollAnchor anchor = view_->readingAnchor();
    updateMargins();
    QTextEdit::resizeEvent(event);
    positionToolbar();
    tables_->setAvailableWidth(availableTableWidth());
    properties_->setAvailableWidth(availableTableWidth());
    view_->restore(anchor);
    styler_->styleVisible();
    // The widgets follow at once; finding tables waits for the event loop, so it never runs in
    // the middle of a document change (opening a file scrolls while it replaces the text).
    tables_->reposition();
    properties_->reposition();
    tables_->scheduleRefresh();
    mathBlocks_->scheduleRefresh();
    view_->settleLater();
}

void EditorWidget::scrollContentsBy(int dx, int dy) {
    QTextEdit::scrollContentsBy(dx, dy);
    styler_->styleVisible();
    tables_->reposition();
    properties_->reposition();
    tables_->scheduleRefresh();
    mathBlocks_->scheduleRefresh();
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
    if (event->button() == Qt::LeftButton && propertiesClickHeld_) {
        propertiesClickHeld_ = false;
        event->accept();
        return;
    }
    QTextEdit::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        properties_->setMouseHeld(false);  // S024 review O6: the block may switch now
    }
}

void EditorWidget::mousePressEvent(QMouseEvent* event) {
    verticalScroller_->stop();
    const QTextCursor hit = cursorForPosition(event->position().toPoint());
    const QTextBlock block = hit.block();
    if (event->button() == Qt::LeftButton && properties_->handleEditorClick(block.blockNumber())) {
        propertiesClickHeld_ = true;
        event->accept();
        return;
    }
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
        if (h.mode != core::HideMode::KeepSpace && column >= h.span.start && column < h.span.end()) {
            c.setPosition(block.position() + static_cast<int>(h.span.end()));
            setTextCursor(c);
            break;
        }
    }
}

void EditorWidget::mouseMoveEvent(QMouseEvent* event) {
    if (propertiesClickHeld_) {
        event->accept();
        return;
    }
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
        verticalScroller_->stop();
        QTextEdit::wheelEvent(event);
        return;
    }

    // Review O4: pixel deltas (trackpads, high-resolution touchpads) scroll directly, as Qt's
    // documentation of QWheelEvent::pixelDelta() asks; on X11 they are driver-specific and
    // unreliable, so there the angle delta is used.
    const QPoint pixels = event->pixelDelta();
    if (pixels.y() != 0 && QGuiApplication::platformName() != QLatin1String("xcb")) {
        verticalScroller_->stop();
        verticalScroller_->shift(-static_cast<double>(pixels.y()));
        event->accept();
        return;
    }

    const int angleY = event->angleDelta().y();
    double delta = 0.0;
    if (angleY != 0) {
        const int lineSpacing =
            fontMetrics().lineSpacing() > 0 ? fontMetrics().lineSpacing() : verticalScrollBar()->singleStep();
        const int lines = QApplication::wheelScrollLines() > 0 ? QApplication::wheelScrollLines() : 3;
        // One notch scrolls 1.8 times the system's wheel lines (S027, Daniel 2026-11-08).
        constexpr double kNotchFactor = 1.8;
        const double distancePerNotch = kNotchFactor * lines * (lineSpacing > 0 ? lineSpacing : 20);
        delta = -(static_cast<double>(angleY) / 120.0) * distancePerNotch;
    }

    if (std::abs(delta) > 0.001 && verticalScrollBar() != nullptr &&
        verticalScrollBar()->maximum() > verticalScrollBar()->minimum()) {
        verticalScroller_->scroll(delta);
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
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const QTextCharFormat fmt = formatAt(help->pos());
        if (!fmt.toolTip().isEmpty()) {
            QToolTip::showText(help->globalPos(), fmt.toolTip(), viewport());
            return true;
        }
        // S047: the reason a display block is not typeset, over its lines.
        const int hovered = cursorForPosition(help->pos()).blockNumber();
        for (const MathBlockEmbedder::Failed& f : mathBlocks_->failed()) {
            if (hovered >= f.open && hovered <= f.last) {
                QToolTip::showText(help->globalPos(), Qt::convertFromPlainText(QString::fromStdString(f.reason)),
                                   viewport());
                return true;
            }
        }
        QToolTip::hideText();
    }
    return QTextEdit::viewportEvent(event);
}

QTextCharFormat EditorWidget::formatAt(const QPoint& pos) const {
    const QPointF docPos(pos.x() + horizontalScrollBar()->value(), pos.y() + verticalScrollBar()->value());
    const QTextBlock first = layout_->firstBlockOfLineAt(docPos.y());
    if (!first.isValid()) {
        return {};
    }
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        if (b != first && !layout_->sharesLineWithPrevious(b)) {
            break;
        }
        if (layout_->reservedHeight(b) > 0) {
            continue;
        }
        layout_->ensureBlockLayout(b);
        const QRectF rect = layout_->blockBoundingRect(b);
        const QPointF local = docPos - rect.topLeft();
        const QTextLayout* layout = b.layout();
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            if (local.y() >= line.y() && local.y() < line.y() + line.height()) {
                if (local.x() < line.x() || local.x() >= line.x() + line.naturalTextWidth()) {
                    continue;
                }
                const int col = line.xToCursor(local.x(), QTextLine::CursorOnCharacter);
                if (col < 0 || col >= b.length()) {
                    continue;
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
    }
    return {};
}

QString EditorWidget::anchorAt(const QPoint& pos) const {
    const QTextCharFormat fmt = formatAt(pos);
    return fmt.isAnchor() ? fmt.anchorHref() : QString{};
}

void EditorWidget::keyPressEvent(QKeyEvent* event) {
    // A key's text enters the document as insertableText makes it (S004, rework of 2026-11-21).
    if (const auto key = insertableKey(*event)) {
        pressKey(key.get());
        event->setAccepted(key->isAccepted());
        return;
    }
    pressKey(event);
}

void EditorWidget::pressKey(QKeyEvent* event) {
    // Qt scrolls to the caret of a key from layouts that the edit has not updated yet; that is no
    // scroll of the reader's, and the caret is followed further (S033).
    const ViewAnchor::Input input(*view_);
    verticalScroller_->stop();
    if (event->key() == Qt::Key_Escape && settingsMenu_->isVisible()) {
        setSettingsMenuOpen(false);
        event->accept();
        return;
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

void EditorWidget::undoEdit() {
    const ViewAnchor::Input input(*view_);
    undo();
}

void EditorWidget::redoEdit() {
    const ViewAnchor::Input input(*view_);
    redo();
}

void EditorWidget::inputMethodEvent(QInputMethodEvent* event) {
    // The committed text enters as insertableText makes it (S004, rework of 2026-11-21).
    const auto insertable = insertableInput(*event);
    QInputMethodEvent* input = insertable ? insertable.get() : event;
    const ViewAnchor::Input inputScope(*view_);
    const TableEmbedder::Typing typing(tables_.get());
    if (!input->commitString().isEmpty()) {
        tables_->beforeWritingBelow();
    }
    QTextEdit::inputMethodEvent(input);
    event->setAccepted(input->isAccepted());
}

void EditorWidget::insertFromMimeData(const QMimeData* source) {
    const ViewAnchor::Input input(*view_);
    if (!plainPaste_ && pasteTables(source)) {
        return;
    }
    const TableEmbedder::Typing typing(tables_.get());
    tables_->beforeWritingBelow();
    // Qt gets the text alone and so never chooses a format (pastedText), and only text that may
    // enter the document (insertableText; S004, rework of 2026-11-21).
    QMimeData plain;
    if (const QString text = pastedText(source); !text.isNull()) {
        plain.setText(insertableText(text));
    }
    QTextEdit::insertFromMimeData(&plain);
}

QString EditorWidget::pastedText(const QMimeData* source) {
    if (source == nullptr) {
        return {};
    }
    if (source->hasText()) {
        return source->text();
    }
    const QString markdown = QStringLiteral("text/markdown");
    return source->hasFormat(markdown) ? QString::fromUtf8(source->data(markdown)) : QString();
}

void EditorWidget::dropEvent(QDropEvent* event) {
    // S031: drag and drop is not converted.
    const QScopedValueRollback<bool> plain(plainPaste_, true);
    QTextEdit::dropEvent(event);
}

QString EditorWidget::ownCopyFormat() {
    return QStringLiteral("application/x-hitokoto-copy");
}

namespace {

/// A copy made in hitokoto (S031 review (code) O9): the formats Qt's own data offers for a
/// selection (plain text, HTML, Markdown, ODF; QTextEditMimeData), each made from the selection
/// only when asked for, plus the marker of EditorWidget::ownCopyFormat. Qt's data makes all of
/// them at the first request, so that pasting even the plain text paid for the HTML and the ODF.
class OwnCopy : public QMimeData {
public:
    explicit OwnCopy(const QTextDocumentFragment& selection) : selection_(selection) {}

    [[nodiscard]] QStringList formats() const override {
        if (selection_.isEmpty()) {
            return QMimeData::formats();
        }
        QStringList all = {QStringLiteral("text/plain"), QStringLiteral("text/html")};
#if QT_CONFIG(textmarkdownwriter)
        all << QStringLiteral("text/markdown");
#endif
#if QT_CONFIG(textodfwriter)
        all << QStringLiteral("application/vnd.oasis.opendocument.text");
#endif
        all << EditorWidget::ownCopyFormat();
        return all;
    }

protected:
    [[nodiscard]] QVariant retrieveData(const QString& mimeType, QMetaType type) const override {
        if (selection_.isEmpty()) {
            return QMimeData::retrieveData(mimeType, type);
        }
        if (mimeType == EditorWidget::ownCopyFormat()) {
            return QByteArrayLiteral("1");  // not empty: some platforms drop empty formats
        }
        if (mimeType == QStringLiteral("text/plain")) {
            return selection_.toPlainText();
        }
        if (mimeType == QStringLiteral("text/html")) {
            return selection_.toHtml();
        }
#if QT_CONFIG(textmarkdownwriter)
        if (mimeType == QStringLiteral("text/markdown")) {
            return selection_.toMarkdown().toUtf8();
        }
#endif
#if QT_CONFIG(textodfwriter)
        if (mimeType == QStringLiteral("application/vnd.oasis.opendocument.text")) {
            QBuffer buffer;
            QTextDocumentWriter writer(&buffer, "ODF");
            writer.write(selection_);
            return buffer.data();
        }
#endif
        return {};
    }

private:
    QTextDocumentFragment selection_;
};

}  // namespace

QMimeData* EditorWidget::createMimeDataFromSelection() const {
    return new OwnCopy(QTextDocumentFragment(textCursor()));
}

bool EditorWidget::canPastePlainText() {
    // The formats only: fetching the text on every clipboard change would make the program that
    // copied render it, Excel a whole column (S031 review O7).
    const QMimeData* data = QApplication::clipboard()->mimeData();
    return data != nullptr && data->hasText();
}

void EditorWidget::pasteAsPlainText() {
    if (isReadOnly() || !canPastePlainText()) {
        return;
    }
    const QString text = QApplication::clipboard()->mimeData()->text();
    if (text.isEmpty()) {
        return;
    }
    QMimeData plain;
    plain.setText(text);
    {
        const QScopedValueRollback<bool> noConversion(plainPaste_, true);
        insertFromMimeData(&plain);
    }
    ensureCursorVisible();
}

void EditorWidget::contextMenuEvent(QContextMenuEvent* event) {
    std::unique_ptr<QMenu> menu(createStandardContextMenu(event->pos()));
    // S031: Paste as Plain Text after Paste.
    auto* plain = new QAction(tr("Paste as Plain Text"), menu.get());
    plain->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    plain->setEnabled(!isReadOnly() && canPastePlainText());
    connect(plain, &QAction::triggered, this, &EditorWidget::pasteAsPlainText);
    const QList<QAction*> actions = menu->actions();
    const auto paste = std::find_if(actions.begin(), actions.end(),
                                    [](const QAction* a) { return a->objectName() == QStringLiteral("edit-paste"); });
    if (paste != actions.end() && std::next(paste) != actions.end()) {
        menu->insertAction(*std::next(paste), plain);
    } else {
        menu->addAction(plain);
    }
    menu->exec(event->globalPos());
}

bool EditorWidget::pasteTables(const QMimeData* source) {
    if (source == nullptr || isReadOnly()) {
        return false;
    }
    // A copy made in hitokoto holds Markdown source, never a table: its HTML is not even asked
    // for, Qt would first generate it from the selection (S031 review (code) O9).
    if (source->hasFormat(ownCopyFormat())) {
        return false;
    }
    QTextCursor cursor = textCursor();
    // Only in the editor text: not in a code block, front matter or a table's lines (S031), and
    // not in a block quote or list item, which a table would split (S031 review O4).
    for (const int position : {cursor.selectionStart(), cursor.selectionEnd()}) {
        const QTextBlock block = document()->findBlock(position);
        const core::LineRender render = styler_->renderFor(block);
        if (render.codeBlock || render.frontMatter || render.table) {
            return false;
        }
        const auto above = [block](std::size_t i) -> std::optional<std::u16string> {
            QTextBlock b = block.previous();
            for (std::size_t n = 0; n < i && b.isValid(); ++n) {
                b = b.previous();
            }
            if (!b.isValid()) {
                return std::nullopt;
            }
            const QString line = b.text();
            return std::u16string(view(line));
        };
        if (core::pasteLineInContainer(view(block.text()), above)) {
            return false;
        }
    }
    if (!source->hasHtml() && !source->hasText()) {
        return false;
    }

    // HTML: one scan first, so that HTML without a table or with nested tables is never imported
    // (S031 review O1, O9). Excel's plain text only without HTML.
    // Qt gets the HTML without what only formats, and the scan reads it as Qt gets it; a paste
    // whose import would take longer than its cells allow stays plain text (rework of 2026-11-20).
    QString html;
    if (source->hasHtml()) {
        const std::u16string withoutFormatting = core::htmlWithoutFormatting(view(source->html()));
        if (core::scanHtmlTables(withoutFormatting) != core::HtmlTables::Flat) {
            return false;
        }
        html = toQString(withoutFormatting);
    }

    // The wait cursor while the paste converts (S031, ADR 0011), on success until the pasted table
    // is shown (review O6): the embedder shows it in its refresh, queued by the insertion below,
    // and the cursor is restored by a call queued after it.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    auto restore = qScopeGuard([] { QGuiApplication::restoreOverrideCursor(); });
    std::optional<std::vector<core::PasteBlock>> blocks;
    if (!html.isEmpty()) {
        blocks = core::pastedTablesToMarkdown(readPastedHtml(html));
    } else if (auto table = core::parseExcelPlainText(view(source->text()))) {
        std::vector<core::PastedPart> parts(1);
        parts.front().table = std::move(*table);
        blocks = core::pastedTablesToMarkdown(parts);
    }
    if (!blocks) {
        return false;
    }

    // One step, undone at once: the text replaces the selection; the tables land as their own
    // blocks around the line split at the caret.
    const int start = cursor.selectionStart();
    const QTextBlock first = document()->findBlock(start);
    const QTextBlock last = document()->findBlock(cursor.selectionEnd());
    const QString firstText = first.text();
    const QString lastText = last.text();
    const std::u16string head(view(firstText.left(start - first.position())));
    const std::u16string tail(view(lastText.mid(cursor.selectionEnd() - last.position())));
    const QString above = first.previous().isValid() ? first.previous().text() : QString();
    const QString below = last.next().isValid() ? last.next().text() : QString();
    core::PastePlace place;
    place.head = head;
    place.tail = tail;
    if (first.previous().isValid()) {
        place.lineAbove = view(above);
    }
    if (last.next().isValid()) {
        place.lineBelow = view(below);
    }
    const core::PasteInsertion insertion = core::placePastedBlocks(*blocks, place);
    const TableEmbedder::NotTyping notTyping(tables_.get());
    // The cells' text comes from other programs, through Qt's HTML import or as Excel's plain text
    // (S004, rework of 2026-11-21).
    cursor.insertText(insertableText(toQString(insertion.text)));
    cursor.setPosition(start + static_cast<int>(insertion.caret));
    setTextCursor(cursor);
    ensureCursorVisible();
    tables_->scheduleRefresh();  // queued already by the insertion; makes the order explicit
    restore.dismiss();
    QMetaObject::invokeMethod(qApp, [] { QGuiApplication::restoreOverrideCursor(); }, Qt::QueuedConnection);
    return true;
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
            drawCodeBox(painter, panel.adjusted(-padding, 0, padding, 0), block);
            inPanel = false;
        }
        paintInlineCodeBoxes(painter, b, render, rect.topLeft(), inlineCode);
    }
    if (inPanel) {
        drawCodeBox(painter, panel.adjusted(-padding, 0, padding, 0), block);
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
    frontMatterAsProperties_ = on;
    properties_->setEnabled(on);
}

void EditorWidget::setFindHold(int first, int last, bool untilCaretLeaves) {
    tables_->setFindHold(first, last, untilCaretLeaves);
}

void EditorWidget::showPosition(int position) {
    QTextCursor c(document());
    c.setPosition(std::clamp(position, 0, std::max(0, document()->characterCount() - 1)));
    const QRect r = cursorRect(c);
    const QRect view = viewport()->rect();
    if (r.top() >= view.top() && r.bottom() <= view.bottom()) {
        return;
    }
    QScrollBar* bar = verticalScrollBar();
    bar->setValue(bar->value() + r.center().y() - view.height() / 2);
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

void EditorWidget::paintDecorations() {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = styler_->formats().bulletColor();
    const QFontMetricsF metrics(document()->defaultFont());
    const qreal radius = std::max<qreal>(1.5, metrics.xHeight() * 0.32);
    const QPointF offset = contentOffset();
    const qreal bottom = viewport()->height();
    std::vector<FormulaHighlight> formulaHighlights;  // made once per paint, when a formula is on screen
    bool highlightsMade = false;
    paintDisplayFormulas(painter, offset, bottom);

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
        if (!render.math.empty()) {
            if (!highlightsMade) {
                formulaHighlights = formulaHighlightRanges();
                highlightsMade = true;
            }
            paintFormulas(painter, block, rect, render, styler_->formats(), formulaHighlights);
        }
        if (render.thematicBreak) {
            paintThematicBreak(painter, block, rect, styler_->formats().thematicBreak(),
                               viewport()->devicePixelRatioF(), static_cast<qreal>(viewport()->width()));
        }
        if (render.decorations.empty()) {
            continue;
        }
        paintBullets(painter, block, rect, render, color, metrics, radius);
    }
}

void EditorWidget::paintDisplayFormulas(QPainter& painter, QPointF offset, qreal bottom) {
    // S047: each typeset block's panel on its opening line, which reserves its height; a block that
    // could not be laid out gets a bar in math.error beside its source.
    const EditorFormats& formats = styler_->formats();
    for (const MathBlockEmbedder::Typeset& t : mathBlocks_->typeset()) {
        const QTextBlock block = document()->findBlockByNumber(t.open);
        if (!block.isValid() || layout_->reservedHeight(block) <= 0 || !mathBlocks_->claims(block)) {
            continue;
        }
        const QRectF rect = blockBoundingGeometry(block).translated(offset);
        if (rect.bottom() < 0 || rect.top() > bottom) {
            continue;
        }
        paintDisplayFormula(painter, rect, *formats.mathLayout(t.source, 0, true), formats.math());
    }
    for (const MathBlockEmbedder::Failed& f : mathBlocks_->failed()) {
        const QTextBlock first = document()->findBlockByNumber(f.open);
        const QTextBlock last = document()->findBlockByNumber(f.last);
        if (!first.isValid() || !last.isValid()) {
            continue;
        }
        const QRectF top = blockBoundingGeometry(first).translated(offset);
        const QRectF end = blockBoundingGeometry(last).translated(offset);
        if (end.bottom() < 0 || top.top() > bottom) {
            continue;
        }
        paintFormulaErrorBar(painter, QRectF(top.topLeft(), end.bottomRight()), formats.math());
    }
}

std::vector<FormulaHighlight> EditorWidget::formulaHighlightRanges() const {
    // S046, decision 6: the find highlights, then the selection on top.
    std::vector<FormulaHighlight> out;
    for (const QTextEdit::ExtraSelection& s : extraSelections()) {
        if (s.cursor.hasSelection() && s.format.hasProperty(QTextFormat::BackgroundBrush)) {
            out.push_back({s.cursor.selectionStart(), s.cursor.selectionEnd(), s.format.background(), std::nullopt});
        }
    }
    const QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        out.push_back({cursor.selectionStart(), cursor.selectionEnd(), palette().brush(QPalette::Highlight),
                       palette().color(QPalette::HighlightedText)});
    }
    return out;
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
    const ListEditing::Outcome outcome = isEnter ? list_.enter(cursor) : list_.tab(cursor, isBacktab);
    if (outcome == ListEditing::Outcome::None) {
        return false;
    }
    if (outcome == ListEditing::Outcome::CaretPlaced) {
        setTextCursor(cursor);
    }
    ensureCursorVisible();
    return true;
}

}  // namespace hitokoto::ui
