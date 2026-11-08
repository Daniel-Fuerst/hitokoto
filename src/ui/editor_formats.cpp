// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>

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
      thematicBreak_(core::thematicBreakStyle(theme)),
      properties_(core::propertiesStyle(theme)),
      codeBlockFamilies_(codeFamilies(codeBlock_.fontFamily)),
      inlineCodeFamilies_(codeFamilies(inlineCode_.fontFamily)) {
    baseFont_ = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!theme.editorFontFamily.empty()) {
        baseFont_.setFamilies({QString::fromStdString(theme.editorFontFamily)});
    }
    baseFont_.setPointSizeF(theme.editorFontSize);
    const QFontMetricsF metrics(baseFont_);
    bulletSpacing_ = std::max<double>(3.0, std::round(metrics.horizontalAdvance(QLatin1Char(' ')) * 0.75));
    for (int level = 1; level <= 6; ++level) {
        QTextCharFormat f;
        f.setFontPointSize(theme.editorFontSize * theme.headingScale[static_cast<std::size_t>(level - 1)]);
        f.setFontWeight(theme.headingWeight);
        heading_[static_cast<std::size_t>(level)] = f;
    }
    for (std::size_t level = 0; level <= 6; ++level) {
        QTextCharFormat ks = heading_[level];
        ks.setForeground(Qt::transparent);  // theme-lint: allow
        keepSpace_[level] = ks;
        QTextCharFormat col = ks;
        col.clearProperty(QTextFormat::FontPointSize);
        col.setProperty(QTextFormat::FontPixelSize, 1);
        collapse_[level] = col;
    }
    bulletSpace_ = heading_[0];
    bulletSpace_.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    bulletSpace_.setFontLetterSpacing(bulletSpacing_);
    italic0_ = heading_[0];
    italic0_.setFontItalic(true);
    if (emphasis_.italicText) {
        italic0_.setForeground(toQColor(*emphasis_.italicText));
    }
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

QList<QTextLayout::FormatRange> EditorFormats::ranges(const core::LineRender& render) const {
    if (render.headingLevel == 0 && !render.codeBlock && render.runs.empty() && render.inlineStyles.empty() &&
        render.hidden.empty() && render.links.empty() && render.wrapIndent == 0) {
        return {};
    }
    if (render.headingLevel == 0 && !render.codeBlock && render.runs.empty() && render.inlineStyles.empty() &&
        render.links.empty() && render.hidden.size() == 1 && render.decorations.size() == 1 &&
        render.decorations[0].kind == core::DecorationKind::Bullet &&
        render.hidden[0].mode == core::HideMode::KeepSpace &&
        render.hidden[0].span == (core::Span{render.decorations[0].column, 1})) {
        const auto col = render.decorations[0].column;
        if (render.wrapIndent > col + 1) {
            return {{static_cast<int>(col), 1, keepSpace_[0]}, {static_cast<int>(col + 1), 1, bulletSpace_}};
        }
        return {{static_cast<int>(col), 1, keepSpace_[0]}};
    }
    if (!render.codeBlock && render.inlineStyles.empty() && render.links.empty() && render.wrapIndent == 0 &&
        render.hidden.size() == 1 && render.runs.size() == 1 && render.runs[0].role == core::Role::HeadingText &&
        render.hidden[0].span.end() == render.runs[0].span.start) {
        const int lvl = render.runs[0].level != 0 ? render.runs[0].level : render.headingLevel;
        const auto idx = static_cast<std::size_t>(lvl >= 1 && lvl <= 6 ? lvl : 0);
        const QTextCharFormat& hfmt =
            render.hidden[0].mode == core::HideMode::Collapse ? collapse_[idx] : keepSpace_[idx];
        return {
            {static_cast<int>(render.hidden[0].span.start), static_cast<int>(render.hidden[0].span.length), hfmt},
            {static_cast<int>(render.runs[0].span.start), static_cast<int>(render.runs[0].span.length), heading(lvl)}};
    }
    QList<QTextLayout::FormatRange> out;
    const QTextCharFormat lineFormat = heading(render.headingLevel);
    auto& boundaries = boundaries_;
    boundaries.clear();
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
    for (const auto& link : render.links) {
        boundaries.push_back(link.span.start);
        boundaries.push_back(link.span.end());
    }
    const auto bulletSpace = bulletSpaceSpan(render);
    if (bulletSpace.has_value()) {
        boundaries.push_back(bulletSpace->start);
        boundaries.push_back(bulletSpace->end());
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    std::size_t runIndex = 0;
    std::size_t inlineIndex = 0;
    std::size_t hiddenIndex = 0;
    std::size_t linkIndex = 0;
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
        while (linkIndex < render.links.size() && render.links[linkIndex].span.end() <= start) {
            ++linkIndex;
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
        const auto* link = linkIndex < render.links.size() && render.links[linkIndex].span.start <= start &&
                                   render.links[linkIndex].span.end() >= end
                               ? &render.links[linkIndex]
                               : nullptr;
        const bool isBulletSpace = bulletSpace.has_value() && start >= bulletSpace->start && end <= bulletSpace->end();
        if (baseRun == nullptr && inlineRun == nullptr && hidden == nullptr && link == nullptr && !isBulletSpace) {
            continue;
        }

        if (isBulletSpace && baseRun == nullptr && inlineRun == nullptr && hidden == nullptr && link == nullptr &&
            render.headingLevel == 0 && !render.codeBlock) {
            out.append({static_cast<int>(start), static_cast<int>(end - start), bulletSpace_});
            continue;
        }
        if (hidden != nullptr && baseRun == nullptr && link == nullptr && !render.codeBlock && !isBulletSpace &&
            !isInlineCodeDelimiter(render, start)) {
            const auto idx = static_cast<std::size_t>(
                render.headingLevel >= 1 && render.headingLevel <= 6 ? render.headingLevel : 0);
            const QTextCharFormat& fmt = hidden->mode == core::HideMode::Collapse ? collapse_[idx] : keepSpace_[idx];
            out.append({static_cast<int>(start), static_cast<int>(end - start), fmt});
            continue;
        }
        if (inlineRun != nullptr && inlineRun->style == core::InlineStyle::Italic && baseRun == nullptr &&
            hidden == nullptr && link == nullptr && render.headingLevel == 0 && !render.codeBlock && !isBulletSpace) {
            out.append({static_cast<int>(start), static_cast<int>(end - start), italic0_});
            continue;
        }
        if (baseRun != nullptr && baseRun->role == core::Role::HeadingText && inlineRun == nullptr &&
            hidden == nullptr && link == nullptr && !render.codeBlock && !isBulletSpace) {
            const int lvl = baseRun->level != 0 ? baseRun->level : render.headingLevel;
            out.append({static_cast<int>(start), static_cast<int>(end - start), heading(lvl)});
            continue;
        }

        const int level = baseRun != nullptr && baseRun->level != 0 ? baseRun->level : render.headingLevel;
        QTextCharFormat format = baseRun != nullptr ? heading(level) : lineFormat;
        // A pointer rather than std::optional<core::Role>: GCC 13 in Release builds (-O3) reports a false
        // -Wmaybe-uninitialized for the optional, which breaks the Release build with -Werror (CI).
        const core::Role* role = baseRun != nullptr ? &baseRun->role : nullptr;
        const auto is = [role](core::Role r) { return role != nullptr && *role == r; };
        if (render.codeBlock) {
            applyCodeFont(format, codeBlockFamilies_, editorFontSize_ * codeBlock_.fontScale);
        }
        if (is(core::Role::Marker)) {
            // A revealed thematic break has its own marker token (S023); a setext underline uses
            // colors.marker like an ATX heading's '#'.
            format.setForeground(render.thematicBreak ? toQColor(thematicBreak_.marker) : marker_);
        } else if (is(core::Role::CodeBlock)) {
            format.setForeground(toQColor(render.fenceLine ? codeBlock_.marker : codeBlock_.text));
        } else if (is(core::Role::InlineCode) || is(core::Role::InlineCodeMarker)) {
            // The background is a box painted by EditorWidget (radius, border, padding).
            applyCodeFont(format, inlineCodeFamilies_, textSize(level) * inlineCode_.fontScale);
            format.setForeground(toQColor(is(core::Role::InlineCode) ? inlineCode_.text : inlineCode_.marker));
        } else if (is(core::Role::EmphasisMarker)) {
            format.setForeground(toQColor(emphasis_.marker));
        } else if (is(core::Role::EscapeMarker)) {
            format.setForeground(toQColor(escape_.marker));
        } else if (is(core::Role::LinkText)) {
            format.setForeground(toQColor(link_.text));
        } else if (is(core::Role::LinkMarker)) {
            format.setForeground(toQColor(link_.marker));
        }
        if (link != nullptr) {
            format.setAnchor(true);
            format.setAnchorHref(QString::fromStdU16String(link->destination));
            if (!link->title.empty()) {
                format.setToolTip(QString::fromStdU16String(link->title));
            }
        }
        if (inlineRun != nullptr && !is(core::Role::InlineCode)) {
            const auto styles = inlineRun->style;
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
                : core::hasStyle(styles, core::InlineStyle::Strikethrough) ? emphasis_.strikethroughText
                                                                           : std::nullopt;
            if (color && !is(core::Role::EscapeMarker) && !is(core::Role::Marker) && !is(core::Role::LinkMarker)) {
                format.setForeground(toQColor(*color));
            }
        }
        if (hidden != nullptr) {
            if (!is(core::Role::CodeBlock) && !render.codeBlock) {
                format = lineFormat;
            }
            if (isInlineCodeDelimiter(render, start)) {
                // Reserves inline-code padding after each concealed backtick (theme token, ADR 0004).
                format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
                format.setFontLetterSpacing(inlineCode_.padding);
            }
            // Concealment mechanism (ADR 0001), not a styling value.
            format.setForeground(Qt::transparent);  // theme-lint: allow
            if (hidden->mode == core::HideMode::Collapse) {
                format.clearProperty(QTextFormat::FontPointSize);
                format.setProperty(QTextFormat::FontPixelSize, 1);
            }
        }
        if (isBulletSpace) {
            format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
            format.setFontLetterSpacing(bulletSpacing_);
        }
        out.append({static_cast<int>(start), static_cast<int>(end - start), format});
    }
    return out;
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
