// SPDX-License-Identifier: GPL-3.0-or-later
// S018 / ADR 0005: the widgets that show one table inside the editor. Qt draws and edits the cells
// (QTextDocument::setMarkdown, QPlainTextEdit); the table view handles navigation, the container
// the hover controls for rows and columns.
#pragma once

#include <QHash>
#include <QPlainTextEdit>
#include <QSizeF>
#include <QStyledItemDelegate>
#include <QTableView>

class QTextDocument;
class QToolButton;

namespace hitokoto::ui {

class EditorFormats;
class TableModel;

/// Edits one cell: Shift+Enter inserts a line break, Enter finishes the cell.
class CellEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CellEditor(QWidget* parent);
    void setPadding(int padding) { setViewportMargins(padding, padding, padding, padding); }

signals:
    void enterPressed();
    void tabPressed(bool backward);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};

/// Draws a cell's Markdown with Qt's renderer and opens a CellEditor on it.
class TableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TableDelegate(QObject* parent = nullptr);
    void setPadding(qreal padding);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    /// Natural (unwrapped) size of a cell's content, padding included.
    [[nodiscard]] QSizeF naturalSize(const QModelIndex& index) const;
    /// Height of a cell's content wrapped at `width` (padding included).
    [[nodiscard]] qreal heightAt(const QModelIndex& index, qreal width) const;

signals:
    void editorEnterPressed();

private:
    void prepare(QTextDocument& document, const QModelIndex& index) const;

    qreal padding_ = 4;
    mutable QHash<QString, QSizeF> naturalCache_;
};

/// One table. Signals ask the embedder to change the document or leave the table.
class TableView : public QTableView {
    Q_OBJECT
public:
    explicit TableView(QWidget* parent);

    [[nodiscard]] TableDelegate* tableDelegate() const noexcept { return delegate_; }
    /// Sizes columns to their content, at most `width` pixels in all, rows to their wrapped
    /// content, and the view to its cells.
    void fit(int width);

signals:
    void leaveUp(int column);
    void leaveDown(int column);
    void leaveBelow();
    void appendRow(int column);
    void removeLastRowAndLeave();
    void deleteRow(int row);
    void addColumn();
    void deleteColumn(int column);
    void toggleSource();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

private:
    void handleEnter();

    TableDelegate* delegate_;
};

/// The table view plus the hover controls around it; this is what the editor places in its
/// viewport, over the space the header line reserves.
class TableWidget : public QWidget {
    Q_OBJECT
public:
    TableWidget(TableModel* model, QWidget* parent);

    [[nodiscard]] TableView* view() const noexcept { return view_; }
    [[nodiscard]] TableModel* model() const noexcept { return model_; }
    /// Space around the view for the controls.
    [[nodiscard]] int gutter() const noexcept { return gutter_; }
    void applyStyle(const EditorFormats& formats);
    /// Fits the view into `width` pixels of text column and sizes the widget around it.
    void fit(int width);

signals:
    void addRowClicked();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateHover(QPoint position);
    void hideControls();

    TableModel* model_;
    TableView* view_;
    QToolButton* addRow_;
    QToolButton* addColumn_;
    QToolButton* deleteRow_;
    QToolButton* deleteColumn_;
    int gutter_ = 22;
    int hoverRow_ = -1;
    int hoverColumn_ = -1;
};

}  // namespace hitokoto::ui
