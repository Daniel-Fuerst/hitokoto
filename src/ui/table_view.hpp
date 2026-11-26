// SPDX-License-Identifier: GPL-3.0-or-later
// S018 / ADR 0005: the widgets that show one table inside the editor. Qt draws and edits the cells
// (QTextDocument::setMarkdown, QPlainTextEdit); the table view handles navigation, the container
// the hover controls for rows and columns.
#pragma once

#include "hitokoto/core/table_selection.hpp"

#include <QColor>
#include <QCursor>
#include <QHash>
#include <QPlainTextEdit>
#include <QSizeF>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>

#include <functional>
#include <optional>

class QMenu;
class QMimeData;
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
    /// S031 review O2, S041: Paste as Plain Text in a cell puts the clipboard's plain text at the
    /// caret and never fills cells with a table; a selection of cells ends first (S040, decision 11).
    void pasteAsPlainText();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// keyPressEvent for a key whose text may enter the cell as it is (text_input.hpp).
    void pressKey(QKeyEvent* event);
    /// S041: Cut, Copy and Paste of `menu` act on the selected cells; Copy as Tab-Separated Text
    /// goes below Copy.
    void cellClipboardEntries(QMenu& menu);

    TableView* view_;
};

/// Draws a cell's Markdown with Qt's renderer and opens a CellEditor on it.
class TableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TableDelegate(QObject* parent = nullptr);
    void setPadding(qreal padding);
    /// S040: selected cells and the current cell (table.selection-background, table.selection-text,
    /// table.current-background).
    void setSelectionColors(const QColor& background, const QColor& text, const QColor& current);
    [[nodiscard]] QColor selectionBackground() const { return selectionBackground_; }
    [[nodiscard]] QColor selectionText() const { return selectionText_; }

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
    /// Lays the cell at `index` out in `document` as it is painted, without padding.
    void prepare(QTextDocument& document, const QModelIndex& index) const;

protected:
    /// Keys go to the cell editor (and from there to the view), not to the delegate, which would
    /// close the editor on Escape and Enter.
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void prepare(QTextDocument& document, const QModelIndex& index, const QString& source) const;
    [[nodiscard]] QSizeF naturalSize(const QModelIndex& index, const QString& markdown) const;
    /// No block margins, the column's alignment.
    static void formatBlocks(QTextDocument& document, const QModelIndex& index);
    [[nodiscard]] static QString cacheKey(const QModelIndex& index, const QString& markdown);

    qreal padding_ = 4;
    QColor selectionBackground_{0xdc, 0xd8, 0xfe};
    QColor selectionText_{0x1f, 0x23, 0x28};
    QColor currentBackground_{0xdc, 0xd8, 0xfe, 60};
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

    /// S040: a selection of cells, held in the selection model (decision 13); none while the caret
    /// is in a cell's text.
    [[nodiscard]] bool hasCellSelection() const noexcept { return cells_.has_value(); }
    [[nodiscard]] std::optional<core::TableCellRange> cellSelection() const;
    /// Ends a selection of cells; with `restoreCaret` the caret goes where it was before it began.
    void endCellSelection(bool restoreCaret = true);
    /// Decision 3 and 7: the whole table, the current rows, the current columns.
    void selectTable();
    void selectRows();
    void selectColumns();
    /// Decision 6: a click in the area of row (`rows`) or column `index`; `extend` keeps the rows
    /// or columns selected so far from where they started (Shift, dragging).
    void selectArea(bool rows, int index, bool extend);
    /// Decision 12: a selection in the editor covers the table; all its cells show as selected.
    void setCoveredByEditor(bool covered);

    /// S041: Ctrl+C (or `Copy as Tab-Separated Text`) and Ctrl+X on the selected cells; nothing
    /// without them. Cutting copies, then deletes as Delete does (S040, decision 10).
    void copyCells(bool tabSeparated);
    void cutCells();
    /// S041, decision 7: what a paste does to the table, decided by the embedder: `range` the
    /// selected cells, or the cell with the caret (`caretOnly`). True if the paste is the table's
    /// (even if it changes nothing); false lets the cell's editor paste text at the caret.
    using PasteHandler = std::function<bool(const QMimeData* source, core::TableCellRange range, bool caretOnly)>;
    void setPasteHandler(PasteHandler handler) { pasteHandler_ = std::move(handler); }
    /// Pastes `source` into the selected cells, or from the cell with the caret; see PasteHandler.
    bool pasteCells(const QMimeData* source);

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
    /// S040, decision 10: empty the cells of `range`; remove body rows `first` to `last`; remove
    /// columns `first` to `last`; remove the whole table.
    void emptyCells(hitokoto::core::TableCellRange range);
    void deleteRows(int first, int last);
    void deleteColumns(int first, int last);
    void removeTable();
    /// Decision 3: a third Ctrl+A selects the whole document.
    void selectDocument();
    /// Decision 12: a selection of cells extended past the last row (`down`) or above the header
    /// row becomes a selection in the editor.
    void extendBeyond(bool down);
    /// S041: put the cells of `range` on the clipboard, as Markdown or as tab-separated text.
    void copySelectedCells(hitokoto::core::TableCellRange range, bool tabSeparated);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    /// One click puts the caret where clicked; dragging selects text in that cell.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private:
    /// S040: keys while cells are selected, and the keys that select them; true if handled.
    bool selectionKey(QKeyEvent* event, CellEditor* editor);
    bool selectAllKey(CellEditor* editor);
    bool startCellsKey(QKeyEvent* event, CellEditor* editor);
    bool cellsKey(QKeyEvent* event);
    void extendCells(core::TableStep step);
    bool deleteKey(Qt::KeyboardModifiers modifiers);
    /// Starts or replaces the selection of cells, remembering the caret first.
    void setCells(const core::TableCellSelection& selection);
    /// The selection model, the look of the current cell's editor and the actions follow cells_.
    void showCells();
    void deleteSelectedRows();
    void deleteSelectedColumns();
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
    } restore_;                                      ///< the caret across a model reset (rows or columns changed, undo)
    std::optional<core::TableCellSelection> cells_;  ///< S040
    struct Before {
        int row = 0;
        int column = 0;
        int position = 0;
    } before_;                ///< S040: where the caret was before the selection of cells began
    bool dragCells_ = false;  ///< S040: a drag that left the text of its cell
    bool showingCells_ = false;
    bool covered_ = false;       ///< S040, decision 12
    PasteHandler pasteHandler_;  ///< S041
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
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// S040, decision 6: the area of a row (between its delete control and the table, the table's
    /// border included) or of a column (the same above the table) at `position`.
    struct Area {
        bool row = true;
        int index = 0;
    };
    [[nodiscard]] std::optional<Area> areaAt(QPoint position) const;
    /// The pointer over the areas: an arrow pointing right (rows) or down (columns), drawn from
    /// icons/select-*.svg in the theme's colours at the screen's scale.
    [[nodiscard]] QCursor areaPointer(bool row) const;
    /// Mouse events over the areas, also where the table's border lies inside the view; true if
    /// handled.
    bool areaMouse(QMouseEvent* event, QPoint position);
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
    std::optional<Area> areaDrag_;              ///< S040: dragging across the areas of rows or columns
    QColor pointerFill_;                        ///< table.select-pointer-fill, from applyStyle
    QColor pointerOutline_;                     ///< table.select-pointer-outline
    mutable QHash<QString, QCursor> pointers_;  ///< by direction, colours and scale
};

}  // namespace hitokoto::ui
