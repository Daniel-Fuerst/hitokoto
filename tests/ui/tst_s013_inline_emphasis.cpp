// SPDX-License-Identifier: GPL-3.0-or-later
// S013: inline emphasis is styled without altering source text.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s013_inline_emphasis : public QObject {
    Q_OBJECT
private slots:
    void appliesEmphasisStylesAndConcealsDelimiters() {
        constexpr auto markdown =
            "**bold** *italic* _under_ ~~gone~~\n"
            "**bold *italic* bold**\n"
            "***both***\n"
            "~~gone **bold gone**~~\n"
            "plain\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("emphasis.md"), markdown)));
        f.settle();
        f.placeCaret(3);

        const auto first = f.editor()->document()->findBlockByNumber(0);
        QVERIFY(test::isCollapsed(first, 0));
        QVERIFY(test::isCollapsed(first, 1));
        QVERIFY(test::isCollapsed(first, 6));
        QVERIFY(test::isCollapsed(first, 7));
        QCOMPARE(test::formatAt(first, 2).fontWeight(), QFont::Bold);
        QVERIFY(test::formatAt(first, 10).fontItalic());
        QVERIFY(test::formatAt(first, 19).fontItalic());
        QVERIFY(test::formatAt(first, 28).fontStrikeOut());

        const auto nested = f.editor()->document()->findBlockByNumber(1);
        QCOMPARE(test::formatAt(nested, 2).fontWeight(), QFont::Bold);
        QVERIFY(!test::formatAt(nested, 2).fontItalic());
        QCOMPARE(test::formatAt(nested, 8).fontWeight(), QFont::Bold);
        QVERIFY(test::formatAt(nested, 8).fontItalic());
        QCOMPARE(test::formatAt(nested, 16).fontWeight(), QFont::Bold);

        const auto combined = f.editor()->document()->findBlockByNumber(2);
        QCOMPARE(test::formatAt(combined, 3).fontWeight(), QFont::Bold);
        QVERIFY(test::formatAt(combined, 3).fontItalic());

        const QString nestedStrikeText = QStringLiteral("~~gone **bold gone**~~");
        const int nestedBoldPosition = nestedStrikeText.indexOf(QStringLiteral("bold"));
        const auto nestedStrike = f.editor()->document()->findBlockByNumber(3);
        QCOMPARE(test::formatAt(nestedStrike, nestedBoldPosition).fontWeight(), QFont::Bold);
        QVERIFY(test::formatAt(nestedStrike, nestedBoldPosition).fontStrikeOut());
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

    void activeLineRevealsMarkersWithoutRemovingEmphasis() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("emphasis.md"), "**bold** *italic*\nplain\n")));
        f.placeCaret(1);
        const auto block = f.editor()->document()->firstBlock();
        QVERIFY(test::isCollapsed(block, 0));
        f.placeCaret(0, 3);
        QVERIFY(!test::isCollapsed(block, 0));
        QVERIFY(!test::isCollapsed(block, 6));
        QCOMPARE(test::formatAt(block, 2).fontWeight(), QFont::Bold);
        QVERIFY(test::formatAt(block, 10).fontItalic());
    }
};

QTEST_MAIN(tst_s013_inline_emphasis)
#include "tst_s013_inline_emphasis.moc"
