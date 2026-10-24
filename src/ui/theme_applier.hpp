// SPDX-License-Identifier: GPL-3.0-or-later
// Maps theme tokens onto Qt's own styling system: Fusion style + QPalette + a generated style
// sheet. Users never write QSS; the theme file is the single source (ADR 0001).
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QPalette>
#include <QString>

class QApplication;

namespace hitokoto::ui {

QPalette makePalette(const core::Theme& theme);
QString makeStyleSheet(const core::Theme& theme);

/// Applies style, palette, application font and style sheet.
void applyThemeToApplication(QApplication& app, const core::Theme& theme);

}  // namespace hitokoto::ui
