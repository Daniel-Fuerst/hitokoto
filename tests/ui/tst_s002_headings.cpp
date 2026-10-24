// SPDX-License-Identifier: GPL-3.0-or-later
// S002: headings render large; markers are hidden unless the caret or selection is on the line.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s002_headings : public QObject {
    Q_OBJECT
private slots:
    void concealedWhenCaretElsewhere() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("h.md"), "## Plans\nbody\n")));
        f.placeCaret(1);
        const QTextBlock heading = f.editor()->document()->firstBlock();
        QVERIFY(test::isCollapsed(heading, 0));
        QVERIFY(test::isCollapsed(heading, 2));
        QVERIFY(!test::isCollapsed(heading, 3));
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        QCOMPARE(test::formatAt(heading, 3).fontPointSize(), theme.editorFontSize * theme.headingScale[1]);
        QCOMPARE(test::formatAt(heading, 3).fontWeight(), theme.headingWeight);
        // Rendered heading line is taller than a body line.
        QTextCursor inHeading(heading);
        inHeading.setPosition(4);
        QTextCursor inBody(heading.next());
        QVERIFY(f.editor()->cursorRect(inHeading).height() > f.editor()->cursorRect(inBody).height());
    }

    void revealedWhenCaretOnLineAndConcealedAgainAfter() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("h.md"), "body\n## Plans\nmore\n")));
        f.placeCaret(2);
        const QTextBlock heading = f.editor()->document()->findBlockByNumber(1);
        QVERIFY(test::isCollapsed(heading, 0));
        f.placeCaret(1, 5);
        QVERIFY(!test::isCollapsed(heading, 0));
        QCOMPARE(test::formatAt(heading, 0).foreground().color(),
                 QColor(QString::fromStdString(core::builtinTheme(core::ColorScheme::Light).marker.toHex())));
        QTest::keyClick(f.editor(), Qt::Key_Down);
        QVERIFY(test::isCollapsed(heading, 0));
    }

    void selectionRevealsEveryTouchedLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("h.md"), "# a\n# b\n# c\n# d\n")));
        f.placeCaret(0, 2);
        QTextCursor c = f.editor()->textCursor();
        c.setPosition(f.editor()->document()->findBlockByNumber(2).position() + 1, QTextCursor::KeepAnchor);
        f.editor()->setTextCursor(c);
        auto* doc = f.editor()->document();
        QVERIFY(!test::isCollapsed(doc->findBlockByNumber(0), 0));
        QVERIFY(!test::isCollapsed(doc->findBlockByNumber(1), 0));
        QVERIFY(!test::isCollapsed(doc->findBlockByNumber(2), 0));
        QVERIFY(test::isCollapsed(doc->findBlockByNumber(3), 0));
    }

    void typingAHeadingRendersItImmediately() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("### Hi"));
        QTest::keyClick(f.editor(), Qt::Key_Return);
        const QTextBlock heading = f.editor()->document()->firstBlock();
        QVERIFY(test::isCollapsed(heading, 0));
        QVERIFY(test::formatAt(heading, 4).fontPointSize() > 0);
    }

    void onlyAffectedLinesAreRestyled() {
        test::Fixture f;
        QByteArray big;
        for (int i = 0; i < 20000; ++i)
            big += (i % 10 == 0) ? "# heading\n" : "body text line\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), big)));
        f.settle();
        f.placeCaret(100);
        auto& styler = f.editor()->styler();
        const auto before = styler.styledCount();
        f.placeCaret(101);
        f.placeCaret(102);
        QTest::keyClicks(f.editor(), QStringLiteral("abc"));
        QVERIFY2(styler.styledCount() - before <= 10, qPrintable(QString::number(styler.styledCount() - before)));
    }
};

QTEST_MAIN(tst_s002_headings)
#include "tst_s002_headings.moc"
