// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <QColor>
#include <QFont>
#include <QList>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextLayout>

#include <array>

namespace hitokoto::ui {

/// Text formats derived from a theme; maps core::LineRender to Qt format ranges.
class EditorFormats {
public:
    EditorFormats() = default;
    explicit EditorFormats(const core::Theme& theme);

    [[nodiscard]] const QFont& baseFont() const noexcept { return baseFont_; }
    [[nodiscard]] const QColor& bulletColor() const noexcept { return bullet_; }
    [[nodiscard]] const QColor& codeBackground() const noexcept { return codeBackground_; }
    [[nodiscard]] double codePanelPadding() const noexcept { return codePanelPadding_; }
    [[nodiscard]] double codePanelRadius() const noexcept { return codePanelRadius_; }
    [[nodiscard]] QTextCharFormat heading(int level) const;

    /// Format ranges for one line, ready for QTextLayout::setFormats().
    [[nodiscard]] QList<QTextLayout::FormatRange> ranges(const core::LineRender& render) const;

private:
    QFont baseFont_;
    std::array<QTextCharFormat, 7> heading_{};  // index 0 = body text
    QColor marker_;
    QColor bullet_;
    QColor codeBackground_;
    QColor codeForeground_;
    QStringList codeFamilies_;
    double codePanelPadding_ = 0;
    double codePanelRadius_ = 0;
};

}  // namespace hitokoto::ui
