// SPDX-License-Identifier: GPL-3.0-or-later
// S042: must not compile. performance.md is not on tests/docs-read-by-tests.txt, so the helper
// refuses it; the test S042_TheHelperRefusesAnUnlistedDocument builds this file and expects the
// helper's message.
#include "hitokoto/test/docs_files.hpp"

#include <string>

std::string unlistedDocument() {
    return test::docsFile<"performance.md">();
}
