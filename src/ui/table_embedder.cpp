// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_embedder.hpp"

#include "block_layout.hpp"
#include "block_styler.hpp"
#include "editor_formats.hpp"
#include "editor_widget.hpp"
#include "qt_bridge.hpp"
#include "table_model.hpp"
#include "table_view.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QMetaObject>
#include <QScopedValueRollback>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>

namespace hitokoto::ui {
namespace {

// Tables this many lines above or below the screen are prepared too, so scrolling meets them
// ready.
constexpr int kMarginBlocks = 200;

/// Replaces the text of `block` with `text` unless it is already that.
void setLine(QTextCursor& cursor, const QTextBlock& block, const QString& text) {
    if (block.text() == text) {
        return;
    }
    cursor.setPosition(block.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(text);
}

}  // namespace

TableEmbedder::TableEmbedder(EditorWidget* editor, BlockStyler* styler, BlockLayout* layout)
    : QObject(editor), editor_(editor), styler_(styler), layout_(layout) {}

TableEmbedder::~TableEmbedder() {
    for (Table& table : tables_) {
        delete table.widget;
    }
}

// --- lookup -----------------------------------------------------------------------------------

TableEmbedder::Table* TableEmbedder::tableAt(int blockNumber) {
    for (Table& table : tables_) {
        if (table.header.isValid() && table.last.isValid() && table.header.blockNumber() <= blockNumber &&
            blockNumber <= table.last.blockNumber()) {
            return &table;
        }
    }
    return nullptr;
}

const TableEmbedder::Table* TableEmbedder::tableAt(int blockNumber) const {
    return const_cast<TableEmbedder*>(this)->tableAt(blockNumber);
}

TableEmbedder::Table* TableEmbedder::tableOf(const TableWidget* widget) {
    for (Table& table : tables_) {
        if (table.widget == widget) {
            return &table;
        }
    }
    return nullptr;
}

QTableView* TableEmbedder::viewAt(int blockNumber) const {
    const Table* table = tableAt(blockNumber);
    return table != nullptr && !table->source && table->widget ? table->widget->view() : nullptr;
}

bool TableEmbedder::isSourceShown(int blockNumber) const {
    const Table* table = tableAt(blockNumber);
    return table != nullptr && table->source;
}

QTextBlock TableEmbedder::lastLineOf(const QTextBlock& header) const {
    QTextBlock last = header;
    for (QTextBlock next = header.next(); next.isValid(); next = next.next()) {
        const core::LineRender render = styler_->renderFor(next);
        if (!render.table || render.tableHeader) {
            break;
        }
        last = next;
    }
    return last;
}

// --- finding and showing tables ---------------------------------------------------------------

void TableEmbedder::setFormats(const EditorFormats* formats) {
    formats_ = formats;
    for (Table& table : tables_) {
        if (table.widget != nullptr) {
            table.widget->applyStyle(*formats_);
        }
        table.signature = 0;
    }
    scheduleRefresh();
}

void TableEmbedder::setAvailableWidth(int width) {
    if (width == availableWidth_) {
        return;
    }
    availableWidth_ = width;
    for (Table& table : tables_) {
        table.signature = 0;
    }
}

void TableEmbedder::scheduleRefresh() {
    if (refreshPending_) {
        return;
    }
    refreshPending_ = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            refreshPending_ = false;
            refresh();
        },
        Qt::QueuedConnection);
}

void TableEmbedder::documentChanged(int position, int /*charsRemoved*/, int charsAdded) {
    // Only typing in the editor holds a table; loading, restyling and the table view's own
    // changes do not.
    if (typingDepth_ > 0 && !styler_->isBusy()) {
        // A line already in a shown table (Backspace below it joins the line to the last row)
        // keeps the table view; only a line that is becoming part of a table is held (S018).
        const QTextBlock typed = editor_->document()->findBlock(position + charsAdded);
        const Table* table = typed.isValid() ? tableAt(typed.blockNumber()) : nullptr;
        typing_ = table != nullptr && table->widget != nullptr && !table->source ? QTextBlock() : typed;
    }
    scheduleRefresh();
}

void TableEmbedder::refresh() {
    if (refreshing_ || formats_ == nullptr) {
        return;
    }
    refreshing_ = true;
    QTextDocument* document = editor_->document();
    const auto [firstVisible, lastVisible] = editor_->visibleBlockRange();
    const int from = std::max(0, firstVisible - kMarginBlocks);
    const int to = std::min(document->blockCount() - 1, std::max(lastVisible, firstVisible) + kMarginBlocks);

    // Start at the header of a table that the scanned range begins inside.
    QTextBlock block = document->findBlockByNumber(from);
    while (block.isValid() && block.previous().isValid()) {
        const core::LineRender render = styler_->renderFor(block);
        if (!render.table || render.tableHeader) {
            break;
        }
        block = block.previous();
    }

    std::vector<Table> found;
    while (block.isValid() && block.blockNumber() <= to) {
        if (styler_->renderFor(block).tableHeader) {
            Table table;
            table.header = block;
            table.last = lastLineOf(block);
            found.push_back(table);
            block = table.last.next();
            continue;
        }
        block = block.next();
    }

    std::vector<Table> next;
    std::vector<bool> kept(tables_.size(), false);
    for (Table& table : found) {
        for (std::size_t i = 0; i < tables_.size(); ++i) {
            if (!kept[i] && tables_[i].header == table.header) {
                kept[i] = true;
                Table existing = tables_[i];
                existing.last = table.last;
                table = existing;
                break;
            }
        }
        next.push_back(table);
    }
    for (std::size_t i = 0; i < tables_.size(); ++i) {
        if (kept[i]) {
            continue;
        }
        Table& old = tables_[i];
        const bool outside = old.header.isValid() && (old.header.blockNumber() < from || old.header.blockNumber() > to);
        if (outside && styler_->renderFor(old.header).tableHeader) {
            next.push_back(old);  // not scanned this time; still a table
        } else {
            release(old);
        }
    }
    tables_ = std::move(next);
    const int typing = typing_.isValid() ? typing_.blockNumber() : -1;
    for (Table& table : tables_) {
        table.held = table.header.isValid() && table.last.isValid() && typing >= table.header.blockNumber() &&
                     typing <= table.last.blockNumber();
        apply(table);
    }
    refreshing_ = false;
    if (editor_->hasFocus()) {
        caretMoved();  // the caret may now be on lines a table view stands for
    }
}

void TableEmbedder::release(Table& table) {
    for (const QTextBlock& line : table.hidden) {
        if (line.isValid()) {
            layout_->setHidden(line, false);
        }
    }
    table.hidden.clear();
    if (table.header.isValid()) {
        layout_->setReservedHeight(table.header, 0);
    }
    if (table.widget != nullptr) {
        // Keys always reach the editor or a cell (S018): a caret in the table goes into its
        // lines before the view goes.
        if (table.widget->isAncestorOf(QApplication::focusWidget())) {
            const QScopedValueRollback<bool> leaving(leaving_, true);
            const TableModel* model = table.widget->model();
            const QModelIndex current = table.widget->view()->currentIndex();
            const QTextBlock line = model->lineOfRow(std::max(current.row(), 0));
            editor_->setFocus();
            // Only while that line is still in the document: a table that went with the whole
            // text (another file opened) leaves the caret to whoever replaced the text.
            const QTextDocument* document = editor_->document();
            if (line.isValid() && line.position() + line.length() <= document->characterCount() &&
                document->findBlock(line.position()) == line) {
                QTextCursor cursor = editor_->textCursor();
                cursor.setPosition(line.position() + std::min(model->positionOfCell(std::max(current.row(), 0),
                                                                                    std::max(current.column(), 0)),
                                                              line.length() - 1));
                editor_->setTextCursor(cursor);
            }
        }
        table.widget->hide();
        table.widget->deleteLater();
        table.widget = nullptr;
    }
    table.signature = 0;
}

TableWidget* TableEmbedder::createWidget() {
    auto* model = new TableModel(editor_->document(), formats_);
    auto* widget = new TableWidget(model, editor_->viewport());
    widget->applyStyle(*formats_);
    TableView* view = widget->view();
    const auto on = [this, widget](auto action) {
        return [this, widget, action](auto... args) {
            if (Table* table = tableOf(widget)) {
                action(*table, args...);
            }
        };
    };
    connect(view, &TableView::appendRow, this, on([this](Table& t, int column) { appendRow(t, column); }));
    connect(view, &TableView::deleteRow, this,
            on([this](Table& t, int row, bool preferPrevious) { deleteRow(t, row, preferPrevious); }));
    connect(view, &TableView::addColumn, this, on([this](Table& t) { addColumn(t); }));
    connect(view, &TableView::deleteColumn, this, on([this](Table& t, int column) { deleteColumn(t, column); }));
    connect(view, &TableView::removeLastRowAndLeave, this, on([this](Table& t) { removeLastRowAndLeave(t); }));
    connect(view, &TableView::leaveAbove, this, on([this](Table& t, bool atEnd) { leaveAbove(t, atEnd); }));
    connect(view, &TableView::leaveDown, this, on([this](Table& t) { leaveDown(t); }));
    connect(view, &TableView::leaveBelow, this, on([this](Table& t) { leaveBelow(t); }));
    connect(view, &TableView::undo, editor_, [this] { editor_->document()->undo(); });
    connect(view, &TableView::redo, editor_, [this] { editor_->document()->redo(); });
    connect(view, &TableView::toggleSource, this, on([this, view](Table& t) {
                const QModelIndex current = view->currentIndex();
                toggleSource(t, std::max(current.row(), 0), std::max(current.column(), 0));
            }));
    return widget;
}

quint64 TableEmbedder::signatureOf(const Table& table) const {
    // Changes when a line of the table, its extent or its place in the document changes.
    quint64 hash = 1469598103934665603ULL;
    const auto mix = [&hash](quint64 value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(static_cast<quint64>(table.header.blockNumber()));
    mix(static_cast<quint64>(table.last.blockNumber()));
    for (QTextBlock line = table.header; line.isValid(); line = line.next()) {
        mix(static_cast<quint64>(line.revision()));
        mix(static_cast<quint64>(line.length()));
        if (line == table.last) {
            break;
        }
    }
    mix(static_cast<quint64>(availableWidth_));
    return hash;
}

void TableEmbedder::apply(Table& table) {
    if (!table.header.isValid() || !table.last.isValid()) {
        release(table);
        return;
    }
    if (table.source || table.held) {
        release(table);
        return;
    }
    const quint64 signature = signatureOf(table);
    if (table.widget != nullptr && signature == table.signature) {
        reposition();
        return;
    }
    std::vector<QTextBlock> hide;
    for (QTextBlock line = table.header.next(); line.isValid(); line = line.next()) {
        hide.push_back(line);
        if (line == table.last) {
            break;
        }
    }
    for (const QTextBlock& line : table.hidden) {
        if (line.isValid() && std::find(hide.begin(), hide.end(), line) == hide.end()) {
            layout_->setHidden(line, false);
        }
    }
    for (const QTextBlock& line : hide) {
        layout_->setHidden(line, true);
    }
    table.hidden = std::move(hide);
    if (table.widget == nullptr) {
        table.widget = createWidget();
    }
    table.widget->model()->setLines(table.header, table.last);
    table.widget->fit(availableWidth_);
    layout_->setReservedHeight(table.header, table.widget->height());
    table.widget->show();
    table.signature = signature;
    reposition();
}

void TableEmbedder::reposition() {
    const qreal margin = editor_->document()->documentMargin();
    const QPointF offset = editor_->contentOffset();
    for (const Table& table : tables_) {
        if (table.widget == nullptr || table.source || !table.header.isValid()) {
            continue;
        }
        const QRectF rect = layout_->blockBoundingRect(table.header);
        table.widget->move(static_cast<int>(margin + offset.x()) - table.widget->gutter(),
                           static_cast<int>(rect.top() + offset.y()));
    }
}

void TableEmbedder::update(Table& table) {
    table.last = lastLineOf(table.header);
    table.signature = 0;
    apply(table);
}

// --- caret and focus --------------------------------------------------------------------------

void TableEmbedder::focusCell(Table& table, int row, int column, bool atEnd) {
    if (table.widget == nullptr) {
        return;
    }
    TableModel* model = table.widget->model();
    if (model->rowCount() == 0 || model->columnCount() == 0) {
        return;
    }
    table.widget->view()->enterCell(std::clamp(row, 0, model->rowCount() - 1),
                                    std::clamp(column, 0, model->columnCount() - 1),
                                    atEnd ? TableView::Caret::End : TableView::Caret::Start);
}

bool TableEmbedder::handleEditorClick(QPoint position) {
    // A click above a table that starts the document or below one that ends it puts the caret
    // on a new line there (S018, kept from the first version).
    QTextDocument* document = editor_->document();
    for (Table& table : tables_) {
        if (table.widget == nullptr || table.source) {
            continue;
        }
        const QRect view = table.widget->view()->geometry().translated(table.widget->pos());
        if (table.header == document->firstBlock() && position.y() < view.top() - 4) {
            leaveAbove(table, false);
            return true;
        }
        if (table.last == document->lastBlock() && position.y() > view.bottom()) {
            leaveBelow(table);
            return true;
        }
    }
    return false;
}

bool TableEmbedder::handleEditorKey(QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (event->key() == Qt::Key_M && modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (Table* table = tableAt(block.blockNumber()); table != nullptr && table->source) {
            toggleSource(*table, block.blockNumber(), cursor.positionInBlock());
            return true;
        }
        return false;
    }
    if (modifiers != Qt::NoModifier || cursor.hasSelection()) {
        return false;
    }
    const QTextLayout* lines = block.layout();
    const QTextLine line = lines->lineForTextPosition(cursor.positionInBlock());
    if (event->key() == Qt::Key_Down) {
        if (line.isValid() && line.lineNumber() < lines->lineCount() - 1) {
            return false;  // still inside a wrapped line
        }
        QTextBlock next = block.next();
        while (next.isValid() && !next.isVisible()) {
            next = next.next();
        }
        if (Table* table = next.isValid() ? tableAt(next.blockNumber()) : nullptr;
            table != nullptr && !table->source && table->header == next) {
            focusCell(*table, 0, 0);
            return true;
        }
    } else if (event->key() == Qt::Key_Up) {
        if (line.isValid() && line.lineNumber() > 0) {
            return false;
        }
        QTextBlock previous = block.previous();
        while (previous.isValid() && !previous.isVisible()) {
            previous = previous.previous();
        }
        if (Table* table = previous.isValid() ? tableAt(previous.blockNumber()) : nullptr;
            table != nullptr && !table->source && table->widget) {
            focusCell(*table, table->widget->model()->rowCount() - 1, 0);
            return true;
        }
    }
    return false;
}

void TableEmbedder::caretMoved() {
    if (leaving_) {
        return;  // the caret is on its way out of a table
    }
    const QTextCursor cursor = editor_->textCursor();
    if (typing_.isValid() && cursor.block() != typing_) {
        typing_ = QTextBlock();  // left the typed line: a table held for it appears
        scheduleRefresh();
    }
    if (cursor.hasSelection()) {
        return;
    }
    Table* table = tableAt(cursor.blockNumber());
    if (table == nullptr || table->source || !table->widget) {
        return;
    }
    // The caret never stays on lines the table view stands for (S009); behind the text of a
    // cell (Backspace below the table joined a line to the last row) it goes to the cell's end.
    TableModel* model = table->widget->model();
    const int row = std::max(model->rowOfBlock(cursor.blockNumber()), 0);
    const int column = model->columnAt(row, cursor.positionInBlock());
    const auto end = model->positionOfCell(row, column) + static_cast<int>(model->cellMarkdown(row, column).size());
    focusCell(*table, row, column, cursor.positionInBlock() >= end);
}

void TableEmbedder::leaveAbove(Table& table, bool atEnd) {
    const QScopedValueRollback<bool> leaving(leaving_, true);
    QTextCursor cursor = editor_->textCursor();
    const QTextBlock above = table.header.previous();
    if (above.isValid()) {
        cursor.setPosition(atEnd ? above.position() + above.length() - 1 : above.position());
    } else {
        // The table starts the document: a line above it, apart from it by a blank line.
        cursor.setPosition(0);
        cursor.insertText(QStringLiteral("\n\n"));
        cursor.setPosition(0);
    }
    editor_->setFocus();
    editor_->setTextCursor(cursor);
}

void TableEmbedder::leaveDown(Table& table) {
    const QTextBlock below = table.last.next();
    if (!below.isValid()) {
        return;
    }
    const QScopedValueRollback<bool> leaving(leaving_, true);
    editor_->setFocus();
    editor_->setTextCursor(QTextCursor(below));
}

void TableEmbedder::leaveBelow(Table& table) {
    // As in the first version: onto the line after the blank line that ends the table; at the
    // end of the document a blank line and a line are added, so text typed there is a paragraph
    // and not a table row (GFM continues a table on the next line).
    const QScopedValueRollback<bool> leaving(leaving_, true);
    const QTextBlock next = table.last.next();
    const QTextBlock after = next.isValid() ? next.next() : QTextBlock();
    const bool blank = next.isValid() && next.text().trimmed().isEmpty();
    QTextCursor cursor = editor_->textCursor();
    if (!blank || !after.isValid()) {
        cursor.beginEditBlock();
        if (!blank) {
            cursor.setPosition(table.last.position() + table.last.length() - 1);
            cursor.insertText(QStringLiteral("\n\n"));
        } else {
            cursor.setPosition(next.position());
            cursor.insertText(QStringLiteral("\n"));
        }
        cursor.endEditBlock();
    }
    cursor.setPosition(table.last.next().next().position());
    editor_->setFocus();
    editor_->setTextCursor(cursor);
}

void TableEmbedder::toggleSource(Table& table, int rowOrBlock, int columnOrPosition) {
    if (!table.source) {
        // From the view: row and column of the current cell.
        TableModel* model = table.widget != nullptr ? table.widget->model() : nullptr;
        const QTextBlock line = model != nullptr ? model->lineOfRow(rowOrBlock) : table.header;
        const int position = model != nullptr ? model->positionOfCell(rowOrBlock, columnOrPosition) : 0;
        table.source = true;
        apply(table);
        QTextCursor cursor(line);
        cursor.setPosition(line.position() + position);
        editor_->setFocus();
        editor_->setTextCursor(cursor);
        return;
    }
    // From the editor: block number and position of the caret.
    table.source = false;
    table.signature = 0;
    apply(table);
    if (table.widget != nullptr) {
        TableModel* model = table.widget->model();
        const int row = std::max(model->rowOfBlock(rowOrBlock), 0);
        focusCell(table, row, model->columnAt(row, columnOrPosition));
    }
}

// --- changing rows and columns ----------------------------------------------------------------

void TableEmbedder::appendRow(Table& table, int column) {
    if (table.widget == nullptr) {
        return;
    }
    const int columns = table.widget->model()->columnCount();
    QTextCursor cursor(table.last);
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QStringLiteral("\n") + toQString(core::makeEmptyTableRow(static_cast<std::size_t>(columns))));
    cursor.endEditBlock();
    update(table);
    focusCell(table, table.widget->model()->rowCount() - 1, column);
}

void TableEmbedder::deleteRow(Table& table, int row, bool preferPrevious) {
    if (table.widget == nullptr || row < 1) {
        return;
    }
    TableModel* model = table.widget->model();
    const QTextBlock line = model->lineOfRow(row);
    if (!line.isValid()) {
        return;
    }
    const int column = std::max(table.widget->view()->currentIndex().column(), 0);
    QTextCursor cursor(line);
    cursor.beginEditBlock();
    cursor.setPosition(line.position() - 1);
    cursor.setPosition(line.position() + static_cast<int>(line.text().size()), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    update(table);
    // Backspace: to the end of the row above; Delete: to the row now in its place (S018).
    if (!preferPrevious && row < model->rowCount()) {
        focusCell(table, row, column);
    } else {
        focusCell(table, row - 1, column, preferPrevious);
    }
}

void TableEmbedder::addColumn(Table& table) {
    if (table.widget == nullptr) {
        return;
    }
    const int column = table.widget->model()->columnCount();
    const QTextBlock delimiter = table.header.next();
    QTextCursor cursor(table.header);
    cursor.beginEditBlock();
    for (QTextBlock line = table.header; line.isValid(); line = line.next()) {
        setLine(cursor, line, toQString(core::appendTableColumn(view(line.text()), line == delimiter)));
        if (line == table.last) {
            break;
        }
    }
    cursor.endEditBlock();
    update(table);
    focusCell(table, 0, column);
}

void TableEmbedder::deleteColumn(Table& table, int column) {
    if (table.widget == nullptr || table.widget->model()->columnCount() <= 1 || column < 0) {
        return;
    }
    const int row = std::max(table.widget->view()->currentIndex().row(), 0);
    QTextCursor cursor(table.header);
    cursor.beginEditBlock();
    for (QTextBlock line = table.header; line.isValid(); line = line.next()) {
        setLine(cursor, line, toQString(core::removeTableColumn(view(line.text()), static_cast<std::size_t>(column))));
        if (line == table.last) {
            break;
        }
    }
    cursor.endEditBlock();
    update(table);
    focusCell(table, row, std::min(column, table.widget->model()->columnCount() - 1));
}

void TableEmbedder::removeLastRowAndLeave(Table& table) {
    if (table.widget == nullptr) {
        return;
    }
    const QTextBlock line = table.last;
    if (table.widget->model()->rowCount() < 2 || line == table.header.next()) {
        return;
    }
    QTextCursor cursor(line);
    cursor.beginEditBlock();
    cursor.setPosition(line.position() - 1);
    cursor.setPosition(line.position() + static_cast<int>(line.text().size()), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    table.last = lastLineOf(table.header);
    leaveBelow(table);
}

}  // namespace hitokoto::ui
