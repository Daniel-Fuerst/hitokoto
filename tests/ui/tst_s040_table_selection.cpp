// SPDX-License-Identifier: GPL-3.0-or-later
// S040: selecting cells, rows and columns in a table. Keys only through test::keyClick (CLAUDE.md).
#include "table_view.hpp"
#include "ui_test_support.hpp"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleTableInterface>
#include <QAction>
#include <QImage>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTimer>

#include <algorithm>
#include <memory>

using namespace hitokoto;

namespace {

// The table of the story's scenarios: a header row and three body rows of four columns. Row 1,
// column 1 holds "b1x" so that the caret can stand in the middle of a text.
constexpr auto kTable =
    "above\n"
    "\n"
    "| A | B | C | D |\n"
    "| --- | --- | --- | --- |\n"
    "| a1 | b1x | c1 | d1 |\n"
    "| a2 | b2 | c2 | d2 |\n"
    "| a3 | b3 |  | d3 |\n"
    "\n"
    "below";
constexpr int kHeaderBlock = 2;

#ifdef Q_OS_MACOS
constexpr Qt::KeyboardModifier kColumnKey = Qt::MetaModifier;  // the Control key (decision 4)
#else
constexpr Qt::KeyboardModifier kColumnKey = Qt::ControlModifier;
#endif

QPlainTextEdit* focusedCell() {
    return qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
}

void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    test::keyClick(QApplication::focusWidget(), key, modifiers);
    QCoreApplication::processEvents();
}

/// The selected cells as "row,column" in reading order, joined by spaces.
QString selected(const QTableView* view) {
    QModelIndexList indexes = view->selectionModel()->selectedIndexes();
    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() != b.row() ? a.row() < b.row() : a.column() < b.column();
    });
    QStringList out;
    for (const QModelIndex& index : indexes) {
        out << QStringLiteral("%1,%2").arg(index.row()).arg(index.column());
    }
    return out.join(u' ');
}

/// The cells of rows `top`..`bottom` and columns `left`..`right` as selected() lists them.
QString cells(int top, int left, int bottom, int right) {
    QStringList out;
    for (int r = top; r <= bottom; ++r) {
        for (int c = left; c <= right; ++c) {
            out << QStringLiteral("%1,%2").arg(r).arg(c);
        }
    }
    return out.join(u' ');
}

/// The colour painted near the top left corner of a cell, inside its padding, where no text is.
QColor paintedAt(QTableView* view, int row, int column) {
    const QImage image = view->grab().toImage();
    const QRect rect = view->visualRect(view->model()->index(row, column));
    const QPoint point = view->viewport()->mapTo(view, rect.topLeft() + QPoint(2, 2));
    return image.pixelColor(point);
}

/// The cells painted with the selection colour, as selected() lists them.
QString paintedSelected(QTableView* view) {
    const QColor selection =
        test::qcolor(core::tableStyle(core::builtinTheme(core::ColorScheme::Light)).selectionBackground);
    QStringList out;
    for (int r = 0; r < view->model()->rowCount(); ++r) {
        for (int c = 0; c < view->model()->columnCount(); ++c) {
            if (paintedAt(view, r, c) == selection) {
                out << QStringLiteral("%1,%2").arg(r).arg(c);
            }
        }
    }
    return out.join(u' ');
}

/// Clicks the cell, then puts the caret at `position` of its text.
QPlainTextEdit* caretIn(QTableView* view, int row, int column, int position) {
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {},
                      view->visualRect(view->model()->index(row, column)).center());
    QCoreApplication::processEvents();
    QPlainTextEdit* cell = focusedCell();
    if (cell != nullptr) {
        QTextCursor cursor = cell->textCursor();
        cursor.setPosition(position);
        cell->setTextCursor(cursor);
    }
    return cell;
}

/// The entries of the table's context menu, without accelerators and shortcuts.
QStringList contextMenuOf(QWidget* widget) {
    auto texts = std::make_shared<QStringList>();
    QTimer::singleShot(0, [texts] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            for (const QAction* entry : menu->actions()) {
                if (!entry->isSeparator() && entry->isVisible()) {
                    *texts << entry->text().remove(u'&').section(u'\t', 0, 0);
                }
            }
            menu->close();
        }
    });
    // A text field takes the event on its viewport, as from the mouse.
    QWidget* target = widget;
    if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
        target = area->viewport();
    }
    const QPoint point = target->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, target->mapToGlobal(point));
    QCoreApplication::sendEvent(target, &event);
    QCoreApplication::processEvents();
    return *texts;
}

QAction* action(QTableView* view, const char* name) {
    return view->findChild<QAction*>(QString::fromLatin1(name));
}

}  // namespace

class tst_s040_table_selection : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<test::Fixture> f_;
    QTableView* view_ = nullptr;

    QString text() const { return f_->editor()->toPlainText(); }

private slots:
    void init() {
        f_ = std::make_unique<test::Fixture>();
        QVERIFY(f_->open(f_->writeFile(QStringLiteral("table.md"), kTable)));
        f_->settle();
        view_ = f_->editor()->tableViewAt(kHeaderBlock);
        QVERIFY(view_ != nullptr);
    }

    void cleanup() {
        view_ = nullptr;
        f_.reset();
    }

    void aRectangleOfCellsIsSelectedAndVisible() {
        QVERIFY(caretIn(view_, 1, 1, 1) != nullptr);
        for (int i = 0; i < 3; ++i) {  // to the end of "b1x", then the cell
            press(Qt::Key_Right, Qt::ShiftModifier);
        }
        QCOMPARE(selected(view_), cells(1, 1, 1, 1));
        press(Qt::Key_Right, Qt::ShiftModifier);
        press(Qt::Key_Right, Qt::ShiftModifier);
        press(Qt::Key_Down, Qt::ShiftModifier);
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(1, 1, 3, 3));
        QCOMPARE(paintedSelected(view_), cells(1, 1, 3, 3));
        QCOMPARE(text(), QString::fromUtf8(kTable));
        QVERIFY(!f_->editor()->document()->isModified());
    }

    void fromTextToACellWithKeys() {
        QPlainTextEdit* cell = caretIn(view_, 1, 1, 1);
        QVERIFY(cell != nullptr);
        press(Qt::Key_Right, Qt::ShiftModifier);
        press(Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(cell->textCursor().selectedText(), QStringLiteral("1x"));
        QCOMPARE(selected(view_), QString());
        QCOMPARE(paintedSelected(view_), QString());
        press(Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(1, 1, 1, 1));
        QCOMPARE(paintedSelected(view_), cells(1, 1, 1, 1));
        // Towards the start: the cell, then the cell to the left.
        QVERIFY(caretIn(view_, 2, 1, 1) != nullptr);
        QCOMPARE(selected(view_), QString());
        press(Qt::Key_Left, Qt::ShiftModifier);
        press(Qt::Key_Left, Qt::ShiftModifier);
        press(Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(2, 0, 2, 1));
        // Down on the last line: the end of the text, then the cell, then the cell below.
        QVERIFY(caretIn(view_, 1, 2, 0) != nullptr);
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), QString());
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(1, 2, 1, 2));
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(1, 2, 2, 2));
    }

    void rowsAndColumnsWithKeys() {
        QPlainTextEdit* cell = caretIn(view_, 2, 1, 1);
        QVERIFY(cell != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(2, 0, 2, 3));
        QCOMPARE(paintedSelected(view_), cells(2, 0, 2, 3));
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(2, 0, 3, 3));
        press(Qt::Key_Space, kColumnKey);
        QCOMPARE(selected(view_), cells(0, 0, 3, 3));  // every column the rows lie in
        QCOMPARE(text(), QString::fromUtf8(kTable));   // no space typed
        press(Qt::Key_Escape);
        QVERIFY(caretIn(view_, 2, 2, 0) != nullptr);
        press(Qt::Key_Space, kColumnKey);
        QCOMPARE(selected(view_), cells(0, 2, 3, 2));  // the header cell included
        press(Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(0, 2, 3, 3));
    }

    void ctrlATwiceSelectsTheTable() {
        QPlainTextEdit* cell = caretIn(view_, 1, 1, 1);
        QVERIFY(cell != nullptr);
        press(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(cell->textCursor().selectedText(), QStringLiteral("b1x"));
        QCOMPARE(selected(view_), QString());
        press(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(selected(view_), cells(0, 0, 3, 3));
        QCOMPARE(paintedSelected(view_), cells(0, 0, 3, 3));
        // An empty cell: the first Ctrl+A selects the cell, the second the table.
        press(Qt::Key_Escape);
        QVERIFY(caretIn(view_, 3, 2, 0) != nullptr);
        press(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(selected(view_), cells(3, 2, 3, 2));
        press(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(selected(view_), cells(0, 0, 3, 3));
    }

    void theContextMenuSelects() {
        QPlainTextEdit* cell = caretIn(view_, 2, 1, 0);
        QVERIFY(cell != nullptr);
        const QStringList entries = contextMenuOf(cell);
        const qsizetype at = entries.indexOf(QStringLiteral("Select All"));
        QVERIFY2(at >= 0, qPrintable(entries.join(u'|')));
        QCOMPARE(entries.mid(at, 3), (QStringList{QStringLiteral("Select All"), QStringLiteral("Select Column"),
                                                  QStringLiteral("Select Row")}));
        QVERIFY(action(view_, "tableSelectRow") != nullptr);
        action(view_, "tableSelectRow")->trigger();
        QCOMPARE(selected(view_), cells(2, 0, 2, 3));
        action(view_, "tableSelectColumn")->trigger();
        QCOMPARE(selected(view_), cells(0, 0, 3, 3));  // the columns of the selected row: all
        press(Qt::Key_Escape);
        QVERIFY(caretIn(view_, 2, 1, 0) != nullptr);
        action(view_, "tableSelectColumn")->trigger();
        QCOMPARE(selected(view_), cells(0, 1, 3, 1));
        action(view_, "tableSelectAll")->trigger();
        QCOMPARE(selected(view_), cells(0, 0, 3, 3));
    }

    void endingASelectionOfCells() {
        QVERIFY(caretIn(view_, 1, 1, 1) != nullptr);
        for (int i = 0; i < 3; ++i) {
            press(Qt::Key_Right, Qt::ShiftModifier);
        }
        press(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selected(view_), cells(1, 1, 2, 1));
        press(Qt::Key_Escape);
        QCOMPARE(selected(view_), QString());
        QCOMPARE(view_->currentIndex(), view_->model()->index(1, 1));
        QCOMPARE(focusedCell()->textCursor().position(), 1);
        QVERIFY(!focusedCell()->textCursor().hasSelection());
        // An arrow without Shift: as if there had been no selection.
        for (int i = 0; i < 3; ++i) {
            press(Qt::Key_Right, Qt::ShiftModifier);
        }
        QCOMPARE(selected(view_), cells(1, 1, 1, 1));
        press(Qt::Key_Right);
        QCOMPARE(selected(view_), QString());
        QCOMPARE(view_->currentIndex(), view_->model()->index(1, 1));
        QCOMPARE(focusedCell()->textCursor().position(), 2);
        // Tab: to the next cell, counted from there.
        press(Qt::Key_Space, Qt::ShiftModifier);
        press(Qt::Key_Tab);
        QCOMPARE(selected(view_), QString());
        QCOMPARE(view_->currentIndex(), view_->model()->index(1, 2));
        QCOMPARE(text(), QString::fromUtf8(kTable));
    }

    void typingNeverReplacesSelectedCells() {
        QPlainTextEdit* cell = caretIn(view_, 1, 1, 3);
        QVERIFY(cell != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);  // Shift held a moment too long
        QCOMPARE(selected(view_), cells(1, 0, 1, 3));
        test::keyClicks(QApplication::focusWidget(), QStringLiteral("y"));
        QCoreApplication::processEvents();
        QCOMPARE(selected(view_), QString());
        QVERIFY(text().contains(QStringLiteral("| a1 | b1xy | c1 | d1 |")));
    }

    void deletingActsOnTheSelection() {
        QVERIFY(caretIn(view_, 1, 1, 3) != nullptr);
        press(Qt::Key_Right, Qt::ShiftModifier);
        press(Qt::Key_Right, Qt::ShiftModifier);
        press(Qt::Key_Down, Qt::ShiftModifier);
        press(Qt::Key_Delete);
        QVERIFY2(text().contains(QStringLiteral("| a1 |  |  | d1 |\n| a2 |  |  | d2 |")), qPrintable(text()));
        f_->editor()->document()->undo();
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable));

        // Two whole body rows, then Delete Row.
        view_ = f_->editor()->tableViewAt(kHeaderBlock);
        QVERIFY(caretIn(view_, 1, 0, 0) != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);
        press(Qt::Key_Down, Qt::ShiftModifier);
        action(view_, "tableDeleteRow")->trigger();
        f_->settle();
        QVERIFY2(!text().contains(QStringLiteral("a1")) && !text().contains(QStringLiteral("a2")) &&
                     text().contains(QStringLiteral("| a3 |")),
                 qPrintable(text()));
        f_->editor()->document()->undo();
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable));

        // Only the header row: Delete Row is disabled.
        view_ = f_->editor()->tableViewAt(kHeaderBlock);
        QVERIFY(caretIn(view_, 0, 0, 0) != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);
        QVERIFY(!action(view_, "tableDeleteRow")->isEnabled());

        // Every column: only the first remains.
        press(Qt::Key_Escape);
        QVERIFY(caretIn(view_, 0, 0, 0) != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);
        press(Qt::Key_Space, kColumnKey);
        action(view_, "tableDeleteColumn")->trigger();
        f_->settle();
        QVERIFY2(text().contains(QStringLiteral("| A |\n| --- |\n| a1 |\n| a2 |\n| a3 |")), qPrintable(text()));
        f_->editor()->document()->undo();
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable));

        // The whole table: its lines and the blank line below it.
        view_ = f_->editor()->tableViewAt(kHeaderBlock);
        QVERIFY(caretIn(view_, 1, 1, 1) != nullptr);
        press(Qt::Key_A, Qt::ControlModifier);
        press(Qt::Key_A, Qt::ControlModifier);
        press(Qt::Key_Backspace);
        f_->settle();
        QCOMPARE(text(), QStringLiteral("above\n\nbelow"));
        f_->editor()->document()->undo();
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable));
    }

    void aScreenReaderLearnsTheSelection() {
        QVERIFY(caretIn(view_, 2, 1, 0) != nullptr);
        press(Qt::Key_Space, Qt::ShiftModifier);
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view_);
        QVERIFY(table != nullptr);
        QAccessibleTableInterface* cellsOf = table->tableInterface();
        QVERIFY(cellsOf != nullptr);
        QCOMPARE(cellsOf->selectedCellCount(), 4);
        QCOMPARE(cellsOf->selectedRows(), QList<int>{2});
    }
};

QTEST_MAIN(tst_s040_table_selection)
#include "tst_s040_table_selection.moc"
