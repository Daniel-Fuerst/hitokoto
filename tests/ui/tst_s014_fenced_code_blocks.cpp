// SPDX-License-Identifier: GPL-3.0-or-later
// S014: fenced code blocks render incrementally without changing Markdown source.
#include "ui_test_support.hpp"

#include <QFontDatabase>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}
QColor pixel(ui::EditorWidget* editor, qreal x, qreal y) {
    return editor->viewport()->grab().toImage().pixelColor(static_cast<int>(x), static_cast<int>(y));
}
}  // namespace

class tst_s014_fenced_code_blocks : public QObject {
    Q_OBJECT
private slots:
    // Theming baseline (ADR 0004): every code-block token reaches the screen.
    void themedCodeBlockUsesEveryToken() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\ncode line\n```\nafter\n")));
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.codeBlock.background = core::Color{0x20, 0x40, 0x60, 0xff};
        theme.codeBlock.text = core::Color{0xa0, 0x10, 0x10, 0xff};
        theme.codeBlock.border = core::Color{0x10, 0xc0, 0x10, 0xff};
        theme.codeBlock.borderWidth = 4;
        theme.codeBlock.radius = 7;
        theme.codeBlock.padding = 12;
        theme.codeBlock.fontScale = 1.5;
        theme.codeBlock.marker = core::Color{0x00, 0x00, 0xff, 0xff};
        f.window->applyTheme(theme);
        f.settle();
        f.placeCaret(0);

        const auto opening = f.editor()->document()->findBlockByNumber(1);
        const auto content = f.editor()->document()->findBlockByNumber(2);
        QCOMPARE(test::formatAt(content, 0).foreground().color(), qcolor(*theme.codeBlock.text));
        QCOMPARE(test::formatAt(content, 0).fontPointSize(), theme.editorFontSize * 1.5);
        QCOMPARE(f.editor()->styler().formats().codeBlock().radius, 7.0);

        const QRect line = f.editor()->cursorRect(QTextCursor(opening));
        const qreal x = f.editor()->viewport()->width() / 2.0;
        QTRY_COMPARE(pixel(f.editor(), x, line.center().y()), qcolor(*theme.codeBlock.background));
        QCOMPARE(pixel(f.editor(), x, line.top() + 2), qcolor(*theme.codeBlock.border));  // in the top border

        // The line directly above the code block is not overlapped by the panel background
        const auto before = f.editor()->document()->findBlockByNumber(0);
        const QRect beforeLine = f.editor()->cursorRect(QTextCursor(before));
        QVERIFY(pixel(f.editor(), x, beforeLine.center().y()) != qcolor(*theme.codeBlock.background));

        f.placeCaret(1);
        f.settle();
        const auto fence = f.editor()->document()->findBlockByNumber(1);
        QCOMPARE(test::formatAt(fence, 0).foreground().color(), qcolor(*theme.codeBlock.marker));
    }

    // A style hint alone keeps the proportional editor family whenever that family exists, so code
    // gets an explicit family: code-block.font-family, else the system's fixed-width font.
    void codeUsesThemeCodeFamilyOrSystemFixedFont() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "```\ncode\n```\n")));
        f.settle();
        const auto content = f.editor()->document()->findBlockByNumber(1);
        QCOMPARE(test::formatAt(content, 0).fontFamilies().toStringList(),
                 QFontDatabase::systemFont(QFontDatabase::FixedFont).families());

        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.editorFontFamily = "Editor Test Family";
        theme.codeBlock.fontFamily = "Code Test Family";
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(test::formatAt(content, 0).fontFamilies().toStringList(),
                 QStringList{QStringLiteral("Code Test Family")});
    }

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
        QVERIFY(!test::isCollapsed(opening, 0));
        QVERIFY(!test::isCollapsed(opening, 5));
        QVERIFY(!test::isCollapsed(closing, 0));
        QCOMPARE(test::formatAt(opening, 0).foreground().color(), Qt::transparent);
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), Qt::transparent);
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
        const auto openingRect = f.editor()->cursorRect(QTextCursor(opening));
        QCOMPARE(f.editor()->viewport()->grab().toImage().pixelColor(25, openingRect.center().y()), panel);
        const auto closingRect = f.editor()->cursorRect(QTextCursor(closing));
        QCOMPARE(f.editor()->viewport()->grab().toImage().pixelColor(25, closingRect.center().y()), panel);

        // When caret is on a code line inside the block, both fences are revealed without geometry change
        f.placeCaret(2);
        f.settle();
        QCOMPARE(f.editor()->cursorRect(QTextCursor(content)), contentRect);
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        QCOMPARE(test::formatAt(opening, 0).foreground().color(), qcolor(theme.marker));
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), qcolor(theme.marker));
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

    void indentedFencedCodeBlockRendersAsCode() {
        constexpr auto markdown =
            "before\n"
            "    ```python\n"
            "    def check_indentation(level: int) -> bool:\n"
            "        return level == 2\n"
            "    ```\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), markdown)));
        f.settle();
        f.placeCaret(0);

        const auto opening = f.editor()->document()->findBlockByNumber(1);
        const auto line1 = f.editor()->document()->findBlockByNumber(2);
        const auto line2 = f.editor()->document()->findBlockByNumber(3);
        const auto closing = f.editor()->document()->findBlockByNumber(4);
        const auto after = f.editor()->document()->findBlockByNumber(5);

        // Fences are concealed without collapsing when caret is outside
        QVERIFY(!test::isCollapsed(opening, 0));
        QVERIFY(!test::isCollapsed(closing, 0));
        QCOMPARE(test::formatAt(opening, 4).foreground().color(), Qt::transparent);
        QCOMPARE(test::formatAt(closing, 4).foreground().color(), Qt::transparent);

        // Content lines are styled as code
        QCOMPARE(test::formatAt(line1, 4).fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(test::formatAt(line2, 8).fontStyleHint(), QFont::TypeWriter);
        QVERIFY(!test::isCollapsed(line1, 0));
        QVERIFY(!test::isCollapsed(line2, 0));

        // After line is plain text
        QVERIFY(test::formatAt(after, 0).fontStyleHint() != QFont::TypeWriter);

        // Raw document text is preserved exactly
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));

        // When caret is on line 3 (return level == 2), both fences are revealed without geometry change
        const auto line2RectBefore = f.editor()->cursorRect(QTextCursor(line2));
        f.placeCaret(3);
        f.settle();
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line2)), line2RectBefore);
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        QCOMPARE(test::formatAt(opening, 4).foreground().color(), qcolor(theme.marker));
        QCOMPARE(test::formatAt(closing, 4).foreground().color(), qcolor(theme.marker));
        QCOMPARE(test::formatAt(opening, 4).fontStyleHint(), QFont::TypeWriter);
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

    // Per-block metadata exists only where a block state has to be kept: lines that leave a code
    // block open, and since S021 the closing fence (the panel margin below it). Plain documents
    // carry none, however long (memory footprint, brief section 4).
    void blockDataOnlyOnLinesThatLeaveACodeBlockOpen() {
        test::Fixture f;
        QByteArray plain;
        for (int i = 0; i < 2000; ++i)
            plain += "# heading\n- item with *emphasis* and `code`\nbody text\n\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("plain.md"), plain)));
        f.settle();
        for (int b = 0; b < f.editor()->document()->blockCount(); b += 997)
            f.placeCaret(b);  // reveal and conceal again
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), QList<int>{});

        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "plain\n# h\n- b\n```\ncode\n```\nafter\n")));
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{3, 4, 5}));

        // Splitting a code line keeps both halves inside the block.
        f.placeCaret(4, 2);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{3, 4, 5, 6}));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("plain\n# h\n- b\n```\nco\nde\n```\nafter\n"));
    }

    void blockDataIsReleasedWhenTheCodeBlockDisappears() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\na\nb\n")));
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{1, 2, 3, 4}));

        f.placeCaret(1);
        test::keyClick(f.editor(), Qt::Key_End);
        test::keyClick(f.editor(), Qt::Key_Backspace);  // "``": no longer a fence
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), QList<int>{});
        QVERIFY(test::formatAt(f.editor()->document()->findBlockByNumber(3), 0).fontStyleHint() != QFont::TypeWriter);

        test::keyClick(f.editor(), Qt::Key_QuoteLeft);  // "```" again
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{1, 2, 3, 4}));
    }

    // Joining lines leaves Qt's per-block state of the last joined line next to the metadata of the
    // first one; restyling must not trust that mix.
    void joiningAFenceIntoThePreviousLineRestylesFollowingLines() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "x\n```\na\nb\n")));
        f.settle();
        const auto isCode = [&](int block) {
            return test::formatAt(f.editor()->document()->findBlockByNumber(block), 0).fontStyleHint() ==
                   QFont::TypeWriter;
        };
        QVERIFY(isCode(2) && isCode(3));

        f.placeCaret(1);
        test::keyClick(f.editor(), Qt::Key_Backspace);  // "x```": not a fence
        f.settle();
        QVERIFY(!isCode(1) && !isCode(2));
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), QList<int>{});

        f.editor()->undo();
        f.settle();
        QVERIFY(isCode(2) && isCode(3));
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{1, 2, 3, 4}));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("x\n```\na\nb\n"));
    }
};

QTEST_MAIN(tst_s014_fenced_code_blocks)
#include "tst_s014_fenced_code_blocks.moc"
