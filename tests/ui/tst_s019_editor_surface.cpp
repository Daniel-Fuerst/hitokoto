// SPDX-License-Identifier: GPL-3.0-or-later
// S019: the editor surface is a QTextEdit with our own document layout (ADR 0003).
#include "ui_test_support.hpp"

#include <QAccessible>
#include <QInputMethodEvent>
#include <QScrollBar>
#include <QTextEdit>

using namespace hitokoto;

namespace {
QByteArray numberedLines(int count) {
    QByteArray text;
    for (int i = 0; i < count; ++i) {
        text += "line " + QByteArray::number(i) + '\n';
    }
    return text;
}

/// Every block's height must equal the extent of its lines (or its reserved height), tops must add
/// up, and the document size must match. Empty when consistent.
QString layoutErrors(ui::EditorWidget* editor) {
    ui::BlockLayout* layout = editor->blockLayout();
    const QTextDocument* doc = editor->document();
    qreal y = doc->documentMargin();
    QString errors;
    int bad = 0;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const QRectF rect = layout->blockBoundingRect(b);
        qreal expected = 0;
        if (b.isVisible()) {
            for (int i = 0; i < b.layout()->lineCount(); ++i) {
                const QTextLine line = b.layout()->lineAt(i);
                expected = std::max(expected, line.y() + line.height());
            }
            if (layout->reservedHeight(b) > 0) {
                expected = layout->reservedHeight(b);
            }
        }
        if (std::abs(rect.top() - y) > 0.01 || std::abs(rect.height() - expected) > 0.01) {
            if (bad++ < 3) {
                errors += QStringLiteral("block %1: top %2 (expected %3), height %4 (expected %5); ")
                              .arg(b.blockNumber())
                              .arg(rect.top())
                              .arg(y)
                              .arg(rect.height())
                              .arg(expected);
            }
        }
        y += expected;
    }
    if (std::abs(layout->documentSize().height() - (y + doc->documentMargin())) > 0.01) {
        errors += QStringLiteral("document height %1 (expected %2)")
                      .arg(layout->documentSize().height())
                      .arg(y + doc->documentMargin());
    }
    return errors;
}
}  // namespace

class tst_s019_editor_surface : public QObject {
    Q_OBJECT
private slots:
    void editorIsATextEditWithBlockLayout() {
        test::Fixture f;
        QVERIFY(qobject_cast<QTextEdit*>(static_cast<QObject*>(f.editor())) != nullptr);
        QVERIFY(f.editor()->blockLayout() != nullptr);
    }

    void scrollRangeIsInPixels() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(200))));
        f.settle();
        auto* layout = f.editor()->blockLayout();
        QVERIFY(layout != nullptr);

        const int documentHeight = layout->documentSize().toSize().height();
        const int viewportHeight = f.editor()->viewport()->height();
        QVERIFY(documentHeight > 2 * viewportHeight);
        QCOMPARE(f.editor()->verticalScrollBar()->maximum(), documentHeight - viewportHeight);

        // At the end of the range the last line is on screen.
        f.editor()->verticalScrollBar()->setValue(f.editor()->verticalScrollBar()->maximum());
        f.settle();
        QTextCursor last(f.editor()->document()->lastBlock().previous());
        const QRect caret = f.editor()->cursorRect(last);
        QVERIFY2(caret.top() >= 0 && caret.bottom() <= viewportHeight,
                 qPrintable(QStringLiteral("last line at %1..%2, viewport %3")
                                .arg(caret.top())
                                .arg(caret.bottom())
                                .arg(viewportHeight)));
    }

    void blocksCanReserveHeightOrBeHidden() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("reserve.md"), numberedLines(20))));
        f.settle();
        auto* layout = f.editor()->blockLayout();
        QVERIFY(layout != nullptr);
        const auto* doc = f.editor()->document();
        const auto block = [doc](int n) { return doc->findBlockByNumber(n); };

        const qreal heightBefore = layout->documentSize().height();
        const qreal lineHeight = layout->blockBoundingRect(block(6)).height();
        QVERIFY(lineHeight > 0);

        layout->setReservedHeight(block(5), 150);
        for (int n = 6; n <= 8; ++n) {
            layout->setHidden(block(n), true);
        }
        f.settle();

        QCOMPARE(layout->reservedHeight(block(5)), 150.0);
        QCOMPARE(layout->blockBoundingRect(block(5)).height(), 150.0);
        QCOMPARE(layout->blockBoundingRect(block(7)).height(), 0.0);
        QCOMPARE(layout->blockBoundingRect(block(9)).top(), layout->blockBoundingRect(block(5)).bottom());
        // Blocks 5..8 were one line each; now 150 px and three times 0.
        QCOMPARE(layout->documentSize().height(), heightBefore - 4 * lineHeight + 150);

        // The caret moves over hidden blocks.
        f.placeCaret(4, 0);
        test::keyClick(f.editor(), Qt::Key_Down);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
        test::keyClick(f.editor(), Qt::Key_Down);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 9);
        test::keyClick(f.editor(), Qt::Key_Up);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);

        // Showing the blocks and releasing the height restores the layout.
        layout->setReservedHeight(block(5), 0);
        for (int n = 6; n <= 8; ++n) {
            layout->setHidden(block(n), false);
        }
        f.settle();
        QCOMPARE(layout->documentSize().height(), heightBefore);
    }

    // Qt emits contentsChange before it tells the layout; the styler lays out visible blocks in
    // between (review of d11f16d, finding 1).
    void heightsStayConsistentWhenManyLinesAreInsertedInView() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("insert.md"), numberedLines(1000))));
        f.settle();
        QString wrapped;
        for (int i = 0; i < 600; ++i) {
            wrapped += QStringLiteral("inserted %1 ").arg(i) +
                       QString(300, QLatin1Char('w')).replace(QStringLiteral("wwww"), QStringLiteral("ww w")) +
                       QLatin1Char('\n');
        }
        f.placeCaret(5, 0);
        f.editor()->insertPlainText(wrapped);
        f.settle();
        QCOMPARE(layoutErrors(f.editor()), QString());
        f.editor()->document()->undo();
        f.settle();
        QCOMPARE(layoutErrors(f.editor()), QString());
    }

    // A new width re-estimates every block; the first line on screen must stay (finding 2).
    void resizingKeepsTheReadingPosition() {
        QByteArray markdown;
        for (int i = 0; i < 1500; ++i) {
            markdown += i % 10 == 0 ? "## Heading " + QByteArray::number(i) + '\n'
                                    : "line " + QByteArray::number(i) + ' ' +
                                          QByteArray(i % 3 == 0 ? 180 : 10, 'a').replace("aaaa", "aa a") + '\n';
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("resize.md"), markdown)));
        f.settle();
        auto* bar = f.editor()->verticalScrollBar();
        for (int page = 0; page < 60 && bar->value() < bar->maximum(); ++page) {
            bar->triggerAction(QAbstractSlider::SliderPageStepAdd);
            f.settle();
        }
        const auto topBlock = [&] { return f.editor()->cursorForPosition(QPoint(40, 2)).blockNumber(); };
        const int before = topBlock();
        QVERIFY(before > 100);

        f.window->resize(f.window->width() - 40, f.window->height());
        f.settle();
        QCOMPARE(topBlock(), before);
        f.window->resize(f.window->width() + 40, f.window->height());
        f.settle();
        QCOMPARE(topBlock(), before);
    }

    void inputMethodComposesAtTheCaret() {
        constexpr auto markdown = "# Heading\n- item two\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("ime.md"), markdown)));
        f.settle();

        for (const auto& [blockNumber, column] : {std::pair{0, 9}, std::pair{1, 6}}) {
            f.placeCaret(blockNumber, column);
            f.settle();
            const QRect caretBefore = f.editor()->cursorRect();

            QInputMethodEvent preedit(QStringLiteral("かな"), {});
            QCoreApplication::sendEvent(f.editor(), &preedit);
            const QRect imeRect = f.editor()->inputMethodQuery(Qt::ImCursorRectangle).toRect();
            QVERIFY2(std::abs(imeRect.center().y() - caretBefore.center().y()) <= caretBefore.height(),
                     qPrintable(QStringLiteral("pre-edit at y %1, caret at y %2")
                                    .arg(imeRect.center().y())
                                    .arg(caretBefore.center().y())));
            QVERIFY(imeRect.left() >= caretBefore.left());

            QInputMethodEvent commit;
            commit.setCommitString(QStringLiteral("仮名"));
            QCoreApplication::sendEvent(f.editor(), &commit);
            f.settle();
        }
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("# Heading仮名\n- item仮名 two\n"));
    }

    void exposesTextThroughAccessibility() {
        constexpr auto markdown = "# Heading\nBody text\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("a11y.md"), markdown)));
        f.settle();
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(f.editor());
        QVERIFY(iface != nullptr);
        QAccessibleTextInterface* text = iface->textInterface();
        QVERIFY(text != nullptr);
        const QString all = text->text(0, text->characterCount());
        QVERIFY2(all.contains(QStringLiteral("Heading")) && all.contains(QStringLiteral("Body text")), qPrintable(all));
    }
};

QTEST_MAIN(tst_s019_editor_surface)
#include "tst_s019_editor_surface.moc"
