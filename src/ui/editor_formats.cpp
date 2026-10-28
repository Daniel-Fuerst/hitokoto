// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>

#include <algorithm>
#include <optional>
#include <vector>

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
      codeBlockFamilies_(codeFamilies(codeBlock_.fontFamily)),
      inlineCodeFamilies_(codeFamilies(inlineCode_.fontFamily)) {
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

QFont EditorFormats::badgeFont() const {
    return baseFont_;
}

double EditorFormats::textSize(int level) const {
    return level >= 1 && level <= 6 ? heading_[static_cast<std::size_t>(level)].fontPointSize() : editorFontSize_;
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
            format.setForeground(marker_);
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
            if (color && !is(core::Role::EscapeMarker) && !is(core::Role::Marker)) {
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
