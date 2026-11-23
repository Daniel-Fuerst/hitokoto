// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/toggle_switch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hitokoto::core {
namespace {

std::uint8_t blendChannel(std::uint8_t from, std::uint8_t to, double t) {
    const double v = static_cast<double>(from) + (static_cast<double>(to) - static_cast<double>(from)) * t;
    return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}

}  // namespace

double easeInOut(double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    if (t < 0.5) {
        return 4.0 * t * t * t;
    }
    const double u = -2.0 * t + 2.0;
    return 1.0 - u * u * u / 2.0;
}

Color blendColors(const Color& from, const Color& to, double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    return {blendChannel(from.r, to.r, t), blendChannel(from.g, to.g, t), blendChannel(from.b, to.b, t),
            blendChannel(from.a, to.a, t)};
}

ToggleGeometry toggleGeometry(const SwitchStyle& style, double position) noexcept {
    position = std::clamp(position, 0.0, 1.0);
    ToggleGeometry g;
    g.knobDiameter = std::max(0.0, style.height - 2.0 * style.knobMargin);
    g.knobTop = style.knobMargin;
    const double start = style.knobMargin;
    const double end = std::max(start, style.width - style.knobMargin - g.knobDiameter);
    g.knobLeft = start + (end - start) * position;
    g.track = blendColors(style.track, style.checkedTrack, position);
    g.knob = blendColors(style.knob, style.checkedKnob, position);
    return g;
}

}  // namespace hitokoto::core
