// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QAbstractButton>
#include <QEasingCurve>
#include <QVariantAnimation>

class QColor;

namespace hitokoto::ui {

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    void applyTheme(const core::Theme& theme);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setKnobPosition(qreal position) noexcept;

    QVariantAnimation animation_;
    QColor trackOff_;
    QColor trackOn_;
    QColor knobColor_;
    QColor borderColor_;
    qreal knobPosition_ = 0.0;
    qreal radius_ = 8.0;
    qreal borderWidth_ = 1.0;
};

}  // namespace hitokoto::ui
