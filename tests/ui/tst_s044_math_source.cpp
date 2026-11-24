// SPDX-License-Identifier: GPL-3.0-or-later
// S044: formula source is styled from `[math]`, its dollars concealed off the caret's line, and the
// text saved byte for byte.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
using test::qcolor;

core::Theme mathTheme() {
    auto theme = core::builtinTheme(core::ColorScheme::Light);
    theme.math.text = core::Color{0x12, 0x34, 0x56, 0xff};
    theme.math.marker = core::Color{0xab, 0x12, 0xcd, 0xff};
    theme.math.fontScale = 1.5;
    return theme;
}
}  // namespace

class tst_s044_math_source : public QObject {
    Q_OBJECT
private slots:
    void inlineFormulaIsStyledAndItsDollarsConcealed() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("math.md"), "a $x^2$ b\nsecond\n")));
        const auto theme = mathTheme();
        f.window->applyTheme(theme);
        f.placeCaret(1);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        QVERIFY(test::isCollapsed(block0, 2));
        QVERIFY(test::isCollapsed(block0, 6));
        QVERIFY(!test::isCollapsed(block0, 3));
        const auto format = test::formatAt(block0, 3);
        QCOMPARE(format.foreground().color(), qcolor(*theme.math.text));
        QCOMPARE(format.fontPointSize(), theme.editorFontSize * 1.5);
    }

    void revealedDollarsUseTheMarkerColour() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("math.md"), "a $x^2$ b\nsecond\n")));
        const auto theme = mathTheme();
        f.window->applyTheme(theme);
        f.placeCaret(0);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        QVERIFY(!test::isCollapsed(block0, 2));
        QCOMPARE(test::formatAt(block0, 2).foreground().color(), qcolor(*theme.math.marker));
        QCOMPARE(test::formatAt(block0, 6).foreground().color(), qcolor(*theme.math.marker));
    }

    void displayBlockLinesAreMath() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("math.md"), "$$\n\\frac{a}{b}\n$$\nafter\n")));
        const auto theme = mathTheme();
        f.window->applyTheme(theme);
        f.placeCaret(3);
        f.settle();

        const auto block1 = f.editor()->document()->findBlockByNumber(1);
        QCOMPARE(test::formatAt(block1, 0).foreground().color(), qcolor(*theme.math.text));
        const auto block0 = f.editor()->document()->firstBlock();
        QCOMPARE(test::formatAt(block0, 0).foreground().color(), QColor(Qt::transparent));
    }

    void theSampleSavesByteForByte() {
        QFile sample(QStringLiteral(HITOKOTO_SOURCE_DIR "/tests/data/s044-latex.md"));
        QVERIFY(sample.open(QIODevice::ReadOnly));
        const QByteArray content = sample.readAll();
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("latex.md"), content)));
        f.settle();
        const QString savePath = f.dir.filePath(QStringLiteral("saved.md"));
        QVERIFY(f.saveAndWait(savePath));
        QCOMPARE(test::Fixture::readFile(savePath), content);
    }
};

QTEST_MAIN(tst_s044_math_source)
#include "tst_s044_math_source.moc"
