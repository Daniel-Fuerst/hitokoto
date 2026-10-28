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
    /// Resolved element tokens (ADR 0004) for what the editor paints itself (panels, boxes).
    [[nodiscard]] const core::CodeStyle& codeBlock() const noexcept { return codeBlock_; }
    [[nodiscard]] const core::CodeStyle& inlineCode() const noexcept { return inlineCode_; }
    /// Resolved table tokens (ADR 0005) and the font of table cells (S018).
    [[nodiscard]] const core::TableStyle& table() const noexcept { return table_; }
    [[nodiscard]] QFont tableFont(bool header) const;
    [[nodiscard]] QTextCharFormat heading(int level) const;

    /// Format ranges for one line, ready for QTextLayout::setFormats().
    [[nodiscard]] QList<QTextLayout::FormatRange> ranges(const core::LineRender& render) const;

private:
    /// Font size of text at heading `level` (0 = body text).
    [[nodiscard]] double textSize(int level) const;

    QFont baseFont_;
    std::array<QTextCharFormat, 7> heading_{};  // index 0 = body text
    QColor marker_;
    QColor bullet_;
    double editorFontSize_ = 0;
    core::CodeStyle codeBlock_;
    core::CodeStyle inlineCode_;
    core::EmphasisStyle emphasis_;
    core::TableStyle table_;
    core::EscapeStyle escape_;
    QStringList codeBlockFamilies_;
    QStringList inlineCodeFamilies_;
};

}  // namespace hitokoto::ui
