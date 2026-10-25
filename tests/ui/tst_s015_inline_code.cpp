// SPDX-License-Identifier: GPL-3.0-or-later
// S015: inline code spans are styled without altering the Markdown document.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s015_inline_code : public QObject {
    Q_OBJECT
private slots:
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
        const auto surface = core::builtinTheme(core::ColorScheme::Light).surface;
        QCOMPARE(test::formatAt(first, 17).background().color(), QColor(surface.r, surface.g, surface.b, surface.a));
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
