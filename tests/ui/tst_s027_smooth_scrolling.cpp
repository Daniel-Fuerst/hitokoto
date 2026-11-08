// SPDX-License-Identifier: GPL-3.0-or-later
// S027: smooth scrolling in the editor view.
#include "smooth_scroller.hpp"
#include "table_view.hpp"
#include "ui_test_support.hpp"

#include <QAbstractAnimation>
#include <QApplication>
#include <QPoint>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

#include <cmath>

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

/// Distance of one wheel notch: the system's lines per notch, 1.8 times as far (Daniel, 2026-11-08).
double notchDistance(const QWidget* editor) {
    const int lineSpacing = editor->fontMetrics().lineSpacing() > 0 ? editor->fontMetrics().lineSpacing() : 20;
    return 1.8 * QApplication::wheelScrollLines() * lineSpacing;
}

/// Review O3: drives every animation by hand, so the tests do not depend on when the animation
/// timer fires on a runner. Install it before the first animation starts; only step() moves the
/// clock. Qt starts a driver and registers new animations on its own schedule within the steps, so
/// the tests wait for states (stepUntil), never for an exact number of milliseconds.
class ManualAnimationDriver final : public QAnimationDriver {
public:
    ManualAnimationDriver() { install(); }
    ~ManualAnimationDriver() override { uninstall(); }
    ManualAnimationDriver(const ManualAnimationDriver&) = delete;
    ManualAnimationDriver& operator=(const ManualAnimationDriver&) = delete;

    /// Lets queued animation starts register, then moves the clock by `ms` and runs one frame.
    void step(int ms = 16) {
        QCoreApplication::processEvents();
        elapsed_ += ms;
        advanceAnimation();
    }

    /// Runs frames of 16 ms until `done()` holds; false after `maxFrames` frames.
    template <typename Done>
    bool stepUntil(Done done, int maxFrames = 100) {
        for (int i = 0; i < maxFrames && !done(); ++i) {
            step();
        }
        return done();
    }

    void advance() override {}  // never driven by a timer
    [[nodiscard]] qint64 elapsed() const override { return elapsed_; }

protected:
    void start() override {
        elapsed_ = 0;  // elapsed() counts from the start of the driver, as Qt's own driver does
        QAnimationDriver::start();
    }

private:
    qint64 elapsed_ = 0;
};

}  // namespace

class tst_s027_smooth_scrolling : public QObject {
    Q_OBJECT
private slots:
    void wheelScrollAnimatesAcrossFramesToTarget() {
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        QVERIFY(bar->maximum() > 0);
        QCOMPARE(bar->value(), 0);

        const double expectedStep = notchDistance(editor);

        QSignalSpy valueSpy(bar, &QScrollBar::valueChanged);

        sendWheel(editor->viewport(), -120);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        QCOMPARE(scroller->target(), expectedStep);

        // Let the animation finish
        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), static_cast<int>(std::lround(expectedStep)));

        // Smooth scrolling delivered intermediate frames, not a single instantaneous jump
        QVERIFY2(valueSpy.count() > 1,
                 qPrintable(QStringLiteral("expected > 1 value changes, got %1").arg(valueSpy.count())));
    }

    void rapidSuccessiveWheelEventsAccumulateTarget() {
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        const double expectedStep = notchDistance(editor);

        sendWheel(editor->viewport(), -120);
        anim.step(30);

        sendWheel(editor->viewport(), -120);

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        QCOMPARE(scroller->target(), expectedStep * 2);

        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), static_cast<int>(std::lround(expectedStep * 2)));
    }

    void reversingWheelDirectionRedirectsImmediately() {
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();

        sendWheel(editor->viewport(), -120);
        QVERIFY(anim.stepUntil([&] { return bar->value() > 0; }));

        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);
        QVERIFY(scroller->isRunning());
        const int intermediate = bar->value();
        QVERIFY(intermediate > 0);

        // Reverse direction: wheel UP (+120)
        sendWheel(editor->viewport(), 120);

        // Must redirect towards 0 immediately without waiting
        QCOMPARE(scroller->target(), 0.0);
        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), 0);
    }

    void sliderPressStopsAnimation() {
        test::Fixture f;
        ManualAnimationDriver anim;
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
        ManualAnimationDriver anim;
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
        ManualAnimationDriver anim;
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
        ManualAnimationDriver anim;
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
        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), bar->maximum());
    }

    void touchpadPixelDeltaScrollsDirectly() {
        // Review O4: Qt says pixelDelta() "should be used directly to scroll content on screen";
        // a trackpad event carries an angle delta too, which must not turn it into wheel notches.
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        const int initial = bar->value();
        sendWheel(editor->viewport(), -60, 0, QPoint(0, -40));

        QVERIFY(!scroller->isRunning());
        QCOMPARE(bar->value(), initial + 40);
        QCOMPARE(scroller->target(), static_cast<double>(initial + 40));
    }

    void touchpadDeltaDuringAnimationContinuesFromTheCurrentPosition() {
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        sendWheel(editor->viewport(), -120);
        QVERIFY(anim.stepUntil([&] { return bar->value() > 0; }));
        QVERIFY(scroller->isRunning());
        const int current = bar->value();

        sendWheel(editor->viewport(), -60, 0, QPoint(0, -40));
        QVERIFY(!scroller->isRunning());
        QCOMPARE(bar->value(), current + 40);
        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), current + 40);
    }

    void optionDisablesSmoothScrolling() {
        test::Fixture f;
        ManualAnimationDriver anim;
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
        ManualAnimationDriver anim;
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

        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QVERIFY(bar->value() > 0);
    }

    void layoutShiftMovesTheBarAndTheAnimation() {
        // A layout shift above the viewport moves the content; the bar, the running animation and
        // its target follow it, and the animation goes on.
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(100).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        sendWheel(editor->viewport(), -120);
        QVERIFY(anim.stepUntil([&] { return bar->value() > 0; }));
        QVERIFY(scroller->isRunning());
        const double targetBefore = scroller->target();
        const int valueBefore = bar->value();

        scroller->shift(40.0);
        QCOMPARE(scroller->target(), targetBefore + 40.0);
        QCOMPARE(bar->value(), valueBefore + 40);
        QVERIFY(scroller->isRunning());

        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), static_cast<int>(std::lround(targetBefore + 40.0)));
    }

    void programmaticScrollDuringAnimationStopsIt() {
        // Review O1: a scroll by the program (find, scroll anchor, caret moves) wins over the wheel.
        test::Fixture f;
        ManualAnimationDriver anim;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), numberedLines(400).toUtf8())));
        f.settle();

        ui::EditorWidget* editor = f.editor();
        QScrollBar* bar = editor->verticalScrollBar();
        ui::SmoothScroller* scroller = editor->smoothScroller();
        QVERIFY(scroller != nullptr);

        sendWheel(editor->viewport(), -120);
        QVERIFY(anim.stepUntil([&] { return bar->value() > 0; }));
        QVERIFY(scroller->isRunning());

        const int jump = bar->maximum() / 2;
        bar->setValue(jump);
        QVERIFY(!scroller->isRunning());
        QCOMPARE(scroller->target(), static_cast<double>(jump));

        QVERIFY(anim.stepUntil([&] { return !scroller->isRunning(); }));
        QCOMPARE(bar->value(), jump);
    }
};

QTEST_MAIN(tst_s027_smooth_scrolling)
#include "tst_s027_smooth_scrolling.moc"
