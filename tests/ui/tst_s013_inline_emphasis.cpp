// SPDX-License-Identifier: GPL-3.0-or-later
// S013: inline emphasis is styled without altering source text.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}
}  // namespace

class tst_s013_inline_emphasis : public QObject {
    Q_OBJECT
private slots:
    // Theming baseline (ADR 0004): every emphasis token reaches the formats.
    void themedEmphasisUsesEveryToken() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("emphasis.md"), "**b** *i* ~~s~~\nplain\n")));
        f.settle();
        f.placeCaret(1);
        const auto block = f.editor()->document()->firstBlock();
        QVERIFY2(!test::formatAt(block, 2).hasProperty(QTextFormat::ForegroundBrush),
                 "unset: the surrounding text colour");

        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.emphasis.boldWeight = 600;
        theme.emphasis.boldText = core::Color{0xa0, 0x10, 0x10, 0xff};
        theme.emphasis.italicText = core::Color{0x10, 0xa0, 0x10, 0xff};
        theme.emphasis.strikethroughText = core::Color{0x10, 0x10, 0xa0, 0xff};
        theme.emphasis.marker = core::Color{0x00, 0x00, 0xff, 0xff};
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(test::formatAt(block, 2).fontWeight(), 600);
        QCOMPARE(test::formatAt(block, 2).foreground().color(), qcolor(*theme.emphasis.boldText));
        QCOMPARE(test::formatAt(block, 7).foreground().color(), qcolor(*theme.emphasis.italicText));
        QCOMPARE(test::formatAt(block, 12).foreground().color(), qcolor(*theme.emphasis.strikethroughText));

        f.placeCaret(0);
        f.settle();
        QCOMPARE(test::formatAt(block, 0).foreground().color(), qcolor(*theme.emphasis.marker));
        QCOMPARE(test::formatAt(block, 6).foreground().color(), qcolor(*theme.emphasis.marker));
    }

    void formatsPunctuationAtEndOfEmphasizedLabels() {
        constexpr auto markdown = "**Projekt:** \u2610 Bayernsschach \u2610 Dame \u2610 Clobber\nplain\n";
        test::Fixture f;
        const auto theme = core::builtinTheme(core::ColorScheme::Dark);
        ui::applyThemeToApplication(*qApp, theme);
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("labels.md"), markdown)));
        f.settle();
        f.placeCaret(1);

        const auto label = f.editor()->document()->firstBlock();
        QVERIFY(test::isCollapsed(label, 0));
        QVERIFY(test::isCollapsed(label, 1));
        QVERIFY(test::isCollapsed(label, 10));
        QVERIFY(test::isCollapsed(label, 11));
        QCOMPARE(test::formatAt(label, 2).fontWeight(), QFont::Bold);
        QCOMPARE(f.editor()->toPlainText(), QString::fromUtf8(markdown));
    }

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
