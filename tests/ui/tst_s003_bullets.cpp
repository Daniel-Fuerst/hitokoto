// SPDX-License-Identifier: GPL-3.0-or-later
// S003: bullets render as dots; the raw marker shows on the caret line.
#include "ui_test_support.hpp"

#include <QImage>

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
};

QTEST_MAIN(tst_s003_bullets)
#include "tst_s003_bullets.moc"
