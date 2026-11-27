// SPDX-License-Identifier: GPL-3.0-or-later
// S051: task list items show as checkboxes. A click toggles the box; keys never do. The prefix up
// to the text is never shown and the caret never stands in it; Backspace before the text removes
// the box; Enter continues task lists.
#include "ui_test_support.hpp"

#include <QImage>

#include <cmath>

using namespace hitokoto;

namespace {
QString lineText(ui::EditorWidget* editor, int blockNumber) {
    return editor->document()->findBlockByNumber(blockNumber).text();
}

/// Pixels in `box` (viewport coordinates) close to `colour`.
int pixelsOf(ui::EditorWidget* editor, const QRectF& box, const QColor& colour) {
    const QImage image = editor->viewport()->grab().toImage();
    const qreal sx = image.width() / static_cast<qreal>(editor->viewport()->width());
    const qreal sy = image.height() / static_cast<qreal>(editor->viewport()->height());
    int hits = 0;
    for (int y = static_cast<int>(box.top()); y <= static_cast<int>(box.bottom()); ++y) {
        for (int x = static_cast<int>(box.left()); x <= static_cast<int>(box.right()); ++x) {
            const QColor p = image.pixelColor(static_cast<int>(x * sx), static_cast<int>(y * sy));
            if (std::abs(p.red() - colour.red()) + std::abs(p.green() - colour.green()) +
                    std::abs(p.blue() - colour.blue()) <
                40) {
                ++hits;
            }
        }
    }
    return hits;
}

QPoint centreOf(const QRectF& box) {
    return box.center().toPoint();
}
}  // namespace

class tst_s051_task_checkboxes : public QObject {
    Q_OBJECT
private slots:
    // Scenarios "unchecked and checked boxes" and "no source view".
    void boxesAreDrawnWithEveryReveal() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "- [ ] milk\n- [x] bread\nother\n")));
        f.settle();
        const core::TaskStyle style = core::taskStyle(core::builtinTheme(core::ColorScheme::Light));
        for (const int caretLine : {2, 0, 1}) {
            f.placeCaret(caretLine, caretLine == 2 ? 0 : 6);
            f.settle();
            const QRectF unchecked = f.editor()->checkboxRect(0);
            const QRectF checked = f.editor()->checkboxRect(1);
            QVERIFY(!unchecked.isEmpty());
            QVERIFY(!checked.isEmpty());
            QVERIFY2(pixelsOf(f.editor(), checked, test::qcolor(style.checkedBackground)) > 4,
                     qPrintable(QStringLiteral("caret on line %1").arg(caretLine)));
            QCOMPARE(pixelsOf(f.editor(), unchecked, test::qcolor(style.checkedBackground)), 0);
            // Not a character of the prefix is visible, with the caret on the line or not.
            for (int line = 0; line < 2; ++line) {
                const QTextBlock block = f.editor()->document()->findBlockByNumber(line);
                for (int column = 0; column < 6; ++column) {
                    QCOMPARE(test::formatAt(block, column).foreground().color().alpha(), 0);
                }
            }
        }
    }

    // Scenario "a click toggles".
    void clickTogglesAndUndoRestores() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "- [ ] milk\n- [] tea\nother\n")));
        f.placeCaret(2, 3);
        f.settle();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, centreOf(f.editor()->checkboxRect(0)));
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [x] milk"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 3);
        f.editor()->undo();
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [ ] milk"));
        f.settle();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, centreOf(f.editor()->checkboxRect(0)));
        f.settle();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, centreOf(f.editor()->checkboxRect(0)));
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [ ] milk"));
        f.settle();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, centreOf(f.editor()->checkboxRect(1)));
        QCOMPARE(lineText(f.editor(), 1), QStringLiteral("- [x] tea"));
    }

    // Scenario "keys never toggle".
    void keysNeverToggle() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "- [ ] milk\n")));
        f.placeCaret(0, 6);
        test::keyClick(f.editor(), Qt::Key_Space);
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [ ]  milk"));
        f.editor()->undo();
        f.placeCaret(0, 6);
        test::keyClick(f.editor(), Qt::Key_Return);
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [ ] "));
        QCOMPARE(lineText(f.editor(), 1), QStringLiteral("- [ ] milk"));
    }

    // Scenario "the caret skips the box".
    void caretSkipsTheBox() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "a\n- [ ] milk\n")));
        f.placeCaret(1, 6);
        f.settle();
        test::keyClick(f.editor(), Qt::Key_Home);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);
        test::keyClick(f.editor(), Qt::Key_Left);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 1);
        test::keyClick(f.editor(), Qt::Key_Right);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 1);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);
        f.placeCaret(1, 2);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);  // a caret placed in the prefix moves to the text
        // A click left of the box (in the margin) puts the caret before the text.
        f.placeCaret(0, 0);
        const QRectF box = f.editor()->checkboxRect(1);
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {},
                          QPoint(static_cast<int>(box.left()) - 6, static_cast<int>(box.center().y())));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 1);
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);
        QCOMPARE(lineText(f.editor(), 1), QStringLiteral("- [ ] milk"));  // a click beside the box does not toggle
    }

    // Scenario "Backspace removes the box".
    void backspaceRemovesTheBox() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "  - [x] milk\n")));
        f.placeCaret(0, 8);
        test::keyClick(f.editor(), Qt::Key_Backspace);
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("  - milk"));
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 4);
        f.editor()->undo();
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("  - [x] milk"));
    }

    // Scenario "typing creates a task item".
    void typingCreatesATaskItem() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "\n")));
        f.placeCaret(0);
        test::keyClicks(f.editor(), QStringLiteral("- [ ]"));
        f.settle();
        QVERIFY(f.editor()->checkboxRect(0).isEmpty());
        test::keyClicks(f.editor(), QStringLiteral(" "));
        f.settle();
        QVERIFY(!f.editor()->checkboxRect(0).isEmpty());
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 6);
        test::keyClicks(f.editor(), QStringLiteral("milk"));
        QCOMPARE(lineText(f.editor(), 0), QStringLiteral("- [ ] milk"));
    }

    // Scenario "Enter continues and ends task lists".
    void enterContinuesAndEndsTaskLists() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "  * [x] done\n")));
        f.placeCaret(0, 12);
        test::keyClick(f.editor(), Qt::Key_Return);
        QCOMPARE(lineText(f.editor(), 1), QStringLiteral("  * [ ] "));
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 8);
        test::keyClick(f.editor(), Qt::Key_Return);
        QCOMPARE(lineText(f.editor(), 1), QStringLiteral("* [ ] "));
        test::keyClick(f.editor(), Qt::Key_Return);
        QCOMPARE(lineText(f.editor(), 1), QString());
        QCOMPARE(f.editor()->document()->blockCount(), 3);
    }

    // Scenario "checked text reads as done".
    void checkedTextReadsAsDone() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "- [x] bread\n- [ ] milk\nother\n")));
        f.placeCaret(2);
        f.settle();
        const core::TaskStyle style = core::taskStyle(core::builtinTheme(core::ColorScheme::Light));
        const QTextCharFormat done = test::formatAt(f.editor()->document()->findBlockByNumber(0), 7);
        QCOMPARE(done.foreground().color(), test::qcolor(style.doneText));
        QVERIFY(done.fontStrikeOut());
        const QTextCharFormat open = test::formatAt(f.editor()->document()->findBlockByNumber(1), 7);
        QVERIFY(!open.fontStrikeOut());
        QVERIFY(open.foreground().color() != test::qcolor(style.doneText));
    }

    // Scenario "wrapped task items".
    void wrappedLinesAlignWithTheText() {
        test::Fixture f;
        const auto line = QByteArrayLiteral(
            "- [ ] Task with a very very very very very very very very very very very very very very very very very "
            "very very very very very long text that wraps\nother\n");
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrap.md"), line)));
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        const QTextLayout* layout = block.layout();
        QVERIFY(layout->lineCount() >= 2);
        QCOMPARE(layout->lineAt(1).x(), layout->lineAt(0).cursorToX(6));
        // The text starts after the box.
        QTextCursor text(block);
        text.setPosition(6);
        QVERIFY(f.editor()->cursorRect(text).left() >= f.editor()->checkboxRect(0).right());
    }
};

QTEST_MAIN(tst_s051_task_checkboxes)
#include "tst_s051_task_checkboxes.moc"
