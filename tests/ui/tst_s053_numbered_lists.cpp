// SPDX-License-Identifier: GPL-3.0-or-later
// S053: the number of an ordered item is drawn in its own tokens; Enter, Tab and Shift+Tab keep the
// numbers of a list in order, each key one undo step.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
QColor themeColor(const core::Color& color) {
    return QColor(QString::fromStdString(color.toHex()));
}

const core::OrderedListStyle& lightStyle() {
    static const core::OrderedListStyle style = core::orderedListStyle(core::builtinTheme(core::ColorScheme::Light));
    return style;
}
}  // namespace

class tst_s053_numbered_lists : public QObject {
    Q_OBJECT
private slots:
    void numberInItsColourWhenConcealedAndInTheMarkerColourWhenRevealed() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("n.md"), "3. milk\nother\n")));
        f.placeCaret(1);
        const QTextBlock item = f.editor()->document()->firstBlock();
        QCOMPARE(test::formatAt(item, 0).foreground().color(), themeColor(lightStyle().text));
        QCOMPARE(test::formatAt(item, 1).foreground().color(), themeColor(lightStyle().text));
        QCOMPARE(test::formatAt(item, 0).fontWeight(), lightStyle().weight);
        QVERIFY(!test::isCollapsed(item, 0));
        f.placeCaret(0, 5);
        QCOMPARE(test::formatAt(item, 0).foreground().color(), themeColor(lightStyle().marker));
    }

    void textDoesNotMoveWhenRevealed() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("n.md"), "12. milk\nother\n")));
        f.placeCaret(1);
        QTextCursor c(f.editor()->document()->firstBlock());
        c.setPosition(4);
        const int concealedX = f.editor()->cursorRect(c).left();
        f.placeCaret(0, 7);
        QCOMPARE(f.editor()->cursorRect(c).left(), concealedX);
    }

    void enterContinuesAndRenumbersInOneUndoStep() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("e.md"), "1. a\n2. b\n3. c\n")));
        f.placeCaret(0, 4);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n2. \n3. b\n4. c\n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.blockNumber(), 1);
        QCOMPARE(c.positionInBlock(), 3);
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n2. b\n3. c\n"));
    }

    void enterKeepsStartAndDelimiter() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("e.md"), "7) a\n")));
        f.placeCaret(0, 4);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("7) a\n8) \n"));
    }

    void enterOnEmptyItemRemovesTheMarkerAndRenumbers() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("e.md"), "1. a\n2. \n3. b\n")));
        f.placeCaret(1, 3);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n\n2. b\n"));
        QCOMPARE(f.editor()->textCursor().blockNumber(), 1);
    }

    void tabNestsAndShiftTabGivesItBack() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("t.md"), "1. a\n2. b\n3. c\n")));
        f.placeCaret(1, 4);
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n   1. b\n2. c\n"));
        QCOMPARE(f.editor()->textCursor().positionInBlock(), 7);
        test::keyClick(f.editor(), Qt::Key_Backtab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n2. b\n3. c\n"));
    }

    void renumberingStaysInsideItsList() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("r.md"), "1. a\n   1. x\n   1. y\n2. b\n- c\n1. d\n")));
        f.placeCaret(0, 4);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("1. a\n2. \n   1. x\n   1. y\n3. b\n- c\n1. d\n"));
    }
};

QTEST_MAIN(tst_s053_numbered_lists)
#include "tst_s053_numbered_lists.moc"
