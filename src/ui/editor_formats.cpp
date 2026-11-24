// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QTextDocument>

#include <algorithm>
#include <optional>

namespace hitokoto::ui {

namespace {
/// True if `column` is the backtick before or after an inline-code run.
bool isInlineCodeDelimiter(const core::LineRender& render, std::uint32_t column) {
    return std::any_of(render.runs.begin(), render.runs.end(), [column](const core::StyleRun& run) {
        return run.role == core::Role::InlineCode && (run.span.start == column + 1 || run.span.end() == column);
    });
}

// A style hint alone does not change an existing family, so code gets an explicit one.
QStringList codeFamilies(const std::string& family) {
    return family.empty() ? QFontDatabase::systemFont(QFontDatabase::FixedFont).families()
                          : QStringList{QString::fromStdString(family)};
}

void applyCodeFont(QTextCharFormat& format, const QStringList& families, double size) {
    format.setFontFamilies(families);
    format.setFontStyleHint(QFont::TypeWriter);
    format.setFontPointSize(size);
}

std::optional<core::Span> bulletSpaceSpan(const core::LineRender& render) {
    if (render.wrapIndent == 0) {
        return std::nullopt;
    }
    std::optional<std::uint32_t> markerChar;
    for (const auto& d : render.decorations) {
        if (d.kind == core::DecorationKind::Bullet) {
            markerChar = d.column;
            break;
        }
    }
    if (!markerChar.has_value()) {
        for (const auto& run : render.runs) {
            if (run.role == core::Role::Marker && run.span.length == 1 && run.span.start + 1 <= render.wrapIndent) {
                markerChar = run.span.start;
                break;
            }
        }
    }
    if (markerChar.has_value() && render.wrapIndent > *markerChar + 1) {
        return core::Span{*markerChar + 1, 1};
    }
    return std::nullopt;
}

/// The item of `items` (sorted, not overlapping, each with a `span`) that covers [start, end), if
/// any; `index` walks along with the pieces of a line, so a line is walked once.
template <class Item>
const Item* covering(const std::vector<Item>& items, std::size_t& index, std::uint32_t start, std::uint32_t end) {
    while (index < items.size() && items[index].span.end() <= start) {
        ++index;
    }
    return index < items.size() && items[index].span.start <= start && items[index].span.end() >= end ? &items[index]
                                                                                                      : nullptr;
}

QTextCharFormat zeroWidthFormat(const QTextCharFormat& base, QStringView text) {
    QTextCharFormat format = base;
    format.setForeground(Qt::transparent);  // theme-lint: allow
    format.clearProperty(QTextFormat::FontPointSize);
    format.setProperty(QTextFormat::FontPixelSize, 1);
    if (text.size() > 1) {
        // Qt has no invisible characters; cancel the 1 px glyph advances across this range.
        QFont font = base.font();
        font.setPixelSize(1);  // theme-lint: allow (concealment mechanism, not styling)
        const QFontMetricsF metrics(font);
        format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        format.setFontLetterSpacing(-metrics.horizontalAdvance(text.toString()) / static_cast<qreal>(text.size() - 1));
    }
    return format;
}
}  // namespace

EditorFormats::EditorFormats(const core::Theme& theme)
    : marker_(toQColor(theme.marker)),
      bullet_(toQColor(theme.bullet)),
      editorFontSize_(theme.editorFontSize),
      codeBlock_(core::codeBlockStyle(theme)),
      inlineCode_(core::inlineCodeStyle(theme)),
      emphasis_(core::emphasisStyle(theme)),
      table_(core::tableStyle(theme)),
      escape_(core::escapeStyle(theme)),
      link_(core::linkStyle(theme)),
      math_(core::mathStyle(theme)),
      thematicBreak_(core::thematicBreakStyle(theme)),
      properties_(core::propertiesStyle(theme)),
      codeBlockFamilies_(codeFamilies(codeBlock_.fontFamily)),
      inlineCodeFamilies_(codeFamilies(inlineCode_.fontFamily)) {
    baseFont_ = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!theme.editorFontFamily.empty()) {
        baseFont_.setFamilies({QString::fromStdString(theme.editorFontFamily)});
    }
    baseFont_.setPointSizeF(theme.editorFontSize);
    {
        // Pixels per point as Qt lays out fonts on this screen: a 72 pt font is that many pixels.
        QFont probe = baseFont_;
        probe.setPointSizeF(72);  // theme-lint: allow (a measurement, not a style)
        pointToPixel_ = QFontInfo(probe).pixelSize() / 72.0;
        if (pointToPixel_ <= 0) {
            pointToPixel_ = 1;
        }
    }
    const QFontMetricsF metrics(baseFont_);
    bulletSpacing_ = std::max<double>(3.0, std::round(metrics.horizontalAdvance(QLatin1Char(' ')) * 0.75));
    for (int level = 1; level <= 6; ++level) {
        QTextCharFormat f;
        f.setFontPointSize(theme.editorFontSize * theme.headingScale[static_cast<std::size_t>(level - 1)]);
        f.setFontWeight(theme.headingWeight);
        heading_[static_cast<std::size_t>(level)] = f;
    }
    // The formats of the most common pieces, which formatFor() and the shortcuts of rangesOf() hand
    // out without composing them again: composed here once per theme, so they cannot differ.
    const core::HiddenRange keep{{0, 1}, core::HideMode::KeepSpace};
    const core::HiddenRange collapse{{0, 1}, core::HideMode::Collapse};
    const core::InlineStyleRun italic{{0, 1}, core::InlineStyle::Italic};
    core::LineRender line;
    for (std::size_t level = 0; level <= 6; ++level) {
        line.headingLevel = static_cast<std::uint8_t>(level);
        keepSpace_[level] = composedFormat(line, Piece{.hidden = &keep}, {});
        collapse_[level] = composedFormat(line, Piece{.hidden = &collapse}, {});
    }
    line.headingLevel = 0;
    bulletSpace_ = composedFormat(line, Piece{.bulletSpace = true}, {});
    italic0_ = composedFormat(line, Piece{.inlineRun = &italic}, {});
}

QFont EditorFormats::badgeFont() const {
    QFont font = baseFont_;
    if (!codeBlock_.badgeFontFamily.empty()) {
        font.setFamilies({QString::fromStdString(codeBlock_.badgeFontFamily)});
    }
    font.setPointSizeF(editorFontSize_ * codeBlock_.badgeFontScale);
    return font;
}

QFont EditorFormats::propertiesFont() const {
    QFont font = baseFont_;
    if (!properties_.fontFamily.empty()) {
        font.setFamilies({QString::fromStdString(properties_.fontFamily)});
    }
    font.setPointSizeF(editorFontSize_ * properties_.fontScale);
    return font;
}

QFont EditorFormats::propertiesBadgeFont() const {
    QFont font = baseFont_;
    if (!properties_.badgeFontFamily.empty()) {
        font.setFamilies({QString::fromStdString(properties_.badgeFontFamily)});
    }
    font.setPointSizeF(editorFontSize_ * properties_.badgeFontScale);
    return font;
}

double EditorFormats::textSize(int level) const {
    return level >= 1 && level <= 6 ? heading_[static_cast<std::size_t>(level)].fontPointSize() : editorFontSize_;
}

QTextCharFormat EditorFormats::heading(int level) const {
    return heading_[static_cast<std::size_t>(level >= 1 && level <= 6 ? level : 0)];
}

QList<QTextLayout::FormatRange> EditorFormats::ranges(const core::LineRender& render, QStringView text) const {
    return rangesOf(render, text, true);
}

QList<QTextLayout::FormatRange> EditorFormats::rangesWithoutShortcuts(const core::LineRender& render,
                                                                      QStringView text) const {
    return rangesOf(render, text, false);
}

QList<QTextLayout::FormatRange> EditorFormats::rangesOf(const core::LineRender& render, QStringView text,
                                                        bool shortcuts) const {
    if (shortcuts) {
        if (auto whole = wholeLineShortcut(render)) {
            return *std::move(whole);
        }
    }
    // The general path: the line is cut at every boundary of a run, inline style, hidden range,
    // link and bullet space, and each piece takes the format of what covers it.
    auto& boundaries = boundaries_;
    boundaries.clear();
    const auto addSpans = [&boundaries](const auto& items) {
        for (const auto& item : items) {
            boundaries.push_back(item.span.start);
            boundaries.push_back(item.span.end());
        }
    };
    addSpans(render.runs);
    addSpans(render.inlineStyles);
    addSpans(render.hidden);
    addSpans(render.links);
    // S046: the source of a typeset formula is cut after its first character, which reserves the
    // formula's width.
    formulas_.clear();
    for (const core::MathSpan& m : render.math) {
        if (!typeset(render, m) || text.size() < static_cast<qsizetype>(m.source.end()) || m.source.length == 0) {
            continue;
        }
        const QStringView source = text.sliced(m.source.start, m.source.length);
        const std::u16string_view view(reinterpret_cast<const char16_t*>(source.utf16()),
                                       static_cast<std::size_t>(source.size()));
        formulas_.push_back({m.source, &mathLayout(view, render.headingLevel, m.display)});
        boundaries.push_back(m.source.start);
        boundaries.push_back(m.source.start + 1);
        boundaries.push_back(m.source.end());
    }
    const auto bulletSpace = bulletSpaceSpan(render);
    if (bulletSpace.has_value()) {
        boundaries.push_back(bulletSpace->start);
        boundaries.push_back(bulletSpace->end());
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    QList<QTextLayout::FormatRange> out;
    std::size_t runIndex = 0;
    std::size_t inlineIndex = 0;
    std::size_t hiddenIndex = 0;
    std::size_t linkIndex = 0;
    std::size_t formulaIndex = 0;
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
        const auto start = boundaries[i - 1];
        const auto end = boundaries[i];
        Piece piece{.run = covering(render.runs, runIndex, start, end),
                    .inlineRun = covering(render.inlineStyles, inlineIndex, start, end),
                    .hidden = covering(render.hidden, hiddenIndex, start, end),
                    .link = covering(render.links, linkIndex, start, end),
                    .bulletSpace = bulletSpace.has_value() && start >= bulletSpace->start && end <= bulletSpace->end()};
        if (piece.run == nullptr && piece.inlineRun == nullptr && piece.hidden == nullptr && piece.link == nullptr &&
            !piece.bulletSpace) {
            continue;
        }
        piece.inlineCodeDelimiter = piece.hidden != nullptr && isInlineCodeDelimiter(render, start);
        while (formulaIndex < formulas_.size() && formulas_[formulaIndex].source.end() <= start) {
            ++formulaIndex;
        }
        if (formulaIndex < formulas_.size() && formulas_[formulaIndex].source.start <= start &&
            formulas_[formulaIndex].source.end() >= end) {
            piece.formula = formulas_[formulaIndex].layout;
            piece.formulaHead = start == formulas_[formulaIndex].source.start;
        }
        const QStringView pieceText =
            text.size() >= static_cast<qsizetype>(end) ? text.sliced(start, end - start) : QStringView{};
        out.append(
            {static_cast<int>(start), static_cast<int>(end - start), formatFor(render, piece, pieceText, shortcuts)});
    }
    return out;
}

std::optional<QList<QTextLayout::FormatRange>> EditorFormats::wholeLineShortcut(const core::LineRender& render) const {
    // Nothing to format, a lone concealed bullet, a plain heading. Their formats are those
    // formatFor() gives the same pieces (made once per theme, see the constructor).
    const bool plain = render.headingLevel == 0 && !render.codeBlock && render.runs.empty() &&
                       render.inlineStyles.empty() && render.links.empty();
    if (plain && render.hidden.empty() && render.wrapIndent == 0) {
        return QList<QTextLayout::FormatRange>{};
    }
    if (plain && render.hidden.size() == 1 && render.decorations.size() == 1 &&
        render.decorations[0].kind == core::DecorationKind::Bullet &&
        render.hidden[0].mode == core::HideMode::KeepSpace &&
        render.hidden[0].span == (core::Span{render.decorations[0].column, 1})) {
        const auto col = static_cast<int>(render.decorations[0].column);
        if (render.wrapIndent > render.decorations[0].column + 1) {
            return QList<QTextLayout::FormatRange>{{col, 1, keepSpace_[0]}, {col + 1, 1, bulletSpace_}};
        }
        return QList<QTextLayout::FormatRange>{{col, 1, keepSpace_[0]}};
    }
    if (!render.codeBlock && render.inlineStyles.empty() && render.links.empty() && render.wrapIndent == 0 &&
        render.hidden.size() == 1 && render.runs.size() == 1 && render.runs[0].role == core::Role::HeadingText &&
        render.hidden[0].span.end() == render.runs[0].span.start) {
        const core::HiddenRange& hidden = render.hidden[0];
        const core::StyleRun& run = render.runs[0];
        const int lvl = run.level != 0 ? run.level : render.headingLevel;
        const auto idx = static_cast<std::size_t>(lvl >= 1 && lvl <= 6 ? lvl : 0);
        return QList<QTextLayout::FormatRange>{
            {static_cast<int>(hidden.span.start), static_cast<int>(hidden.span.length),
             hidden.mode == core::HideMode::KeepSpace ? keepSpace_[idx] : collapse_[idx]},
            {static_cast<int>(run.span.start), static_cast<int>(run.span.length), heading(lvl)}};
    }
    return std::nullopt;
}

QTextCharFormat EditorFormats::formatFor(const core::LineRender& render, const Piece& piece, QStringView pieceText,
                                         bool shortcuts) const {
    if (shortcuts) {
        // Shortcuts for the most common pieces: formats made once per theme by composedFormat().
        const bool plainLine = render.headingLevel == 0 && !render.codeBlock;
        if (piece.bulletSpace && piece.run == nullptr && piece.inlineRun == nullptr && piece.hidden == nullptr &&
            piece.link == nullptr && plainLine) {
            return bulletSpace_;
        }
        if (piece.hidden != nullptr && piece.run == nullptr && piece.link == nullptr && !render.codeBlock &&
            !piece.bulletSpace && !piece.inlineCodeDelimiter) {
            const auto idx = static_cast<std::size_t>(
                render.headingLevel >= 1 && render.headingLevel <= 6 ? render.headingLevel : 0);
            if (piece.hidden->mode == core::HideMode::ZeroWidth && !pieceText.isEmpty()) {
                return zeroWidthFormat(heading_[idx], pieceText);
            }
            return piece.hidden->mode == core::HideMode::KeepSpace ? keepSpace_[idx] : collapse_[idx];
        }
        if (piece.inlineRun != nullptr && piece.inlineRun->style == core::InlineStyle::Italic && piece.run == nullptr &&
            piece.hidden == nullptr && piece.link == nullptr && plainLine && !piece.bulletSpace) {
            return italic0_;
        }
        if (piece.run != nullptr && piece.run->role == core::Role::HeadingText && piece.inlineRun == nullptr &&
            piece.hidden == nullptr && piece.link == nullptr && !render.codeBlock && !piece.bulletSpace) {
            return heading(piece.run->level != 0 ? piece.run->level : render.headingLevel);
        }
    }
    return composedFormat(render, piece, pieceText);
}

namespace {
/// What a role does to the format of its text (the role chain of EditorFormats).
struct RoleFormat {
    core::Role role;
    void (EditorFormats::*apply)(QTextCharFormat&, const core::LineRender&, int level) const;
    bool takesInlineStyles;    ///< bold, italic and strikethrough apply to the text
    bool takesEmphasisColour;  ///< and so do their colours
};
}  // namespace

QTextCharFormat EditorFormats::composedFormat(const core::LineRender& render, const Piece& piece,
                                              QStringView pieceText) const {
    static constexpr std::array<RoleFormat, 11> kRoles{{
        {core::Role::Marker, &EditorFormats::applyMarker, true, false},
        {core::Role::HeadingText, nullptr, true, true},
        {core::Role::CodeBlock, &EditorFormats::applyCodeBlock, true, true},
        {core::Role::InlineCode, &EditorFormats::applyInlineCode, false, false},
        {core::Role::InlineCodeMarker, &EditorFormats::applyInlineCodeMarker, true, true},
        {core::Role::EmphasisMarker, &EditorFormats::applyEmphasisMarker, true, true},
        {core::Role::EscapeMarker, &EditorFormats::applyEscapeMarker, true, false},
        {core::Role::LinkText, &EditorFormats::applyLinkText, true, true},
        {core::Role::LinkMarker, &EditorFormats::applyLinkMarker, true, false},
        // S044: formula source keeps its look inside emphasis, as inline code does.
        {core::Role::Math, &EditorFormats::applyMath, false, false},
        {core::Role::MathMarker, &EditorFormats::applyMathMarker, true, false},
    }};
    static_assert(kRoles.size() == static_cast<std::size_t>(core::Role::MathMarker) + 1, "a row for every role");
    static_assert(
        [] {
            for (std::size_t i = 0; i < kRoles.size(); ++i) {
                if (static_cast<std::size_t>(kRoles[i].role) != i) {
                    return false;
                }
            }
            return true;
        }(),
        "the rows in the order of core::Role");

    const core::StyleRun* run = piece.run;
    const RoleFormat* role = run != nullptr ? &kRoles[static_cast<std::size_t>(run->role)] : nullptr;
    const int level = run != nullptr && run->level != 0 ? run->level : render.headingLevel;
    QTextCharFormat format = heading(level);
    if (render.codeBlock) {
        applyCodeFont(format, codeBlockFamilies_, editorFontSize_ * codeBlock_.fontScale);
    }
    if (role != nullptr && role->apply != nullptr) {
        (this->*role->apply)(format, render, level);
    }
    if (piece.link != nullptr) {
        format.setAnchor(true);
        format.setAnchorHref(QString::fromStdU16String(piece.link->destination));
        if (!piece.link->title.empty()) {
            // As its characters, wherever Qt shows the tooltip of the format: Qt shows text that
            // looks like rich text as rich text.
            format.setToolTip(
                Qt::convertFromPlainText(QString::fromStdU16String(piece.link->title), Qt::WhiteSpacePre));
        }
    }
    if (piece.inlineRun != nullptr && (role == nullptr || role->takesInlineStyles)) {
        const auto styles = piece.inlineRun->style;
        if (core::hasStyle(styles, core::InlineStyle::Bold)) {
            // Never lighter than the surrounding text: in a heading the bold weight applies only
            // when it is above headings.weight (docs/theming.md).
            format.setFontWeight(std::max(format.fontWeight(), emphasis_.boldWeight));
        }
        if (core::hasStyle(styles, core::InlineStyle::Italic)) {
            format.setFontItalic(true);
        }
        if (core::hasStyle(styles, core::InlineStyle::Strikethrough)) {
            format.setFontStrikeOut(true);
        }
        // Precedence: bold, italic, strikethrough (docs/theming.md).
        const std::optional<core::Color> color =
            core::hasStyle(styles, core::InlineStyle::Bold) && emphasis_.boldText       ? emphasis_.boldText
            : core::hasStyle(styles, core::InlineStyle::Italic) && emphasis_.italicText ? emphasis_.italicText
            : core::hasStyle(styles, core::InlineStyle::Strikethrough)                  ? emphasis_.strikethroughText
                                                                                        : std::nullopt;
        if (color && (role == nullptr || role->takesEmphasisColour)) {
            format.setForeground(toQColor(*color));
        }
    }
    if (piece.hidden != nullptr) {
        if ((run == nullptr || run->role != core::Role::CodeBlock) && !render.codeBlock) {
            format = heading(render.headingLevel);
        }
        if (piece.inlineCodeDelimiter) {
            // Reserves inline-code padding after each concealed backtick (theme token, ADR 0004).
            format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
            format.setFontLetterSpacing(inlineCode_.padding);
        }
        // Concealment mechanism (ADR 0001), not a styling value.
        format.setForeground(Qt::transparent);  // theme-lint: allow
        if (piece.hidden->mode == core::HideMode::Collapse) {
            format.clearProperty(QTextFormat::FontPointSize);
            format.setProperty(QTextFormat::FontPixelSize, 1);
        } else if (piece.hidden->mode == core::HideMode::ZeroWidth && !pieceText.isEmpty()) {
            format = zeroWidthFormat(format, pieceText);
        }
    }
    if (piece.bulletSpace) {
        format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        format.setFontLetterSpacing(bulletSpacing_);
    }
    applyFormula(format, piece, pieceText);
    return format;
}

void EditorFormats::applyFormula(QTextCharFormat& format, const Piece& piece, QStringView pieceText) const {
    if (piece.formula == nullptr) {
        return;
    }
    const core::MathLayout& layout = *piece.formula;
    if (layout.error) {
        // Decision 5: the source stays, in the error colour, with the reason.
        format.setForeground(toQColor(math_.error));
        format.setToolTip(Qt::convertFromPlainText(QString::fromStdString(layout.error->message)));
        return;
    }
    if (!piece.formulaHead) {
        format = zeroWidthFormat(format, pieceText);
        return;
    }
    // Decisions 1 and 2: the first character is invisible and takes the formula's width; its font
    // grows until its ascent and descent cover the formula, so the line makes room.
    format.setForeground(Qt::transparent);  // theme-lint: allow (concealment mechanism)
    QFont font = format.font();
    const QFontMetricsF metrics(font);
    const double scale = std::max({1.0, metrics.ascent() > 0 ? layout.ascent / metrics.ascent() : 1.0,
                                   metrics.descent() > 0 ? layout.descent / metrics.descent() : 1.0});
    if (scale > 1.0) {
        format.setFontPointSize(font.pointSizeF() * scale);
        font.setPointSizeF(font.pointSizeF() * scale);
    }
    const qreal advance = pieceText.isEmpty() ? 0 : QFontMetricsF(font).horizontalAdvance(pieceText.toString());
    format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    format.setFontLetterSpacing(layout.width - advance);
}

double EditorFormats::mathPixelSize(int level) const {
    return textSize(level) * math_.fontScale * pointToPixel_;
}

const core::MathLayout& EditorFormats::mathLayout(std::u16string_view source, int level, bool display) const {
    return mathCache_->layout(source, mathPixelSize(level), display);
}

bool EditorFormats::typeset(const core::LineRender& render, const core::MathSpan& formula) {
    if (render.mathBlock || formula.source.start == 0) {
        return false;
    }
    // Concealed: the dollar before the source is hidden (S044 hides it only off the caret's line).
    const std::uint32_t dollar = formula.source.start - 1;
    return std::any_of(render.hidden.begin(), render.hidden.end(), [dollar](const core::HiddenRange& h) {
        return dollar >= h.span.start && dollar < h.span.end();
    });
}

void EditorFormats::applyMarker(QTextCharFormat& format, const core::LineRender& render, int /*level*/) const {
    // A revealed thematic break has its own marker token (S023); a setext underline uses
    // colors.marker like an ATX heading's '#'.
    format.setForeground(render.thematicBreak ? toQColor(thematicBreak_.marker) : marker_);
}

void EditorFormats::applyCodeBlock(QTextCharFormat& format, const core::LineRender& render, int /*level*/) const {
    format.setForeground(toQColor(render.fenceLine ? codeBlock_.marker : codeBlock_.text));
}

void EditorFormats::applyInlineCode(QTextCharFormat& format, const core::LineRender& /*render*/, int level) const {
    // The background is a box painted by EditorWidget (radius, border, padding).
    applyCodeFont(format, inlineCodeFamilies_, textSize(level) * inlineCode_.fontScale);
    format.setForeground(toQColor(inlineCode_.text));
}

void EditorFormats::applyInlineCodeMarker(QTextCharFormat& format, const core::LineRender& /*render*/,
                                          int level) const {
    applyCodeFont(format, inlineCodeFamilies_, textSize(level) * inlineCode_.fontScale);
    format.setForeground(toQColor(inlineCode_.marker));
}

void EditorFormats::applyEmphasisMarker(QTextCharFormat& format, const core::LineRender& /*render*/,
                                        int /*level*/) const {
    format.setForeground(toQColor(emphasis_.marker));
}

void EditorFormats::applyEscapeMarker(QTextCharFormat& format, const core::LineRender& /*render*/,
                                      int /*level*/) const {
    format.setForeground(toQColor(escape_.marker));
}

void EditorFormats::applyLinkText(QTextCharFormat& format, const core::LineRender& /*render*/, int /*level*/) const {
    format.setForeground(toQColor(link_.text));
}

void EditorFormats::applyLinkMarker(QTextCharFormat& format, const core::LineRender& /*render*/, int /*level*/) const {
    format.setForeground(toQColor(link_.marker));
}

void EditorFormats::applyMath(QTextCharFormat& format, const core::LineRender& /*render*/, int level) const {
    if (!math_.fontFamily.empty()) {
        format.setFontFamilies({QString::fromStdString(math_.fontFamily)});
    }
    format.setFontPointSize(textSize(level) * math_.fontScale);
    format.setForeground(toQColor(math_.text));
}

void EditorFormats::applyMathMarker(QTextCharFormat& format, const core::LineRender& render, int level) const {
    applyMath(format, render, level);
    format.setForeground(toQColor(math_.marker));
}

QFont EditorFormats::tableFont(bool header) const {
    QFont font = baseFont_;
    if (!table_.fontFamily.empty()) {
        font.setFamilies({QString::fromStdString(table_.fontFamily)});
    }
    font.setPointSizeF(editorFontSize_ * table_.fontScale);
    if (header) {
        font.setWeight(static_cast<QFont::Weight>(table_.headerWeight));
    }
    return font;
}

}  // namespace hitokoto::ui
