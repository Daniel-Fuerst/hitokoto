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
#include <QToolButton>

class QTextDocument;

namespace hitokoto::ui {

class EditorFormats;
class TableModel;

class TableView;

/// The text of the current cell (S018, "kept from the first version"): it stays open while its
/// cell is current and writes every keystroke to the Markdown line at once. Keys at the cell's
/// edges (Tab, Enter, arrows, Escape, row and column shortcuts) go to the table view.
class CellEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CellEditor(TableView* view, QWidget* parent);
    void setPadding(int padding) { setViewportMargins(padding, padding, padding, padding); }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    TableView* view_;
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

protected:
    /// Keys go to the cell editor (and from there to the view), not to the delegate, which would
    /// close the editor on Escape and Enter.
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void prepare(QTextDocument& document, const QModelIndex& index) const;
    [[nodiscard]] static QString cacheKey(const QModelIndex& index);

    qreal padding_ = 4;
    mutable QHash<QString, QSizeF> naturalCache_;
    mutable QHash<QString, qreal> heightCache_;  ///< by cache key and width
};

/// An add (`+`) or delete (`−`) control along a table edge, drawn in the table's header colours
/// as before the revision of S018: a rounded bar with the sign in its middle.
class TableControl : public QToolButton {
    Q_OBJECT
public:
    TableControl(bool plus, QWidget* parent);
    void setColors(const QColor& background, const QColor& border, const QColor& sign, qreal borderWidth, qreal radius);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool plus_;
    QColor background_;
    QColor border_;
    QColor sign_;
    qreal borderWidth_ = 1;
    qreal radius_ = 0;
};

/// One table. Signals ask the embedder to change the document or leave the table.
class TableView : public QTableView {
    Q_OBJECT
public:
    enum class Caret { Start, End };

    explicit TableView(QWidget* parent);

    void setModel(QAbstractItemModel* model) override;
    [[nodiscard]] TableDelegate* tableDelegate() const noexcept { return delegate_; }
    /// Sizes columns to their content, at most `width` pixels in all, rows to their wrapped
    /// content, and the view to its cells.
    void fit(int width);
    /// Makes (row, column) current and puts the caret at the start or end of its text.
    void enterCell(int row, int column, Caret caret);
    /// The editor of the current cell, if open.
    [[nodiscard]] CellEditor* currentEditor() const;
    /// A key in the current cell's editor; true if it was a table key and is handled.
    bool cellKey(QKeyEvent* event, CellEditor* editor);
    /// Whether every cell of `column` is empty, the header's included.
    [[nodiscard]] bool isColumnEmpty(int column) const;

signals:
    /// To the line above the table; `atEnd` puts the caret at its end (Left), else its start.
    void leaveAbove(bool atEnd);
    /// To the line below the table, if there is one (Down, Right).
    void leaveDown();
    /// Below the table, adding lines at the end of the document (Escape, Ctrl+Enter).
    void leaveBelow();
    void appendRow(int column);
    void removeLastRowAndLeave();
    /// `preferPrevious`: the caret goes to the row above (Backspace), else to the row now there.
    void deleteRow(int row, bool preferPrevious);
    void addColumn();
    void deleteColumn(int column);
    void toggleSource();
    void undo();
    void redo();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    /// One click puts the caret where clicked; dragging selects text in that cell.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private:
    void handleEnter();
    /// Tab and Shift+Tab: the next or previous cell, appending a row after the last.
    void moveToNeighbour(bool backward);

    TableDelegate* delegate_;
    CellEditor* dragEditor_ = nullptr;  ///< selecting by dragging from a click into another cell
    int dragAnchor_ = 0;
    struct Restore {
        bool focus = false;
        int row = 0;
        int column = 0;
        int position = 0;
    } restore_;  ///< the caret across a model reset (rows or columns changed, undo)
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
    TableControl* addRow_;
    TableControl* addColumn_;
    TableControl* deleteRow_;
    TableControl* deleteColumn_;
    int thickness_ = 14;  ///< of the controls, from the editor font
    int gutter_ = 18;     ///< thickness_ plus the gaps around it
    int hoverRow_ = -1;
    int hoverColumn_ = -1;
};

}  // namespace hitokoto::ui
