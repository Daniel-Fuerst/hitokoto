// SPDX-License-Identifier: GPL-3.0-or-later
#include "toggle_switch.hpp"

#include "hitokoto/core/toggle_switch.hpp"
#include "qt_bridge.hpp"

#include <QPainter>
#include <QPen>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent), animation_(new QVariantAnimation(this)) {
    setCheckable(true);
    // Like every control of the flyout, the switch never takes the focus from the editor (S039, decision 1).
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // The animation runs linearly; the easing is the core's, so that the motion is tested without Qt.
    animation_->setEasingCurve(QEasingCurve::Linear);
    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        progress_ = value.toDouble();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animateTo);
}

void ToggleSwitch::setSwitchStyle(const core::SwitchStyle& style) {
    style_ = style;
    setFixedSize(sizeHint());
    update();
}

void ToggleSwitch::setCheckedImmediately(bool on) {
    animation_->stop();
    {
        const QSignalBlocker blocker(this);
        setChecked(on);
    }
    progress_ = on ? 1.0 : 0.0;
    update();
}

double ToggleSwitch::knobPosition() const noexcept {
    return core::easeInOut(progress_);
}

bool ToggleSwitch::isAnimating() const {
    return animation_->state() == QAbstractAnimation::Running;
}

core::Color ToggleSwitch::trackColor() const noexcept {
    return core::toggleGeometry(style_, knobPosition()).track;
}

QSize ToggleSwitch::sizeHint() const {
    return {static_cast<int>(std::lround(style_.width)), static_cast<int>(std::lround(style_.height))};
}

bool ToggleSwitch::hitButton(const QPoint& pos) const {
    return rect().contains(pos);
}

void ToggleSwitch::animateTo(bool on) {
    const double target = on ? 1.0 : 0.0;
    animation_->stop();
    if (!isVisible()) {
        progress_ = target;
        update();
        return;
    }
    // From where the knob is: a reversal mid-way turns back without jumping to an end first.
    const double distance = std::abs(target - progress_);
    animation_->setStartValue(progress_);
    animation_->setEndValue(target);
    animation_->setDuration(std::max(1, static_cast<int>(std::lround(kTravelMs * distance))));
    animation_->start();
}

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/) {
    const core::ToggleGeometry g = core::toggleGeometry(style_, knobPosition());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal border = style_.borderWidth;
    const QRectF track = QRectF(rect()).adjusted(border / 2.0, border / 2.0, -border / 2.0, -border / 2.0);
    const qreal trackRadius = std::clamp<qreal>(style_.radius - border / 2.0, 0, track.height() / 2.0);
    painter.setPen(border > 0 ? QPen(toQColor(style_.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(g.track));
    painter.drawRoundedRect(track, trackRadius, trackRadius);

    if (g.knobDiameter > 0) {
        const QRectF knob(g.knobLeft, g.knobTop, g.knobDiameter, g.knobDiameter);
        const qreal knobRadius = std::clamp<qreal>(style_.radius - style_.knobMargin, 0, g.knobDiameter / 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(toQColor(g.knob));
        painter.drawRoundedRect(knob, knobRadius, knobRadius);
    }
}

}  // namespace hitokoto::ui
