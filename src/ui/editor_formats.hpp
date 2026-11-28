// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/math.hpp"
#include "hitokoto/core/theme.hpp"
#include "math_render.hpp"

#include <QColor>
#include <QFont>
#include <QList>
#include <QStringList>
#include <QStringView>
#include <QTextCharFormat>
#include <QTextLayout>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

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
    /// Resolved thematic-break tokens (S023): the line the editor draws and its margin.
    [[nodiscard]] const core::ThematicBreakStyle& thematicBreak() const noexcept { return thematicBreak_; }
    /// Resolved properties tokens (S024) and the fonts of the properties block and its badge.
    [[nodiscard]] const core::PropertiesStyle& properties() const noexcept { return properties_; }
    [[nodiscard]] QFont propertiesFont() const;
    [[nodiscard]] QFont propertiesBadgeFont() const;
    [[nodiscard]] QFont tableFont(bool header) const;
    /// Font of the language badge on an opening fence (S021).
    [[nodiscard]] QFont badgeFont() const;
    [[nodiscard]] QTextCharFormat heading(int level) const;
    /// Resolved math tokens (S044, S046).
    [[nodiscard]] const core::MathStyle& math() const noexcept { return math_; }
    /// Pixels per em of a formula on a line of heading `level` (0 = body text): the text size times
    /// math.font-scale (S046, decision 3).
    [[nodiscard]] double mathPixelSize(int level) const;
    /// The layout of a formula on a line of heading `level`, from the theme's cache (S046); never
    /// null, and alive while held even if the cache drops it (S044 code review O1).
    [[nodiscard]] std::shared_ptr<const core::MathLayout> mathLayout(std::u16string_view source, int level,
                                                                     bool display) const;
    /// Whether `formula` on a line rendered as `render` is shown typeset: an inline formula whose
    /// dollars are concealed (S046, decision 1).
    [[nodiscard]] static bool typeset(const core::LineRender& render, const core::MathSpan& formula);
    /// UTF-16 units of the first character of a formula's source, the one that reserves its width:
    /// 2 for a surrogate pair, else 1 (S044 code review O4).
    [[nodiscard]] static qsizetype formulaHeadLength(QStringView source) noexcept;
    /// The cache of formula layouts, for tests.
    [[nodiscard]] const MathCache& mathCache() const noexcept { return *mathCache_; }

    /// Format ranges for one line, ready for QTextLayout::setFormats().
    [[nodiscard]] QList<QTextLayout::FormatRange> ranges(const core::LineRender& render, QStringView text = {}) const;
    /// The same ranges without the shortcuts ranges() takes for common lines: every piece of the
    /// line through the general path. A test compares the two.
    [[nodiscard]] QList<QTextLayout::FormatRange> rangesWithoutShortcuts(const core::LineRender& render,
                                                                         QStringView text = {}) const;

private:
    // What covers one piece of a line (rangesOf() cuts a line at every boundary of these).
    struct Piece {
        const core::StyleRun* run = nullptr;
        const core::InlineStyleRun* inlineRun = nullptr;
        const core::HiddenRange* hidden = nullptr;
        const core::LinkTarget* link = nullptr;
        bool bulletSpace = false;                   ///< the space after a bullet with a hanging indent (S003)
        bool inlineCodeDelimiter = false;           ///< a hidden backtick that opens or closes inline code
        const core::MathLayout* formula = nullptr;  ///< inside the source of a concealed formula (S046)
        bool formulaHead = false;                   ///< its first character, which reserves the width
        QStringView formulaSource{};                ///< the whole source of that formula
    };
    // The roles of runs (core::Role), each what it does to the format of its text: the table of
    // roles in composedFormat() points to them.
    void applyMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyCodeBlock(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyInlineCode(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyInlineCodeMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyEmphasisMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyEscapeMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyLinkText(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyLinkMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyMath(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyMathMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyListNumber(QTextCharFormat& format, const core::LineRender& render, int level) const;
    void applyListNumberMarker(QTextCharFormat& format, const core::LineRender& render, int level) const;
    [[nodiscard]] QList<QTextLayout::FormatRange> rangesOf(const core::LineRender& render, QStringView text,
                                                           bool shortcuts) const;
    /// The ranges of a line the shortcuts for whole lines give, if one applies.
    [[nodiscard]] std::optional<QList<QTextLayout::FormatRange>> wholeLineShortcut(
        const core::LineRender& render) const;
    /// The format of `piece`: with `shortcuts` a format made once per theme where the piece is a
    /// common one, else composedFormat(). `pieceText` is its text, if known (empty if not).
    [[nodiscard]] QTextCharFormat formatFor(const core::LineRender& render, const Piece& piece, QStringView pieceText,
                                            bool shortcuts) const;
    /// The one place that says how a piece looks: the line's heading level and code block, the
    /// role of the run (a table in editor_formats.cpp), a link, inline styles, concealment and the
    /// bullet space, in that order.
    [[nodiscard]] QTextCharFormat composedFormat(const core::LineRender& render, const Piece& piece,
                                                 QStringView pieceText) const;
    /// Font size of text at heading `level` (0 = body text).
    [[nodiscard]] double textSize(int level) const;
    /// The format of a piece of a typeset formula's source (S046, decisions 1, 2 and 5); nothing
    /// for a piece outside one.
    void applyFormula(QTextCharFormat& format, const Piece& piece, QStringView pieceText) const;
    /// The width Qt gives a formula's source with its first character in `head` and the rest
    /// concealed from `rest` (S046: measured with a layout of its own).
    [[nodiscard]] qreal laidOutWidth(QStringView source, const QTextCharFormat& head,
                                     const QTextCharFormat& rest) const;

    QFont baseFont_;
    std::array<QTextCharFormat, 7> heading_{};    // index 0 = body text
    std::array<QTextCharFormat, 7> keepSpace_{};  // transparent foreground at heading level
    std::array<QTextCharFormat, 7> collapse_{};   // 1px collapsed format at heading level
    QTextCharFormat bulletSpace_{};
    QTextCharFormat italic0_{};
    QColor marker_;
    QColor bullet_;
    double bulletSpacing_ = 0;
    double editorFontSize_ = 0;
    core::CodeStyle codeBlock_;
    core::CodeStyle inlineCode_;
    core::EmphasisStyle emphasis_;
    core::TableStyle table_;
    core::EscapeStyle escape_;
    core::LinkStyle link_;
    core::MathStyle math_;
    core::ThematicBreakStyle thematicBreak_;
    core::OrderedListStyle orderedList_;
    core::PropertiesStyle properties_;
    double pointToPixel_ = 1;  ///< pixels per point of the screen fonts are laid out for
    std::shared_ptr<MathCache> mathCache_ = std::make_shared<MathCache>();
    QStringList codeBlockFamilies_;
    QStringList inlineCodeFamilies_;
    /// Working buffer of ranges(): once grown, formatting a line allocates nothing (GUI thread
    /// only). Not a QVarLengthArray: its growth path made MSVC report C4702 in Release builds.
    mutable std::vector<std::uint32_t> boundaries_;
    /// The typeset formulas of the line rangesOf() works on (GUI thread only).
    struct Formula {
        core::Span source;
        std::shared_ptr<const core::MathLayout> layout;  ///< held for the line (S044 code review O1)
        QStringView text{};
    };
    mutable std::vector<Formula> formulas_;
};

}  // namespace hitokoto::ui
