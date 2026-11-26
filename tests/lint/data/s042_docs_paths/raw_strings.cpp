// SPDX-License-Identifier: GPL-3.0-or-later
// S042 lint sample, never compiled: text in raw string literals.
#include <string_view>

// The marker on the line before the raw string allows its lines.
constexpr std::string_view kMarked =  // S042-mention: front matter text, not a file
    R"(mode: spec
spec: docs/stories/042-docs-only-pull-requests.md
)";

constexpr std::string_view kUnmarked = R"x(first line
path: docs/performance.md
)x";  // finding 5 is the line above

constexpr std::string_view kOneLine = R"(docs/performance.md)";  // finding 6
