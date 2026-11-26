// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find over arbitrary documents and search texts. The input, read as UTF-16, is a search text
// and the lines of a document. Nothing may crash; the segments of each unit cover its text in
// order; every match lies inside its units and the document and never holds half of a character;
// scanning in two slices that resume where the first stopped finds what one scan finds; the
// context for a screen reader stays within its length.
#include "hitokoto/core/find.hpp"
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace {

using hitokoto::core::DocPos;
using hitokoto::core::FindMatch;
using hitokoto::core::FindUnit;

void check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool isHigh(char16_t c) {
    return c >= 0xD800 && c <= 0xDBFF;
}
bool isLow(char16_t c) {
    return c >= 0xDC00 && c <= 0xDFFF;
}

/// Whether `offset` falls between the two halves of a surrogate pair of `text`.
bool splitsAPair(const std::u16string& text, std::uint32_t offset) {
    return offset > 0 && offset < text.size() && isHigh(text[offset - 1]) && isLow(text[offset]);
}

bool inDocument(const std::vector<std::u16string>& lines, DocPos p) {
    return p.line < lines.size() && p.column <= lines[p.line].size();
}

auto key(const FindMatch& m) {
    return std::tie(m.unit, m.textStart, m.endUnit, m.textEnd);
}

std::vector<FindMatch> sorted(std::vector<FindMatch> matches) {
    std::sort(matches.begin(), matches.end(), [](const FindMatch& a, const FindMatch& b) { return key(a) < key(b); });
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace hitokoto::core;
    static const ElementRegistry registry = ElementRegistry::withBuiltins();
    if (size < 2) {
        return 0;
    }
    // Byte 0: options; byte 1: length of the search text in code units; then the search text and
    // the document, two bytes per code unit, '\n' separating lines.
    const bool matchCase = (data[0] & 1U) != 0;
    FindDocumentOptions documentOptions;
    documentOptions.frontMatterAsProperties = (data[0] & 2U) != 0;
    const std::size_t units16 = (size - 2) / 2;
    const std::size_t queryLength = std::min<std::size_t>(data[1] % 16U, units16);
    const auto unitAt = [&](std::size_t i) {
        return static_cast<char16_t>(data[2 + (2 * i)] | (data[3 + (2 * i)] << 8U));
    };
    std::u16string query;
    for (std::size_t i = 0; i < queryLength; ++i) {
        query += unitAt(i);
    }
    std::vector<std::u16string> lines(1);
    for (std::size_t i = queryLength; i < units16; ++i) {
        const char16_t c = unitAt(i);
        if (c == u'\n') {
            lines.emplace_back();
        } else {
            lines.back() += c;
        }
    }

    const std::vector<FindUnit> units = documentUnits(registry, lines, documentOptions);
    for (const FindUnit& unit : units) {
        std::uint32_t at = 0;
        for (const FindSegment& s : unit.segments) {
            check(s.textStart == at);
            check(s.source <= s.sourceEnd);
            check(inDocument(lines, s.source) && inDocument(lines, s.sourceEnd));
            at += s.textLength;
        }
        check(at == unit.text.size());
    }

    const FindQuery q(query, FindOptions{matchCase});
    const std::vector<FindMatch> matches = findInUnits(units, q);
    for (const FindMatch& m : matches) {
        check(m.unit < units.size() && m.endUnit < units.size());
        check(m.unit < m.endUnit || (m.unit == m.endUnit && m.textStart <= m.textEnd));
        check(m.textStart <= units[m.unit].text.size() && m.textEnd <= units[m.endUnit].text.size());
        check(!splitsAPair(units[m.unit].text, m.textStart) && !splitsAPair(units[m.endUnit].text, m.textEnd));
        check(m.start <= m.end && inDocument(lines, m.start) && inDocument(lines, m.end));
        check(matchContext(units, m, 80).size() <= 80 + 2);
    }

    // The UI scans in slices: a first slice that may stop where a match needs more units, then the
    // rest from where it stopped.
    const std::size_t half = units.size() / 2;
    const FindScan first = scanUnits(std::span<const FindUnit>(units).first(half), 0, 0, q, true);
    check(first.resumeUnit <= half);
    const FindScan second = scanUnits(units, first.resumeUnit, first.resumeOffset, q, false);
    std::vector<FindMatch> pieces = first.matches;
    pieces.insert(pieces.end(), second.matches.begin(), second.matches.end());
    const FindScan whole = scanUnits(units, 0, 0, q, false);
    check(sorted(pieces) == sorted(whole.matches));

    check(findInDocument(registry, lines, query, FindOptions{matchCase}, documentOptions) == matches);
    return 0;
}
