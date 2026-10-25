// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>

#include <algorithm>
#include <vector>

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
    std::vector<std::uint32_t> boundaries;
    boundaries.reserve((render.runs.size() + render.inlineStyles.size() + render.hidden.size()) * 2);
    for (const auto& run : render.runs) {
        boundaries.push_back(run.span.start);
        boundaries.push_back(run.span.end());
    }
    for (const auto& run : render.inlineStyles) {
        boundaries.push_back(run.span.start);
        boundaries.push_back(run.span.end());
    }
    for (const auto& hidden : render.hidden) {
        boundaries.push_back(hidden.span.start);
        boundaries.push_back(hidden.span.end());
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    std::size_t runIndex = 0;
    std::size_t inlineIndex = 0;
    std::size_t hiddenIndex = 0;
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
        const auto start = boundaries[i - 1];
        const auto end = boundaries[i];
        if (start == end) {
            continue;
        }

        while (runIndex < render.runs.size() && render.runs[runIndex].span.end() <= start) {
            ++runIndex;
        }
        while (inlineIndex < render.inlineStyles.size() && render.inlineStyles[inlineIndex].span.end() <= start) {
            ++inlineIndex;
        }
        while (hiddenIndex < render.hidden.size() && render.hidden[hiddenIndex].span.end() <= start) {
            ++hiddenIndex;
        }
        const auto* baseRun = runIndex < render.runs.size() && render.runs[runIndex].span.start <= start &&
                                      render.runs[runIndex].span.end() >= end
                                  ? &render.runs[runIndex]
                                  : nullptr;
        const auto* inlineRun = inlineIndex < render.inlineStyles.size() &&
                                        render.inlineStyles[inlineIndex].span.start <= start &&
                                        render.inlineStyles[inlineIndex].span.end() >= end
                                    ? &render.inlineStyles[inlineIndex]
                                    : nullptr;
        const auto* hidden = hiddenIndex < render.hidden.size() && render.hidden[hiddenIndex].span.start <= start &&
                                     render.hidden[hiddenIndex].span.end() >= end
                                 ? &render.hidden[hiddenIndex]
                                 : nullptr;
        if (baseRun == nullptr && inlineRun == nullptr && hidden == nullptr) {
            continue;
        }

        QTextCharFormat format =
            baseRun != nullptr ? heading(baseRun->level != 0 ? baseRun->level : render.headingLevel) : lineFormat;
        if (baseRun != nullptr && baseRun->role == core::Role::Marker) {
            format.setForeground(marker_);
        }
        if (inlineRun != nullptr) {
            if (core::hasStyle(inlineRun->style, core::InlineStyle::Bold)) {
                format.setFontWeight(QFont::Bold);
            }
            if (core::hasStyle(inlineRun->style, core::InlineStyle::Italic)) {
                format.setFontItalic(true);
            }
            if (core::hasStyle(inlineRun->style, core::InlineStyle::Strikethrough)) {
                format.setFontStrikeOut(true);
            }
        }
        if (hidden != nullptr) {
            format = lineFormat;
            // Concealment mechanism (ADR 0001), not a styling value.
            format.setForeground(Qt::transparent);  // theme-lint: allow
            if (hidden->mode == core::HideMode::Collapse) {
                format.clearProperty(QTextFormat::FontPointSize);
                format.setProperty(QTextFormat::FontPixelSize, 1);
            }
        }
        out.append({static_cast<int>(start), static_cast<int>(end - start), format});
    }
    return out;
}

}  // namespace hitokoto::ui
