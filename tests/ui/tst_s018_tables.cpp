// SPDX-License-Identifier: GPL-3.0-or-later
// S018: Markdown pipe tables shown as an embedded table view (ADR 0005, revised 2026-10-27).
#include "table_model.hpp"
#include "ui_test_support.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QClipboard>
#include <QHeaderView>
#include <QInputMethodEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTableView>
#include <QTextCursor>
#include <QToolButton>

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

        // Undo in the document restores the previous lines.
        f.editor()->document()->undo();
        f.editor()->document()->undo();
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

        f.editor()->setFocus();
        f.placeCaret(0, 2);
        test::keyClick(f.editor(), Qt::Key_Down);
        QTRY_VERIFY(view->hasFocus());
        QCOMPARE(view->currentIndex().row(), 0);
        test::keyClick(view, Qt::Key_Up);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);

        f.placeCaret(5, 0);
        test::keyClick(f.editor(), Qt::Key_Up);
        QTRY_VERIFY(view->hasFocus());
        QCOMPARE(view->currentIndex().row(), 2);
        test::keyClick(view, Qt::Key_Down);
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
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
        // document an empty line is added for the caret.
        test::keyClick(view, Qt::Key_Return);
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n"));
        QTRY_VERIFY(f.editor()->hasFocus());
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);

        // The context menu actions add and delete columns and rows; undo restores.
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        QVERIFY(action(view, "tableAddColumn") != nullptr);
        action(view, "tableAddColumn")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n"));
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        action(view, "tableDeleteColumn")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| B |  |\n| --- | --- |\n| 2 |  |\n"));
        view = f.editor()->tableViewAt(0);
        view->setCurrentIndex(view->model()->index(1, 0));
        action(view, "tableDeleteRow")->trigger();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| B |  |\n| --- | --- |\n"));
        f.editor()->document()->undo();
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n"));

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
        QCOMPARE(text(f), QStringLiteral("| A | B |  |\n| --- | --- | --- |\n| 1 | 2 |  |\n|  |  |  |\n"));
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

        // A line typed directly under the table is a row (GFM); it stays text while typed.
        QTextCursor end(f.editor()->document());
        end.movePosition(QTextCursor::End);
        f.editor()->setTextCursor(end);
        f.editor()->setFocus();
        typeAsUser(QStringLiteral("|c"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("|a|b|\n|---|---|\n|c"));
        QVERIFY(f.editor()->hasFocus());
        QVERIFY(f.editor()->tableViewAt(0) == nullptr);
        typeAsUser(QStringLiteral("|d|\n"));
        f.settle();
        QCOMPARE(text(f), QStringLiteral("|a|b|\n|---|---|\n|c|d|\n"));
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QCOMPARE(view->model()->rowCount(), 2);
        QVERIFY(f.editor()->hasFocus());
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
        test::keyClick(QApplication::focusWidget(), Qt::Key_Return);
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
};

QTEST_MAIN(tst_s018_tables)
#include "tst_s018_tables.moc"
