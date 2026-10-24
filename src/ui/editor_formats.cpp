// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>

namespace hitokoto::ui {

EditorFormats::EditorFormats(const core::Theme& theme)
    : marker_(toQColor(theme.marker)), bullet_(toQColor(theme.bullet)) {
    baseFont_ = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!theme.editorFontFamily.empty()) {
        baseFont_.setFamilies({QString::fromStdString(theme.editorFontFamily)});
    }
    baseFont_.setPointSizeF(theme.editorFontSize);
    for (int level = 1; level <= 6; ++level) {
        QTextCharFormat f;
        f.setFontPointSize(theme.editorFontSize * theme.headingScale[static_cast<std::size_t>(level - 1)]);
        f.setFontWeight(theme.headingWeight);
        heading_[static_cast<std::size_t>(level)] = f;
    }
}

QTextCharFormat EditorFormats::heading(int level) const {
    return heading_[static_cast<std::size_t>(level >= 1 && level <= 6 ? level : 0)];
}

QList<QTextLayout::FormatRange> EditorFormats::ranges(const core::LineRender& render) const {
    QList<QTextLayout::FormatRange> out;
    const QTextCharFormat lineFormat = heading(render.headingLevel);
    for (const auto& run : render.runs) {
        QTextCharFormat f = heading(run.level != 0 ? run.level : render.headingLevel);
        if (run.role == core::Role::Marker) {
            f.setForeground(marker_);
        }
        out.append({static_cast<int>(run.span.start), static_cast<int>(run.span.length), f});
    }
    for (const auto& hidden : render.hidden) {
        QTextCharFormat f = lineFormat;
        // Concealment mechanism (ADR 0001), not a styling value.
        f.setForeground(Qt::transparent);  // theme-lint: allow
        if (hidden.mode == core::HideMode::Collapse) {
            f.clearProperty(QTextFormat::FontPointSize);
            f.setProperty(QTextFormat::FontPixelSize, 1);
        }
        out.append({static_cast<int>(hidden.span.start), static_cast<int>(hidden.span.length), f});
    }
    return out;
}

}  // namespace hitokoto::ui
