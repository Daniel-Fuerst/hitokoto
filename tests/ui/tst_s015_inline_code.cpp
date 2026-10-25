// SPDX-License-Identifier: GPL-3.0-or-later
// S015: inline code spans are styled without altering the Markdown document.
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

class tst_s015_inline_code : public QObject {
    Q_OBJECT
private slots:
    // Theming baseline (ADR 0004): every inline-code token reaches the screen.
    void themedInlineCodeUsesEveryToken() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline-code.md"), "x `code` y\nplain\n")));
        f.placeCaret(1);
        const auto block = f.editor()->document()->firstBlock();
        const auto cursorAt = [&](int column) {
            QTextCursor c(block);
            c.setPosition(block.position() + column);
            return f.editor()->cursorRect(c);
        };
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.inlineCode.padding = 0;
        f.window->applyTheme(theme);
        f.settle();
        const int unpadded = cursorAt(9).x();
        theme.inlineCode.padding = 10;
        f.window->applyTheme(theme);
        f.settle();
        QVERIFY2(std::abs(cursorAt(9).x() - unpadded - 20) <= 2, "padding on both sides moves the following text");

        theme.inlineCode.background = core::Color{0x20, 0x40, 0x60, 0xff};
        theme.inlineCode.text = core::Color{0xa0, 0x10, 0x10, 0xff};
        theme.inlineCode.border = core::Color{0x10, 0xc0, 0x10, 0xff};
        theme.inlineCode.radius = 0;
        theme.inlineCode.fontScale = 1.5;
        theme.inlineCode.marker = core::Color{0x00, 0x00, 0xff, 0xff};
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(test::formatAt(block, 3).foreground().color(), qcolor(*theme.inlineCode.text));
        QCOMPARE(test::formatAt(block, 3).fontPointSize(), theme.editorFontSize * 1.5);
        const qreal y = cursorAt(3).center().y();
        QTRY_COMPARE(pixel(f.editor(), cursorAt(2).x() + 5, y), qcolor(*theme.inlineCode.background));

        theme.inlineCode.borderWidth = 4;
        f.window->applyTheme(theme);
        f.settle();
        QTRY_COMPARE(pixel(f.editor(), cursorAt(2).x() + 2, y), qcolor(*theme.inlineCode.border));

        f.placeCaret(0);
        f.settle();
        QCOMPARE(test::formatAt(block, 2).foreground().color(), qcolor(*theme.inlineCode.marker));
        QCOMPARE(test::formatAt(block, 7).foreground().color(), qcolor(*theme.inlineCode.marker));
    }

    void inlineCodeUsesThemeCodeFamilyOrSystemFixedFont() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline-code.md"), "text `code` text\n")));
        f.settle();
        const auto block = f.editor()->document()->firstBlock();
        QCOMPARE(test::formatAt(block, 6).fontFamilies().toStringList(),
                 QFontDatabase::systemFont(QFontDatabase::FixedFont).families());
        QVERIFY2(test::formatAt(block, 1).fontFamilies().toStringList().isEmpty(), "body text keeps the editor font");

        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.codeBlock.fontFamily = "Block Test Family";
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(test::formatAt(block, 6).fontFamilies().toStringList(),
                 QStringList{QStringLiteral("Block Test Family")});  // falls back to code-block.font-family
        theme.inlineCode.fontFamily = "Code Test Family";
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(test::formatAt(block, 6).fontFamilies().toStringList(),
                 QStringList{QStringLiteral("Code Test Family")});
    }

    void stylesInlineCodeAndConcealsDelimiters() {
        constexpr auto markdown = "Teammitglieder: `__________________________`\nDatum: `______________`\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline-code.md"), markdown)));
        f.settle();
        f.placeCaret(2);

        const auto first = f.editor()->document()->firstBlock();
        const auto second = first.next();
        QVERIFY(test::isCollapsed(first, 16));
        QVERIFY(test::isCollapsed(first, first.text().size() - 1));
        QCOMPARE(test::formatAt(first, 17).fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(test::formatAt(second, 8).fontStyleHint(), QFont::TypeWriter);
        QVERIFY2(!test::formatAt(first, 17).hasProperty(QTextFormat::BackgroundBrush),
                 "the editor paints the inline-code box; a character format cannot round or pad it");
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

    void activeLineRevealsBackticksAndKeepsCodeFormatting() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline-code.md"), "`**literal**`\n")));
        f.settle();
        f.placeCaret(0);
        const auto block = f.editor()->document()->firstBlock();
        QVERIFY(!test::isCollapsed(block, 0));
        QCOMPARE(test::formatAt(block, 1).fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(test::formatAt(block, 3).fontWeight(), QFont::Normal);
    }

    void inlineCodeInHeadingKeepsHeadingSize() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("inline-code.md"), "# `heading code`\n")));
        f.settle();
        const auto block = f.editor()->document()->firstBlock();
        const auto heading = test::formatAt(block, 2).fontPointSize();
        const auto code = test::formatAt(block, 4);
        QCOMPARE(code.fontStyleHint(), QFont::TypeWriter);
        QCOMPARE(code.fontPointSize(), heading);
    }
};

QTEST_MAIN(tst_s015_inline_code)
#include "tst_s015_inline_code.moc"
