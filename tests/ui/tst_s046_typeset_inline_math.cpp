// SPDX-License-Identifier: GPL-3.0-or-later
// S046: inline formulas are drawn typeset while the caret is off their line: the source keeps its
// characters but only the first takes width (the formula's), the editor paints the formula there.
#include "ui_test_support.hpp"

#include <QScrollBar>
#include <QTextLayout>

using namespace hitokoto;

namespace {
using test::qcolor;

/// The x of the cursor before `column` of block `blockNumber`, in viewport coordinates.
qreal xAt(ui::EditorWidget* editor, int blockNumber, int column) {
    QTextCursor c(editor->document()->findBlockByNumber(blockNumber));
    c.setPosition(c.block().position() + column);
    return editor->cursorRect(c).left();
}

/// Pixels in `rect` of the viewport whose colour is near `color` (antialiased glyph edges aside).
int pixelsNear(ui::EditorWidget* editor, const QRect& rect, const QColor& color) {
    const QImage image = editor->viewport()->grab().toImage();
    const qreal dpr = image.devicePixelRatio();
    int count = 0;
    for (int y = static_cast<int>(rect.top() * dpr); y < static_cast<int>(rect.bottom() * dpr); ++y) {
        for (int x = static_cast<int>(rect.left() * dpr); x < static_cast<int>(rect.right() * dpr); ++x) {
            if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
                continue;
            }
            const QColor p = image.pixelColor(x, y);
            if (std::abs(p.red() - color.red()) < 40 && std::abs(p.green() - color.green()) < 40 &&
                std::abs(p.blue() - color.blue()) < 40) {
                ++count;
            }
        }
    }
    return count;
}

constexpr core::Color kMathText{0xff, 0x00, 0x00, 0xff};
constexpr core::Color kMathError{0x00, 0x80, 0x00, 0xff};

core::Theme redMath() {
    auto theme = core::builtinTheme(core::ColorScheme::Light);
    theme.math.text = kMathText;
    theme.math.error = kMathError;
    return theme;
}

/// The line box of block `blockNumber` in viewport coordinates.
QRect lineRect(ui::EditorWidget* editor, int blockNumber) {
    QTextCursor c(editor->document()->findBlockByNumber(blockNumber));
    const QRect r = editor->cursorRect(c);
    return {0, r.top(), editor->viewport()->width(), r.height()};
}

}  // namespace

class tst_s046_typeset_inline_math : public QObject {
    Q_OBJECT
private slots:
    void anInlineFormulaIsTypesetOffTheCaretLine_data() {
        QTest::addColumn<QString>("family");
        QTest::addColumn<double>("size");
        QTest::newRow("theme font") << QString() << 0.0;
        QTest::newRow("DejaVu Sans 11") << QStringLiteral("DejaVu Sans") << 11.0;
        QTest::newRow("Liberation Serif 10.5") << QStringLiteral("Liberation Serif") << 10.5;
        QTest::newRow("DejaVu Sans Mono 13") << QStringLiteral("DejaVu Sans Mono") << 13.0;
    }

    void anInlineFormulaIsTypesetOffTheCaretLine() {
        QFETCH(QString, family);
        QFETCH(double, size);
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "a $x^2$ b\nsecond\n")));
        auto theme = redMath();
        if (!family.isEmpty()) {
            theme.editorFontFamily = family.toStdString();
            theme.editorFontSize = size;
        }
        f.window->applyTheme(theme);
        f.placeCaret(1);
        f.settle();
        auto* editor = f.editor();
        const QTextBlock block = editor->document()->firstBlock();
        // The source keeps its characters; after the first, they take no width.
        QVERIFY(test::formatAt(block, 3).fontLetterSpacing() > 0);
        QVERIFY(test::isCollapsed(block, 4));
        const core::MathLayout& formula = editor->styler().formats().mathLayout(u"x^2", 0, false);
        QVERIFY(!formula.error);
        QVERIFY(formula.width > 0);
        // From the start of the source to the closing dollar: the formula's width.
        const qreal reserved = xAt(editor, 0, 6) - xAt(editor, 0, 3);
        QVERIFY2(std::abs(reserved - formula.width) < 2.0,
                 qPrintable(QStringLiteral("reserved %1, formula %2").arg(reserved).arg(formula.width)));
        // The formula is painted in math.text between "a" and "b".
        const QRect area(static_cast<int>(xAt(editor, 0, 3)), lineRect(editor, 0).top(),
                         static_cast<int>(formula.width) + 1, lineRect(editor, 0).height());
        QVERIFY(pixelsNear(editor, area, qcolor(kMathText)) > 5);

        if (const QByteArray out = qgetenv("HITOKOTO_MATH_PREVIEW"); !out.isEmpty()) {
            QVERIFY(editor->viewport()->grab().save(QString::fromLocal8Bit(out)));
        }
    }

    void theCaretOnTheLineShowsTheSource() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "a $x^2$ b\nsecond\n")));
        f.window->applyTheme(redMath());
        f.placeCaret(0);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        QCOMPARE(test::formatAt(block, 3).fontLetterSpacing(), 0.0);
        QVERIFY(!test::isCollapsed(block, 4));
        QVERIFY2(!test::isCollapsed(block, 2), "the dollar shows");
    }

    void aTallFormulaMakesRoom() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "plain\n$\\frac{a}{b}$ and $\\sum_{k=1}^n k$\nend\n")));
        f.placeCaret(2);
        f.settle();
        auto* editor = f.editor();
        const qreal plain = editor->document()->findBlockByNumber(0).layout()->boundingRect().height();
        const QTextBlock tall = editor->document()->findBlockByNumber(1);
        const core::MathLayout& frac = editor->styler().formats().mathLayout(u"\\frac{a}{b}", 0, false);
        QVERIFY(frac.ascent + frac.descent > plain * 0.9);
        const QTextLine line = tall.layout()->lineAt(0);
        QVERIFY(line.ascent() >= frac.ascent - 0.5);
        QVERIFY(line.descent() >= frac.descent - 0.5);
        QVERIFY(tall.layout()->boundingRect().height() > plain);
    }

    void aFormulaThatCannotBeTypesetShowsItsSourceInTheErrorColour() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "x $\\frac{a$ y\nsecond\n")));
        const auto theme = redMath();
        f.window->applyTheme(theme);
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        QVERIFY2(test::isCollapsed(block, 2), "the dollars stay hidden");
        QVERIFY(!test::isCollapsed(block, 4));
        const QTextCharFormat format = test::formatAt(block, 4);
        QCOMPARE(format.foreground().color(), qcolor(kMathError));
        QVERIFY(test::shownText(format.toolTip()).contains(QStringLiteral("missing }")));
        QCOMPARE(format.fontLetterSpacing(), 0.0);
    }

    void typingElsewhereLaysOutNoFormula() {
        test::Fixture f;
        QFile sample(QStringLiteral(HITOKOTO_SOURCE_DIR "/tests/data/s044-latex.md"));
        QVERIFY(sample.open(QIODevice::ReadOnly));
        QByteArray content = sample.readAll();
        content.append("\nlast line\n");
        QVERIFY(f.open(f.writeFile(QStringLiteral("latex.md"), content)));
        auto* editor = f.editor();
        f.placeCaret(editor->document()->blockCount() - 2);
        f.settle();
        const std::size_t before = editor->styler().formats().mathCache().layoutsMade();
        QVERIFY2(before >= 4, "the inline formulas of the sample are laid out");
        // A picture of the sample in the editor: HITOKOTO_MATH_SAMPLE_PREVIEW=<file.png> writes it.
        if (const QByteArray out = qgetenv("HITOKOTO_MATH_SAMPLE_PREVIEW"); !out.isEmpty()) {
            f.window->resize(1100, 1000);
            f.settle();
            editor->verticalScrollBar()->setValue(0);
            f.settle();
            QVERIFY(editor->viewport()->grab().save(QString::fromLocal8Bit(out)));
        }
        test::keyClicks(editor, QStringLiteral("typing"));
        f.settle();
        QCOMPARE(editor->styler().formats().mathCache().layoutsMade(), before);
    }

    void clickingAFormulaPutsTheCaretIntoItsSource() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "a $\\alpha + \\beta$ b\nsecond\n")));
        f.placeCaret(1);
        f.settle();
        auto* editor = f.editor();
        const qreal left = xAt(editor, 0, 3);
        const qreal right = xAt(editor, 0, 17);
        const QPoint target(static_cast<int>((left + right) / 2), lineRect(editor, 0).center().y());
        QTest::mouseClick(editor->viewport(), Qt::LeftButton, {}, target);
        f.settle();
        const QTextCursor caret = editor->textCursor();
        QCOMPARE(caret.blockNumber(), 0);
        QVERIFY(caret.positionInBlock() >= 2 && caret.positionInBlock() <= 18);
        QVERIFY2(!test::isCollapsed(editor->document()->firstBlock(), 2), "the line is revealed");
    }

    void aFindHighlightMarksTheWholeFormula() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("m.md"), "first\na $x^2 + y^2$ b\nthird\n")));
        auto* editor = f.editor();
        f.settle();
        // A selection over lines reveals them (S002), so a formula under the caret's own selection
        // shows its source; a find highlight touches a formula on a concealed line.
        editor->setTextCursor(QTextCursor(editor->document()->findBlockByNumber(2)));
        QTextEdit::ExtraSelection mark;
        mark.cursor = QTextCursor(editor->document());
        mark.cursor.setPosition(editor->document()->findBlockByNumber(1).position() + 3);
        mark.cursor.setPosition(editor->document()->findBlockByNumber(1).position() + 4, QTextCursor::KeepAnchor);
        const QColor background(0, 0, 255);
        mark.format.setBackground(background);
        editor->setExtraSelections({mark});
        f.settle();
        editor->viewport()->update();
        QCoreApplication::processEvents();
        const qreal left = xAt(editor, 1, 3);
        const core::MathLayout& formula = editor->styler().formats().mathLayout(u"x^2 + y^2", 0, false);
        const QRect area(static_cast<int>(left), lineRect(editor, 1).top(), static_cast<int>(formula.width),
                         lineRect(editor, 1).height());
        QVERIFY(pixelsNear(editor, area, background) > area.width() * area.height() / 3);
    }
};

QTEST_MAIN(tst_s046_typeset_inline_math)
#include "tst_s046_typeset_inline_math.moc"
