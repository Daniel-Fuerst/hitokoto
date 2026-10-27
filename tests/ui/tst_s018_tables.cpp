// SPDX-License-Identifier: GPL-3.0-or-later
// S018: Markdown pipe tables shown as an embedded table view (ADR 0005, revised 2026-10-27).
#include "table_model.hpp"
#include "ui_test_support.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QClipboard>
#include <QHeaderView>
#include <QInputMethodEvent>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTableView>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>

#include <cstdlib>
#include <memory>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

QString text(const test::Fixture& f) {
    return f.editor()->toPlainText();
}

/// The cell editor that the view opened, once it has focus.
QWidget* cellEditor() {
    return QApplication::focusWidget();
}

/// Opens the editor on (row, column) of `view`.
QWidget* editCell(QTableView* view, int row, int column) {
    const QModelIndex index = view->model()->index(row, column);
    view->setCurrentIndex(index);
    view->edit(index);
    QCoreApplication::processEvents();
    return cellEditor();
}

QToolButton* control(QTableView* view, const char* name) {
    return view->parentWidget()->findChild<QToolButton*>(QString::fromLatin1(name));
}

/// Types into whatever has the focus, as a user does; "\n" is Enter.
void typeAsUser(const QString& keys) {
    for (const QChar c : keys) {
        QWidget* target = QApplication::focusWidget();
        if (c == u'\n') {
            test::keyClick(target, Qt::Key_Return);
        } else {
            test::keyClicks(target, QString(c));
        }
        QCoreApplication::processEvents();
    }
}

/// Every column lies inside the view, without horizontal scrolling.
bool showsEveryColumn(const QTableView* view) {
    const int last = view->model()->columnCount() - 1;
    return view->horizontalScrollBar()->maximum() == 0 && view->columnViewportPosition(0) == 0 &&
           view->columnViewportPosition(last) + view->columnWidth(last) <= view->viewport()->width();
}

QString geometryOf(const QTableView* view) {
    QString out = QStringLiteral("viewport %1, scroll max %2, columns")
                      .arg(view->viewport()->width())
                      .arg(view->horizontalScrollBar()->maximum());
    for (int c = 0; c < view->model()->columnCount(); ++c) {
        out += QStringLiteral(" %1@%2").arg(view->columnWidth(c)).arg(view->columnViewportPosition(c));
    }
    return out;
}

QAction* action(QTableView* view, const char* name) {
    return view->findChild<QAction*>(QString::fromLatin1(name));
}

/// The text editor of the current cell, while a cell has the focus.
QPlainTextEdit* focusedCell() {
    return qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
}

/// Presses a key in whatever has the focus, as a user does.
void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    test::keyClick(QApplication::focusWidget(), key, modifiers);
    QCoreApplication::processEvents();
}

/// Clicks at `point` of `root` on whatever widget is there, as a user does.
void clickAt(QWidget* root, QPoint point) {
    QWidget* target = root->childAt(point);
    if (target == nullptr) {
        target = root;
    }
    QTest::mouseClick(target, Qt::LeftButton, {}, target->mapFrom(root, point));
    QCoreApplication::processEvents();
}

/// Clicks the middle of a cell; returns the editor of the cell then.
QPlainTextEdit* clickCell(QTableView* view, int row, int column) {
    clickAt(view->viewport(), view->visualRect(view->model()->index(row, column)).center());
    return focusedCell();
}

/// The current cell as "row,column".
QString current(const QTableView* view) {
    return QStringLiteral("%1,%2").arg(view->currentIndex().row()).arg(view->currentIndex().column());
}

/// The caret's position in the text of the current cell; -1 without a cell editor.
int caretInCell() {
    const QPlainTextEdit* cell = focusedCell();
    return cell != nullptr ? cell->textCursor().position() : -1;
}

/// The entries of the context menu that `widget` shows at `point`, without accelerators and
/// shortcuts.
QStringList contextMenuOf(QWidget* widget, QPoint point) {
    auto texts = std::make_shared<QStringList>();
    QTimer::singleShot(0, [texts] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            for (const QAction* entry : menu->actions()) {
                if (!entry->isSeparator()) {
                    *texts << entry->text().remove(u'&').section(u'\t', 0, 0);
                }
            }
            menu->close();
        }
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, widget->mapToGlobal(point));
    QCoreApplication::sendEvent(widget, &event);
    QCoreApplication::processEvents();
    return *texts;
}
}  // namespace

class tst_s018_tables : public QObject {
    Q_OBJECT
private slots:
    void showsAGfmTableAsATableView() {
        constexpr auto markdown =
            "before\n"
            "| Name | Score | Rank |\n"
            "| :--- | :---: | ---: |\n"
            "| Alice | 95 | 1 |\n"
            "| Bob | 100 | 2 |\n"
            "\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("table.md"), markdown)));
        f.settle();

        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QCOMPARE(f.editor()->tableViewAt(4), view);
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);
        QAbstractItemModel* model = view->model();
        QCOMPARE(model->rowCount(), 3);  // header + two body rows
        QCOMPARE(model->columnCount(), 3);
        QCOMPARE(model->data(model->index(0, 0)).toString(), QStringLiteral("Name"));
        QCOMPARE(model->data(model->index(2, 1)).toString(), QStringLiteral("100"));
        QCOMPARE(model->data(model->index(1, 0), Qt::TextAlignmentRole).toInt() & Qt::AlignHorizontal_Mask,
                 int(Qt::AlignLeft));
        QCOMPARE(model->data(model->index(1, 1), Qt::TextAlignmentRole).toInt() & Qt::AlignHorizontal_Mask,
                 int(Qt::AlignHCenter));
        QCOMPARE(model->data(model->index(1, 2), Qt::TextAlignmentRole).toInt() & Qt::AlignHorizontal_Mask,
                 int(Qt::AlignRight));

        // The header line reserves the space, the other lines are hidden; the view sits there.
        auto* layout = f.editor()->blockLayout();
        const auto* doc = f.editor()->document();
        QVERIFY(layout->reservedHeight(doc->findBlockByNumber(1)) > 0);
        for (int n = 2; n <= 4; ++n) {
            QVERIFY(!doc->findBlockByNumber(n).isVisible());
        }
        QVERIFY(view->isVisible());
        QVERIFY(view->parentWidget()->geometry().height() <= layout->reservedHeight(doc->findBlockByNumber(1)) + 0.5);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QVERIFY(!doc->isModified());
    }

    void followsGfmForPipesLineBreaksAndContinuationLines() {
        constexpr auto markdown =
            "| A | B |\n"
            "| --- | --- |\n"
            "| `x | y` | z |\n"
            "| a \\| b | **bold**<br>next |\n"
            "continued\n"
            "\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("gfm.md"), markdown)));
        f.settle();
        // Opening separates "continued" from the table (ADR 0002, decision 6) ...
        QCOMPARE(f.editor()->document()->findBlockByNumber(4).text(), QString());
        QVERIFY(f.editor()->document()->findBlockByNumber(5).isVisible());
        QCOMPARE(f.editor()->tableViewAt(0)->model()->rowCount(), 3);
        // ... and once that blank line is gone, GFM makes it a row again.
        QTextCursor blank(f.editor()->document()->findBlockByNumber(4));
        blank.deletePreviousChar();
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QAbstractItemModel* model = view->model();
        QCOMPARE(model->rowCount(), 4);  // header, three body rows ("continued" is a row)
        QCOMPARE(model->data(model->index(1, 0)).toString(), QStringLiteral("`x"));
        QCOMPARE(model->data(model->index(1, 1)).toString(), QStringLiteral("y`"));
        QCOMPARE(model->data(model->index(2, 0)).toString(), QStringLiteral("a | b"));
        QCOMPARE(model->data(model->index(2, 1)).toString(), QStringLiteral("**bold**\nnext"));
        QCOMPARE(model->data(model->index(3, 0)).toString(), QStringLiteral("continued"));
        QCOMPARE(model->data(model->index(3, 1)).toString(), QString());
        // The row with a line break is a line higher than a one-line row.
        const int lineHeight = QFontMetrics(model->data(model->index(3, 0), Qt::FontRole).value<QFont>()).height();
        QVERIFY(view->rowHeight(2) >= view->rowHeight(3) + lineHeight * 8 / 10);
        QVERIFY(f.editor()->tableViewAt(6) == nullptr);
        QVERIFY(f.editor()->document()->findBlockByNumber(6).isVisible());
    }

    void editingACellChangesOnlyThatCell() {
        constexpr auto markdown =
            "| Key | Value |\n"
            "| --- | --- |\n"
            "|foo|  bar  |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("edit.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);

        QWidget* editor = editCell(view, 1, 0);
        QVERIFY(editor != nullptr);
        test::keyClick(editor, Qt::Key_End);
        test::keyClicks(editor, QStringLiteral("2|x"));
        test::keyClick(editor, Qt::Key_Return, Qt::ShiftModifier);
        test::keyClicks(editor, QStringLiteral("y"));
        test::keyClick(editor, Qt::Key_Tab);  // commits and moves on
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n|foo2\\|x<br>y|  bar  |\n"));

        // Pasted pipes and line breaks are written as \| and <br>.
        QApplication::clipboard()->setText(QStringLiteral("p|q\nr"));
        editor = editCell(view, 1, 1);
        QVERIFY(editor != nullptr);
        test::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
        test::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
        test::keyClick(editor, Qt::Key_Backtab);  // commits; Tab in the last cell would add a row
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n|foo2\\|x<br>y|  p\\|q<br>r  |\n"));

        // Undo in the document restores the previous lines, a keystroke at a time (S018, kept from
        // the first version: no editing mode).
        for (int i = 0; i < 20 && text(f) != QString::fromUtf8(markdown); ++i) {
            f.editor()->document()->undo();
        }
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    void inputMethodCommitsIntoTheCell() {
        constexpr auto markdown = "| A |\n| --- |\n| x |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("ime.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QWidget* editor = editCell(view, 1, 0);
        QVERIFY(editor != nullptr);
        test::keyClick(editor, Qt::Key_End);
        QInputMethodEvent preedit(QStringLiteral("かな"), {});
        QCoreApplication::sendEvent(editor, &preedit);
        QInputMethodEvent commit;
        commit.setCommitString(QStringLiteral("仮名"));
        QCoreApplication::sendEvent(editor, &commit);
        test::keyClick(editor, Qt::Key_Backtab);  // Tab in the last cell would append a row
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A |\n| --- |\n| x仮名 |\n"));
    }

    void movingAroundNeverChangesTheFile() {
        constexpr auto markdown =
            "above\n"
            "| A | B | C |\n"
            "| --- | --- | --- |\n"
            "| a |\n"
            "| d | e | f |\n"
            "\n"
            "below";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("move.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);

        view->setFocus();
        view->setCurrentIndex(view->model()->index(0, 0));
        for (int i = 0; i < 8; ++i) {  // to the last cell; one more would append a row
            test::keyClick(view, Qt::Key_Tab);
        }
        for (const Qt::Key key : {Qt::Key_Up, Qt::Key_Left, Qt::Key_Down, Qt::Key_Right, Qt::Key_Home, Qt::Key_End}) {
            test::keyClick(view, key);
        }
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, view->visualRect(view->model()->index(1, 2)).center());
        test::keyClick(view, Qt::Key_Backtab);
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QVERIFY(!f.editor()->document()->isModified());
    }

    void upAndDownLeaveAndEnterTheTable() {
        constexpr auto markdown =
            "above\n"
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n"
            "| 3 | 4 |\n"
            "\n"
            "below\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("updown.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);

        // The caret is in the text of a cell then: its editor inside the view has the focus.
        const auto inTable = [view] { return view->isAncestorOf(QApplication::focusWidget()); };
        f.editor()->setFocus();
        f.placeCaret(0, 2);
        press(Qt::Key_Down);
        QTRY_VERIFY(inTable());
        QCOMPARE(view->currentIndex().row(), 0);
        press(Qt::Key_Up);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);

        f.placeCaret(6, 0);  // "below"; the blank line before it is hidden
        press(Qt::Key_Up);
        QTRY_VERIFY(inTable());
        QCOMPARE(view->currentIndex().row(), 2);
        press(Qt::Key_Down);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 6);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    void addsAndRemovesRowsAndColumns() {
        constexpr auto markdown =
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("rows.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);

        // Tab in the last cell appends a row and moves into it.
        view->setFocus();
        view->setCurrentIndex(view->model()->index(1, 1));
        test::keyClick(view, Qt::Key_Tab);
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n|  |  |"));
        view = f.editor()->tableViewAt(0);
        QCOMPARE(view->currentIndex().row(), 2);

        // Enter in the empty last row removes it and moves below the table; at the end of the
        // document a blank line and a line for the caret are added (kept from the first version, 13).
        test::keyClick(view, Qt::Key_Return);
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n\n"));
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);

        // The context menu actions add and delete columns and rows; undo restores.
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        QVERIFY(action(view, "tableAddColumn") != nullptr);
        action(view, "tableAddColumn")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n\n"));
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        action(view, "tableDeleteColumn")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| B |  |\n| --- | --- |\n| 2 |  |\n\n"));
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        action(view, "tableDeleteRow")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| B |  |\n| --- | --- |\n\n"));
        f.editor()->document()->undo();
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n\n"));

        // The graphical controls show only near their edge.
        view = f.editor()->tableViewAt(0);
        QToolButton* addRow = control(view, "tableAddRow");
        QVERIFY(addRow != nullptr);
        QVERIFY(!addRow->isVisible());
        const QPoint nearBottom = view->mapTo(f.editor()->viewport(), QPoint(view->width() / 2, view->height() - 2));
        QTest::mouseMove(f.editor()->viewport(), nearBottom);
        QTest::mouseMove(view->viewport(), view->viewport()->mapFrom(f.editor()->viewport(), nearBottom));
        QTRY_VERIFY(addRow->isVisible());
        addRow->click();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n|  |  |  |\n\n"));
    }

    void sourceViewShowsTheMarkdownLines() {
        constexpr auto markdown =
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 | hidden |\n"
            "| 3 | 4 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("source.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QAbstractItemModel* model = view->model();
        QVERIFY(model->data(model->index(1, 0), ui::TableModel::HiddenTextRole).toBool());
        QVERIFY(!model->data(model->index(2, 0), ui::TableModel::HiddenTextRole).toBool());

        view->setFocus();
        view->setCurrentIndex(model->index(1, 1));
        test::keyClick(view, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);
        QVERIFY(f.editor()->isTableSourceShown(0));
        const auto* doc = f.editor()->document();
        for (int n = 0; n <= 3; ++n) {
            QVERIFY(doc->findBlockByNumber(n).isVisible());
        }
        QCOMPARE(f.editor()->blockLayout()->reservedHeight(doc->findBlockByNumber(0)), 0.0);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);  // in cell "2"

        test::keyClick(f.editor(), Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QVERIFY(!f.editor()->isTableSourceShown(0));
        QVERIFY(f.editor()->tableViewAt(0) != nullptr);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    void nonTablesStayLiteral() {
        constexpr auto markdown = "| A | B |\nplain\n| --- |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("literal.md"), markdown)));
        f.settle();
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);
        QVERIFY(f.editor()->document()->findBlockByNumber(0).isVisible());
    }

    void themedTableUsesEveryToken() {
        constexpr auto markdown =
            "before\n"
            "| H1 | H2 |\n"
            "| --- | --- |\n"
            "| R1 | B1 |\n"
            "| R2 | B2 |\n"
            "\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("themed.md"), markdown)));
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.table.background = core::Color{0xf0, 0xe0, 0xd0, 0xff};
        theme.table.headerBackground = core::Color{0x20, 0x30, 0x50, 0xff};
        theme.table.stripeBackground = core::Color{0xd0, 0xe8, 0xf8, 0xff};
        theme.table.text = core::Color{0x12, 0x34, 0x56, 0xff};
        theme.table.headerText = core::Color{0xfa, 0xfb, 0xfc, 0xff};
        theme.table.headerWeight = 600;
        theme.table.border = core::Color{0x80, 0x10, 0x40, 0xff};
        theme.table.borderWidth = 2;
        theme.table.radius = 0;
        theme.table.padding = 10;
        theme.table.fontFamily = "Table Test Family";
        theme.table.fontScale = 1.25;
        f.window->applyTheme(theme);
        f.settle();

        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QAbstractItemModel* model = view->model();
        QCOMPARE(model->data(model->index(0, 0), Qt::BackgroundRole).value<QColor>(),
                 qcolor(*theme.table.headerBackground));
        QCOMPARE(model->data(model->index(1, 0), Qt::BackgroundRole).value<QColor>(), qcolor(*theme.table.background));
        QCOMPARE(model->data(model->index(2, 0), Qt::BackgroundRole).value<QColor>(),
                 qcolor(*theme.table.stripeBackground));
        QCOMPARE(model->data(model->index(0, 0), Qt::ForegroundRole).value<QColor>(), qcolor(*theme.table.headerText));
        QCOMPARE(model->data(model->index(1, 0), Qt::ForegroundRole).value<QColor>(), qcolor(*theme.table.text));
        const auto headerFont = model->data(model->index(0, 0), Qt::FontRole).value<QFont>();
        const auto bodyFont = model->data(model->index(1, 0), Qt::FontRole).value<QFont>();
        QCOMPARE(headerFont.weight(), QFont::Weight(600));
        QCOMPARE(bodyFont.families(), QStringList{QStringLiteral("Table Test Family")});
        QCOMPARE(bodyFont.pointSizeF(), theme.editorFontSize * 1.25);

        // Background and border pixels as drawn.
        const QImage image = view->viewport()->grab().toImage();
        const QRect headerCell = view->visualRect(model->index(0, 0));
        const QRect row1 = view->visualRect(model->index(1, 0));
        const QRect row2 = view->visualRect(model->index(2, 0));
        QCOMPARE(image.pixelColor(headerCell.right() - 4, headerCell.center().y()),
                 qcolor(*theme.table.headerBackground));
        QCOMPARE(image.pixelColor(row1.right() - 4, row1.center().y()), qcolor(*theme.table.background));
        QCOMPARE(image.pixelColor(row2.right() - 4, row2.center().y()), qcolor(*theme.table.stripeBackground));
        QCOMPARE(image.pixelColor(headerCell.right() + 1, headerCell.center().y()), qcolor(*theme.table.border));
        // Padding: the cell is at least text width plus twice the padding wide.
        QVERIFY(view->columnWidth(0) >= QFontMetrics(bodyFont).horizontalAdvance(QStringLiteral("R1")) + 20);
    }

    // Review objection O2 (kept): the table width must not depend on the vertical scroll bar.
    void tableWidthDoesNotDependOnScrollBarVisibility() {
        constexpr auto markdown =
            "| Name | Note |\n"
            "| --- | --- |\n"
            "| Alice | a note long enough to wrap when the window is narrow enough to force it |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("bar.md"), markdown)));
        f.settle();
        QVERIFY(!f.editor()->verticalScrollBar()->isVisible());
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        const int withoutBar = view->width();

        f.editor()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        QTRY_VERIFY(f.editor()->verticalScrollBar()->isVisible());
        f.settle();
        QCOMPARE(f.editor()->tableViewAt(0)->width(), withoutBar);
    }

    // Daniel's recording (2026-10-27): typing the delimiter row pulled the caret into the table
    // after "|---|-", and the rest of the row landed in a cell.
    void typingATableKeepsTheSourceUntilTheCaretLeavesTheLine() {
        test::Fixture f;
        f.editor()->setFocus();
        typeAsUser(QStringLiteral("|a|b|\n|---|-"));
        f.settle();
        QVERIFY(f.editor()->hasFocus());
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);  // still the Markdown lines
        typeAsUser(QStringLiteral("--|"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("|a|b|\n|---|---|"));
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);

        typeAsUser(QStringLiteral("\n"));  // leaving the line shows the table
        f.settle();
        QCOMPARE(text(f), QStringLiteral("|a|b|\n|---|---|\n"));
        QVERIFY(f.editor()->tableViewAt(0) != nullptr);
        QVERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);

        // Typing in the blank line below the table starts a paragraph instead of a row
        // (typingBelowATableStartsAParagraph).
    }

    // Only typing in the editor holds a table: a change made otherwise (loading in slices, the
    // table view, undo from elsewhere) while the editor has the focus keeps the table view.
    void changesThatAreNotTypingKeepTheTableView() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("other.md"), "text\n\n| A |\n| --- |\n| 1 |\n\nend\n")));
        f.settle();
        f.editor()->setFocus();
        f.placeCaret(0, 0);
        QTextCursor row(f.editor()->document()->findBlockByNumber(4));
        row.movePosition(QTextCursor::EndOfBlock);
        row.insertText(QStringLiteral(" x |"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("text\n\n| A |\n| --- |\n| 1 | x |\n\nend\n"));
        QVERIFY(f.editor()->tableViewAt(2) != nullptr);
    }

    // Daniel's recording: the view was narrower than its columns and scrolled horizontally. The
    // header's minimum section size widened narrow columns beyond what the view was sized for.
    void theTableIsAsWideAsItsColumns() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("narrow.md"), "|a|b|\n|---|---|\n")));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY2(showsEveryColumn(view), qPrintable(geometryOf(view)));
        view->setCurrentIndex(view->model()->index(0, 1));
        action(view, "tableAddColumn")->trigger();  // an empty column: narrower than the minimum
        f.settle();
        view = f.editor()->tableViewAt(0);
        QCOMPARE(view->model()->columnCount(), 3);
        QVERIFY2(showsEveryColumn(view), qPrintable(geometryOf(view)));
        QVERIFY(view->columnWidth(2) < view->horizontalHeader()->defaultSectionSize());
    }

    // Daniel's recording: editing a cell needed a double click.
    void oneClickEditsTheCellWhereClicked() {
        constexpr auto markdown = "| Name |\n| --- |\n| Alice |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("click.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        const QRect cell = view->visualRect(view->model()->index(1, 0));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, QPoint(cell.left() + 2, cell.center().y()));
        QCoreApplication::processEvents();
        auto* editor = qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->textCursor().position(), 0);  // where clicked, not at the end
        typeAsUser(QStringLiteral("x"));
        test::keyClick(QApplication::focusWidget(), Qt::Key_Backtab);  // Enter would add a row
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| Name |\n| --- |\n| xAlice |\n"));
    }

    // Daniel's recording: the controls moved. They sit along the edge they act on, as before the
    // revision: bars under and to the right of the table, delete buttons outside the row and column.
    void controlsSitAlongTheEdgeTheyActOn() {
        constexpr auto markdown = "| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("controls.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QWidget* table = view->parentWidget();
        const QRect v = view->geometry();  // in the table widget
        const auto hover = [&](QPoint inTable) {
            QTest::mouseMove(table, inTable);
            QCoreApplication::processEvents();
        };

        hover(QPoint(v.center().x(), v.bottom() - 2));
        QToolButton* addRow = control(view, "tableAddRow");
        QTRY_VERIFY(addRow->isVisible());
        QCOMPARE(addRow->geometry().left(), v.left());
        QCOMPARE(addRow->geometry().width(), v.width());
        QVERIFY(addRow->geometry().top() > v.bottom());

        hover(QPoint(v.right() - 2, v.center().y()));
        QToolButton* addColumn = control(view, "tableAddColumn");
        QTRY_VERIFY(addColumn->isVisible());
        QCOMPARE(addColumn->geometry().top(), v.top());
        QCOMPARE(addColumn->geometry().height(), v.height());
        QVERIFY(addColumn->geometry().left() > v.right());

        const QRect row1 =
            view->visualRect(view->model()->index(1, 0)).translated(v.topLeft() + QPoint(1, 1) * view->frameWidth());
        hover(QPoint(v.left() + 3, row1.center().y()));
        QToolButton* deleteRow = control(view, "tableDeleteRow");
        QTRY_VERIFY(deleteRow->isVisible());
        QCOMPARE(deleteRow->geometry().top(), row1.top());
        QCOMPARE(deleteRow->geometry().height(), row1.height());
        QVERIFY(deleteRow->geometry().right() < v.left());

        const QRect column1 =
            view->visualRect(view->model()->index(0, 1)).translated(v.topLeft() + QPoint(1, 1) * view->frameWidth());
        hover(QPoint(column1.center().x(), v.top() + 3));
        QToolButton* deleteColumn = control(view, "tableDeleteColumn");
        QTRY_VERIFY(deleteColumn->isVisible());
        QCOMPARE(deleteColumn->geometry().left(), column1.left());
        QCOMPARE(deleteColumn->geometry().width(), column1.width());
        QVERIFY(deleteColumn->geometry().bottom() < v.top());
    }

    void typingInATableNearTheScrollBarThresholdSettles() {
        QByteArray markdown = "| C1 | C2 | C3 | C4 | C5 |\n| --- | --- | --- | --- | --- |\n";
        for (int i = 0; i < 50; ++i) {
            markdown += "| alpha | beta gamma | delta | epsilon zeta eta | theta |\n";
        }
        markdown += "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("threshold.md"), markdown)));
        f.settle();

        constexpr int kRunaway = 20;
        struct ResizeCounter : QObject {
            QAbstractScrollArea* editor = nullptr;
            int count = 0;
            bool eventFilter(QObject*, QEvent* event) override {
                if (event->type() == QEvent::Resize && ++count == kRunaway) {
                    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
                }
                return false;
            }
        } counter;
        counter.editor = f.editor();
        f.editor()->viewport()->installEventFilter(&counter);

        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QWidget* editor = editCell(view, 25, 1);
        QVERIFY(editor != nullptr);
        test::keyClicks(editor, QStringLiteral("q"));
        test::keyClick(editor, Qt::Key_Tab);
        f.settle();
        QTest::qWait(200);
        f.editor()->viewport()->removeEventFilter(&counter);
        QVERIFY2(counter.count < kRunaway, qPrintable(QStringLiteral("%1 viewport resizes").arg(counter.count)));
    }
    // --- Kept from the first version (story S018, points 11 to 21; Daniel's tests at 620b6cb) ---

    // 11: every keystroke writes the cell at once, no editing mode; Backspace and Delete stop at
    // the cell's edges; a typed pipe is escaped and deleted as one character; Ctrl+Z undoes.
    void everyKeystrokeWritesTheCellAtOnce() {
        constexpr auto markdown =
            "| Key | Value |\n"
            "| --- | --- |\n"
            "| foo | bar |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("live.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 0) != nullptr);
        press(Qt::Key_Home);
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_Backspace);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        press(Qt::Key_End);
        QCOMPARE(caretInCell(), 3);
        press(Qt::Key_Delete);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QVERIFY(!f.editor()->document()->isModified());

        typeAsUser(QStringLiteral("2"));  // still in the cell: the line has it already
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n| foo2 | bar |\n"));
        QVERIFY(f.editor()->document()->isModified());
        typeAsUser(QStringLiteral("|x"));
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n| foo2\\|x | bar |\n"));
        press(Qt::Key_Left);
        press(Qt::Key_Backspace);  // the escaped pipe goes as one character
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n| foo2x | bar |\n"));
        QCOMPARE(current(view), QStringLiteral("1,0"));

        for (int i = 0; i < 10 && text(f) != QString::fromUtf8(markdown); ++i) {
            press(Qt::Key_Z, Qt::ControlModifier);
        }
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    // 11 (Daniel's navigatesCellsWithTabAndArrowsAndAppendsRowAtEnd): Tab and Shift+Tab go to the
    // start of the next or previous cell, skipping the delimiter row; Home and End stay in the cell;
    // Enter goes to the same column of the next row.
    void tabEnterHomeAndEndMoveTheCaretInCells() {
        constexpr auto markdown =
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("nav.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 0, 0) != nullptr);
        press(Qt::Key_Tab);
        QCOMPARE(current(view), QStringLiteral("0,1"));
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_Tab);
        QCOMPARE(current(view), QStringLiteral("1,0"));
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_Tab);
        press(Qt::Key_Tab);  // the last cell: appends a row
        f.settle();
        QCOMPARE(current(view), QStringLiteral("2,0"));
        typeAsUser(QStringLiteral("3"));
        press(Qt::Key_Tab);
        typeAsUser(QStringLiteral("4"));
        QCOMPARE(text(f), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |\n"));
        press(Qt::Key_Backtab);
        QCOMPARE(current(view), QStringLiteral("2,0"));
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_End);
        QCOMPARE(caretInCell(), 1);
        press(Qt::Key_Home);
        QCOMPARE(caretInCell(), 0);

        QVERIFY(clickCell(view, 0, 1) != nullptr);
        press(Qt::Key_Return);
        QCOMPARE(current(view), QStringLiteral("1,1"));
        QCOMPARE(text(f), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |\n"));
    }

    // 12 and 13 (Daniel's exitsTableViaEnterOnEmptyRowCtrlEnterEscapeAndClickingOutside): Escape,
    // Ctrl+Enter and Enter in an empty last row leave the table below and keep what was typed; at
    // the end of the document a blank line separates the new line, so text there is a paragraph.
    void escapeCtrlEnterAndAnEmptyLastRowLeaveBelow() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |";
        {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("enter.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 1, 0) != nullptr);
            press(Qt::Key_End);
            press(Qt::Key_Return);  // the last row has text: a new row
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n|  |  |"));
            QCOMPARE(current(view), QStringLiteral("2,0"));
            press(Qt::Key_Return);  // the new row is empty: removed, and out below
            f.settle();
            QVERIFY(f.editor()->hasFocus());
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\n"));
            QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
            typeAsUser(QStringLiteral("Paragraph below"));
            typeAsUser(QStringLiteral("\n"));
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nParagraph below\n"));
            QCOMPARE(f.editor()->tableViewAt(0)->model()->rowCount(), 2);
        }
        {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("escape.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 1, 1) != nullptr);
            press(Qt::Key_End);
            typeAsUser(QStringLiteral("x"));
            press(Qt::Key_Escape);
            f.settle();
            QVERIFY(f.editor()->hasFocus());
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2x |\n\n"));
            QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        }
        {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("ctrlenter.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 0, 0) != nullptr);
            press(Qt::Key_Return, Qt::ControlModifier);
            f.settle();
            QVERIFY(f.editor()->hasFocus());
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\n"));
            QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        }
    }

    // 14: Left and Right at the edge of a cell's text move to the neighbouring cell, across rows,
    // and out of the table at its first and last cell.
    void leftAndRightCrossCellsAndLeaveTheTable() {
        constexpr auto markdown =
            "above\n"
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n"
            "\n"
            "below\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("leftright.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 0, 0) != nullptr);
        press(Qt::Key_Home);
        press(Qt::Key_Right);  // past "A"
        QCOMPARE(current(view), QStringLiteral("0,0"));
        press(Qt::Key_Right);
        QCOMPARE(current(view), QStringLiteral("0,1"));
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_Right);
        press(Qt::Key_Right);  // from the end of the row's last cell to the next row
        QCOMPARE(current(view), QStringLiteral("1,0"));
        QCOMPARE(caretInCell(), 0);
        press(Qt::Key_Left);
        QCOMPARE(current(view), QStringLiteral("0,1"));
        QCOMPARE(caretInCell(), 1);
        press(Qt::Key_Left);
        press(Qt::Key_Left);
        QCOMPARE(current(view), QStringLiteral("0,0"));
        QCOMPARE(caretInCell(), 1);
        press(Qt::Key_Left);
        press(Qt::Key_Left);  // from the start of the first cell to the end of the line above
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 5);

        view = f.editor()->tableViewAt(1);
        QVERIFY(clickCell(view, 1, 1) != nullptr);
        press(Qt::Key_End);
        press(Qt::Key_Right);  // from the end of the last cell to the line after the hidden blank line
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 0);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QVERIFY(!f.editor()->document()->isModified());
    }

    // 15 (Daniel's wrapsLongColumnInsideItsColumnBoundsAndStaysInViewport, step 4): Up and Down
    // move through the wrapped text of a cell first, then to the row above or below, and out of
    // the table from its first and last row.
    void upAndDownGoThroughWrappedTextThenRows() {
        const QString longText = QStringLiteral("This text is very long ").repeated(20);
        const QString markdown = QStringLiteral(
                                     "above\n| Col 1 | Col 2 |\n| --- | --- |\n| Say yes | %1|\n"
                                     "| x | y |\n\nbelow\n")
                                     .arg(longText);
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrapped.md"), markdown.toUtf8())));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 1) != nullptr);
        press(Qt::Key_Home);
        const int firstLine = focusedCell()->cursorRect().top();
        press(Qt::Key_Down);
        QCOMPARE(current(view), QStringLiteral("1,1"));
        QVERIFY(focusedCell()->cursorRect().top() > firstLine);
        press(Qt::Key_Up);
        QCOMPARE(current(view), QStringLiteral("1,1"));
        QCOMPARE(focusedCell()->cursorRect().top(), firstLine);
        press(Qt::Key_Up);
        QCOMPARE(current(view), QStringLiteral("0,1"));
        press(Qt::Key_Up);  // from the header row to the line above
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);

        view = f.editor()->tableViewAt(1);
        QVERIFY(clickCell(view, 2, 0) != nullptr);
        press(Qt::Key_Down);  // from the last row to the line after the hidden blank line
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 6);
        QCOMPARE(text(f), markdown);
    }

    // 16 (Daniel's deletesRowsVia... step 3 and deletesColumnsVia... step 3): Backspace or Delete
    // in an empty row or an empty column deletes it.
    void backspaceOrDeleteInAnEmptyRowOrColumnDeletesIt() {
        constexpr auto markdown =
            "| Name | Role |\n"
            "| --- | --- |\n"
            "| Alice | Dev |\n"
            "| Carol | Ops |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("emptydelete.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 2, 1) != nullptr);
        press(Qt::Key_Tab);  // appends an empty row
        f.settle();
        QCOMPARE(text(f),
                 QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n| Carol | Ops |\n|  |  |\n"));
        press(Qt::Key_Backspace);  // deletes it; the caret goes to the end of the row above
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QCOMPARE(current(view), QStringLiteral("2,0"));
        QCOMPARE(caretInCell(), 5);

        press(Qt::Key_Tab);
        press(Qt::Key_Tab);  // appends an empty row again
        f.settle();
        QCOMPARE(current(view), QStringLiteral("3,0"));
        press(Qt::Key_Delete);
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QCOMPARE(current(view), QStringLiteral("2,0"));

        // An empty cell in a row with text deletes nothing.
        press(Qt::Key_Home);
        press(Qt::Key_Backspace);
        QCOMPARE(text(f), QString::fromUtf8(markdown));

        // A new, empty column: Backspace in it deletes it.
        action(view, "tableAddColumn")->trigger();
        f.settle();
        QCOMPARE(text(f),
                 QStringLiteral("| Name | Role |  |\n| --- | --- | --- |\n| Alice | Dev |  |\n| Carol | Ops |  |\n"));
        view = f.editor()->tableViewAt(0);
        QCOMPARE(current(view), QStringLiteral("0,2"));
        QVERIFY(focusedCell() != nullptr);
        press(Qt::Key_Backspace);
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    // 17 (Daniel's deletesRowsVia... step 4 and deletesColumnsVia... step 4): Ctrl+Shift+Backspace
    // or Delete deletes the row, in the header row the column; Ctrl+Alt+Backspace or Delete the
    // column; the last column stays; Ctrl+Z restores.
    void shortcutsDeleteRowsAndColumns() {
        {
            constexpr auto markdown =
                "| Name | Role |\n"
                "| --- | --- |\n"
                "| Alice | Dev |\n"
                "| Carol | Ops |\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("rowkeys.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 2, 0) != nullptr);
            press(Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n"));
            QCOMPARE(current(view), QStringLiteral("1,0"));
            press(Qt::Key_Delete, Qt::ControlModifier | Qt::ShiftModifier);
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| Name | Role |\n| --- | --- |\n"));
            QCOMPARE(current(view), QStringLiteral("0,0"));
            press(Qt::Key_Z, Qt::ControlModifier);
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n"));
        }
        {
            constexpr auto markdown =
                "| ColA | ColB | ColC |\n"
                "| :--- | :---: | ---: |\n"
                "| a1 | b1 | c1 |\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("columnkeys.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 0, 0) != nullptr);
            press(Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);  // header: the column
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| ColB | ColC |\n| :---: | ---: |\n| b1 | c1 |\n"));
            view = f.editor()->tableViewAt(0);
            QVERIFY(clickCell(view, 1, 1) != nullptr);
            press(Qt::Key_Delete, Qt::ControlModifier | Qt::AltModifier);
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| ColB |\n| :---: |\n| b1 |\n"));
            press(Qt::Key_Backspace, Qt::ControlModifier | Qt::AltModifier);  // the last column stays
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| ColB |\n| :---: |\n| b1 |\n"));
            press(Qt::Key_Z, Qt::ControlModifier);
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| ColB | ColC |\n| :---: | ---: |\n| b1 | c1 |\n"));
        }
    }

    // 18 (Daniel's exitsTableVia..., steps 2 and 3): a click below a table that ends the document
    // or above one that starts it puts the caret on a new line there, apart from the table.
    void clicksAboveOrBelowATableAtTheDocumentEdgesAddALine() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("clicks.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        const QRect table = view->parentWidget()->geometry();  // in the editor's viewport
        clickAt(f.editor()->viewport(), QPoint(table.left() + 30, table.bottom() + 40));
        f.settle();
        QVERIFY(f.editor()->hasFocus());
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\n"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        typeAsUser(QStringLiteral("Clicked below"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nClicked below"));

        view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        clickAt(f.editor()->viewport(), QPoint(view->parentWidget()->geometry().left() + 30, 2));
        f.settle();
        QVERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        typeAsUser(QStringLiteral("# Heading above"));
        f.settle();
        QVERIFY2(text(f).startsWith(QStringLiteral("# Heading above\n\n| a | b |\n")), qPrintable(text(f)));
    }

    // 19: Up in the header row or Left in the first cell of a table that starts the document put
    // the caret on a new line above it.
    void leavingUpwardsFromATableAtTheStartAddsALine() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n";
        for (const Qt::Key key : {Qt::Key_Up, Qt::Key_Left}) {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("top.md"), markdown)));
            f.settle();
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(clickCell(view, 0, 0) != nullptr);
            press(Qt::Key_Home);
            press(key);
            f.settle();
            QVERIFY(f.editor()->hasFocus());
            QCOMPARE(text(f), QStringLiteral("\n\n") + QString::fromUtf8(markdown));
            QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        }
    }

    // 20: the context menu of a cell has the text actions and the table actions.
    void theContextMenuOfACellHasTextAndTableActions() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("menu.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QPlainTextEdit* cell = clickCell(view, 1, 0);
        QVERIFY(cell != nullptr);
        const QStringList entries = contextMenuOf(cell->viewport(), cell->viewport()->rect().center());
        for (const char* expected :
             {"Cut", "Copy", "Paste", "Add Row", "Delete Row", "Add Column", "Delete Column", "Show Markdown Source"}) {
            QVERIFY2(
                entries.contains(QString::fromLatin1(expected)),
                qPrintable(QStringLiteral("%1 missing in: %2").arg(QString::fromLatin1(expected), entries.join(u'|'))));
        }
    }

    // 21: text in a cell can be selected with the mouse, also starting in a cell that is not the
    // current one, and with Shift.
    void textInACellCanBeSelectedWithTheMouse() {
        constexpr auto markdown =
            "| Name |\n"
            "| --- |\n"
            "| Alice Smith |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("select.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 0, 0) != nullptr);  // another cell is current

        const QRect cell = view->visualRect(view->model()->index(1, 0));
        const QPoint from(cell.left() + 1, cell.center().y());
        const QPoint to(cell.right() - 1, cell.center().y());
        QWidget* target = view->viewport()->childAt(from);
        if (target == nullptr) {
            target = view->viewport();
        }
        const auto local = [&](QPoint p) { return target->mapFrom(view->viewport(), p); };
        QTest::mousePress(target, Qt::LeftButton, {}, local(from));
        QMouseEvent drag(QEvent::MouseMove, QPointF(local(to)), QPointF(target->mapToGlobal(local(to))), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(target, &drag);
        QTest::mouseRelease(target, Qt::LeftButton, {}, local(to));
        QCoreApplication::processEvents();
        QVERIFY(focusedCell() != nullptr);
        QCOMPARE(focusedCell()->textCursor().selectedText(), QStringLiteral("Alice Smith"));

        press(Qt::Key_End);
        press(Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(focusedCell()->textCursor().selectedText(), QStringLiteral("Alice Smith"));
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    // Armin's report (2026-10-27): Backspace at the start of the line below a table joined it to
    // the last row; the table then showed its Markdown lines and no key reached anything until a
    // click. As in the first version, the table stays a table and the caret goes to the end of
    // the last cell, where typing goes on. Since the blank line below a table is hidden, the empty
    // line after it is the one Backspace removes.
    void backspaceBelowATableJoinsTheLastRowAndStaysInTheTable() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n"
            "\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("backspace.md"), markdown)));
        f.settle();
        f.editor()->setFocus();
        f.placeCaret(4, 0);  // the empty line after the hidden blank line: removed, as in any text
        press(Qt::Key_Backspace);
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n"));
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY2(view != nullptr, "the table shows its Markdown lines");
        QVERIFY(focusedCell() != nullptr);
        QCOMPARE(current(view), QStringLiteral("1,1"));
        QCOMPARE(caretInCell(), 1);
        typeAsUser(QStringLiteral("3"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 23 |\n"));
        press(Qt::Key_Z, Qt::ControlModifier);
        press(Qt::Key_Z, Qt::ControlModifier);
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    // Keys always reach the editor or a cell: a table that stops being one while the caret is in
    // it (here its delimiter row goes, as an undo could do it) gives the caret back to the editor.
    void aTableThatGoesAwayGivesTheCaretBackToTheEditor() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("goes.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 0) != nullptr);
        QTextCursor delimiter(f.editor()->document()->findBlockByNumber(1));
        delimiter.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        delimiter.insertText(QStringLiteral("x"));
        f.settle();
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);
        QTRY_VERIFY(f.editor()->hasFocus());
        typeAsUser(QStringLiteral("y"));
        QVERIFY(text(f).contains(u'y'));
    }

    // CI benchmark on bac0ca3 (2026-10-27): opening another file while a cell had the caret
    // crashed. The view scrolled to the top while the document was being replaced, the table went
    // and its caret was moved into lines that were no longer there.
    void openingAnotherFileWhileACellHasTheCaret() {
        QByteArray markdown = "Text\n\n| a | b |\n| --- | --- |\n";
        for (int i = 0; i < 60; ++i) {
            markdown += "| row " + QByteArray::number(i) + " | x |\n";
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("first.md"), markdown)));
        f.settle();
        f.editor()->verticalScrollBar()->setValue(f.editor()->verticalScrollBar()->maximum());
        f.settle();
        QTableView* view = f.editor()->tableViewAt(2);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 50, 0) != nullptr);
        typeAsUser(QStringLiteral("z"));
        f.editor()->document()->setModified(false);
        f.session()->newDocument();  // as the benchmark does before the next file
        QByteArray second;
        for (int i = 0; i < 20000; ++i) {
            second += "Line " + QByteArray::number(i) + " of plain text\n";
        }
        QVERIFY(f.open(f.writeFile(QStringLiteral("second.md"), second)));
        f.settle();
        QCOMPARE(text(f), QString::fromUtf8(second));
        QTRY_VERIFY(f.editor()->hasFocus());
    }

    // Armin's test (2026-10-27): a space at the end of a cell could not be typed; the cell read its
    // text back without the space (GFM padding) and the editor took that over.
    void spacesAtACellsEdgeCanBeTyped() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 |  |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("spaces.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 1) != nullptr);
        typeAsUser(QStringLiteral("hello "));
        f.settle();
        QCOMPARE(focusedCell()->toPlainText(), QStringLiteral("hello "));
        typeAsUser(QStringLiteral("world"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | hello world |\n"));

        QVERIFY(clickCell(view, 1, 0) != nullptr);
        press(Qt::Key_End);
        typeAsUser(QStringLiteral(" "));  // padding only: the file stays as it is
        f.settle();
        QCOMPARE(focusedCell()->toPlainText(), QStringLiteral("1 "));
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | hello world |\n"));
        typeAsUser(QStringLiteral("2"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 2 | hello world |\n"));
    }

    // Armin's test (2026-10-27): typing in the blank line below a table made the text a row of the
    // table (GFM) and showed the table's Markdown. The text becomes a paragraph instead: a blank
    // line is added first (user decision).
    void typingBelowATableStartsAParagraph() {
        {
            constexpr auto markdown =
                "| a | b |\n"
                "| --- | --- |\n"
                "| 1 | 2 |\n"
                "\n"
                "below\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("below.md"), markdown)));
            f.settle();
            f.editor()->setFocus();
            f.placeCaret(3, 0);  // the hidden blank line, as an undo can put the caret there
            QCOMPARE(f.editor()->textCursor().blockNumber(), 4);  // the caret goes to the line after it
            typeAsUser(QStringLiteral("x"));
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nxbelow\n"));
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY2(view != nullptr, "the table shows its Markdown lines");
            QCOMPARE(view->model()->rowCount(), 2);
            QVERIFY(f.editor()->hasFocus());
            QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        }
        {
            constexpr auto markdown =
                "| a | b |\n"
                "| --- | --- |\n"
                "| 1 | 2 |\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("end.md"), markdown)));
            f.settle();
            f.editor()->setFocus();
            f.placeCaret(3, 0);  // the empty line at the end of the document
            typeAsUser(QStringLiteral("Text"));
            f.settle();
            QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nText"));
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QCOMPARE(view->model()->rowCount(), 2);
        }
    }

    // The blank line below a table is syntax (user decision 2026-10-27): hidden, never the caret's
    // place, and Backspace below the table goes into it without deleting text.
    void theBlankLineBelowATableIsHidden() {
        constexpr auto markdown =
            "above\n"
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n"
            "\n"
            "below\n"
            "\n"
            "\n"
            "last\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("hidden.md"), markdown)));
        f.settle();
        const auto* doc = f.editor()->document();
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QVERIFY(!doc->findBlockByNumber(4).isVisible());
        QVERIFY(doc->findBlockByNumber(6).isVisible());  // further blank lines stay
        QVERIFY(doc->findBlockByNumber(7).isVisible());

        // "below" follows the table at the add-row bar's distance, the end of the table's space.
        const int tableEnd = view->parentWidget()->geometry().bottom() + 1;
        const int belowTop = f.editor()->cursorRect(QTextCursor(doc->findBlockByNumber(5))).top();
        QVERIFY2(std::abs(belowTop - tableEnd) <= 2, qPrintable(QStringLiteral("%1 %2").arg(belowTop).arg(tableEnd)));

        // Down and Right go to "below"; Up and Left come back into the table.
        QVERIFY(clickCell(view, 1, 1) != nullptr);
        press(Qt::Key_Down);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
        press(Qt::Key_Up);
        QTRY_VERIFY(focusedCell() != nullptr);
        view = f.editor()->tableViewAt(1);
        QCOMPARE(current(view), QStringLiteral("1,1"));
        press(Qt::Key_End);
        press(Qt::Key_Right);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 0);
        press(Qt::Key_Left);
        QTRY_VERIFY(focusedCell() != nullptr);
        QCOMPARE(current(f.editor()->tableViewAt(1)), QStringLiteral("1,1"));
        QCOMPARE(caretInCell(), 1);

        // Backspace at the start of "below" goes to the end of the last cell and deletes nothing.
        f.editor()->setFocus();
        f.placeCaret(5, 0);
        press(Qt::Key_Backspace);
        f.settle();
        QVERIFY(focusedCell() != nullptr);
        QCOMPARE(current(f.editor()->tableViewAt(1)), QStringLiteral("1,1"));
        QCOMPARE(caretInCell(), 1);
        QCOMPARE(text(f), QString::fromUtf8(markdown));
        QVERIFY(!doc->isModified());

        // The source view shows the blank line; back in the table view it is hidden again.
        press(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QVERIFY(f.editor()->isTableSourceShown(1));
        QVERIFY(doc->findBlockByNumber(4).isVisible());
        f.editor()->setFocus();
        press(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QVERIFY(!f.editor()->isTableSourceShown(1));
        QVERIFY(!doc->findBlockByNumber(4).isVisible());
        QCOMPARE(text(f), QString::fromUtf8(markdown));
    }

    // ADR 0002, decision 6 (S018, files from other editors): Obsidian writes text directly below a
    // table; hitokoto adds the blank line when opening, marks the document modified and says so.
    void openingAddsABlankLineBelowATable() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n"
            "Ich schreibe\n";
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("obsidian.md"), markdown);
        QVERIFY(f.open(path));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nIch schreibe\n"));
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QCOMPARE(view->model()->rowCount(), 2);
        QVERIFY(f.editor()->document()->isModified());
        QCOMPARE(f.messages.size(), 1);
        QVERIFY2(f.messages.first().contains(u'1'), qPrintable(f.messages.first()));
        QCOMPARE(test::Fixture::readFile(path), QByteArray(markdown));  // nothing written yet

        // A file that needs nothing opens unmodified and without a message.
        test::Fixture g;
        QVERIFY(g.open(g.writeFile(QStringLiteral("gfm.md"), "| a |\n| --- |\n| 1 |\n\nText\n")));
        g.settle();
        QVERIFY(!g.editor()->document()->isModified());
        QVERIFY(g.messages.isEmpty());
    }

    // The open cell keeps its column's alignment: the text does not jump when the caret enters.
    void anOpenCellKeepsItsColumnsAlignment() {
        constexpr auto markdown =
            "| Name | Score |\n"
            "| :---: | ---: |\n"
            "| Alice | 95 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("align.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QPlainTextEdit* cell = clickCell(view, 1, 1);
        QVERIFY(cell != nullptr);
        press(Qt::Key_End);
        const int width = cell->viewport()->width();
        QVERIFY2(cell->cursorRect().right() >= width - 4,
                 qPrintable(QStringLiteral("caret at %1 of %2").arg(cell->cursorRect().right()).arg(width)));
        cell = clickCell(view, 1, 0);
        press(Qt::Key_Home);
        QVERIFY2(cell->cursorRect().left() > 4,
                 qPrintable(QStringLiteral("caret at %1").arg(cell->cursorRect().left())));
    }

    // Daniel's emptyRowsCreatedWithEnterKeepFullRowAndCaretHeight: empty rows, also "|||" from a
    // file, keep the full row height, and the caret in an empty cell its full height.
    void emptyRowsKeepTheFullRowAndCaretHeight() {
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1. Row | 2. Row |";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("emptyrows.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 0) != nullptr);
        const int caretHeight = focusedCell()->cursorRect().height();
        const int populated = view->rowHeight(1);
        press(Qt::Key_End);
        press(Qt::Key_Return);
        for (int i = 0; i < 3; ++i) {
            press(Qt::Key_Tab);
            press(Qt::Key_Tab);
        }
        f.settle();
        view = f.editor()->tableViewAt(0);
        QCOMPARE(view->model()->rowCount(), 6);
        QVERIFY(focusedCell() != nullptr);
        QVERIFY(std::abs(focusedCell()->cursorRect().height() - caretHeight) <= 2);
        for (int row = 2; row <= 5; ++row) {
            QVERIFY2(std::abs(view->rowHeight(row) - populated) <= 2,
                     qPrintable(QStringLiteral("row %1: %2 vs %3").arg(row).arg(view->rowHeight(row)).arg(populated)));
        }

        test::Fixture f2;
        QVERIFY(f2.open(f2.writeFile(QStringLiteral("compact.md"), "| a | b |\n|---|---|\n|||\n| x | y |\n")));
        f2.settle();
        QTableView* compact = f2.editor()->tableViewAt(0);
        QVERIFY(compact != nullptr);
        QVERIFY(std::abs(compact->rowHeight(1) - compact->rowHeight(2)) <= 2);
    }

    // Daniel's report (2026-10-27): typing more than 1 character into a cell left it blank while
    // editing because QPlainTextEdit wrapped and scrolled by block index to nonexistent block 1.
    void typingMoreThanOneCharacterInCellDoesNotBlankCell() {
        constexpr auto markdown =
            "| Key | Value |\n"
            "| --- | --- |\n"
            "|  |  |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("typing.md"), markdown)));
        f.settle();
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QVERIFY(clickCell(view, 1, 0) != nullptr);

        typeAsUser(QStringLiteral("hello"));
        QCOMPARE(focusedCell()->toPlainText(), QStringLiteral("hello"));
        QCOMPARE(focusedCell()->verticalScrollBar()->value(), 0);
        QCOMPARE(focusedCell()->horizontalScrollBar()->value(), 0);

        const QImage rendered = focusedCell()->viewport()->grab().toImage();
        const QRgb base = focusedCell()->palette().color(QPalette::Base).rgb();
        int nonBackgroundPixels = 0;
        for (int y = 0; y < rendered.height(); ++y) {
            for (int x = 0; x < rendered.width(); ++x) {
                if (rendered.pixel(x, y) != base) {
                    ++nonBackgroundPixels;
                }
            }
        }
        QVERIFY2(nonBackgroundPixels > 0, "the cell editor renders visible text while editing");

        // Switching cells commits the text and keeps it visible in the table.
        press(Qt::Key_Tab);
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| Key | Value |\n| --- | --- |\n| hello |  |\n"));
    }
};

QTEST_MAIN(tst_s018_tables)
#include "tst_s018_tables.moc"
