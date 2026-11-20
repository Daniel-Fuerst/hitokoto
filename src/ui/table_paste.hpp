// SPDX-License-Identifier: GPL-3.0-or-later
// S031 / ADR 0011: reads the HTML of a paste with Qt's HTML import into the core's model; the core
// decides whether it converts and writes the Markdown. Nothing is shown or loaded as HTML.
#pragma once

#include "hitokoto/core/paste.hpp"

#include <QString>

#include <cstdint>
#include <vector>

namespace hitokoto::ui {

/// The parts of `html` in source order: text outside tables (one line per block) and tables (each
/// cell with its span and the lines of its blocks; a table in a cell marks the outer table).
/// `html` is the paste without its style attributes (`core::htmlWithoutStyles`): Qt imports
/// Outlook's markup in half the time without them, and the tables and their text stay the same
/// (docs/performance.md, 2026-11-10). The editor scans that HTML first (`core::scanHtml`).
[[nodiscard]] std::vector<core::PastedPart> readPastedHtml(const QString& html);

/// How often readPastedHtml has run in this process (tests and benchmarks: S031 review O1, O9).
[[nodiscard]] std::uint64_t pastedHtmlImports() noexcept;

}  // namespace hitokoto::ui
