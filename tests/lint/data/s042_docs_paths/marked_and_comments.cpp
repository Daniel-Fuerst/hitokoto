// SPDX-License-Identifier: GPL-3.0-or-later
// S042 lint sample, never compiled: no finding in this file.
// A comment may name docs/performance.md.
/* So may a block comment: docs/performance.md
 * and its continuation: docs/theming.md */
constexpr auto kRecord = "spec: docs/stories/022-inline-links.md\n";  // S042-mention: text only
constexpr auto kUrl = "https://example.org/";                         // docs/performance.md after a // in a string
