// SPDX-License-Identifier: GPL-3.0-or-later
// S026: floating toolbar on the right-hand side of the editor.
#include "floating_toolbar.hpp"
#include "ui_test_support.hpp"

#include <QAbstractItemModel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTableView>
#include <QToolButton>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

QToolButton* toolbarButton(ui::EditorWidget* editor, const char* name) {
    return editor->floatingToolbar()->findChild<QToolButton*>(QString::fromLatin1(name));
}

QPlainTextEdit* focusedCell() {
    return qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
}
}  // namespace

class tst_s026_floating_toolbar : public QObject {
    Q_OBJECT
private slots:
    void toolbarIsCenteredOnRightEdgeAndStaysInPlaceWhenScrollingOrResizing() {
        test::Fixture f;
        QString lines;
        for (int i = 0; i < 80; ++i) {
            lines += QStringLiteral("Line %1\n").arg(i);
        }
        QVERIFY(f.open(f.writeFile(QStringLiteral("doc.md"), lines.toUtf8())));
        f.settle();

        ui::FloatingToolbar* bar = f.editor()->floatingToolbar();
        QVERIFY(bar != nullptr);
        QVERIFY(bar->isVisible());

        QToolButton* tableBtn = toolbarButton(f.editor(), "createTableButton");
        QToolButton* codeBtn = toolbarButton(f.editor(), "createInlineCodeButton");
        QVERIFY(tableBtn != nullptr);
        QVERIFY(codeBtn != nullptr);
        QVERIFY(tableBtn->isVisible());
        QVERIFY(codeBtn->isVisible());
        QCOMPARE(tableBtn->focusPolicy(), Qt::NoFocus);
        QCOMPARE(codeBtn->focusPolicy(), Qt::NoFocus);
        QVERIFY(!tableBtn->toolTip().isEmpty());
        QVERIFY(!codeBtn->toolTip().isEmpty());
        QVERIFY(!tableBtn->accessibleName().isEmpty());
        QVERIFY(!codeBtn->accessibleName().isEmpty());
        // Vertical layout: inline-code button sits below table button.
        QVERIFY(codeBtn->y() > tableBtn->y());
        QCOMPARE(codeBtn->x(), tableBtn->x());

        const auto checkCenteredOnRight = [&] {
            const int expectedY = (f.editor()->height() - bar->height()) / 2;
            QVERIFY2(std::abs(bar->y() - expectedY) <= 1, "vertically centered");
            QVERIFY2(bar->geometry().right() < f.editor()->width(), "inside right edge");
            QVERIFY2(bar->x() > f.editor()->width() / 2, "on the right-hand side");
        };
        checkCenteredOnRight();

        const QRect beforeScroll = bar->geometry();
        f.editor()->verticalScrollBar()->setValue(200);
        f.settle();
        QCOMPARE(bar->geometry(), beforeScroll);

        f.window->resize(960, 720);
        f.settle();
        checkCenteredOnRight();
    }

    void createTableOnEmptyLineInsertsEmbeddedTableAndFocusesFirstHeaderCell() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("empty.md"), "before\n\nafter\n")));
        f.settle();
        f.placeCaret(1, 0);

        QToolButton* tableBtn = toolbarButton(f.editor(), "createTableButton");
        QVERIFY(tableBtn != nullptr);
        QTest::mouseClick(tableBtn, Qt::LeftButton);
        f.settle();

        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QVERIFY(view->isVisible());
        QCOMPARE(view->model()->columnCount(), 2);
        QCOMPARE(view->model()->rowCount(), 2);  // header + 1 body row
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(view->currentIndex().column(), 0);
        QVERIFY(focusedCell() != nullptr);

        // Typing goes straight into the first header cell.
        test::keyClicks(focusedCell(), QStringLiteral("Col 1"));
        test::keyClick(focusedCell(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(view->model()->data(view->model()->index(0, 0)).toString(), QStringLiteral("Col 1"));

        // Following text stays a visible paragraph outside the table.
        const QTextBlock last = f.editor()->document()->lastBlock().previous();
        QCOMPARE(last.text(), QStringLiteral("after"));
        QVERIFY(last.isVisible());
    }

    void createTableOnNonEmptyLineInsertsBelowAndUndoesInOneStep() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("para.md"), "first paragraph\nsecond paragraph\n")));
        f.settle();
        f.placeCaret(0, 5);

        QToolButton* tableBtn = toolbarButton(f.editor(), "createTableButton");
        QVERIFY(tableBtn != nullptr);
        QTest::mouseClick(tableBtn, Qt::LeftButton);
        f.settle();

        QCOMPARE(f.editor()->document()->firstBlock().text(), QStringLiteral("first paragraph"));
        QTableView* view = f.editor()->tableViewAt(1);
        QVERIFY(view != nullptr);
        QCOMPARE(view->model()->columnCount(), 2);
        QCOMPARE(view->model()->rowCount(), 2);
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(view->currentIndex().column(), 0);
        // "second paragraph" is separated by a blank line and not swallowed as a table row.
        QVERIFY(f.editor()->tableViewAt(5) == nullptr);
        QCOMPARE(f.editor()->document()->findBlockByNumber(5).text(), QStringLiteral("second paragraph"));

        // Undo removes the inserted table in one step.
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("first paragraph\nsecond paragraph\n"));
        QVERIFY(f.editor()->tableViewAt(1) == nullptr);
    }

    void createInlineCodeInsertsBackticksOrWrapsSelection() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "hello world\n")));
        f.settle();
        f.placeCaret(0, 6);
        f.editor()->setFocus();

        QToolButton* codeBtn = toolbarButton(f.editor(), "createInlineCodeButton");
        QVERIFY(codeBtn != nullptr);
        QTest::mouseClick(codeBtn, Qt::LeftButton);
        f.settle();

        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("hello ``world\n"));
        QCOMPARE(f.editor()->textCursor().position(), 7);  // between the two backticks
        QVERIFY(f.editor()->hasFocus());

        test::keyClicks(f.editor(), QStringLiteral("fn()"));
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("hello `fn()`world\n"));

        // Wrapping a selection in backticks.
        QTextCursor sel = f.editor()->textCursor();
        sel.setPosition(0);
        sel.setPosition(5, QTextCursor::KeepAnchor);
        f.editor()->setTextCursor(sel);
        QTest::mouseClick(codeBtn, Qt::LeftButton);
        f.settle();

        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("`hello` `fn()`world\n"));

        // Undo restores the text before wrapping in one step.
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("hello `fn()`world\n"));
    }

    void themedToolbarUsesEveryToken() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Dark);
        theme.toolbar.background = core::Color{0x22, 0x24, 0x2a, 0xff};
        theme.toolbar.border = core::Color{0x44, 0x48, 0x52, 0xff};
        theme.toolbar.borderWidth = 2;
        theme.toolbar.radius = 10;
        theme.toolbar.padding = 6;
        theme.toolbar.spacing = 8;
        theme.toolbar.margin = 20;
        theme.toolbar.icon = core::Color{0xe0, 0xe8, 0xf0, 0xff};
        theme.toolbar.buttonSize = 34;
        theme.toolbar.buttonRadius = 8;
        theme.toolbar.hoverBackground = core::Color{0x35, 0x39, 0x42, 0xff};
        theme.toolbar.activeBackground = core::Color{0x58, 0x65, 0xf2, 0xff};
        theme.toolbar.activeIcon = core::Color{0xff, 0xff, 0xff, 0xff};
        f.window->applyTheme(theme);
        f.settle();

        ui::FloatingToolbar* bar = f.editor()->floatingToolbar();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->toolbarStyle(), core::toolbarStyle(theme));

        QToolButton* tableBtn = toolbarButton(f.editor(), "createTableButton");
        QToolButton* codeBtn = toolbarButton(f.editor(), "createInlineCodeButton");
        QCOMPARE(tableBtn->size(), QSize(34, 34));
        QCOMPARE(codeBtn->size(), QSize(34, 34));
        QCOMPARE(bar->width(), 34 + 2 * 6);
        QCOMPARE(bar->height(), 2 * 34 + 8 + 2 * 6);
        QCOMPARE(codeBtn->y() - tableBtn->y() - tableBtn->height(), 8);

        const QImage img = bar->grab().toImage();
        QCOMPARE(img.pixelColor(bar->width() / 2, 4), qcolor(*theme.toolbar.background));
        QCOMPARE(img.pixelColor(bar->width() / 2, 0), qcolor(*theme.toolbar.border));
    }
};

QTEST_MAIN(tst_s026_floating_toolbar)
#include "tst_s026_floating_toolbar.moc"
