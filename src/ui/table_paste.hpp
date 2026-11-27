// SPDX-License-Identifier: GPL-3.0-or-later
// S031 / ADR 0011: reads the HTML of a paste with Qt's HTML import into the core's model; the core
// decides whether it converts and writes the Markdown. Nothing is shown or loaded as HTML.
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/paste.hpp"
#include "hitokoto/core/table_clipboard.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

class QMimeData;

namespace hitokoto::ui {

/// The parts of `html` in source order: text outside tables (one line per block) and tables (each
/// cell with its span and the lines of its blocks; a table in a cell marks the outer table).
/// `html` is the paste without what only formats (`core::htmlWithoutFormatting`): Qt imports
/// Outlook's markup in half the time without its style attributes (docs/performance.md,
/// 2026-11-10), and the tables and their text stay the same. The editor scans that HTML first
/// (`core::scanHtml`).
[[nodiscard]] std::vector<core::PastedPart> readPastedHtml(const QString& html);

/// How often readPastedHtml has run in this process (tests and benchmarks: S031 review O1, O9).
[[nodiscard]] std::uint64_t pastedHtmlImports() noexcept;

/// S041, decision 7: what a paste into cells holds.
struct PastedCells {
    /// The table, its cells as they are to be written: hitokoto's own copy if its Markdown is
    /// exactly one table (cells as written); else an HTML paste with exactly one table of two cells
    /// or more and no other text, or, without HTML, a spreadsheet's text of two cells or more in a
    /// row (cells as S031 writes them; review (code) O1). Nothing if the paste is text.
    std::optional<core::TableCellGrid> cells;
    /// The HTML holds a table and text beside it, so the paste is text (review (code) O2).
    bool textBesideTable = false;
};

/// Reads the clipboard data of a paste into cells. HTML that S031 keeps plain text (nested
/// tables, over its time bound) is text here too.
[[nodiscard]] PastedCells pastedCells(const QMimeData* source, const core::ElementRegistry& registry);

}  // namespace hitokoto::ui
