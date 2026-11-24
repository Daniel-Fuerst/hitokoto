// SPDX-License-Identifier: GPL-3.0-or-later
// S047: a closed `$$` block off the caret is one typeset formula: its opening line reserves the
// panel's height, its other lines are hidden, the editor paints the panel; the caret in it shows
// the source of every line.
#include "math_block_embedder.hpp"
#include "ui_test_support.hpp"

#include <QScrollBar>

using namespace hitokoto;

namespace {
using test::qcolor;

constexpr auto kBlock = "above\n$$\n\\frac{a}{b}\n$$\nbelow\n";

ui::BlockLayout* layoutOf(ui::EditorWidget* editor) {
    return qobject_cast<ui::BlockLayout*>(editor->document()->documentLayout());
}

QTextBlock blockAt(ui::EditorWidget* editor, int number) {
    return editor->document()->findBlockByNumber(number);
}

/// The styler's lazy work and the embedder's queued refresh.
void settleAll(const test::Fixture& f) {
    f.settle();
    QCoreApplication::processEvents();
    f.settle();
}

/// The rectangle of a block in viewport coordinates.
QRectF viewportRect(ui::EditorWidget* editor, const QTextBlock& block) {
    return layoutOf(editor)->blockBoundingRect(block).translated(-editor->horizontalScrollBar()->value(),
                                                                 -editor->verticalScrollBar()->value());
}

bool typesetAt(ui::EditorWidget* editor, int open, int last) {
    auto* layout = layoutOf(editor);
    if (layout->reservedHeight(blockAt(editor, open)) <= 0) {
        return false;
    }
    for (int n = open + 1; n <= last; ++n) {
        if (blockAt(editor, n).isVisible()) {
            return false;
        }
    }
    return true;
}

bool sourceAt(ui::EditorWidget* editor, int open, int last) {
    auto* layout = layoutOf(editor);
    for (int n = open; n <= last; ++n) {
        if (!blockAt(editor, n).isVisible() || layout->reservedHeight(blockAt(editor, n)) > 0) {
            return false;
        }
    }
    return true;
}

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

constexpr core::Color kText{0xff, 0x00, 0x00, 0xff};
constexpr core::Color kPanel{0x20, 0xc0, 0x20, 0xff};

}  // namespace

class tst_s047_typeset_display_math : public QObject {
    Q_OBJECT
private slots:
    void aClosedBlockIsTypesetInItsOpeningLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), kBlock)));
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.math.text = kText;
        theme.math.displayBackground = kPanel;
        f.window->applyTheme(theme);
        f.placeCaret(4);
        settleAll(f);
        auto* editor = f.editor();
        QVERIFY(typesetAt(editor, 1, 3));
        const auto& embedder = editor->mathBlocks();
        QCOMPARE(embedder.typeset().size(), std::size_t{1});
        const double height = embedder.panelHeight(u"\\frac{a}{b}\n\n");
        QCOMPARE(layoutOf(editor)->reservedHeight(blockAt(editor, 1)), height);
        // The panel in its colour and the formula in math.text.
        const QRect panel = viewportRect(editor, blockAt(editor, 1)).toRect().adjusted(0, 4, 0, -4);
        QVERIFY(pixelsNear(editor, panel, qcolor(kPanel)) > panel.width() * panel.height() / 2);
        QVERIFY(pixelsNear(editor, panel, qcolor(kText)) > 10);
        QVERIFY2(test::differenceFromTheReference(editor).isEmpty(),
                 qPrintable(test::differenceFromTheReference(editor)));
    }

    void theCaretInTheBlockShowsTheSourceOfEveryLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), kBlock)));
        f.placeCaret(4);
        settleAll(f);
        auto* editor = f.editor();
        QVERIFY(typesetAt(editor, 1, 3));
        f.placeCaret(2);
        settleAll(f);
        QVERIFY(sourceAt(editor, 1, 3));
        QVERIFY2(!test::isCollapsed(blockAt(editor, 1), 0), "the opening dollars show");
        QVERIFY2(!test::isCollapsed(blockAt(editor, 3), 0), "the closing dollars show");
        QVERIFY2(test::differenceFromTheReference(editor).isEmpty(),
                 qPrintable(test::differenceFromTheReference(editor)));
        f.placeCaret(0);
        settleAll(f);
        QVERIFY2(typesetAt(editor, 1, 3), "typeset again once the caret leaves");
    }

    void anEditAboveMovesTheClaimWithTheBlockBeforeTheRefresh() {
        // S044 code review O5: between an edit and the queued refresh, the claim follows the
        // block, not the number it had.
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), kBlock)));
        f.placeCaret(4);
        settleAll(f);
        auto* editor = f.editor();
        QVERIFY(typesetAt(editor, 1, 3));
        const auto& embedder = editor->mathBlocks();
        QVERIFY(embedder.claims(blockAt(editor, 1)));
        QVERIFY(!embedder.claims(blockAt(editor, 0)));
        QTextCursor top(editor->document());
        top.insertText(QStringLiteral("new line\n"));  // no event loop: the refresh is still queued
        QVERIFY2(!embedder.claims(blockAt(editor, 1)), "block 1 is now the old first line");
        QVERIFY2(embedder.claims(blockAt(editor, 2)), "the opening line moved to block 2");
        QVERIFY(embedder.claims(blockAt(editor, 4)));
        QVERIFY(!embedder.claims(blockAt(editor, 5)));
        settleAll(f);
        QVERIFY(typesetAt(editor, 2, 4));
    }

    void theCaretEntersFromAboveAndBelow() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), kBlock)));
        auto* editor = f.editor();
        f.placeCaret(0);
        settleAll(f);
        QVERIFY(typesetAt(editor, 1, 3));
        test::keyClick(editor, Qt::Key_Down);
        settleAll(f);
        QCOMPARE(editor->textCursor().blockNumber(), 1);
        QVERIFY(sourceAt(editor, 1, 3));

        f.placeCaret(4);
        settleAll(f);
        QVERIFY(typesetAt(editor, 1, 3));
        test::keyClick(editor, Qt::Key_Up);
        settleAll(f);
        const int block = editor->textCursor().blockNumber();
        QVERIFY2(block >= 1 && block <= 3, qPrintable(QString::number(block)));
        QVERIFY(sourceAt(editor, 1, 3));
    }

    void aClickOnThePanelShowsTheSource() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), kBlock)));
        auto* editor = f.editor();
        f.placeCaret(4);
        settleAll(f);
        QVERIFY(typesetAt(editor, 1, 3));
        const QRectF panel = viewportRect(editor, blockAt(editor, 1));
        QTest::mouseClick(editor->viewport(), Qt::LeftButton, {}, panel.center().toPoint());
        settleAll(f);
        QCOMPARE(editor->textCursor().blockNumber(), 1);
        QVERIFY(sourceAt(editor, 1, 3));
    }

    void unclosedAndInvalidBlocksStaySource() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("d.md"), "top\n$$\n\\frac{a\n$$\nmiddle\n$$\nx\n")));
        auto* editor = f.editor();
        f.placeCaret(0);
        settleAll(f);
        QVERIFY2(sourceAt(editor, 1, 3), "the invalid block");
        QVERIFY2(sourceAt(editor, 5, 6), "the unclosed block");
        const auto& failed = editor->mathBlocks().failed();
        QCOMPARE(failed.size(), std::size_t{1});
        QCOMPARE(failed[0].open, 1);
        QCOMPARE(failed[0].reason, std::string("missing }"));
    }

    void theSampleIsTypesetAndSavesByteForByte() {
        QFile sample(QStringLiteral(HITOKOTO_SOURCE_DIR "/tests/data/s044-latex.md"));
        QVERIFY(sample.open(QIODevice::ReadOnly));
        QByteArray content = sample.readAll();
        content.append("\nlast\n");
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("latex.md"), content)));
        auto* editor = f.editor();
        f.placeCaret(editor->document()->blockCount() - 2);
        settleAll(f);
        QCOMPARE(editor->mathBlocks().typeset().size(), std::size_t{4});
        QVERIFY2(test::differenceFromTheReference(editor).isEmpty(),
                 qPrintable(test::differenceFromTheReference(editor)));
        if (const QByteArray out = qgetenv("HITOKOTO_MATH_SAMPLE_PREVIEW"); !out.isEmpty()) {
            f.window->resize(1100, 1000);
            settleAll(f);
            editor->verticalScrollBar()->setValue(0);
            settleAll(f);
            QVERIFY(editor->viewport()->grab().save(QString::fromLocal8Bit(out)));
        }
        const QString saved = f.dir.filePath(QStringLiteral("saved.md"));
        QVERIFY(f.saveAndWait(saved));
        QCOMPARE(test::Fixture::readFile(saved), content);
    }
};

QTEST_MAIN(tst_s047_typeset_display_math)
#include "tst_s047_typeset_display_math.moc"
