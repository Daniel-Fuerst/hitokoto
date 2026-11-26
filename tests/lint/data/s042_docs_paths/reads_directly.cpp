// SPDX-License-Identifier: GPL-3.0-or-later
// S042 lint sample, never compiled: a test that opens a document through the source directory.
#include <fstream>
#include <string>

void readsDirectly() {
    std::ifstream in(std::string(HITOKOTO_SOURCE_DIR) + "/docs/performance.md");  // finding 1
}
