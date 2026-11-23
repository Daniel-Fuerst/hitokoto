// SPDX-License-Identifier: GPL-3.0-or-later
// S043: an on/off switch for the settings flyout, with a knob that slides with an ease-in-out motion.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QAbstractButton>

class QVariantAnimation;

namespace hitokoto::ui {

/// A checkable button painted as a track with a knob. Qt Widgets has no switch control, and a styled
/// QCheckBox cannot animate its indicator (S043, decision 1). Where the knob stands and which colours
/// it shows come from the core (toggleGeometry, easeInOut); this class only drives and paints them.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    /// The knob's travel from one end to the other; a shorter way takes a share of it.
    static constexpr int kTravelMs = 160;

    explicit ToggleSwitch(QWidget* parent = nullptr);
    ~ToggleSwitch() override = default;
    ToggleSwitch(const ToggleSwitch&) = delete;
    ToggleSwitch& operator=(const ToggleSwitch&) = delete;

    void setSwitchStyle(const core::SwitchStyle& style);
    [[nodiscard]] const core::SwitchStyle& switchStyle() const noexcept { return style_; }
    /// Sets the state without animation and without emitting toggled() (a state the program restores).
    void setCheckedImmediately(bool on);

    /// 0 = off, 1 = on, linear in time while the knob moves.
    [[nodiscard]] double linearProgress() const noexcept { return progress_; }
    /// Where the knob stands between its ends (0 = left, 1 = right): the eased progress.
    [[nodiscard]] double knobPosition() const noexcept;
    [[nodiscard]] bool isAnimating() const;
    [[nodiscard]] core::Color trackColor() const noexcept;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    [[nodiscard]] bool hitButton(const QPoint& pos) const override;

private:
    void animateTo(bool on);

    core::SwitchStyle style_;
    QVariantAnimation* animation_ = nullptr;
    double progress_ = 0;
};

}  // namespace hitokoto::ui
