// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. Stubs for the red tests; the implementation follows.
#include "hitokoto/core/find.hpp"

namespace hitokoto::core {

char32_t foldCaseSimple(char32_t c) noexcept {
    return c;
}

std::string_view caseFoldingUnicodeVersion() noexcept {
    return {};
}

bool isUnicodeWhitespace(char32_t c) noexcept {
    (void)c;
    return false;
}

FindQuery::FindQuery(std::u16string_view text, FindOptions options) : options_(options) {
    (void)text;
}

std::vector<FindUnit> lineUnits(std::u16string_view line, std::size_t lineIndex, const LineRender& render) {
    (void)line;
    (void)lineIndex;
    (void)render;
    return {};
}

std::vector<FindUnit> tableRowUnits(const ElementRegistry& registry, std::u16string_view line, std::size_t lineIndex,
                                    std::size_t columns) {
    (void)registry;
    (void)line;
    (void)lineIndex;
    (void)columns;
    return {};
}

std::vector<FindUnit> propertyUnits(std::span<const std::u16string> yamlLines, std::size_t firstLine) {
    (void)yamlLines;
    (void)firstLine;
    return {};
}

std::vector<FindUnit> documentUnits(const ElementRegistry& registry, std::span<const std::u16string> lines,
                                    const FindDocumentOptions& options) {
    (void)registry;
    (void)lines;
    (void)options;
    return {};
}

std::vector<FindMatch> findInUnits(std::span<const FindUnit> units, const FindQuery& query) {
    (void)units;
    (void)query;
    return {};
}

std::vector<FindMatch> findInDocument(const ElementRegistry& registry, std::span<const std::u16string> lines,
                                      std::u16string_view text, FindOptions options,
                                      const FindDocumentOptions& documentOptions) {
    return findInUnits(documentUnits(registry, lines, documentOptions), FindQuery(text, options));
}

std::u16string matchContext(std::span<const FindUnit> units, const FindMatch& match, std::size_t maxLength) {
    (void)units;
    (void)match;
    (void)maxLength;
    return {};
}

}  // namespace hitokoto::core
