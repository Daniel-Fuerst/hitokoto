// SPDX-License-Identifier: GPL-3.0-or-later
#include "toggle_switch.hpp"

#include "qt_bridge.hpp"

#include <QPaintEvent>
#include <QPainter>

#include <algorithm>

namespace hitokoto::ui {

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(42, 24);
    knobPosition_ = 1.0;
    setChecked(true);

    animation_.setDuration(300);
    animation_.setEasingCurve(QEasingCurve::OutElastic);
    connect(&animation_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { setKnobPosition(value.toReal()); });
    connect(this, &QAbstractButton::toggled, this, [this](bool) {
        animation_.stop();
        animation_.setStartValue(knobPosition_);
        animation_.setEndValue(isChecked() ? 1.0 : 0.0);
        animation_.start();
    });

    applyTheme(core::builtinTheme(core::ColorScheme::Light));
}

void ToggleSwitch::applyTheme(const core::Theme& theme) {
    trackOff_ = toQColor(theme.border);
    trackOn_ = toQColor(theme.accent);
    knobColor_ = toQColor(theme.background);
    borderColor_ = toQColor(theme.border);
    radius_ = std::max<qreal>(8.0, core::toolbarStyle(theme).radius);
    borderWidth_ = std::max<qreal>(0.0, core::toolbarStyle(theme).borderWidth);
    update();
}

QSize ToggleSwitch::sizeHint() const {
    return QSize(42, 24);
}

void ToggleSwitch::setKnobPosition(qreal position) noexcept {
    knobPosition_ = position;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect();
    const qreal radius = std::min({bounds.height() / 2.0, radius_, bounds.width() / 2.0});
    painter.setPen(borderWidth_ > 0.0 ? QPen(borderColor_, borderWidth_) : QPen(Qt::NoPen));
    painter.setBrush(isChecked() ? trackOn_ : trackOff_);
    painter.drawRoundedRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    const qreal margin = 3.0;
    const qreal diameter = bounds.height() - margin * 2.0;
    const qreal travel = bounds.width() - margin * 2.0 - diameter;
    const qreal centerX = margin + diameter / 2.0 + travel * std::clamp(knobPosition_, 0.0, 1.0);
    const qreal centerY = bounds.height() / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(knobColor_);
    painter.drawEllipse(QPointF(centerX, centerY), diameter / 2.0, diameter / 2.0);
}

}  // namespace hitokoto::ui
