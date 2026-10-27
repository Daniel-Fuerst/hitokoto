// SPDX-License-Identifier: GPL-3.0-or-later
// S018: interactive Markdown pipe tables (ADR 0005).
#include "ui_test_support.hpp"

#include <QScrollBar>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}
QColor pixel(ui::EditorWidget* editor, qreal x, qreal y) {
    return editor->viewport()->grab().toImage().pixelColor(static_cast<int>(x), static_cast<int>(y));
}
}  // namespace

class tst_s018_tables : public QObject {
    Q_OBJECT
private slots:
    void rendersTableAndAlwaysConcealsSyntaxEvenWhenFocused() {
        constexpr auto markdown =
            "before\n"
            "| Name | Score |\n"
            "| :--- | ---: |\n"
            "| Alice | 95 |\n"
            "| Bob | 100 |\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("table.md"), markdown)));
        f.settle();

        const auto header = f.editor()->document()->findBlockByNumber(1);
        const auto delim = f.editor()->document()->findBlockByNumber(2);
        const auto row1 = f.editor()->document()->findBlockByNumber(3);

        for (int caretBlock : {0, 1, 3, 4}) {
            f.placeCaret(caretBlock, caretBlock == 1 || caretBlock == 3 ? 3 : 0);
            f.settle();
            // Pipe delimiters and the delimiter row stay collapsed even when the caret is inside the table.
            QVERIFY(test::isCollapsed(header, 0));
            QVERIFY(test::isCollapsed(header, 7));
            QVERIFY(test::isCollapsed(header, header.text().size() - 1));
            QVERIFY(test::isCollapsed(delim, 0));
            QVERIFY(test::isCollapsed(delim, 5));
            QVERIFY(test::isCollapsed(row1, 0));
            QVERIFY(test::isCollapsed(row1, 8));
            QCOMPARE(test::formatAt(header, 2).fontWeight(), QFont::Bold);
        }
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

    void alignsColumnsAcrossRowsAndHonoursColumnAlignment() {
        constexpr auto markdown =
            "| ColA | Centre | Right |\n"
            "| :--- | :---: | ---: |\n"
            "| A | X | 1 |\n"
            "| VeryWideFirstColumn | X | 1 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("align.md"), markdown)));
        f.settle();

        const auto row1 = f.editor()->document()->findBlockByNumber(2);
        const auto row2 = f.editor()->document()->findBlockByNumber(3);
        const auto cursorX = [&](const QTextBlock& b, int col) {
            QTextCursor c(b);
            c.setPosition(b.position() + col);
            return f.editor()->cursorRect(c).left();
        };

        // 'X' in column 1 must align horizontally between row1 (index 6) and row2 (index 24).
        const int xRow1Col1 = cursorX(row1, 6);
        const int xRow2Col1 = cursorX(row2, 24);
        QVERIFY2(std::abs(xRow1Col1 - xRow2Col1) <= 3,
                 qPrintable(QStringLiteral("col 1 x mismatch: %1 vs %2").arg(xRow1Col1).arg(xRow2Col1)));

        // Column 0 is left-aligned, column 1 is centred, column 2 is right-aligned within their cell rects.
        const QRectF cell0 = f.editor()->cellRect(2, 0);
        const QRectF cell1 = f.editor()->cellRect(2, 1);
        const QRectF cell2 = f.editor()->cellRect(2, 2);
        const double pad = f.editor()->styler().formats().table().padding;
        QVERIFY(std::abs(cursorX(row1, 2) - (cell0.left() + pad)) <= 4);
        QVERIFY(cursorX(row1, 6) > cell1.left() + pad + 4);
        QVERIFY(cursorX(row1, 10) > cell2.center().x());
    }

    void editsInsideCellsAndProtectsPipeSyntax() {
        constexpr auto markdown =
            "| Key | Value |\n"
            "| --- | --- |\n"
            "| foo | bar |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("edit.md"), markdown)));
        f.settle();

        // Click into cell (row 2, col 0) "foo" and try to Backspace past the start of the cell.
        const QRectF cell = f.editor()->cellRect(2, 0);
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, cell.center().toPoint());
        test::keyClick(f.editor(), Qt::Key_Home);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 2);
        test::keyClick(f.editor(), Qt::Key_Backspace);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 2);
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));

        // Try to Delete past the end of the cell.
        test::keyClick(f.editor(), Qt::Key_End);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 5);
        test::keyClick(f.editor(), Qt::Key_Delete);
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));

        // Type characters and an escaped pipe inside the cell.
        test::keyClicks(f.editor(), QStringLiteral("2|x"));
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| Key | Value |\n| --- | --- |\n| foo2\\|x | bar |\n"));
        const auto row = f.editor()->document()->findBlockByNumber(2);
        QVERIFY(test::isCollapsed(row, 6));  // the '\' before '|' is collapsed
    }

    void navigatesCellsWithTabAndArrowsAndAppendsRowAtEnd() {
        constexpr auto markdown =
            "| A | B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("nav.md"), markdown)));
        f.settle();

        f.placeCaret(0, 2);  // Header cell (0, 0) "A"
        test::keyClick(f.editor(), Qt::Key_Tab);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);  // Header cell (0, 1) "B"

        // Tab from end of header row skips delimiter row (block 1) and enters body row (block 2, col 0).
        test::keyClick(f.editor(), Qt::Key_Tab);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 2);  // Body cell (2, 0) "1"

        // Up from first body row skips delimiter row back to header row.
        test::keyClick(f.editor(), Qt::Key_Up);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);

        // Down from header row skips delimiter row to first body row.
        test::keyClick(f.editor(), Qt::Key_Down);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);

        // Tab to last cell (2, 1), then Tab again appends a new row and places the caret in its first cell.
        test::keyClick(f.editor(), Qt::Key_Tab);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);
        test::keyClicks(f.editor(), QStringLiteral("3"));
        test::keyClick(f.editor(), Qt::Key_Tab);
        test::keyClicks(f.editor(), QStringLiteral("4"));
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |\n"));

        // Shift+Tab moves backward to the previous cell.
        test::keyClick(f.editor(), Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 2);
    }

    void rendersInlineMarkdownInsideCellsAndKeepsNonTablesLiteral() {
        constexpr auto markdown =
            "not | a | table\n"
            "| **Bold** | `code | pipe` |\n"
            "| --- | --- |\n"
            "| *italic* | ~~strike~~ |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline.md"), markdown)));
        f.settle();
        f.placeCaret(0);
        f.settle();

        const auto nonTable = f.editor()->document()->findBlockByNumber(0);
        const auto header = f.editor()->document()->findBlockByNumber(1);
        const auto body = f.editor()->document()->findBlockByNumber(3);

        QVERIFY(!test::isCollapsed(nonTable, 4));
        QCOMPARE(test::formatAt(header, 4).fontWeight(), QFont::Bold);
        QCOMPARE(test::formatAt(header, 14).fontStyleHint(), QFont::TypeWriter);
        QVERIFY(test::formatAt(body, 3).fontItalic());
        QVERIFY(test::formatAt(body, 15).fontStrikeOut());
    }

    void addsRowsAndColumnsGraphicallyWithPlusButtons() {
        constexpr auto markdown =
            "| HeaderLeft | HeaderRight |\n"
            "| --- | --- |\n"
            "| CellLeft | CellRight |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("plus.md"), markdown)));
        f.settle();

        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        const QColor editorBg = qcolor(theme.background);
        const QColor buttonBg = qcolor(f.editor()->styler().formats().table().headerBackground);

        const QRectF addRow = f.editor()->addRowButtonRect(0);
        const QRectF addCol = f.editor()->addColumnButtonRect(0);
        QVERIFY(!addRow.isEmpty());
        QVERIFY(!addCol.isEmpty());

        // 1. Neither '+' button is visible before hovering near it.
        QVERIFY(!f.editor()->isAddRowButtonVisible(0));
        QVERIFY(!f.editor()->isAddColumnButtonVisible(0));
        QCOMPARE(pixel(f.editor(), addRow.left() + 6, addRow.center().y()), editorBg);
        QCOMPARE(pixel(f.editor(), addCol.center().x(), addCol.top() + 6), editorBg);

        // 2. Hovering inside the top-left cell (away from bottom and right edges) keeps both buttons hidden.
        const QRectF topLeftCell = f.editor()->cellRect(0, 0);
        QTest::mouseMove(f.editor()->viewport(), topLeftCell.center().toPoint());
        QCoreApplication::processEvents();
        QVERIFY(!f.editor()->isAddRowButtonVisible(0));
        QVERIFY(!f.editor()->isAddColumnButtonVisible(0));

        // 3. Hovering near the bottom edge reveals only the add-row '+' button.
        const QRectF tRect = f.editor()->tableRect(0);
        QTest::mouseMove(f.editor()->viewport(),
                         QPoint(static_cast<int>(topLeftCell.center().x()), static_cast<int>(tRect.bottom() - 3)));
        QTRY_VERIFY(f.editor()->isAddRowButtonVisible(0));
        QVERIFY(!f.editor()->isAddColumnButtonVisible(0));
        QTRY_COMPARE(pixel(f.editor(), addRow.left() + 6, addRow.center().y()), buttonBg);
        QCOMPARE(pixel(f.editor(), addCol.center().x(), addCol.top() + 6), editorBg);

        // Click the revealed '+' add-row button below the table.
        QTest::mouseMove(f.editor()->viewport(), addRow.center().toPoint());
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, addRow.center().toPoint());
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);
        test::keyClicks(f.editor(), QStringLiteral("r2c1"));
        f.settle();
        QCOMPARE(
            f.editor()->toPlainText(),
            QStringLiteral("| HeaderLeft | HeaderRight |\n| --- | --- |\n| CellLeft | CellRight |\n| r2c1 |  |\n"));

        // 4. Hovering near the right edge of the header row reveals only the add-column '+' button.
        const QRectF updatedTable = f.editor()->tableRect(0);
        const QRectF updatedAddRow = f.editor()->addRowButtonRect(0);
        const QRectF updatedAddCol = f.editor()->addColumnButtonRect(0);
        QTest::mouseMove(f.editor()->viewport(), QPoint(static_cast<int>(updatedTable.right() - 3),
                                                        static_cast<int>(topLeftCell.center().y())));
        QTRY_VERIFY(f.editor()->isAddColumnButtonVisible(0));
        QVERIFY(!f.editor()->isAddRowButtonVisible(0));
        QTRY_COMPARE(pixel(f.editor(), updatedAddCol.center().x(), updatedAddCol.top() + 6), buttonBg);
        QCOMPARE(pixel(f.editor(), updatedAddRow.left() + 6, updatedAddRow.center().y()), editorBg);

        // Click the revealed '+' add-column button to the right of the table.
        QTest::mouseMove(f.editor()->viewport(), updatedAddCol.center().toPoint());
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, updatedAddCol.center().toPoint());
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        test::keyClicks(f.editor(), QStringLiteral("C"));
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| HeaderLeft | HeaderRight | C |\n| --- | --- | --- |\n| CellLeft | CellRight |  |\n"
                                "| r2c1 |  |  |\n"));

        // 5. Moving the pointer away — or leaving the viewport altogether — hides both '+' buttons again.
        const QRectF finalAddRow = f.editor()->addRowButtonRect(0);
        QTest::mouseMove(f.editor()->viewport(), finalAddRow.center().toPoint());
        QTRY_VERIFY(f.editor()->isAddRowButtonVisible(0));
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(f.editor()->viewport(), &leave);
        QVERIFY(!f.editor()->isAddRowButtonVisible(0));
        QVERIFY(!f.editor()->isAddColumnButtonVisible(0));
        QTRY_COMPARE(pixel(f.editor(), finalAddRow.left() + 6, finalAddRow.center().y()), editorBg);
    }

    void themedTableUsesEveryToken() {
        constexpr auto markdown =
            "before\n"
            "| H1 | H2 |\n"
            "| --- | --- |\n"
            "| R1 | B1 |\n"
            "| R2 | B2 |\n"
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
        f.placeCaret(0);

        const auto header = f.editor()->document()->findBlockByNumber(1);
        const auto row1 = f.editor()->document()->findBlockByNumber(3);

        QCOMPARE(test::formatAt(header, 2).foreground().color(), qcolor(*theme.table.headerText));
        QCOMPARE(test::formatAt(header, 2).fontWeight(), 600);
        QCOMPARE(test::formatAt(header, 2).fontPointSize(), theme.editorFontSize * 1.25);
        QCOMPARE(test::formatAt(header, 2).fontFamilies().toStringList(),
                 QStringList{QStringLiteral("Table Test Family")});

        QCOMPARE(test::formatAt(row1, 2).foreground().color(), qcolor(*theme.table.text));
        QCOMPARE(test::formatAt(row1, 2).fontPointSize(), theme.editorFontSize * 1.25);

        const QRectF headerCell = f.editor()->cellRect(1, 0);
        const QRectF bodyCell1 = f.editor()->cellRect(3, 0);
        const QRectF bodyCell2 = f.editor()->cellRect(4, 0);
        // Sample inside each cell's right padding area (away from glyph pixels).
        QTRY_COMPARE(pixel(f.editor(), headerCell.right() - 3, headerCell.center().y()),
                     qcolor(*theme.table.headerBackground));
        QCOMPARE(pixel(f.editor(), bodyCell1.right() - 3, bodyCell1.center().y()), qcolor(*theme.table.background));
        QCOMPARE(pixel(f.editor(), bodyCell2.right() - 3, bodyCell2.center().y()),
                 qcolor(*theme.table.stripeBackground));
        // Sample on the vertical column border between col 0 and col 1.
        QCOMPARE(pixel(f.editor(), headerCell.right(), headerCell.center().y()), qcolor(*theme.table.border));
    }

    void wrapsLongColumnInsideItsColumnBoundsAndStaysInViewport() {
        constexpr auto markdown =
            "| Col 1 | Col 2 |\n"
            "| --- | --- |\n"
            "| Say yes | This text is very very very very very veryvery very veryvery very veryvery very very very "
            "very long |\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrap.md"), markdown)));
        f.settle();

        // 1. Neither the table nor its right-side '+' add-column button overflows the viewport.
        const QRectF tRect = f.editor()->tableRect(0);
        const QRectF addCol = f.editor()->addColumnButtonRect(0);
        QVERIFY(!tRect.isEmpty());
        QVERIFY2(tRect.right() <= f.editor()->viewport()->width(),
                 qPrintable(QStringLiteral("table right %1 > viewport %2")
                                .arg(tRect.right())
                                .arg(f.editor()->viewport()->width())));
        QVERIFY2(addCol.right() <= f.editor()->viewport()->width(),
                 qPrintable(QStringLiteral("addCol right %1 > viewport %2")
                                .arg(addCol.right())
                                .arg(f.editor()->viewport()->width())));

        // 2. Column 1 wraps onto >= 2 sub-lines, and every wrapped sub-line starts inside Column 1's horizontal bounds
        //    (never back at the start of the row under Column 0).
        const QTextBlock body = f.editor()->document()->findBlockByNumber(2);
        const QRectF cell0 = f.editor()->cellRect(2, 0);
        const QRectF cell1 = f.editor()->cellRect(2, 1);
        const double pad = f.editor()->styler().formats().table().padding;

        QTextCursor startCol1(body);
        startCol1.setPosition(body.position() + 12);  // 'T' of "This"
        QTextCursor endCol1(body);
        endCol1.setPosition(body.position() + static_cast<int>(body.text().indexOf(QStringLiteral("long"))));

        const QRect firstLineCaret = f.editor()->cursorRect(startCol1);
        const QRect wrappedLineCaret = f.editor()->cursorRect(endCol1);
        QVERIFY2(wrappedLineCaret.top() > firstLineCaret.top() + 8, "long cell text must wrap onto a lower sub-line");
        QVERIFY2(wrappedLineCaret.left() >= static_cast<int>(cell1.left() + pad) - 4,
                 qPrintable(QStringLiteral("wrapped caret x=%1 < col1 text left=%2")
                                .arg(wrappedLineCaret.left())
                                .arg(cell1.left() + pad)));
        QVERIFY(wrappedLineCaret.left() > cell0.right());

        // 3. No wrapped glyph pixels bleed into Column 0 on the wrapped sub-line's vertical band.
        const QColor expectedBg = qcolor(f.editor()->styler().formats().table().background);
        const QImage grab = f.editor()->viewport()->grab().toImage();
        const int sampleY = wrappedLineCaret.center().y();
        for (int x = static_cast<int>(cell0.left()) + 4; x < static_cast<int>(cell0.right()) - 4; ++x) {
            QCOMPARE(grab.pixelColor(x, sampleY), expectedBg);
        }

        // 4. Up/Down arrow keys step across wrapped sub-lines inside Column 1 before moving to adjacent rows.
        f.editor()->setTextCursor(startCol1);
        test::keyClick(f.editor(), Qt::Key_Down);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QVERIFY(f.editor()->cursorRect().top() > firstLineCaret.top());
        test::keyClick(f.editor(), Qt::Key_Up);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QCOMPARE(f.editor()->cursorRect().top(), firstLineCaret.top());

        // 5. Narrowing the window dynamically re-wraps the table within the new viewport width.
        f.window->resize(480, 700);
        f.settle();
        QVERIFY(f.editor()->tableRect(0).right() <= f.editor()->viewport()->width());
        QVERIFY(f.editor()->addColumnButtonRect(0).right() <= f.editor()->viewport()->width());
        QVERIFY(f.editor()->cursorRect(endCol1).left() > f.editor()->cellRect(2, 0).right());
    }

    void emptyRowsCreatedWithEnterKeepFullRowAndCaretHeight() {
        // Note: no trailing newline so the last table row is also document()->lastBlock().
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1. Row | 2. Row |";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("empty_rows.md"), markdown)));
        f.settle();

        const qreal populatedHeight = f.editor()->cellRect(2, 0).height();
        QVERIFY2(populatedHeight >= 16.0, qPrintable(QStringLiteral("populatedHeight=%1").arg(populatedHeight)));

        // Place caret on the last row: pressing Enter once creates an empty row; Tab at end adds 3 more empty rows.
        f.placeCaret(2, 2);
        const int fullCaretHeight = f.editor()->cursorRect().height();
        test::keyClick(f.editor(), Qt::Key_Return);
        for (int i = 0; i < 3; ++i) {
            test::keyClick(f.editor(), Qt::Key_Tab);
            test::keyClick(f.editor(), Qt::Key_Tab);
        }
        f.settle();
        QCOMPARE(f.editor()->document()->blockCount(), 7);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 6);

        // Caret inside an empty cell must have full single-line caret height, not 1-2 px.
        QVERIFY2(std::abs(f.editor()->cursorRect().height() - fullCaretHeight) <= 2,
                 qPrintable(QStringLiteral("empty cell caret height=%1 vs populated=%2")
                                .arg(f.editor()->cursorRect().height())
                                .arg(fullCaretHeight)));

        // Move caret back up to row 2 ("1. Row") so none of the empty rows 3..6 are focused.
        f.placeCaret(2, 2);
        f.settle();

        for (int row : {3, 4, 5, 6}) {
            const qreal h = f.editor()->cellRect(row, 0).height();
            QVERIFY2(
                std::abs(h - populatedHeight) <= 2.0,
                qPrintable(QStringLiteral("row %1 height=%2 vs populated=%3").arg(row).arg(h).arg(populatedHeight)));
        }

        // Also verify compact '|||' empty rows loaded from file keep full row height.
        test::Fixture f2;
        QVERIFY(f2.open(f2.writeFile(QStringLiteral("compact_empty.md"), "| a | b |\n|---|---|\n|||\n| x | y |\n")));
        f2.settle();
        const qreal compactEmptyH = f2.editor()->cellRect(2, 0).height();
        const qreal compactTextH = f2.editor()->cellRect(3, 0).height();
        QVERIFY2(std::abs(compactEmptyH - compactTextH) <= 2.0,
                 qPrintable(QStringLiteral("compact empty=%1 vs text=%2").arg(compactEmptyH).arg(compactTextH)));
    }

    void exitsTableViaEnterOnEmptyRowCtrlEnterEscapeAndClickingOutside() {
        // Table at line 0 and at the end of the file (no trailing newline).
        constexpr auto markdown =
            "| a | b |\n"
            "| --- | --- |\n"
            "| 1 | 2 |";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("exit.md"), markdown)));
        f.settle();

        // 1. Pressing Enter on populated last row appends an empty row; pressing Enter again on that empty row
        //    removes the empty row and exits onto a plain paragraph line below the table.
        f.placeCaret(2, 2);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);
        QVERIFY(f.editor()->styler().renderFor(f.editor()->textCursor().block()).table);

        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QVERIFY(!f.editor()->styler().renderFor(f.editor()->textCursor().block()).table);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        test::keyClicks(f.editor(), QStringLiteral("Paragraph below"));
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nParagraph below"));
        QVERIFY(!f.editor()->styler().renderFor(f.editor()->textCursor().block()).table);

        // 2. Clicking below a table that ends at EOF creates a plain line below the table and exits cleanly.
        test::Fixture f2;
        QVERIFY(f2.open(f2.writeFile(QStringLiteral("click_below.md"), markdown)));
        f2.settle();
        f2.placeCaret(2, 2);
        const QRectF addRow = f2.editor()->addRowButtonRect(0);
        const QPoint belowTable(static_cast<int>(addRow.center().x()), static_cast<int>(addRow.bottom() + 40));
        QTest::mouseMove(f2.editor()->viewport(), belowTable);
        QTest::mouseClick(f2.editor()->viewport(), Qt::LeftButton, {}, belowTable);
        f2.settle();
        QVERIFY(!f2.editor()->styler().renderFor(f2.editor()->textCursor().block()).table);
        test::keyClicks(f2.editor(), QStringLiteral("Clicked below"));
        f2.settle();
        QCOMPARE(f2.editor()->toPlainText(), QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nClicked below"));

        // 3. Clicking above a table at block 0 creates a plain line above the table.
        const QRectF topTable = f2.editor()->tableRect(0);
        const QPoint aboveTable(static_cast<int>(topTable.left() + 20), 6);
        QTest::mouseMove(f2.editor()->viewport(), aboveTable);
        QTest::mouseClick(f2.editor()->viewport(), Qt::LeftButton, {}, aboveTable);
        f2.settle();
        QCOMPARE(f2.editor()->textCursor().blockNumber(), 0);
        QVERIFY(!f2.editor()->styler().renderFor(f2.editor()->textCursor().block()).table);
        test::keyClicks(f2.editor(), QStringLiteral("# Heading above"));
        f2.settle();
        QVERIFY(f2.editor()->toPlainText().startsWith(QStringLiteral("# Heading above\n\n| a | b |\n")));

        // 4. Ctrl+Enter and Escape exit below the table from any cell without needing an empty row.
        test::Fixture f3;
        QVERIFY(f3.open(f3.writeFile(QStringLiteral("ctrl_enter.md"), markdown)));
        f3.settle();
        f3.placeCaret(0, 2);  // Header cell
        test::keyClick(f3.editor(), Qt::Key_Return, Qt::ControlModifier);
        f3.settle();
        QVERIFY(!f3.editor()->styler().renderFor(f3.editor()->textCursor().block()).table);
        QCOMPARE(f3.editor()->textCursor().blockNumber(), 4);

        f3.placeCaret(2, 2);
        test::keyClick(f3.editor(), Qt::Key_Escape);
        f3.settle();
        QVERIFY(!f3.editor()->styler().renderFor(f3.editor()->textCursor().block()).table);
        QCOMPARE(f3.editor()->textCursor().blockNumber(), 4);
    }

    void deletesRowsViaHoverMinusButtonEmptyRowBackspaceAndShortcut() {
        constexpr auto markdown =
            "| Name | Role |\n"
            "| --- | --- |\n"
            "| Alice | Dev |\n"
            "| Bob | QA |\n"
            "| Carol | Ops |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("delete_rows.md"), markdown)));
        f.settle();

        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        const QColor editorBg = qcolor(theme.background);
        const QColor buttonBg = qcolor(f.editor()->styler().formats().table().headerBackground);

        // Header and delimiter rows never have a delete-row button; body rows do.
        QVERIFY(f.editor()->deleteRowButtonRect(0).isEmpty());
        QVERIFY(f.editor()->deleteRowButtonRect(1).isEmpty());
        const QRectF delAlice = f.editor()->deleteRowButtonRect(2);
        const QRectF delBob = f.editor()->deleteRowButtonRect(3);
        const QRectF delCarol = f.editor()->deleteRowButtonRect(4);
        QVERIFY(!delAlice.isEmpty());
        QVERIFY(!delBob.isEmpty());
        QVERIFY(!delCarol.isEmpty());
        QVERIFY(delBob.right() <= f.editor()->tableRect(0).left());

        // 1. Delete-row '-' buttons stay hidden when not hovering near a body row's left edge.
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(2));
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(3));
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(4));
        QCOMPARE(pixel(f.editor(), delBob.center().x(), delBob.top() + 4), editorBg);

        QTest::mouseMove(f.editor()->viewport(), f.editor()->cellRect(3, 1).center().toPoint());
        QCoreApplication::processEvents();
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(3));

        // 2. Hovering near the left edge of Bob's row reveals only Bob's '-' button; clicking it deletes Bob.
        QTest::mouseMove(f.editor()->viewport(), QPoint(static_cast<int>(f.editor()->cellRect(3, 0).left() + 4),
                                                        static_cast<int>(delBob.center().y())));
        QTRY_VERIFY(f.editor()->isDeleteRowButtonVisible(3));
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(2));
        QVERIFY(!f.editor()->isDeleteRowButtonVisible(4));
        QTRY_COMPARE(pixel(f.editor(), delBob.center().x(), delBob.top() + 4), buttonBg);

        QTest::mouseMove(f.editor()->viewport(), delBob.center().toPoint());
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, delBob.center().toPoint());
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n| Carol | Ops |\n"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);

        // 3. Backspace or Delete on an empty body row deletes that row.
        f.placeCaret(3, 11);  // Carol's second cell
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        test::keyClick(f.editor(), Qt::Key_Backspace);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n| Carol | Ops |\n"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 3);

        f.placeCaret(3, 11);
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
        test::keyClick(f.editor(), Qt::Key_Delete);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n| Carol | Ops |\n"));

        // 4. Ctrl+Shift+Backspace deletes a populated body row, even down to header + delimiter, and undo restores it.
        f.placeCaret(3, 2);  // Carol
        test::keyClick(f.editor(), Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);

        test::keyClick(f.editor(), Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| Name | Role |\n| --- | --- |\n"));
        QVERIFY(f.editor()->styler().renderFor(f.editor()->document()->findBlockByNumber(0)).table);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);

        test::keyClick(f.editor(), Qt::Key_Z, Qt::ControlModifier);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| Name | Role |\n| --- | --- |\n| Alice | Dev |\n"));
    }

    void deletesColumnsViaHoverMinusButtonEmptyColumnBackspaceAndShortcut() {
        constexpr auto markdown =
            "| ColA | ColB | ColC |\n"
            "| :--- | :---: | ---: |\n"
            "| a1 | b1 | c1 |\n"
            "| a2 | b2 | c2 |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("delete_cols.md"), markdown)));
        f.settle();

        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        const QColor editorBg = qcolor(theme.background);
        const QColor buttonBg = qcolor(f.editor()->styler().formats().table().headerBackground);

        const QRectF delCol0 = f.editor()->deleteColumnButtonRect(0, 0);
        const QRectF delCol1 = f.editor()->deleteColumnButtonRect(0, 1);
        const QRectF delCol2 = f.editor()->deleteColumnButtonRect(0, 2);
        QVERIFY(!delCol0.isEmpty());
        QVERIFY(!delCol1.isEmpty());
        QVERIFY(!delCol2.isEmpty());
        QVERIFY(delCol1.bottom() <= f.editor()->tableRect(0).top());

        // 1. Delete-column '-' buttons stay hidden when not hovering near a column's top edge.
        QVERIFY(!f.editor()->isDeleteColumnButtonVisible(0, 0));
        QVERIFY(!f.editor()->isDeleteColumnButtonVisible(0, 1));
        QVERIFY(!f.editor()->isDeleteColumnButtonVisible(0, 2));
        QCOMPARE(pixel(f.editor(), delCol1.left() + 6, delCol1.center().y()), editorBg);

        // 2. Hovering near the top edge of ColB reveals only ColB's '-' button; clicking it deletes ColB.
        const QRectF headerCol1 = f.editor()->cellRect(0, 1);
        QTest::mouseMove(f.editor()->viewport(),
                         QPoint(static_cast<int>(headerCol1.center().x()), static_cast<int>(headerCol1.top() + 3)));
        QTRY_VERIFY(f.editor()->isDeleteColumnButtonVisible(0, 1));
        QVERIFY(!f.editor()->isDeleteColumnButtonVisible(0, 0));
        QVERIFY(!f.editor()->isDeleteColumnButtonVisible(0, 2));
        QTRY_COMPARE(pixel(f.editor(), delCol1.left() + 6, delCol1.center().y()), buttonBg);

        QTest::mouseMove(f.editor()->viewport(), delCol1.center().toPoint());
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, delCol1.center().toPoint());
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| ColA | ColC |\n| :--- | ---: |\n| a1 | c1 |\n| a2 | c2 |\n"));

        // 3. Adding an empty column with '+' and pressing Backspace inside that empty column deletes it.
        const QRectF addCol = f.editor()->addColumnButtonRect(0);
        QTest::mouseMove(f.editor()->viewport(), addCol.center().toPoint());
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, addCol.center().toPoint());
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| ColA | ColC |  |\n| :--- | ---: | --- |\n| a1 | c1 |  |\n| a2 | c2 |  |\n"));
        test::keyClick(f.editor(), Qt::Key_Backspace);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| ColA | ColC |\n| :--- | ---: |\n| a1 | c1 |\n| a2 | c2 |\n"));

        // 4. Ctrl+Shift+Backspace on a header cell (or Ctrl+Alt+Backspace in any cell) deletes that column,
        //    leaving a 1-column table whose last column cannot be deleted, and Ctrl+Z restores it.
        f.placeCaret(0, 2);  // ColA header
        test::keyClick(f.editor(), Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| ColC |\n| ---: |\n| c1 |\n| c2 |\n"));
        QVERIFY(f.editor()->deleteColumnButtonRect(0, 0).isEmpty());

        test::keyClick(f.editor(), Qt::Key_Z, Qt::ControlModifier);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("| ColA | ColC |\n| :--- | ---: |\n| a1 | c1 |\n| a2 | c2 |\n"));
    }

    // Review objection O2: the table width must not depend on whether the vertical scroll bar is
    // shown. Otherwise showing the bar narrows the table, it wraps and grows, and hiding the bar
    // widens it again: QAbstractScrollArea relayouts without end.
    void tableWidthDoesNotDependOnScrollBarVisibility() {
        constexpr auto markdown =
            "| Name | Note |\n"
            "| --- | --- |\n"
            "| Alice | short |\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("bar.md"), markdown)));
        f.settle();
        QVERIFY(!f.editor()->verticalScrollBar()->isVisible());
        const double withoutBar = f.editor()->styler().availableWidth();

        f.editor()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        QTRY_VERIFY(f.editor()->verticalScrollBar()->isVisible());
        f.settle();
        QCOMPARE(f.editor()->styler().availableWidth(), withoutBar);
    }

    void typingInATableNearTheScrollBarThresholdSettles() {
        QByteArray markdown = "| C1 | C2 | C3 | C4 | C5 |\n| --- | --- | --- | --- | --- |\n";
        for (int i = 0; i < 50; ++i) {
            markdown += "| alpha | beta gamma | delta | epsilon zeta eta | theta |\n";
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("threshold.md"), markdown)));
        f.settle();

        // Counts viewport resizes; once they run away, the bar is pinned so the test ends instead
        // of hanging, and the count makes it fail.
        constexpr int kRunaway = 20;
        struct ResizeCounter : QObject {
            QPlainTextEdit* editor = nullptr;
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

        f.placeCaret(27, 4);
        test::keyClicks(f.editor(), QStringLiteral("q"));
        f.settle();
        QTest::qWait(200);
        f.editor()->viewport()->removeEventFilter(&counter);
        QVERIFY2(counter.count < kRunaway, qPrintable(QStringLiteral("%1 viewport resizes").arg(counter.count)));
    }
};

QTEST_MAIN(tst_s018_tables)
#include "tst_s018_tables.moc"
