// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_view.hpp"

#include "editor_formats.hpp"
#include "qt_bridge.hpp"
#include "table_model.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <vector>

namespace hitokoto::ui {

// --- CellEditor -------------------------------------------------------------------------------

CellEditor::CellEditor(QWidget* parent) : QPlainTextEdit(parent) {
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    document()->setDocumentMargin(0);
}

void CellEditor::keyPressEvent(QKeyEvent* event) {
    // Qt's item delegates leave Tab to text editors; in a cell it moves to the next cell.
    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        emit tabPressed(event->key() == Qt::Key_Backtab || (event->modifiers() & Qt::ShiftModifier) != 0);
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if ((event->modifiers() & Qt::ShiftModifier) != 0) {
            insertPlainText(QStringLiteral("\n"));  // written as <br> (S018)
        } else {
            emit enterPressed();
        }
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

// --- TableDelegate ----------------------------------------------------------------------------

TableDelegate::TableDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void TableDelegate::setPadding(qreal padding) {
    padding_ = padding;
    naturalCache_.clear();
}

void TableDelegate::prepare(QTextDocument& document, const QModelIndex& index) const {
    document.setDocumentMargin(0);
    const QVariant font = index.data(Qt::FontRole);
    if (font.isValid()) {
        document.setDefaultFont(font.value<QFont>());
    }
    QString markdown = index.data(TableModel::MarkdownRole).toString();
    // GFM shows an escaped pipe as a pipe, also in code; Qt 6.12 drops everything after a <br>
    // without slash, so every spelling becomes <br/> (ADR 0005).
    markdown.replace(QStringLiteral("\\|"), QStringLiteral("|"));
    static const QRegularExpression br(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    markdown.replace(br, QStringLiteral("<br/>"));
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

    QTextBlockFormat format;
    format.setTopMargin(0);
    format.setBottomMargin(0);
    const auto horizontal =
        static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt()) & Qt::AlignHorizontal_Mask;
    format.setAlignment(horizontal != 0 ? horizontal : Qt::AlignLeft);
    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(format);
}

QSizeF TableDelegate::naturalSize(const QModelIndex& index) const {
    const QString key = index.data(TableModel::MarkdownRole).toString() + QChar(index.row() == 0 ? u'h' : u'b');
    if (const auto it = naturalCache_.constFind(key); it != naturalCache_.constEnd()) {
        return *it;
    }
    QTextDocument document;
    prepare(document, index);
    document.setTextWidth(-1);
    const QSizeF size(document.idealWidth() + 2 * padding_, document.size().height() + 2 * padding_);
    naturalCache_.insert(key, size);
    return size;
}

qreal TableDelegate::heightAt(const QModelIndex& index, qreal width) const {
    QTextDocument document;
    prepare(document, index);
    document.setTextWidth(std::max<qreal>(1.0, width - 2 * padding_));
    return document.size().height() + 2 * padding_;
}

void TableDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    const QVariant background = index.data(Qt::BackgroundRole);
    if (background.isValid()) {
        painter->fillRect(option.rect, background.value<QColor>());
    }
    if ((option.state & QStyle::State_HasFocus) != 0) {
        QColor highlight = option.palette.color(QPalette::Highlight);
        highlight.setAlpha(60);
        painter->fillRect(option.rect, highlight);
    }
    QTextDocument document;
    prepare(document, index);
    const qreal width = std::max<qreal>(1.0, option.rect.width() - 2 * padding_);
    document.setTextWidth(width);
    painter->translate(QPointF(option.rect.topLeft()) + QPointF(padding_, padding_));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette = option.palette;
    const QVariant foreground = index.data(Qt::ForegroundRole);
    if (foreground.isValid()) {
        context.palette.setColor(QPalette::Text, foreground.value<QColor>());
    }
    context.clip = QRectF(0, 0, width, std::max<qreal>(0.0, option.rect.height() - 2 * padding_));
    painter->setClipRect(context.clip);
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

QSize TableDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QSizeF natural = naturalSize(index);
    if (option.rect.width() > 0) {
        return {option.rect.width(), static_cast<int>(std::ceil(heightAt(index, option.rect.width())))};
    }
    return natural.toSize();
}

QWidget* TableDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                     const QModelIndex& index) const {
    auto* editor = new CellEditor(parent);
    const QVariant font = index.data(Qt::FontRole);
    if (font.isValid()) {
        editor->setFont(font.value<QFont>());
    }
    editor->setPadding(static_cast<int>(padding_));
    auto* self = const_cast<TableDelegate*>(this);
    connect(editor, &CellEditor::tabPressed, self, [self, editor](bool backward) {
        emit self->commitData(editor);
        emit self->closeEditor(
            editor, backward ? QAbstractItemDelegate::EditPreviousItem : QAbstractItemDelegate::EditNextItem);
    });
    connect(editor, &CellEditor::enterPressed, self, [self, editor] {
        emit self->commitData(editor);
        emit self->closeEditor(editor, QAbstractItemDelegate::NoHint);
        emit self->editorEnterPressed();
    });
    return editor;
}

void TableDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    if (auto* text = qobject_cast<QPlainTextEdit*>(editor)) {
        text->setPlainText(index.data(Qt::EditRole).toString());
        text->moveCursor(QTextCursor::End);
    }
}

void TableDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    if (auto* text = qobject_cast<QPlainTextEdit*>(editor)) {
        model->setData(index, text->toPlainText(), Qt::EditRole);
    }
}

void TableDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                         const QModelIndex& /*index*/) const {
    editor->setGeometry(option.rect);
}

// --- TableView --------------------------------------------------------------------------------

TableView::TableView(QWidget* parent) : QTableView(parent), delegate_(new TableDelegate(this)) {
    setItemDelegate(delegate_);
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWordWrap(true);
    setTabKeyNavigation(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked |
                    QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    connect(delegate_, &TableDelegate::editorEnterPressed, this, &TableView::handleEnter);

    setContextMenuPolicy(Qt::ActionsContextMenu);
    const auto addAction = [this](const QString& text, const char* name, auto slot) {
        auto* action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        connect(action, &QAction::triggered, this, slot);
        QWidget::addAction(action);
    };
    addAction(tr("Add Row"), "tableAddRow", [this] { emit appendRow(std::max(currentIndex().column(), 0)); });
    addAction(tr("Delete Row"), "tableDeleteRow", [this] { emit deleteRow(currentIndex().row()); });
    addAction(tr("Add Column"), "tableAddColumn", [this] { emit addColumn(); });
    addAction(tr("Delete Column"), "tableDeleteColumn", [this] { emit deleteColumn(currentIndex().column()); });
    addAction(tr("Show Markdown Source\tCtrl+Shift+M"), "tableShowSource", [this] { emit toggleSource(); });
}

void TableView::fit(int width) {
    QAbstractItemModel* m = model();
    const int columns = m != nullptr ? m->columnCount() : 0;
    const int rows = m != nullptr ? m->rowCount() : 0;
    if (columns == 0) {
        return;
    }
    const int grid = showGrid() ? 1 : 0;
    const int frame = 2 * frameWidth();
    std::vector<qreal> widths(static_cast<std::size_t>(columns), 24.0);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            auto& w = widths[static_cast<std::size_t>(c)];
            w = std::max(w, delegate_->naturalSize(m->index(r, c)).width());
        }
    }
    // Wider than the text column: cap the widest columns (water filling), they wrap.
    const qreal available = std::max<qreal>(columns * 24.0, width - frame - columns * grid);
    qreal total = 0;
    for (const qreal w : widths) {
        total += w;
    }
    if (total > available) {
        qreal low = 0;
        qreal high = *std::max_element(widths.begin(), widths.end());
        for (int i = 0; i < 40; ++i) {
            const qreal cap = (low + high) / 2;
            qreal sum = 0;
            for (const qreal w : widths) {
                sum += std::min(w, cap);
            }
            (sum <= available ? low : high) = cap;
        }
        for (qreal& w : widths) {
            w = std::min(w, low);
        }
    }
    int viewWidth = frame;
    for (int c = 0; c < columns; ++c) {
        const int w = static_cast<int>(std::ceil(widths[static_cast<std::size_t>(c)])) + grid;
        setColumnWidth(c, w);
        viewWidth += w;
    }
    int viewHeight = frame;
    for (int r = 0; r < rows; ++r) {
        qreal h = 0;
        for (int c = 0; c < columns; ++c) {
            h = std::max(h, delegate_->heightAt(m->index(r, c), columnWidth(c) - grid));
        }
        const int rowHeight = static_cast<int>(std::ceil(h)) + grid;
        setRowHeight(r, rowHeight);
        viewHeight += rowHeight;
    }
    setFixedSize(viewWidth, viewHeight);
}

void TableView::keyPressEvent(QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (event->key() == Qt::Key_M && modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
        emit toggleSource();
        return;
    }
    if (state() == QAbstractItemView::EditingState) {
        QTableView::keyPressEvent(event);
        return;
    }
    const QModelIndex current = currentIndex();
    const int row = current.isValid() ? current.row() : 0;
    const int column = current.isValid() ? current.column() : 0;
    const int lastRow = model() != nullptr ? model()->rowCount() - 1 : 0;
    switch (event->key()) {
        case Qt::Key_Escape:
            emit leaveBelow();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if ((modifiers & Qt::ControlModifier) != 0) {
                emit leaveBelow();
            } else {
                handleEnter();
            }
            return;
        case Qt::Key_Up:
            if (row == 0 && modifiers == Qt::NoModifier) {
                emit leaveUp(column);
                return;
            }
            break;
        case Qt::Key_Down:
            if (row == lastRow && modifiers == Qt::NoModifier) {
                emit leaveDown(column);
                return;
            }
            break;
        default:
            break;
    }
    QTableView::keyPressEvent(event);
}

void TableView::handleEnter() {
    auto* tableModel = qobject_cast<TableModel*>(model());
    if (tableModel == nullptr) {
        return;
    }
    const QModelIndex current = currentIndex();
    const int row = current.isValid() ? current.row() : 0;
    const int column = current.isValid() ? current.column() : 0;
    if (row < tableModel->rowCount() - 1) {
        setCurrentIndex(tableModel->index(row + 1, column));
    } else if (row > 0 && tableModel->isRowEmpty(row)) {
        emit removeLastRowAndLeave();
    } else {
        emit appendRow(column);
    }
}

QModelIndex TableView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    QAbstractItemModel* m = model();
    const QModelIndex current = currentIndex();
    if (cursorAction == MoveNext && m != nullptr && current.isValid() && current.row() == m->rowCount() - 1 &&
        current.column() == m->columnCount() - 1) {
        // Tab in the last cell appends a row (S018); the embedder updates the model at once.
        const int row = current.row();
        emit appendRow(0);
        return m->index(row + 1, 0);
    }
    return QTableView::moveCursor(cursorAction, modifiers);
}

// --- TableWidget ------------------------------------------------------------------------------

TableWidget::TableWidget(TableModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      view_(new TableView(this)),
      addRow_(new QToolButton(this)),
      addColumn_(new QToolButton(this)),
      deleteRow_(new QToolButton(this)),
      deleteColumn_(new QToolButton(this)) {
    model_->setParent(this);
    view_->setModel(model_);
    const auto setUp = [](QToolButton* button, const char* name, const QString& text, const QString& tip) {
        button->setObjectName(QString::fromLatin1(name));
        button->setText(text);
        button->setToolTip(tip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(18, 18);
        button->hide();
    };
    setUp(addRow_, "tableAddRow", QStringLiteral("+"), tr("Add Row"));
    setUp(addColumn_, "tableAddColumn", QStringLiteral("+"), tr("Add Column"));
    setUp(deleteRow_, "tableDeleteRow", QStringLiteral("−"), tr("Delete Row"));
    setUp(deleteColumn_, "tableDeleteColumn", QStringLiteral("−"), tr("Delete Column"));
    connect(addRow_, &QToolButton::clicked, this, [this] { emit view_->appendRow(0); });
    connect(addColumn_, &QToolButton::clicked, this, [this] { emit view_->addColumn(); });
    connect(deleteRow_, &QToolButton::clicked, this, [this] {
        if (hoverRow_ > 0) {
            emit view_->deleteRow(hoverRow_);
        }
    });
    connect(deleteColumn_, &QToolButton::clicked, this, [this] {
        if (hoverColumn_ >= 0) {
            emit view_->deleteColumn(hoverColumn_);
        }
    });
    setMouseTracking(true);
    view_->setMouseTracking(true);
    view_->viewport()->setMouseTracking(true);
    view_->viewport()->installEventFilter(this);
    installEventFilter(this);
}

void TableWidget::applyStyle(const EditorFormats& formats) {
    const core::TableStyle& style = formats.table();
    model_->setFormats(&formats);
    view_->tableDelegate()->setPadding(style.padding);
    view_->setStyleSheet(
        QStringLiteral("QTableView { background: %1; gridline-color: %2; border: %3px solid %2; "
                       "border-radius: %4px; }")
            .arg(toQColor(style.background).name(QColor::HexArgb), toQColor(style.border).name(QColor::HexArgb))
            .arg(style.borderWidth)
            .arg(style.radius));
}

void TableWidget::fit(int width) {
    view_->fit(width);
    view_->move(gutter_, gutter_);
    resize(view_->width() + 2 * gutter_, view_->height() + 2 * gutter_);
}

bool TableWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseMove) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const auto* widget = static_cast<QWidget*>(watched);
        updateHover(widget == this ? mouse->position().toPoint() : widget->mapTo(this, mouse->position().toPoint()));
    } else if (event->type() == QEvent::Leave && watched == this) {
        hideControls();
    }
    return QWidget::eventFilter(watched, event);
}

void TableWidget::hideControls() {
    addRow_->hide();
    addColumn_->hide();
    deleteRow_->hide();
    deleteColumn_->hide();
}

void TableWidget::updateHover(QPoint position) {
    // Controls appear only while the pointer is near the edge they act on (S018).
    constexpr int kReach = 6;
    const QRect v = view_->geometry();
    const int frame = view_->frameWidth();
    const QPoint inView = position - v.topLeft() - QPoint(frame, frame);
    const bool insideX = position.x() >= v.left() && position.x() <= v.right();
    const bool insideY = position.y() >= v.top() && position.y() <= v.bottom();

    const bool nearBottom = insideX && position.y() >= v.bottom() - kReach && position.y() <= height();
    addRow_->setVisible(nearBottom);
    if (nearBottom) {
        addRow_->move(v.center().x() - addRow_->width() / 2, v.bottom() + 2);
    }
    const bool nearRight = insideY && position.x() >= v.right() - kReach && position.x() <= width();
    addColumn_->setVisible(nearRight);
    if (nearRight) {
        addColumn_->move(v.right() + 2, v.center().y() - addColumn_->height() / 2);
    }
    hoverRow_ = position.x() <= v.left() + kReach && insideY ? view_->rowAt(inView.y()) : -1;
    deleteRow_->setVisible(hoverRow_ > 0);
    if (hoverRow_ > 0) {
        deleteRow_->move(v.left() - deleteRow_->width() - 2, v.top() + frame + view_->rowViewportPosition(hoverRow_) +
                                                                 view_->rowHeight(hoverRow_) / 2 -
                                                                 deleteRow_->height() / 2);
    }
    const int columns = model_->columnCount();
    hoverColumn_ = position.y() <= v.top() + kReach && insideX && columns > 1 ? view_->columnAt(inView.x()) : -1;
    deleteColumn_->setVisible(hoverColumn_ >= 0);
    if (hoverColumn_ >= 0) {
        deleteColumn_->move(v.left() + frame + view_->columnViewportPosition(hoverColumn_) +
                                view_->columnWidth(hoverColumn_) / 2 - deleteColumn_->width() / 2,
                            v.top() - deleteColumn_->height() - 2);
    }
}

}  // namespace hitokoto::ui
