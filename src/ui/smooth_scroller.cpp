// SPDX-License-Identifier: GPL-3.0-or-later
#include "smooth_scroller.hpp"

#include <QScrollBar>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

SmoothScroller::SmoothScroller(QScrollBar* scrollBar, QObject* parent)
    : QObject(parent),
      scrollBar_(scrollBar),
      target_(scrollBar != nullptr ? scrollBar->value() : 0.0),
      lastMin_(scrollBar != nullptr ? scrollBar->minimum() : 0),
      lastMax_(scrollBar != nullptr ? scrollBar->maximum() : 0) {
    animation_.setEasingCurve(easingCurve_);
    connect(&animation_, &QVariantAnimation::valueChanged, this, &SmoothScroller::onValueChanged);
    connect(&animation_, &QVariantAnimation::finished, this, &SmoothScroller::onFinished);

    if (scrollBar_ != nullptr) {
        connect(scrollBar_, &QAbstractSlider::sliderPressed, this, &SmoothScroller::stop);
        connect(scrollBar_, &QAbstractSlider::actionTriggered, this, [this](int /*action*/) {
            if (!updating_) {
                stop();
            }
        });
        connect(scrollBar_, &QAbstractSlider::valueChanged, this, [this](int val) {
            if (!updating_ && animation_.state() != QAbstractAnimation::Running) {
                target_ = static_cast<double>(val);
            }
        });
        connect(scrollBar_, &QAbstractSlider::rangeChanged, this, [this](int min, int max) {
            if (animation_.state() == QAbstractAnimation::Running) {
                if (target_ >= static_cast<double>(lastMax_) && max > lastMax_) {
                    target_ = static_cast<double>(max);
                    animation_.setEndValue(target_);
                } else if (target_ <= static_cast<double>(lastMin_) && min < lastMin_) {
                    target_ = static_cast<double>(min);
                    animation_.setEndValue(target_);
                } else {
                    target_ = std::clamp(target_, static_cast<double>(min), static_cast<double>(max));
                    animation_.setEndValue(target_);
                }
            } else {
                target_ = std::clamp(target_, static_cast<double>(min), static_cast<double>(max));
            }
            lastMin_ = min;
            lastMax_ = max;
        });
    }
}

SmoothScroller::~SmoothScroller() {
    animation_.stop();
}

bool SmoothScroller::isRunning() const {
    return animation_.state() == QAbstractAnimation::Running;
}

void SmoothScroller::stop() {
    if (animation_.state() == QAbstractAnimation::Running) {
        animation_.stop();
        if (scrollBar_ != nullptr) {
            target_ = static_cast<double>(scrollBar_->value());
        }
        emit scrollFinished();
    }
}

void SmoothScroller::shift(double delta) {
    if (std::abs(delta) < 0.001) {
        return;
    }
    target_ += delta;
    if (scrollBar_ != nullptr) {
        target_ =
            std::clamp(target_, static_cast<double>(scrollBar_->minimum()), static_cast<double>(scrollBar_->maximum()));
    }
    if (animation_.state() == QAbstractAnimation::Running) {
        const qreal curStart = animation_.startValue().toReal();
        const qreal curEnd = animation_.endValue().toReal();
        animation_.setStartValue(curStart + delta);
        animation_.setEndValue(curEnd + delta);
    }
}

void SmoothScroller::scroll(double deltaPixels) {
    if (scrollBar_ == nullptr || std::abs(deltaPixels) < 0.001) {
        return;
    }
    const int min = scrollBar_->minimum();
    const int max = scrollBar_->maximum();
    lastMin_ = min;
    lastMax_ = max;
    if (min >= max) {
        return;
    }

    const double current = static_cast<double>(scrollBar_->value());
    const bool running = (animation_.state() == QAbstractAnimation::Running);

    if (running) {
        const double remaining = target_ - current;
        const bool sameDirection = (deltaPixels > 0 && remaining >= 0) || (deltaPixels < 0 && remaining <= 0);
        if (sameDirection) {
            target_ += deltaPixels;
        } else {
            // Immediate reversal from current position
            target_ = current + deltaPixels;
        }
    } else {
        target_ = current + deltaPixels;
    }

    target_ = std::clamp(target_, static_cast<double>(min), static_cast<double>(max));

    if (std::lround(target_) == scrollBar_->value()) {
        if (running) {
            stop();
        }
        return;
    }

    animation_.stop();
    animation_.setStartValue(current);
    animation_.setEndValue(target_);
    animation_.setDuration(durationMs_);
    animation_.setEasingCurve(easingCurve_);
    animation_.start();
    emit scrollStarted();
}

void SmoothScroller::onValueChanged(const QVariant& value) {
    if (scrollBar_ == nullptr) {
        return;
    }
    const int newPos =
        std::clamp(static_cast<int>(std::lround(value.toReal())), scrollBar_->minimum(), scrollBar_->maximum());
    if (newPos != scrollBar_->value()) {
        updating_ = true;
        scrollBar_->setValue(newPos);
        updating_ = false;
    }
}

void SmoothScroller::onFinished() {
    if (scrollBar_ == nullptr) {
        return;
    }
    const int finalPos =
        std::clamp(static_cast<int>(std::lround(target_)), scrollBar_->minimum(), scrollBar_->maximum());
    if (finalPos != scrollBar_->value()) {
        updating_ = true;
        scrollBar_->setValue(finalPos);
        updating_ = false;
    }
    emit scrollFinished();
}

}  // namespace hitokoto::ui
