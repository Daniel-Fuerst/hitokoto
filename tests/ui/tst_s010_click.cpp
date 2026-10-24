// SPDX-License-Identifier: GPL-3.0-or-later
// S010: clicking a line with hidden markers puts the caret where clicked.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
QPoint pointBefore(ui::EditorWidget* editor, int position) {
    QTextCursor c(editor->document());
    c.setPosition(position);
    const QRect r = editor->cursorRect(c);
    return {r.left() + 1, r.center().y()};
}
}  // namespace

class tst_s010_click : public QObject {
    Q_OBJECT
private slots:
    void clickBetweenCharactersOfConcealedHeading() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("c.md"), "## Plans\nbody\n")));
        f.placeCaret(1);
        const QPoint target = pointBefore(f.editor(), 4);  // between 'P' and 'l', concealed layout
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {}, target);
        QCOMPARE(f.editor()->textCursor().position(), 4);
        QVERIFY(!test::isCollapsed(f.editor()->document()->firstBlock(), 0));
    }

    void clickLeftOfTextLandsBehindTheMarker() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("c.md"), "### Plans\nbody\n")));
        f.placeCaret(1);
        const QRect line = f.editor()->cursorRect(QTextCursor(f.editor()->document()->firstBlock()));
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, {},
                          QPoint(std::max(0, line.left() - 1), line.center().y()));
        QCOMPARE(f.editor()->textCursor().position(), 4);
    }

    void dragSelectionStartsAtClickedCharacter() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("c.md"), "## Plans\nbody\n")));
        f.placeCaret(1);
        const QPoint start = pointBefore(f.editor(), 4);
        QTest::mousePress(f.editor()->viewport(), Qt::LeftButton, {}, start);
        const QPoint end = pointBefore(f.editor(), 11);  // inside "body"
        QMouseEvent move(QEvent::MouseMove, end, f.editor()->viewport()->mapToGlobal(end), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(f.editor()->viewport(), &move);
        QTest::mouseRelease(f.editor()->viewport(), Qt::LeftButton, {}, end);
        QCOMPARE(f.editor()->textCursor().anchor(), 4);
        QVERIFY(f.editor()->textCursor().position() > 9);
    }
};

QTEST_MAIN(tst_s010_click)
#include "tst_s010_click.moc"
