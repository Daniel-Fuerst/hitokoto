// SPDX-License-Identifier: GPL-3.0-or-later
// S014: fenced code blocks render incrementally without changing Markdown source.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s014_fenced_code_blocks : public QObject {
    Q_OBJECT
private slots:
    void stylesFenceAndContentWhileKeepingSource() {
        constexpr auto markdown = "before\n```cpp\n# **literal** - item\n\nreturn;\n```\nafter\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), markdown)));
        f.settle();
        f.placeCaret(0);

        const auto opening = f.editor()->document()->findBlockByNumber(1);
        const auto content = f.editor()->document()->findBlockByNumber(2);
        const auto emptyCodeLine = f.editor()->document()->findBlockByNumber(3);
        const auto codeLine = f.editor()->document()->findBlockByNumber(4);
        const auto closing = f.editor()->document()->findBlockByNumber(5);
        QVERIFY(test::isCollapsed(opening, 0));
        QVERIFY(test::isCollapsed(opening, 5));
        QVERIFY(test::isCollapsed(closing, 0));
        QCOMPARE(test::formatAt(content, 0).fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(test::formatAt(content, 4).fontWeight(), QFont::Normal);
        const auto surface = core::builtinTheme(core::ColorScheme::Light).surface;
        const auto cursor = QTextCursor(content);
        const auto contentRect = f.editor()->cursorRect(cursor);
        const int y = contentRect.center().y();
        const QColor panel(surface.r, surface.g, surface.b, surface.a);
        QTRY_VERIFY(f.editor()->viewport()->grab().toImage().pixelColor(25, y) == panel);
        const auto emptyLineY = f.editor()->cursorRect(QTextCursor(emptyCodeLine)).center().y();
        QCOMPARE(f.editor()->viewport()->grab().toImage().pixelColor(25, emptyLineY), panel);
        QCOMPARE(test::formatAt(codeLine, 0).fontStyleHint(), QFont::TypeWriter);
        const int topPaddingY = y - contentRect.height() / 2 - 3;
        QCOMPARE(f.editor()->viewport()->grab().toImage().pixelColor(50, topPaddingY), panel);
        QVERIFY(!test::isCollapsed(content, 4));
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

    void revealsFenceOnActiveLineWithoutChangingCodeStyle() {
        constexpr auto markdown = "```` c++\nint value;\n````\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), markdown)));
        f.settle();
        f.placeCaret(0);

        const auto opening = f.editor()->document()->firstBlock();
        const auto content = opening.next();
        QVERIFY(!test::isCollapsed(opening, 0));
        QCOMPARE(test::formatAt(opening, 0).fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(test::formatAt(content, 0).fontStyleHint(), QFont::TypeWriter);

        f.placeCaret(2);
        QVERIFY(!test::isCollapsed(opening.next().next(), 0));
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

    void removingOpeningFenceRestylesFollowingLines() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\n**text**\nafter\n")));
        f.settle();
        auto opening = f.editor()->document()->findBlockByNumber(1);
        auto content = f.editor()->document()->findBlockByNumber(2);
        QCOMPARE(test::formatAt(content, 2).fontStyleHint(), QFont::TypeWriter);

        QTextCursor cursor(opening);
        cursor.setPosition(opening.position());
        cursor.setPosition(opening.position() + opening.length() - 1, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("plain"));
        f.settle();

        content = f.editor()->document()->findBlockByNumber(2);
        QVERIFY(test::isCollapsed(content, 0));
        QCOMPARE(test::formatAt(content, 2).fontWeight(), QFont::Bold);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("before\nplain\n**text**\nafter\n"));
    }

    // An edit that keeps the length (select one backtick, type a letter) must still restyle every
    // line whose block membership changes, in both directions.
    void sameLengthFenceEditRestylesFollowingLines() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "```\na\nb\nc\nd\n")));
        f.settle();
        f.placeCaret(5);
        const auto isCode = [&](int block) {
            return test::formatAt(f.editor()->document()->findBlockByNumber(block), 0).fontStyleHint() ==
                   QFont::TypeWriter;
        };
        for (int b = 1; b <= 4; ++b)
            QVERIFY(isCode(b));

        QTextCursor cursor(f.editor()->document()->firstBlock());
        cursor.setPosition(1, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("x"));  // "x``": no longer a fence
        f.settle();
        for (int b = 1; b <= 4; ++b)
            QVERIFY2(!isCode(b), qPrintable(QStringLiteral("block %1 still styled as code").arg(b)));

        cursor.setPosition(0);
        cursor.setPosition(1, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("`"));  // "```" again
        f.settle();
        for (int b = 1; b <= 4; ++b)
            QVERIFY2(isCode(b), qPrintable(QStringLiteral("block %1 not styled as code").arg(b)));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("```\na\nb\nc\nd\n"));
    }
};

QTEST_MAIN(tst_s014_fenced_code_blocks)
#include "tst_s014_fenced_code_blocks.moc"
