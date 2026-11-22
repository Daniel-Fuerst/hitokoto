// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullets render as dots; the raw marker shows on the caret line.
#include "hitokoto/core/bullet.hpp"
#include "ui_test_support.hpp"

#include <QFontMetricsF>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace hitokoto;

namespace {
/// Pixels close to the bullet colour inside the marker character's box.
int bulletPixels(ui::EditorWidget* editor, int blockNumber) {
    const QTextBlock block = editor->document()->findBlockByNumber(blockNumber);
    QTextCursor c(block);
    const QRect r0 = editor->cursorRect(c);
    c.movePosition(QTextCursor::NextCharacter);
    const QRect r1 = editor->cursorRect(c);
    const QRect box(r0.left(), r0.top(), std::max(2, r1.left() - r0.left()), r0.height());
    const QImage image = editor->viewport()->grab().toImage();
    const QColor target(QString::fromStdString(core::builtinTheme(core::ColorScheme::Light).bullet.toHex()));
    int hits = 0;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            const QColor p = image.pixelColor(x * image.width() / editor->viewport()->width(),
                                              y * image.height() / editor->viewport()->height());
            if (std::abs(p.red() - target.red()) + std::abs(p.green() - target.green()) +
                    std::abs(p.blue() - target.blue()) <
                60) {
                ++hits;
            }
        }
    }
    return hits;
}

/// Pixels of the bullet colour in the box of the dot of the item on `blockNumber` (its centre
/// alone with `centreOnly`): a filled dot has its centre, a ring only its edge.
int dotPixels(ui::EditorWidget* editor, int blockNumber, bool centreOnly) {
    const QTextBlock block = editor->document()->findBlockByNumber(blockNumber);
    const auto info = core::parseBulletLine(block.text().toStdU16String());
    if (!info) {
        return 0;
    }
    QTextCursor c(block);
    c.setPosition(block.position() + static_cast<int>(info->markerChar));
    const QRect r0 = editor->cursorRect(c);
    c.movePosition(QTextCursor::NextCharacter);
    const QRect r1 = editor->cursorRect(c);
    const QFontMetricsF metrics(editor->document()->defaultFont());
    const QPointF centre((r0.left() + r1.left()) / 2.0, r0.top() + metrics.ascent() - metrics.xHeight() / 2.0);
    const int reach = centreOnly ? 0 : static_cast<int>(std::ceil(std::max<qreal>(1.5, metrics.xHeight() * 0.32))) + 1;
    const QImage image = editor->viewport()->grab().toImage();
    const QColor target(QString::fromStdString(core::builtinTheme(core::ColorScheme::Light).bullet.toHex()));
    int hits = 0;
    for (int dy = -reach; dy <= reach; ++dy) {
        for (int dx = -reach; dx <= reach; ++dx) {
            const QColor p =
                image.pixelColor(static_cast<int>((centre.x() + dx) * image.width() / editor->viewport()->width()),
                                 static_cast<int>((centre.y() + dy) * image.height() / editor->viewport()->height()));
            if (std::abs(p.red() - target.red()) + std::abs(p.green() - target.green()) +
                    std::abs(p.blue() - target.blue()) <
                60) {
                ++hits;
            }
        }
    }
    return hits;
}
}  // namespace

class tst_s003_bullets : public QObject {
    Q_OBJECT
private slots:
    void dotIsDrawnAndMarkerHiddenButKeepsItsSpace() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("b.md"), "- milk\nother\n")));
        f.placeCaret(1);
        const QTextBlock item = f.editor()->document()->firstBlock();
        const auto fmt = test::formatAt(item, 0);
        QCOMPARE(fmt.foreground().color().alpha(), 0);
        QVERIFY(!test::isCollapsed(item, 0));  // keeps its width: text does not move
        QVERIFY(bulletPixels(f.editor(), 0) > 0);
    }

    // Items alternate with their depth: a filled dot at even depths, a ring at odd ones.
    void nestedItemsAlternateDotsAndRings() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("nested.md"), "- a\n  - b\n    - c\nother\n")));
        f.placeCaret(3);
        f.settle();
        QCOMPARE(dotPixels(f.editor(), 0, true), 1);
        QCOMPARE(dotPixels(f.editor(), 1, true), 0);
        QVERIFY(dotPixels(f.editor(), 1, false) > 0);
        QCOMPARE(dotPixels(f.editor(), 2, true), 1);
    }

    void rawMarkerOnCaretLineAndNoDot() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("b.md"), "- milk\nother\n")));
        f.placeCaret(0, 3);
        const QTextBlock item = f.editor()->document()->firstBlock();
        QVERIFY(test::formatAt(item, 0).foreground().color().alpha() > 0);
        QCOMPARE(bulletPixels(f.editor(), 0), 0);
    }

    void textDoesNotMoveWhenRevealed() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("b.md"), "* milk\nother\n")));
        f.placeCaret(1);
        QTextCursor c(f.editor()->document()->firstBlock());
        c.setPosition(2);
        const int concealedX = f.editor()->cursorRect(c).left();
        f.placeCaret(0, 6);
        const int revealedX = f.editor()->cursorRect(c).left();
        QCOMPARE(concealedX, revealedX);
    }

    void wrappedContinuationLinesAlignWithItemText() {
        test::Fixture f;
        const auto line = QByteArrayLiteral(
            "- Bullet with a very very very very very very very very very very very very very very very very very very "
            "very very very very long text that wraps\nother\n");
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrap.md"), line)));
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        const QTextLayout* layout = block.layout();
        QVERIFY(layout->lineCount() >= 2);
        const QTextLine line0 = layout->lineAt(0);
        const QTextLine line1 = layout->lineAt(1);
        const qreal textStartX = line0.cursorToX(2);
        QCOMPARE(line1.x(), textStartX);
    }

    void nestedWrappedBulletsAlignWithTheirOwnItemText() {
        test::Fixture f;
        const auto line = QByteArrayLiteral(
            "  * Nested bullet with a very very very very very very very very very very very very very very very very "
            "very very very very very very long text that wraps\nother\n");
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrap_nested.md"), line)));
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        const QTextLayout* layout = block.layout();
        QVERIFY(layout->lineCount() >= 2);
        const QTextLine line0 = layout->lineAt(0);
        const QTextLine line1 = layout->lineAt(1);
        const qreal textStartX = line0.cursorToX(4);
        QCOMPARE(line1.x(), textStartX);
    }

    void wrappedContinuationLinesKeepAlignmentWhenRevealed() {
        test::Fixture f;
        const auto line = QByteArrayLiteral(
            "- Bullet with a very very very very very very very very very very very very very very very very very very "
            "very very very very long text that wraps\nother\n");
        QVERIFY(f.open(f.writeFile(QStringLiteral("wrap_rev.md"), line)));
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        QVERIFY(block.layout()->lineCount() >= 2);
        const qreal concealedLine1X = block.layout()->lineAt(1).x();
        f.placeCaret(0, 10);
        f.settle();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        QVERIFY(block.layout()->lineCount() >= 2);
        const qreal revealedLine1X = block.layout()->lineAt(1).x();
        QCOMPARE(concealedLine1X, revealedLine1X);
    }

    void enterOnBulletItemContinuesListWithSameMarker() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("enter1.md"), "- milk\nother\n")));
        f.placeCaret(0, 6);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("- milk\n- \nother\n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.blockNumber(), 1);
        QCOMPARE(c.positionInBlock(), 2);
    }

    void enterOnNestedAsteriskItemPreservesMarkerAndNesting() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("enter2.md"), "  * item\n")));
        f.placeCaret(0, 8);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("  * item\n  * \n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.blockNumber(), 1);
        QCOMPARE(c.positionInBlock(), 4);
    }

    void enterOnEmptyBulletItemClearsMarkerAndExitsList() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("enter3.md"), "- milk\n- \n")));
        f.placeCaret(1, 2);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("- milk\n\n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.blockNumber(), 1);
        QCOMPARE(c.positionInBlock(), 0);
    }

    void enterOnEmptyNestedBulletOutdentsOneLevel() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("enter4.md"), "  - \n")));
        f.placeCaret(0, 4);
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("- \n"));
        test::keyClick(f.editor(), Qt::Key_Return);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("\n"));
    }

    void tabBesideBulletItemIndentsItFromAnywhereOnLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("tab1.md"), "- milk\n")));
        f.placeCaret(0, 4);
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("  - milk\n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.positionInBlock(), 6);
    }

    void shiftTabBesideBulletItemOutdentsItFromAnywhereOnLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("tab2.md"), "  - milk\n")));
        f.placeCaret(0, 8);
        test::keyClick(f.editor(), Qt::Key_Backtab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("- milk\n"));
        const QTextCursor c = f.editor()->textCursor();
        QCOMPARE(c.positionInBlock(), 6);
    }

    // Shift+Tab takes away one level: two spaces, or the one space there is.
    void shiftTabOutdentsByTwoSpacesOrTheOneThereIs() {
        for (const auto& [line, outdented] :
             {std::pair{"   - milk\n", " - milk\n"}, std::pair{" - milk\n", "- milk\n"}}) {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("outdent.md"), line)));
            f.placeCaret(0, 5);
            test::keyClick(f.editor(), Qt::Key_Backtab);
            f.settle();
            QCOMPARE(f.editor()->toPlainText(), QString::fromLatin1(outdented));
        }
    }

    // Tab over a selection of several lines indents the bullet items in it and leaves other lines.
    void tabOverSeveralLinesIndentsOnlyTheItems() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("lines.md"), "- a\nb\n- c\n")));
        QTextCursor c(f.editor()->document());
        c.setPosition(0);
        c.setPosition(8, QTextCursor::KeepAnchor);
        f.editor()->setTextCursor(c);
        test::keyClick(f.editor(), Qt::Key_Tab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("  - a\nb\n  - c\n"));
        test::keyClick(f.editor(), Qt::Key_Backtab);
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("- a\nb\n- c\n"));
    }

    void bulletPointsHaveDefaultIndentComparedToParagraphs() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("indent.md"), "- bullet\nparagraph\n")));
        f.placeCaret(1);
        f.settle();
        const QTextBlock bulletBlock = f.editor()->document()->firstBlock();
        const QTextBlock paraBlock = bulletBlock.next();
        f.editor()->blockLayout()->ensureBlockLayout(bulletBlock);
        f.editor()->blockLayout()->ensureBlockLayout(paraBlock);
        const qreal bulletX = bulletBlock.layout()->lineAt(0).x();
        const qreal paraX = paraBlock.layout()->lineAt(0).x();
        QVERIFY2(bulletX > paraX, "bullet items must have a slight default indent compared to paragraphs");
    }

    void spaceAfterBulletHasAdditionalSeparation() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("sep.md"), "- milk\nother\n")));
        f.placeCaret(1);
        f.settle();
        const QTextBlock block = f.editor()->document()->firstBlock();
        f.editor()->blockLayout()->ensureBlockLayout(block);
        const auto spaceFmt = test::formatAt(block, 1);
        QCOMPARE(spaceFmt.fontLetterSpacingType(), QFont::AbsoluteSpacing);
        QVERIFY(spaceFmt.fontLetterSpacing() > 0);

        const QTextLayout* layout = block.layout();
        const QTextLine line = layout->lineAt(0);
        const qreal markerCenter = (line.cursorToX(0) + line.cursorToX(1)) / 2.0;
        const qreal textX = line.cursorToX(2);
        const QFontMetricsF metrics(f.editor()->document()->defaultFont());
        const qreal standardSpace = metrics.horizontalAdvance(QLatin1Char(' '));
        QVERIFY2(textX - markerCenter > standardSpace,
                 "distance from bullet dot center to text must be greater than a standard space");
    }
};

QTEST_MAIN(tst_s003_bullets)
#include "tst_s003_bullets.moc"
