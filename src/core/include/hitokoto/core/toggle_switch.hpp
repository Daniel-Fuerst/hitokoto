// SPDX-License-Identifier: GPL-3.0-or-later
// S043: where the knob of a toggle switch stands and which colours it shows during its animation.
#pragma once

#include "hitokoto/core/theme.hpp"

namespace hitokoto::core {

/// Ease-in-out of a linear progress `t` (clamped to 0..1): slow at both ends, fast in the middle
/// (a cubic, as Qt's InOutCubic), so the knob starts gently and settles gently.
[[nodiscard]] double easeInOut(double t) noexcept;

/// The painted parts of a switch, in pixels from its top-left corner.
struct ToggleGeometry {
    double knobLeft = 0;
    double knobTop = 0;
    double knobDiameter = 0;
    Color track;  ///< blended between the off and the on colour
    Color knob;
};

/// The switch at `position` (0 = off, 1 = on; values between are taken as they are, already eased).
[[nodiscard]] ToggleGeometry toggleGeometry(const SwitchStyle& style, double position) noexcept;

/// `from` blended towards `to` by `t` (0..1), each channel rounded.
[[nodiscard]] Color blendColors(const Color& from, const Color& to, double t) noexcept;

}  // namespace hitokoto::core
