// SPDX-License-Identifier: GPL-3.0-or-later
// S027: smooth scrolling for a scroll bar using an easing curve animation.
#pragma once

#include <QEasingCurve>
#include <QObject>
#include <QVariantAnimation>

class QScrollBar;

namespace hitokoto::ui {

class SmoothScroller : public QObject {
    Q_OBJECT
public:
    explicit SmoothScroller(QScrollBar* scrollBar, QObject* parent = nullptr);
    ~SmoothScroller() override;

    /// Scrolls by `deltaPixels`. If an animation is already running in the same direction,
    /// `deltaPixels` is added to the accumulated target. If the direction reverses, the
    /// animation reverses immediately from the current position.
    void scroll(double deltaPixels);

    /// Stops any running animation immediately at the current scroll position.
    void stop();

    /// Whether an animation is currently gliding.
    [[nodiscard]] bool isRunning() const;

    /// Target position towards which the scroller is animating, or the current position if idle.
    [[nodiscard]] double target() const noexcept { return target_; }

    /// Moves the scroll bar, the target and a running animation by `delta` together (e.g. when lazy
    /// layout shifts content above the viewport); a running animation goes on.
    void shift(double delta);

    /// Animation duration in milliseconds (default 200 ms).
    [[nodiscard]] int duration() const noexcept { return durationMs_; }
    void setDuration(int ms) noexcept { durationMs_ = ms; }

    /// Easing curve (default OutCubic).
    [[nodiscard]] const QEasingCurve& easingCurve() const noexcept { return easingCurve_; }
    void setEasingCurve(const QEasingCurve& curve) { easingCurve_ = curve; }

signals:
    void scrollStarted();
    void scrollFinished();

private:
    void onValueChanged(const QVariant& value);
    void onFinished();

    void setBarValue(int value);

    // Owned by the scroll area; a plain pointer, cleared on destroyed() (review O2: clang-tidy's
    // analyzer reports a false NewDelete in the destructor of a QPointer member).
    QScrollBar* scrollBar_ = nullptr;
    QVariantAnimation animation_;
    double target_ = 0.0;
    int durationMs_ = 200;
    QEasingCurve easingCurve_ = QEasingCurve::OutCubic;
    int lastMin_ = 0;
    int lastMax_ = 0;
    bool updating_ = false;
};

}  // namespace hitokoto::ui
