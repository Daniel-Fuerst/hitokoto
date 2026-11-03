// SPDX-License-Identifier: GPL-3.0-or-later
// S027: smooth scrolling in the editor view.
#include "smooth_scroller.hpp"
#include "table_view.hpp"
#include "ui_test_support.hpp"

#include <QApplication>
#include <QPoint>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

using namespace hitokoto;

namespace {

QString numberedLines(int count) {
    QString out;
    for (int i = 0; i < count; ++i) {
        out += QStringLiteral("Line %1 with some text to ensure standard height.\n").arg(i);
    }
    return out;
}

void sendWheel(QWidget* target, int angleDeltaY, int angleDeltaX = 0, QPoint pixelDelta = QPoint(0, 0),
               Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPointF pos(target->rect().center());
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    QWheelEvent event(pos, globalPos, pixelDelta, QPoint(angleDeltaX, angleDeltaY), Qt::NoButton, modifiers,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &event);
}

}  // namespace

class tst_s027_smooth_scrolling : public QObject {
    Q_OBJECT
private slots:
    void wheelScrollAnimatesAcrossFramesToTarget() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        QVERIFY(bar->maximum() > 0);
        QCOMPARE(bar->value(), 0);

        const int lineSpacing = editor->fontMetrics().lineSpacing() > 0 ? editor->fontMetrics().lineSpacing() : 20;
        const int expectedStep = QApplication::wheelScrollLines() * lineSpacing;

        QSignalSpy valueSpy(bar, &QScrollBar::valueChanged);

        sendWheel(editor->viewport(), -120);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        QCOMPARE(scroller->target(), static_cast<double>(expectedStep));

        // Let the animation finish
        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QCOMPARE(bar->value(), expectedStep);

        // Smooth scrolling delivered intermediate frames, not a single instantaneous jump
        QVERIFY2(valueSpy.count() > 1,
                 qPrintable(QStringLiteral("expected > 1 value changes, got %1").arg(valueSpy.count())));
    }

    void rapidSuccessiveWheelEventsAccumulateTarget() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        const int lineSpacing = editor->fontMetrics().lineSpacing() > 0 ? editor->fontMetrics().lineSpacing() : 20;
        const int expectedStep = QApplication::wheelScrollLines() * lineSpacing;

        sendWheel(editor->viewport(), -120);
        QTest::qWait(30);

        sendWheel(editor->viewport(), -120);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        QCOMPARE(scroller->target(), static_cast<double>(expectedStep * 2));

        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QCOMPARE(bar->value(), expectedStep * 2);
    }

    void reversingWheelDirectionRedirectsImmediately() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();

        sendWheel(editor->viewport(), -120);
        QTest::qWait(40);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        const int intermediate = bar->value();
        QVERIFY(intermediate > 0);

        // Reverse direction: wheel UP (+120)
        sendWheel(editor->viewport(), 120);

        // Must redirect towards 0 immediately without waiting
        QCOMPARE(scroller->target(), 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QCOMPARE(bar->value(), 0);
    }

    void sliderPressStopsAnimation() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();

        sendWheel(editor->viewport(), -120);
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());

        bar->sliderPressed();
        QVERIFY(!scroller->isRunning());
    }

    void keyPressStopsAnimation() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();

        sendWheel(editor->viewport(), -120);
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());

        test::keyClick(editor, Qt::Key_Down);
        QVERIFY(!scroller->isRunning());
    }

    void mousePressStopsAnimation() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();

        sendWheel(editor->viewport(), -120);
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());

        QTest::mousePress(editor->viewport(), Qt::LeftButton, {}, QPoint(50, 50));
        QVERIFY(!scroller->isRunning());
    }

    void clampingAtBounds() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        // Scroll up when at top (0): target clamped at 0
        sendWheel(editor->viewport(), 12000);
        QCOMPARE(bar->value(), 0);
        QCOMPARE(scroller->target(), 0.0);

        // Huge scroll down beyond maximum
        sendWheel(editor->viewport(), -120000);
        QCOMPARE(scroller->target(), static_cast<double>(bar->maximum()));
        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QCOMPARE(bar->value(), bar->maximum());
    }

    void touchpadPixelDeltaAnimatesSmoothly() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        const int initial = bar->value();
        // High-resolution trackpad event with pixelDelta only (angleDelta 0)
        sendWheel(editor->viewport(), 0, 0, QPoint(0, -40));

        QVERIFY(scroller->isRunning());
        QCOMPARE(scroller->target(), static_cast<double>(initial + 40));

        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QCOMPARE(bar->value(), initial + 40);
    }

    void optionDisablesSmoothScrolling() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        ui::Options opts = f.window->options();
        opts.smoothScrolling = false;
        f.window->setOptions(opts);

        QVERIFY(!editor->isSmoothScrollingEnabled());

        QSignalSpy valueSpy(editor->verticalScrollBar(), &QScrollBar::valueChanged);
        sendWheel(editor->viewport(), -120);

        // Did not animate
        QVERIFY(!scroller->isRunning());
        // Jumped in a single value change
        QCOMPARE(valueSpy.count(), 1);
    }

    void wheelOverEmbeddedTableScrollsEditor() {
        test::Fixture f;
        QString text = numberedLines(20);
        text += QStringLiteral("| A | B |\n| --- | --- |\n| 1 | 2 |\n");
        text += numberedLines(60);
        QVERIFY(f.open(f.writeFile(QStringLiteral("table.md"), text.toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QTableView* table = editor->tableViewAt(20);
        QVERIFY(table != nullptr);

        QScrollBar* bar = editor->verticalScrollBar();
        QCOMPARE(bar->value(), 0);

        // Send wheel to the table's viewport
        sendWheel(table->viewport(), -120);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());

        QTRY_VERIFY_WITH_TIMEOUT(!scroller->isRunning(), 1000);
        QVERIFY(bar->value() > 0);
    }

    void layoutShiftAdjustsActiveTarget() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        sendWheel(editor->viewport(), -120);
        QVERIFY(scroller->isRunning());
        const double targetBefore = scroller->target();

        scroller->shift(40.0);
        QCOMPARE(scroller->target(), targetBefore + 40.0);

        scroller->stop();
    }
};

QTEST_MAIN(tst_s027_smooth_scrolling)
#include "tst_s027_smooth_scrolling.moc"
