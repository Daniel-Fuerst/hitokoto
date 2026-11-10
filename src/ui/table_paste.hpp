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
[[nodiscard]] std::vector<core::PastedPart> readPastedHtml(const QString& html);

/// How often readPastedHtml has run in this process (tests and benchmarks: S031 review O1, O9).
[[nodiscard]] std::uint64_t pastedHtmlImports() noexcept;

}  // namespace hitokoto::ui
