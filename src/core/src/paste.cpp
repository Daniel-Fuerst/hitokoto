// SPDX-License-Identifier: GPL-3.0-or-later
// S031: stubs for the red tests; the implementation follows.
#include "hitokoto/core/paste.hpp"

namespace hitokoto::core {

bool countsAsTable(const PastedTable& /*table*/) {
    return false;
}

std::u16string escapeTableCellText(std::u16string_view text) {
    return std::u16string(text);
}

std::u16string pastedTableToMarkdown(const PastedTable& /*table*/) {
    return {};
}

std::optional<std::vector<PasteBlock>> pastedTablesToMarkdown(std::span<const PastedPart> /*parts*/) {
    return std::nullopt;
}

std::optional<PastedTable> parseExcelPlainText(std::u16string_view /*text*/) {
    return std::nullopt;
}

PasteInsertion placePastedBlocks(std::span<const PasteBlock> /*blocks*/, const PastePlace& /*place*/) {
    return {};
}

}  // namespace hitokoto::core
