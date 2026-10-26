// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_formats.hpp"

#include "qt_bridge.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>

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
      codeBlockFamilies_(codeFamilies(codeBlock_.fontFamily)),
      inlineCodeFamilies_(codeFamilies(inlineCode_.fontFamily)),
      tableFamilies_(table_.fontFamily.empty() ? QStringList{}
                                               : QStringList{QString::fromStdString(table_.fontFamily)}) {
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

double EditorFormats::textSize(int level) const {
    return level >= 1 && level <= 6 ? heading_[static_cast<std::size_t>(level)].fontPointSize() : editorFontSize_;
}

QTextCharFormat EditorFormats::heading(int level) const {
    return heading_[static_cast<std::size_t>(level >= 1 && level <= 6 ? level : 0)];
}

QList<QTextLayout::FormatRange> EditorFormats::ranges(const core::LineRender& render) const {
    QList<QTextLayout::FormatRange> out;
    const QTextCharFormat lineFormat = heading(render.headingLevel);
    std::vector<std::uint32_t> emptyCellAnchors;
    if (render.table && !render.tableDelimiter && !render.cells.empty()) {
        emptyCellAnchors.reserve(render.cells.size());
        for (std::size_t c = 0; c < render.cells.size(); ++c) {
            const auto& cell = render.cells[c];
            if (cell.content.length == 0) {
                // Anchor on the cell's leading '|' (non-whitespace, since QTextLine ignores trailing spaces
                // when computing line height). Its width is subtracted by layoutTableRow's prefixW.
                const std::uint32_t anchor =
                    c == 0 ? (cell.raw.start > 0 ? cell.raw.start - 1 : 0U) : render.cells[c - 1].raw.end();
                emptyCellAnchors.push_back(anchor);
            }
        }
    }

    std::vector<std::uint32_t> boundaries;
    boundaries.reserve(
        (render.runs.size() + render.inlineStyles.size() + render.hidden.size() + emptyCellAnchors.size()) * 2);
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
    for (const std::uint32_t anchor : emptyCellAnchors) {
        boundaries.push_back(anchor);
        boundaries.push_back(anchor + 1);
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
        const double surroundingSize = render.table ? editorFontSize_ * table_.fontScale : textSize(level);
        if (is(core::Role::Marker)) {
            format.setForeground(marker_);
        } else if (is(core::Role::CodeBlock)) {
            applyCodeFont(format, codeBlockFamilies_, editorFontSize_ * codeBlock_.fontScale);
            format.setForeground(toQColor(render.fenceLine ? codeBlock_.marker : codeBlock_.text));
        } else if (is(core::Role::InlineCode) || is(core::Role::InlineCodeMarker)) {
            // The background is a box painted by EditorWidget (radius, border, padding).
            applyCodeFont(format, inlineCodeFamilies_, surroundingSize * inlineCode_.fontScale);
            format.setForeground(toQColor(is(core::Role::InlineCode) ? inlineCode_.text : inlineCode_.marker));
        } else if (is(core::Role::EmphasisMarker)) {
            format.setForeground(toQColor(emphasis_.marker));
        } else if (is(core::Role::TableHeader) || is(core::Role::TableCell)) {
            if (!tableFamilies_.isEmpty()) {
                format.setFontFamilies(tableFamilies_);
            }
            format.setFontPointSize(surroundingSize);
            if (is(core::Role::TableHeader)) {
                format.setFontWeight(table_.headerWeight);
                format.setForeground(toQColor(table_.headerText));
            } else {
                format.setForeground(toQColor(table_.text));
            }
        }
        if (inlineRun != nullptr && !is(core::Role::InlineCode)) {
            const auto styles = inlineRun->style;
            if (core::hasStyle(styles, core::InlineStyle::Bold)) {
                format.setFontWeight(emphasis_.boldWeight);
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
            if (color) {
                format.setForeground(toQColor(*color));
            }
        }
        if (hidden != nullptr) {
            const bool keepEmptyCellHeight =
                end == start + 1 && std::binary_search(emptyCellAnchors.begin(), emptyCellAnchors.end(), start);
            if (keepEmptyCellHeight) {
                // Keep one concealed character per empty cell at table font size so empty cells and
                // empty rows retain full single-line height and a full-height caret.
                format = lineFormat;
                if (!tableFamilies_.isEmpty()) {
                    format.setFontFamilies(tableFamilies_);
                }
                format.setFontPointSize(surroundingSize);
                if (render.tableHeader) {
                    format.setFontWeight(table_.headerWeight);
                }
                format.setForeground(Qt::transparent);  // theme-lint: allow
            } else {
                if (!is(core::Role::CodeBlock)) {
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
        }
        out.append({static_cast<int>(start), static_cast<int>(end - start), format});
    }
    return out;
}

double EditorFormats::cellTextWidth(const QString& lineText, const core::LineRender& render,
                                    const core::TableCell& cell) const {
    if (cell.content.length == 0) {
        return 0.0;
    }
    const auto fmtRanges = ranges(render);
    const int cellStart = static_cast<int>(cell.content.start);
    const int cellEnd = static_cast<int>(cell.content.end());
    double width = 0.0;
    int pos = cellStart;
    for (const auto& r : fmtRanges) {
        const int rStart = std::max(cellStart, r.start);
        const int rEnd = std::min(cellEnd, r.start + r.length);
        if (rStart >= rEnd) {
            continue;
        }
        if (rStart > pos) {
            QFont f = baseFont_;
            f.setPointSizeF(editorFontSize_ * table_.fontScale);
            if (!tableFamilies_.isEmpty()) {
                f.setFamilies(tableFamilies_);
            }
            width += QFontMetricsF(f).horizontalAdvance(lineText.mid(pos, rStart - pos));
        }
        if (r.format.intProperty(QTextFormat::FontPixelSize) == 1) {
            // Concealed characters inside the cell (e.g. emphasis delimiters, backticks, '\|' backslash).
            width += (rEnd - rStart) * (1.0 + r.format.fontLetterSpacing());
        } else {
            QFont f = baseFont_;
            if (r.format.hasProperty(QTextFormat::FontFamilies)) {
                const QStringList fams = r.format.fontFamilies().toStringList();
                if (!fams.isEmpty()) {
                    f.setFamilies(fams);
                }
            }
            if (r.format.hasProperty(QTextFormat::FontStyleHint)) {
                f.setStyleHint(r.format.fontStyleHint());
            }
            if (r.format.fontPointSize() > 0) {
                f.setPointSizeF(r.format.fontPointSize());
            }
            if (r.format.fontWeight() > 0) {
                f.setWeight(static_cast<QFont::Weight>(r.format.fontWeight()));
            }
            f.setItalic(r.format.fontItalic());
            width += QFontMetricsF(f).horizontalAdvance(lineText.mid(rStart, rEnd - rStart));
        }
        pos = rEnd;
    }
    if (pos < cellEnd) {
        QFont f = baseFont_;
        f.setPointSizeF(editorFontSize_ * table_.fontScale);
        if (!tableFamilies_.isEmpty()) {
            f.setFamilies(tableFamilies_);
        }
        width += QFontMetricsF(f).horizontalAdvance(lineText.mid(pos, cellEnd - pos));
    }
    return width;
}

}  // namespace hitokoto::ui
