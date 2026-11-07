// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. The searched text is the concealed view of the document, split into
// units with soft and hard boundaries; a search text is words between whitespace runs; words are
// compared code point by code point after simple Unicode case folding (ADR 0008).
#include "hitokoto/core/find.hpp"

#include "hitokoto/core/properties.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace hitokoto::core {
namespace {

struct CaseFoldingEntry {
    char32_t from;
    char32_t to;
};

#include "case_folding_table.inc"

struct CodePoint {
    char32_t value = 0;
    std::uint32_t length = 1;  ///< UTF-16 code units
};

CodePoint codePointAt(std::u16string_view s, std::size_t i) noexcept {
    const char16_t c = s[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) {
        return {0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10U) + (static_cast<char32_t>(s[i + 1]) - 0xDC00), 2};
    }
    return {c, 1};
}

std::uint32_t u32(std::size_t value) noexcept {
    return static_cast<std::uint32_t>(value);
}

/// Appends `length` code units of `text` from `start` and their one-to-one source to `unit`,
/// merging with the previous segment where the source continues.
void appendOneToOne(FindUnit& unit, std::u16string_view text, DocPos source) {
    if (text.empty()) {
        return;
    }
    const auto start = u32(unit.text.size());
    unit.text += text;
    if (!unit.segments.empty()) {
        FindSegment& last = unit.segments.back();
        if (last.oneToOne() && last.sourceEnd == source && last.textStart + last.textLength == start) {
            last.textLength += u32(text.size());
            last.sourceEnd.column += u32(text.size());
            return;
        }
    }
    unit.segments.push_back(
        FindSegment{start, u32(text.size()), source, DocPos{source.line, source.column + u32(text.size())}});
}

void appendAtomic(FindUnit& unit, std::u16string_view text, DocPos source, DocPos sourceEnd) {
    if (text.empty()) {
        return;
    }
    const auto start = u32(unit.text.size());
    unit.text += text;
    unit.segments.push_back(FindSegment{start, u32(text.size()), source, sourceEnd});
}

/// The segment that holds code unit `offset` of `unit`.
const FindSegment& segmentAt(const FindUnit& unit, std::uint32_t offset) {
    const auto it = std::upper_bound(unit.segments.begin(), unit.segments.end(), offset,
                                     [](std::uint32_t o, const FindSegment& s) { return o < s.textStart; });
    return it == unit.segments.begin() ? *it : *(it - 1);
}

DocPos sourceStart(const FindUnit& unit, std::uint32_t offset) {
    const FindSegment& s = segmentAt(unit, offset);
    if (!s.oneToOne()) {
        return s.source;
    }
    return {s.source.line, s.source.column + (offset - s.textStart)};
}

/// The source end of the code unit at `offset` (exclusive).
DocPos sourceEndOf(const FindUnit& unit, std::uint32_t offset) {
    const FindSegment& s = segmentAt(unit, offset);
    if (!s.oneToOne()) {
        return s.sourceEnd;
    }
    return {s.source.line, s.source.column + (offset - s.textStart) + 1};
}

/// A unit made from text the core read with a map (a cell after the inline elements, a property).
struct MappedText {
    std::u16string text;
    std::vector<FindSegment> segments;
};

/// Removes the code units in `hidden` from `in` (cell text before concealment, offsets into
/// in.text), keeping the map.
FindUnit withoutHidden(const MappedText& in, const std::vector<HiddenRange>& hidden, FindUnitKind kind) {
    std::vector<bool> drop(in.text.size(), false);
    for (const HiddenRange& h : hidden) {
        for (std::uint32_t i = h.span.start; i < h.span.end() && i < drop.size(); ++i) {
            drop[i] = true;
        }
    }
    FindUnit unit;
    unit.kind = kind;
    for (const FindSegment& seg : in.segments) {
        for (std::uint32_t i = 0; i < seg.textLength;) {
            const std::uint32_t at = seg.textStart + i;
            if (drop[at]) {
                ++i;
                continue;
            }
            if (seg.oneToOne()) {
                std::uint32_t n = 1;
                while (i + n < seg.textLength && !drop[at + n]) {
                    ++n;
                }
                appendOneToOne(unit, std::u16string_view(in.text).substr(at, n),
                               DocPos{seg.source.line, seg.source.column + i});
                i += n;
            } else {
                appendAtomic(unit, std::u16string_view(in.text).substr(at, 1), seg.source, seg.sourceEnd);
                ++i;
            }
        }
    }
    return unit;
}

/// The source range of [start, end) of a mapped text (for link text inside a cell).
std::pair<DocPos, DocPos> mappedRange(const MappedText& in, std::uint32_t start, std::uint32_t end) {
    FindUnit probe;
    probe.text = in.text;
    probe.segments = in.segments;
    if (in.text.empty() || start >= in.text.size()) {
        const DocPos p = in.segments.empty() ? DocPos{} : in.segments.back().sourceEnd;
        return {p, p};
    }
    if (end <= start) {
        const DocPos p = sourceStart(probe, start);
        return {p, p};
    }
    return {sourceStart(probe, start), sourceEndOf(probe, end - 1)};
}

FindUnit destinationUnit(const LinkTarget& link, DocPos linkStart, DocPos linkEnd) {
    FindUnit unit;
    unit.kind = FindUnitKind::Destination;
    unit.before = FindBoundary::Hard;
    unit.linkStart = linkStart;
    unit.linkEnd = linkEnd;
    appendAtomic(unit, link.destination, linkStart, linkEnd);
    return unit;
}

/// A cell's Markdown as the core reads it for editing (`tableCellToText`), with its map.
MappedText cellText(std::u16string_view line, std::size_t lineIndex, Span content) {
    MappedText out;
    FindUnit unit;
    const std::u16string_view md = line.substr(content.start, content.length);
    for (std::size_t i = 0; i < md.size(); ++i) {
        const char16_t c = md[i];
        const DocPos at{lineIndex, content.start + u32(i)};
        if (c == u'\\' && i + 1 < md.size() && md[i + 1] == u'|') {
            appendAtomic(unit, u"|", at, DocPos{lineIndex, at.column + 2});
            ++i;
            continue;
        }
        if (c == u'<' && i + 3 < md.size() && (md[i + 1] == u'b' || md[i + 1] == u'B') &&
            (md[i + 2] == u'r' || md[i + 2] == u'R')) {
            std::size_t j = i + 3;
            while (j < md.size() && md[j] == u' ') {
                ++j;
            }
            if (j < md.size() && md[j] == u'/') {
                ++j;
            }
            if (j < md.size() && md[j] == u'>') {
                appendAtomic(unit, u"\n", at, DocPos{lineIndex, content.start + u32(j) + 1});
                i = j;
                continue;
            }
        }
        appendOneToOne(unit, md.substr(i, 1), at);
    }
    out.text = std::move(unit.text);
    out.segments = std::move(unit.segments);
    return out;
}

}  // namespace

char32_t foldCaseSimple(char32_t c) noexcept {
    const auto it = std::lower_bound(kCaseFolding.begin(), kCaseFolding.end(), c,
                                     [](const CaseFoldingEntry& e, char32_t v) { return e.from < v; });
    return it != kCaseFolding.end() && it->from == c ? it->to : c;
}

std::string_view caseFoldingUnicodeVersion() noexcept {
    return kCaseFoldingVersion;
}

bool isUnicodeWhitespace(char32_t c) noexcept {
    switch (c) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x0085:
        case 0x00A0:
        case 0x1680:
        case 0x2028:
        case 0x2029:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            return true;
        default:
            return c >= 0x2000 && c <= 0x200A;
    }
}

FindQuery::FindQuery(std::u16string_view text, FindOptions options) : options_(options) {
    std::u32string word;
    bool any = false;
    for (std::size_t i = 0; i < text.size();) {
        const CodePoint cp = codePointAt(text, i);
        i += cp.length;
        if (isUnicodeWhitespace(cp.value)) {
            if (!any) {
                leading_ = true;
            }
            if (!word.empty()) {
                words_.push_back(std::move(word));
                word.clear();
            }
            trailing_ = true;
            continue;
        }
        any = true;
        trailing_ = false;
        word += options_.matchCase ? cp.value : foldCaseSimple(cp.value);
    }
    if (!word.empty()) {
        words_.push_back(std::move(word));
    }
    if (words_.empty()) {
        leading_ = false;
        trailing_ = false;
    }
}

std::vector<FindUnit> lineUnits(std::u16string_view line, std::size_t lineIndex, const LineRender& render) {
    std::vector<FindUnit> units;
    MappedText whole;
    FindUnit raw;
    appendOneToOne(raw, line, DocPos{lineIndex, 0});
    whole.text = std::move(raw.text);
    whole.segments = std::move(raw.segments);
    units.push_back(withoutHidden(whole, render.hidden, FindUnitKind::Line));
    for (const LinkTarget& link : render.links) {
        const Span shown = link.span.length > 0 ? link.span : link.full;
        units.push_back(destinationUnit(link, DocPos{lineIndex, shown.start}, DocPos{lineIndex, shown.end()}));
    }
    return units;
}

std::vector<FindUnit> tableRowUnits(const ElementRegistry& registry, std::u16string_view line, std::size_t lineIndex,
                                    std::size_t columns) {
    std::vector<FindUnit> units;
    const std::vector<TableCell> cells = tableRowCells(line);
    for (std::size_t c = 0; c < cells.size() && c < columns; ++c) {
        const MappedText text = cellText(line, lineIndex, cells[c].content);
        const LineRender r = registry.renderInline(text.text);
        units.push_back(withoutHidden(text, r.hidden, FindUnitKind::Cell));
        for (const LinkTarget& link : r.links) {
            const Span shown = link.span.length > 0 ? link.span : link.full;
            const auto [from, to] = mappedRange(text, shown.start, shown.end());
            units.push_back(destinationUnit(link, from, to));
        }
    }
    return units;
}

std::vector<FindUnit> propertyUnits(std::span<const std::u16string> yamlLines, std::size_t firstLine) {
    std::vector<FindUnit> units;
    for (const PropertyUnit& p : propertySourceUnits(yamlLines)) {
        FindUnit unit;
        unit.kind = FindUnitKind::Property;
        unit.text = p.text;
        for (const PropertySourceRun& run : p.runs) {
            unit.segments.push_back(FindSegment{u32(run.textStart), u32(run.textLength),
                                                DocPos{firstLine + run.line, u32(run.column)},
                                                DocPos{firstLine + run.endLine, u32(run.endColumn)}});
        }
        units.push_back(std::move(unit));
    }
    if (!units.empty()) {
        units.front().before = FindBoundary::Hard;
    }
    return units;
}

void FindUnitBuilder::push(std::vector<FindUnit>&& units, std::vector<FindUnit>& out) {
    for (FindUnit& u : units) {
        if (hardNext_) {
            u.before = FindBoundary::Hard;
            hardNext_ = false;
        }
        out.push_back(std::move(u));
    }
}

void FindUnitBuilder::addLine(std::u16string_view line, std::size_t index, const LineRender& render,
                              std::vector<FindUnit>& out) {
    if (render.table) {
        if (!inTable_) {
            inTable_ = true;
            hardNext_ = true;
        }
        if (render.tableHeader) {
            columns_ = render.cells.size();
        }
        if (!render.tableDelimiter) {
            push(tableRowUnits(*registry_, line, index, columns_), out);
        }
        return;
    }
    if (inTable_) {
        inTable_ = false;
        hardNext_ = true;
    }
    push(lineUnits(line, index, render), out);
}

void FindUnitBuilder::addProperties(std::span<const std::u16string> yaml, std::size_t firstLine,
                                    std::vector<FindUnit>& out) {
    push(propertyUnits(yaml, firstLine), out);
    hardNext_ = true;
}

void FindUnitBuilder::continueTable(std::size_t columns) noexcept {
    (void)columns;  // red: not yet
}

std::vector<FindUnit> documentUnits(const ElementRegistry& registry, std::span<const std::u16string> lines,
                                    const FindDocumentOptions& options) {
    const std::vector<LineRender> renders = registry.renderDocument(lines);
    std::vector<FindUnit> units;
    FindUnitBuilder builder(registry);
    std::size_t i = 0;
    // Front matter shown as properties: lines 0 to the closing line, if its YAML is valid.
    if (options.frontMatterAsProperties && !lines.empty() && renders[0].frontMatter) {
        std::size_t close = 0;
        while (close + 1 < lines.size() && renders[close + 1].frontMatter) {
            ++close;
        }
        const auto yaml = lines.subspan(1, close > 0 ? close - 1 : 0);
        if (close > 0 && !parseProperties(yaml).error) {
            builder.addProperties(yaml, 1, units);
            i = close + 1;
        }
    }
    for (; i < lines.size(); ++i) {
        builder.addLine(lines[i], i, renders[i], units);
    }
    return units;
}

namespace {

/// A position in a sequence of units: the unit and a code unit in its text.
struct Cursor {
    std::size_t unit = 0;
    std::uint32_t offset = 0;
};

class Matcher {
public:
    Matcher(std::span<const FindUnit> units, const FindQuery& query, bool more)
        : units_(units), query_(query), more_(more) {}

    FindScan run(Cursor from) {
        FindScan scan;
        scan.resumeUnit = units_.size();
        if (query_.findsNothing()) {
            return scan;
        }
        const char32_t first = query_.words().front().front();
        Cursor c = from;
        while (c.unit < units_.size()) {
            const std::u16string& text = units_[c.unit].text;
            if (c.offset >= text.size()) {
                ++c.unit;
                c.offset = 0;
                continue;
            }
            const CodePoint cp = codePointAt(text, c.offset);
            if (fold(cp.value) == first && leadingOk(c)) {
                needMore_ = false;
                const auto end = matchFrom(c);
                if (needMore_) {
                    scan.resumeUnit = c.unit;
                    scan.resumeOffset = c.offset;
                    return scan;
                }
                if (end) {
                    scan.matches.push_back(toMatch(c, *end));
                    c = *end;
                    continue;
                }
            }
            c.offset += cp.length;
        }
        return scan;
    }

private:
    [[nodiscard]] char32_t fold(char32_t c) const noexcept {
        return query_.options().matchCase ? c : foldCaseSimple(c);
    }

    [[nodiscard]] bool leadingOk(Cursor c) const {
        if (!query_.leadingWhitespace() || c.offset == 0) {
            return true;  // a unit starts after a boundary or at the document start
        }
        const std::u16string& text = units_[c.unit].text;
        std::size_t prev = c.offset - 1;
        if (prev > 0 && text[prev] >= 0xDC00 && text[prev] < 0xE000) {
            --prev;
        }
        return isUnicodeWhitespace(codePointAt(text, prev).value);
    }

    /// Matches the word at c within its unit; the cursor after it, or nothing.
    [[nodiscard]] std::optional<Cursor> word(Cursor c, const std::u32string& w) const {
        const std::u16string& text = units_[c.unit].text;
        for (const char32_t expected : w) {
            if (c.offset >= text.size()) {
                return std::nullopt;
            }
            const CodePoint cp = codePointAt(text, c.offset);
            if (fold(cp.value) != expected) {
                return std::nullopt;
            }
            c.offset += cp.length;
        }
        return c;
    }

    /// Consumes a whitespace run of at least one whitespace character or soft boundary; the
    /// cursor at the next non-whitespace character, or nothing.
    [[nodiscard]] std::optional<Cursor> whitespace(Cursor c) const {
        bool any = false;
        while (c.unit < units_.size()) {
            const std::u16string& text = units_[c.unit].text;
            while (c.offset < text.size()) {
                const CodePoint cp = codePointAt(text, c.offset);
                if (!isUnicodeWhitespace(cp.value)) {
                    return any ? std::optional<Cursor>(c) : std::nullopt;
                }
                any = true;
                c.offset += cp.length;
            }
            // A destination is a side unit: no match leaves it, and the text around its link
            // goes on past it as if it were not there.
            if (units_[c.unit].kind == FindUnitKind::Destination) {
                return std::nullopt;
            }
            std::size_t next = c.unit + 1;
            while (next < units_.size() && units_[next].kind == FindUnitKind::Destination) {
                ++next;
            }
            if (next >= units_.size()) {
                needMore_ = more_;
                return std::nullopt;
            }
            if (units_[next].before == FindBoundary::Hard) {
                return std::nullopt;
            }
            c.unit = next;
            c.offset = 0;
            any = true;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool trailingOk(Cursor c) const {
        if (!query_.trailingWhitespace()) {
            return true;
        }
        const std::u16string& text = units_[c.unit].text;
        return c.offset >= text.size() || isUnicodeWhitespace(codePointAt(text, c.offset).value);
    }

    [[nodiscard]] std::optional<Cursor> matchFrom(Cursor c) const {
        const auto& words = query_.words();
        for (std::size_t w = 0; w < words.size(); ++w) {
            if (w > 0) {
                const auto next = whitespace(c);
                if (!next) {
                    return std::nullopt;
                }
                c = *next;
            }
            const auto after = word(c, words[w]);
            if (!after) {
                return std::nullopt;
            }
            c = *after;
        }
        if (!trailingOk(c)) {
            return std::nullopt;
        }
        return c;
    }

    [[nodiscard]] FindMatch toMatch(Cursor start, Cursor end) const {
        const FindUnit& first = units_[start.unit];
        FindMatch m;
        if (first.kind == FindUnitKind::Destination) {
            m.start = first.linkStart;
            m.end = first.linkEnd;
            m.inDestination = true;
        } else {
            m.start = sourceStart(first, start.offset);
            m.end = sourceEndOf(units_[end.unit], end.offset - 1);
        }
        m.unit = start.unit;
        m.textStart = start.offset;
        m.endUnit = end.unit;
        m.textEnd = end.offset;
        return m;
    }

    std::span<const FindUnit> units_;
    const FindQuery& query_;
    bool more_ = false;
    mutable bool needMore_ = false;  ///< a whitespace run reached the end of the units with more to come
};

}  // namespace

FindScan scanUnits(std::span<const FindUnit> units, std::size_t fromUnit, std::uint32_t fromOffset,
                   const FindQuery& query, bool more) {
    return Matcher(units, query, more).run(Cursor{fromUnit, fromOffset});
}

std::vector<FindMatch> findInUnits(std::span<const FindUnit> units, const FindQuery& query) {
    std::vector<FindMatch> matches = scanUnits(units, 0, 0, query, false).matches;
    std::stable_sort(matches.begin(), matches.end(),
                     [](const FindMatch& a, const FindMatch& b) { return a.start < b.start; });
    return matches;
}

std::vector<FindMatch> findInDocument(const ElementRegistry& registry, std::span<const std::u16string> lines,
                                      std::u16string_view text, FindOptions options,
                                      const FindDocumentOptions& documentOptions) {
    const std::vector<FindUnit> units = documentUnits(registry, lines, documentOptions);
    return findInUnits(units, FindQuery(text, options));
}

std::u16string matchContext(std::span<const FindUnit> units, const FindMatch& match, std::size_t maxLength) {
    if (match.unit >= units.size() || match.endUnit >= units.size() || maxLength < 3) {
        return {};
    }
    // The units the match lies in, joined by a space; whitespace runs become one space.
    std::u16string text;
    std::size_t from = 0;
    std::size_t to = 0;
    bool space = false;
    const auto put = [&](char16_t c) {
        if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\u00A0' || c == u'\u2028') {
            space = !text.empty();
            return;
        }
        if (space) {
            text += u' ';
            space = false;
        }
        text += c;
    };
    for (std::size_t u = match.unit; u <= match.endUnit; ++u) {
        if (u > match.unit) {
            space = true;
        }
        const std::u16string& t = units[u].text;
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (u == match.unit && i == match.textStart) {
                put(t[i]);
                from = text.size() - 1;
                continue;
            }
            put(t[i]);
            if (u == match.endUnit && i + 1 == match.textEnd) {
                to = text.size();
            }
        }
    }
    if (to <= from) {
        to = std::min(text.size(), from + 1);
    }
    if (text.size() <= maxLength) {
        return text;
    }
    // Cut around the match, with an ellipsis where text was cut.
    const std::size_t room = maxLength - 2;
    const std::size_t length = to - from;
    std::size_t start = 0;
    if (length >= room) {
        start = from;
    } else {
        const std::size_t before = (room - length) / 2;
        start = from > before ? from - before : 0;
        start = std::min(start, text.size() - room);
    }
    const std::size_t end = std::min(text.size(), start + room);
    std::u16string out;
    if (start > 0) {
        out += u'\u2026';
    }
    out += text.substr(start, end - start);
    if (end < text.size()) {
        out += u'\u2026';
    }
    return out;
}

}  // namespace hitokoto::core
