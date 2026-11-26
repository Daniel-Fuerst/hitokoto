// SPDX-License-Identifier: GPL-3.0-or-later
// S042 lint sample, never compiled: a relative path from the build directory, and the helper.
#include <fstream>

void readsRelatively() {
    std::ifstream in("../../docs/performance.md");  // finding 2
    std::ifstream ok(test::docsFile<"theming.md">());
    const char quote = '"';
    std::ifstream also(quote == '"' ? "../docs/x.md" : "");  // finding 3, after a character literal
}
