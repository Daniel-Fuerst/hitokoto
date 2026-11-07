// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. The searched text is the concealed view (decision 1) split into
// units (lines, link destinations, table cells, properties of front matter) with soft and hard
// boundaries; whitespace runs match each other (decision 4); leading and trailing whitespace is a
// condition (decision 5); simple Unicode case folding (decision 6, ADR 0008). Case folding
// expectations taken from Unicode 18.0.0 CaseFolding.txt (status C and S).
#include "hitokoto/core/find.hpp"
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::vector<std::u16string> splitLines(std::u16string_view text) {
    std::vector<std::u16string> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find(u'\n', start);
        lines.emplace_back(
            text.substr(start, nl == std::u16string_view::npos ? std::u16string_view::npos : nl - start));
        if (nl == std::u16string_view::npos) {
            return lines;
        }
        start = nl + 1;
    }
}

std::string utf8(std::u16string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < text.size()) {
            c = 0x10000 + ((c - 0xD800) << 10U) + (text[i + 1] - 0xDC00);
            ++i;
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6U));
            out += static_cast<char>(0x80 | (c & 0x3FU));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12U));
            out += static_cast<char>(0x80 | ((c >> 6U) & 0x3FU));
            out += static_cast<char>(0x80 | (c & 0x3FU));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18U));
            out += static_cast<char>(0x80 | ((c >> 12U) & 0x3FU));
            out += static_cast<char>(0x80 | ((c >> 6U) & 0x3FU));
            out += static_cast<char>(0x80 | (c & 0x3FU));
        }
    }
    return out;
}

/// The source text a match covers, lines joined with "\n"; "@" marks a destination match.
std::string sourceOf(const std::vector<std::u16string>& lines, const FindMatch& m) {
    std::u16string out;
    for (std::size_t l = m.start.line; l <= m.end.line && l < lines.size(); ++l) {
        const std::size_t from = l == m.start.line ? m.start.column : 0;
        const std::size_t to = l == m.end.line ? m.end.column : lines[l].size();
        if (l != m.start.line) {
            out += u'\n';
        }
        if (from <= to && to <= lines[l].size()) {
            out += lines[l].substr(from, to - from);
        }
    }
    return (m.inDestination ? "@" : "") + utf8(out);
}

struct Search {
    std::vector<std::u16string> lines;
    std::vector<FindMatch> matches;
    [[nodiscard]] std::vector<std::string> sources() const {
        std::vector<std::string> out;
        out.reserve(matches.size());
        for (const auto& m : matches) {
            out.push_back(sourceOf(lines, m));
        }
        return out;
    }
};

Search find(std::u16string_view doc, std::u16string_view text, FindOptions options = {},
            FindDocumentOptions documentOptions = {}) {
    Search s;
    s.lines = splitLines(doc);
    s.matches = findInDocument(registry(), s.lines, text, options, documentOptions);
    return s;
}

using Strings = std::vector<std::string>;

/// The concealed text of a line computed independently: the line without its hidden ranges.
std::u16string concealedReference(std::u16string_view line, const LineRender& r) {
    std::vector<bool> hidden(line.size(), false);
    for (const auto& h : r.hidden) {
        for (std::uint32_t i = h.span.start; i < h.span.end() && i < line.size(); ++i) {
            hidden[i] = true;
        }
    }
    std::u16string out;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (!hidden[i]) {
            out += line[i];
        }
    }
    return out;
}

double luminance(const Color& c) {
    auto channel = [](int v) {
        const double s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

double contrast(const Color& a, const Color& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
}  // namespace

// --- Case folding and whitespace (decision 6, ADR 0008) ---------------------------------------

TEST(S028_FindInFile, CaseFoldingIsSimpleUnicodeFolding) {
    EXPECT_EQ(caseFoldingUnicodeVersion(), "18.0.0");
    EXPECT_EQ(foldCaseSimple(U'A'), U'a');
    EXPECT_EQ(foldCaseSimple(U'a'), U'a');
    EXPECT_EQ(foldCaseSimple(U'Ä'), U'ä');  // Ä
    EXPECT_EQ(foldCaseSimple(U'Σ'), U'σ');  // Σ
    EXPECT_EQ(foldCaseSimple(U'ς'), U'σ');  // final ς
    EXPECT_EQ(foldCaseSimple(U'ẞ'), U'ß');  // ẞ (status S)
    EXPECT_EQ(foldCaseSimple(U'ß'), U'ß');  // ß: only full folding gives "ss"
    EXPECT_EQ(foldCaseSimple(U'K'), U'k');  // Kelvin sign
    EXPECT_EQ(foldCaseSimple(U'ꭰ'), U'Ꭰ');  // Cherokee small letter folds to capital
    EXPECT_EQ(foldCaseSimple(U'Ꭰ'), U'Ꭰ');
    EXPECT_EQ(foldCaseSimple(U'\U00010400'), U'\U00010428');  // Deseret, outside the BMP
    EXPECT_EQ(foldCaseSimple(U'İ'), U'İ');                    // İ has only F and T mappings
    EXPECT_EQ(foldCaseSimple(U'I'), U'i');                    // I: status C, the Turkic T mapping is not used
    EXPECT_EQ(foldCaseSimple(U'ä'), U'ä');                    // no diacritic folding
}

TEST(S028_FindInFile, WhitespaceIsTheUnicodeWhiteSpaceProperty) {
    for (const char32_t c : {U'\t', U'\n', U'\v', U'\f', U'\r', U' ', U'\u0085', U' ', U' ', U' ', U' ', U' ', U' ',
                             U' ', U' ', U' ', U'　'}) {
        EXPECT_TRUE(isUnicodeWhitespace(c)) << static_cast<std::uint32_t>(c);
    }
    for (const char32_t c : {U'a', U'​', U'﻿', U'᠎', U'_', U'­'}) {
        EXPECT_FALSE(isUnicodeWhitespace(c)) << static_cast<std::uint32_t>(c);
    }
}

TEST(S028_FindInFile, SearchTextIsWordsBetweenWhitespaceRuns) {
    const FindQuery q(u"  Foo \t  BÄR ", {});
    ASSERT_EQ(q.words().size(), 2U);
    EXPECT_EQ(q.words()[0], U"foo");
    EXPECT_EQ(q.words()[1], U"bär");
    EXPECT_TRUE(q.leadingWhitespace());
    EXPECT_TRUE(q.trailingWhitespace());
    const FindQuery exact(u"Foo", {.matchCase = true});
    ASSERT_EQ(exact.words().size(), 1U);
    EXPECT_EQ(exact.words()[0], U"Foo");
    EXPECT_FALSE(exact.leadingWhitespace());
    EXPECT_TRUE(FindQuery(u"", {}).findsNothing());
    EXPECT_TRUE(FindQuery(u" \t  ", {}).findsNothing());
    EXPECT_FALSE(FindQuery(u"x", {}).findsNothing());
}

TEST(S028_FindInFile, EmptyAndWhitespaceSearchTextsFindNothing) {
    EXPECT_TRUE(find(u"a b\n\nc", u"").matches.empty());
    EXPECT_TRUE(find(u"a b\n\nc", u"  ").matches.empty());
}

// --- Decision 1: the concealed view ------------------------------------------------------------

TEST(S028_FindInFile, FormattingCharactersDoNotCount) {
    const auto s = find(u"Das ist **wichtig:** denken", u"wichtig: denken");
    EXPECT_EQ(s.sources(), (Strings{"wichtig:** denken"}));
    ASSERT_EQ(s.matches.size(), 1U);
    EXPECT_EQ(s.matches[0].start, (DocPos{0, 10}));
    EXPECT_EQ(s.matches[0].end, (DocPos{0, 27}));
}

TEST(S028_FindInFile, ConcealedSyntaxDoesNotCount) {
    const std::u16string doc =
        u"# Titel\n\n- Punkt\n\n`code` und \\* und ~~weg~~\n\n[Text](https://x.at)\n\n```python\nprint(1)\n```";
    EXPECT_EQ(find(doc, u"Titel").sources(), (Strings{"Titel"}));
    EXPECT_EQ(find(doc, u"Punkt").sources(), (Strings{"Punkt"}));
    EXPECT_EQ(find(doc, u"code und * und weg").sources(), (Strings{"code` und \\* und ~~weg"}));
    EXPECT_EQ(find(doc, u"Text").sources(), (Strings{"Text"}));
    EXPECT_EQ(find(doc, u"print(1)").sources(), (Strings{"print(1)"}));
    for (const std::u16string_view absent : {u"# Titel", u"- Punkt", u"`code`", u"\\*", u"~~", u"python", u"```"}) {
        EXPECT_TRUE(find(doc, absent).matches.empty()) << utf8(absent);
    }
}

TEST(S028_FindInFile, TextNotRenderedYetCountsAsWritten) {
    const std::u16string doc = u"1. erster\n\n> Zitat\n\nK&auml;se";
    EXPECT_EQ(find(doc, u"1. erster").sources(), (Strings{"1. erster"}));
    EXPECT_EQ(find(doc, u"> Zitat").sources(), (Strings{"> Zitat"}));
    EXPECT_EQ(find(doc, u"K&auml;se").sources(), (Strings{"K&auml;se"}));
    EXPECT_TRUE(find(doc, u"Käse").matches.empty());
}

TEST(S028_FindInFile, ThematicBreaksSetextUnderlinesAndFencesAreNotText) {
    const std::u16string doc = u"Kopf\n===\n\n---\n\n~~~\nx\n~~~";
    EXPECT_TRUE(find(doc, u"===").matches.empty());
    EXPECT_TRUE(find(doc, u"---").matches.empty());
    EXPECT_TRUE(find(doc, u"~~~").matches.empty());
    EXPECT_EQ(find(doc, u"Kopf x").sources(), (Strings{"Kopf\n===\n\n---\n\n~~~\nx"}));
}

TEST(S028_FindInFile, LineUnitsAreTheLineWithoutItsHiddenRanges) {
    const std::u16string_view line = u"- **a** `b` [c](d \"t\") \\_";
    const LineRender r = registry().render(line, {});
    const auto units = lineUnits(line, 7, r);
    ASSERT_GE(units.size(), 2U);
    EXPECT_EQ(units[0].kind, FindUnitKind::Line);
    EXPECT_EQ(utf8(units[0].text), utf8(concealedReference(line, r)));
    EXPECT_EQ(units[1].kind, FindUnitKind::Destination);
    EXPECT_EQ(units[1].text, u"d");
    EXPECT_EQ(units[1].before, FindBoundary::Hard);
    for (const auto& seg : units[0].segments) {
        EXPECT_EQ(seg.source.line, 7U);
    }
}

// --- Decision 2: link destinations -------------------------------------------------------------

TEST(S028_FindInFile, AMatchInADestinationHighlightsTheLinkText) {
    const auto s = find(u"Absatz [Doku](https://example.com/handbuch) hier", u"handbuch");
    EXPECT_EQ(s.sources(), (Strings{"@Doku"}));
    ASSERT_EQ(s.matches.size(), 1U);
    EXPECT_EQ(s.matches[0].start, (DocPos{0, 8}));
    EXPECT_EQ(s.matches[0].end, (DocPos{0, 12}));
}

TEST(S028_FindInFile, ADestinationMatchStandsAtTheStartOfItsLinkText) {
    const auto s = find(u"[Doku](https://x/hier) und hier", u"hier");
    EXPECT_EQ(s.sources(), (Strings{"@Doku", "hier"}));
}

TEST(S028_FindInFile, NoMatchCrossesTheEdgesOfADestination) {
    const std::u16string doc = u"Absatz [Doku](https://example.com/handbuch) hier\nweiter";
    EXPECT_TRUE(find(doc, u"hier https").matches.empty());
    EXPECT_TRUE(find(doc, u"Doku https").matches.empty());
    EXPECT_TRUE(find(doc, u"handbuch weiter").matches.empty());
    EXPECT_EQ(find(doc, u"Absatz Doku hier weiter").sources(),
              (Strings{"Absatz [Doku](https://example.com/handbuch) hier\nweiter"}));
    // A hard edge is a line boundary for decision 5.
    EXPECT_EQ(find(doc, u" https").sources(), (Strings{"@Doku"}));
}

TEST(S028_FindInFile, DestinationsResolveEscapesButNotEntitiesAndTitlesAreNotSearched) {
    EXPECT_EQ(find(u"[x](y\\)z)", u"y)z").sources(), (Strings{"@x"}));
    EXPECT_EQ(find(u"[x](/hand&amp;buch)", u"hand&amp;buch").sources(), (Strings{"@x"}));
    EXPECT_TRUE(find(u"[x](/hand&amp;buch)", u"hand&buch").matches.empty());
    EXPECT_TRUE(find(u"[a](u \"Titel\")", u"Titel").matches.empty());
}

// --- Decision 4: whitespace runs ---------------------------------------------------------------

TEST(S028_FindInFile, WhitespaceRunsMatchAcrossLinesBlankLinesAndBlocks) {
    const std::u16string doc = u"ein foo\n\nbar zwei\nfoo\tbar\nfoo bar\n# Kopf\nText";
    EXPECT_EQ(find(doc, u"foo bar").sources(), (Strings{"foo\n\nbar", "foo\tbar", "foo bar"}));
    EXPECT_EQ(find(doc, u"foo  bar").sources(), (Strings{"foo\n\nbar", "foo\tbar", "foo bar"}));
    EXPECT_EQ(find(doc, u"Kopf Text").sources(), (Strings{"Kopf\nText"}));
    EXPECT_TRUE(find(doc, u"foobar").matches.empty());
}

TEST(S028_FindInFile, AWhitespaceRunNeedsAtLeastOneWhitespaceCharacterOrBoundary) {
    EXPECT_TRUE(find(u"ab", u"a b").matches.empty());
    EXPECT_EQ(find(u"a\nb", u"a b").sources(), (Strings{"a\nb"}));
    // Concealed delimiters are not whitespace.
    EXPECT_TRUE(find(u"**a**b", u"a b").matches.empty());
}

// --- Decision 5: whitespace at the start or end ------------------------------------------------

TEST(S028_FindInFile, LeadingWhitespaceRequiresWhitespaceOrALineBoundary) {
    const std::u16string doc = u"Die Agenten\nSubagent\nAgent ist da\n- **Agent** fett";
    EXPECT_EQ(find(doc, u" Agent").sources(), (Strings{"Agent", "Agent", "Agent"}));
    const auto s = find(doc, u" Agent");
    ASSERT_EQ(s.matches.size(), 3U);
    EXPECT_EQ(s.matches[0].start, (DocPos{0, 4}));  // highlighted without the space
    EXPECT_EQ(s.matches[1].start, (DocPos{2, 0}));
    EXPECT_EQ(s.matches[2].start, (DocPos{3, 4}));
}

TEST(S028_FindInFile, TrailingWhitespaceRequiresWhitespaceOrALineBoundary) {
    const auto s = find(u"Die Agenten\nSubagent\nAgent ist da", u"Agent ");
    EXPECT_EQ(s.sources(), (Strings{"agent", "Agent"}));
    ASSERT_EQ(s.matches.size(), 2U);
    EXPECT_EQ(s.matches[0].start, (DocPos{1, 3}));
    EXPECT_EQ(s.matches[1].end, (DocPos{2, 5}));  // without the space
}

// --- Decision 6: case --------------------------------------------------------------------------

TEST(S028_FindInFile, MatchCaseOffFoldsCase) {
    const std::u16string doc = u"Äpfel und äpfel und Apfel";
    EXPECT_EQ(find(doc, u"äpfel").sources(), (Strings{"Äpfel", "äpfel"}));
    EXPECT_EQ(find(doc, u"apfel").sources(), (Strings{"Apfel"}));
    EXPECT_EQ(find(doc, u"äpfel", {.matchCase = true}).sources(), (Strings{"äpfel"}));
    EXPECT_EQ(find(u"ΟΔΟΣ οδος", u"οδοσ").sources(), (Strings{"ΟΔΟΣ", "οδος"}));
    EXPECT_EQ(find(u"Straẞe", u"straße").sources(), (Strings{"Straẞe"}));
    EXPECT_TRUE(find(u"STRASSE", u"straße").matches.empty());
    EXPECT_EQ(find(u"\U00010400x", u"\U00010428X").sources(), (Strings{"\U00010400x"}));
}

TEST(S028_FindInFile, NoNormalisation) {
    EXPECT_TRUE(find(u"äpfel", u"äpfel").matches.empty());
    EXPECT_TRUE(find(u"äpfel", u"äpfel").matches.empty());
}

TEST(S028_FindInFile, MatchesDoNotOverlap) {
    EXPECT_EQ(find(u"aaaa", u"aa").sources(), (Strings{"aa", "aa"}));
    const auto s = find(u"aaaa", u"aa");
    ASSERT_EQ(s.matches.size(), 2U);
    EXPECT_EQ(s.matches[1].start, (DocPos{0, 2}));
}

// --- Decision 3: tables --------------------------------------------------------------------------

namespace {
const std::u16string kTable =
    u"Vorher\n"
    u"\n"
    u"| Farbe | Text |\n"
    u"| --- | --- |\n"
    u"| **rot** und grün | a \\| b |\n"
    u"| - x | c<br>d | extra |\n"
    u"| [L](https://ziel) | # h |\n"
    u"\n"
    u"Nachher";
}  // namespace

TEST(S028_FindInFile, CellsAreSearchedWithoutSyntax) {
    EXPECT_EQ(find(kTable, u"rot und").sources(), (Strings{"rot** und"}));
    const auto s = find(kTable, u"rot und");
    ASSERT_EQ(s.matches.size(), 1U);
    EXPECT_EQ(s.matches[0].start, (DocPos{4, 4}));
    EXPECT_EQ(find(kTable, u"a | b").sources(), (Strings{"a \\| b"}));
    EXPECT_EQ(find(kTable, u"- x").sources(), (Strings{"- x"}));
    EXPECT_EQ(find(kTable, u"# h").sources(), (Strings{"# h"}));
    EXPECT_EQ(find(kTable, u"c d").sources(), (Strings{"c<br>d"}));
    EXPECT_EQ(find(kTable, u"ziel").sources(), (Strings{"@L"}));
}

TEST(S028_FindInFile, PipesTheDelimiterRowAndExtraCellsDoNotCount) {
    EXPECT_TRUE(find(kTable, u"Farbe | Text").matches.empty());
    EXPECT_TRUE(find(kTable, u"---").matches.empty());
    EXPECT_TRUE(find(kTable, u"extra").matches.empty());
    EXPECT_TRUE(find(kTable, u"|").matches.size() == 1U);  // only the escaped pipe in a cell
}

TEST(S028_FindInFile, CellsAreSeparatedBySoftBoundariesAndTheTableByHardEdges) {
    EXPECT_EQ(find(kTable, u"Farbe Text").sources(), (Strings{"Farbe | Text"}));
    EXPECT_EQ(find(kTable, u"Text rot").sources(), (Strings{"Text |\n| --- | --- |\n| **rot"}));
    EXPECT_EQ(find(kTable, u"grün a").sources(), (Strings{"grün | a"}));
    EXPECT_TRUE(find(kTable, u"Vorher Farbe").matches.empty());
    EXPECT_TRUE(find(kTable, u"h Nachher").matches.empty());
    EXPECT_EQ(find(kTable, u" Farbe").sources(), (Strings{"Farbe"}));
}

TEST(S028_FindInFile, TableRowUnitsMapBackToTheLine) {
    const std::u16string_view line = u"| **rot** und | a \\| b | x |";
    const auto units = tableRowUnits(registry(), line, 3, 2);
    ASSERT_EQ(units.size(), 2U);
    EXPECT_EQ(units[0].kind, FindUnitKind::Cell);
    EXPECT_EQ(units[0].text, u"rot und");
    EXPECT_EQ(units[1].text, u"a | b");
    ASSERT_FALSE(units[1].segments.empty());
    bool atomicPipe = false;
    for (const auto& seg : units[1].segments) {
        EXPECT_EQ(seg.source.line, 3U);
        atomicPipe = atomicPipe || (seg.textLength == 1 && seg.sourceEnd.column - seg.source.column == 2);
    }
    EXPECT_TRUE(atomicPipe);
}

// Code review O8: a lookup starts at a table row with the header's column count, not at the header.
TEST(S028_FindInFile, ABuilderContinuedInATableBuildsItsRowsAsFromTheHeader) {
    const auto lines = splitLines(kTable);
    const auto renders = registry().renderDocument(lines);
    const auto lineOf = [](const FindUnit& u) {
        if (u.kind == FindUnitKind::Destination) {
            return u.linkStart.line;
        }
        return u.segments.empty() ? std::size_t{0} : u.segments.front().source.line;
    };
    std::vector<FindUnit> fromHeader;
    FindUnitBuilder whole(registry());
    for (std::size_t i = 2; i < lines.size(); ++i) {
        whole.addLine(lines[i], i, renders[i], fromHeader);
    }
    // From the first unit of line 5 on (an empty line has a unit without segments).
    const auto row = std::find_if(fromHeader.begin(), fromHeader.end(), [&](const FindUnit& u) {
        return u.kind != FindUnitKind::Destination && !u.segments.empty() && lineOf(u) == 5;
    });
    const std::vector<FindUnit> expected(row, fromHeader.end());
    std::vector<FindUnit> fromRow;
    FindUnitBuilder builder(registry());
    builder.continueTable(2);
    for (std::size_t i = 5; i < lines.size(); ++i) {
        builder.addLine(lines[i], i, renders[i], fromRow);
    }
    ASSERT_EQ(fromRow.size(), expected.size());
    for (std::size_t k = 0; k < expected.size(); ++k) {
        EXPECT_EQ(fromRow[k].text, expected[k].text) << k;
        EXPECT_EQ(fromRow[k].kind, expected[k].kind) << k;
        EXPECT_EQ(fromRow[k].before, expected[k].before) << k;
        EXPECT_EQ(lineOf(fromRow[k]), lineOf(expected[k])) << k;
    }
}

// --- Decision 3: front matter --------------------------------------------------------------------

namespace {
const std::u16string kFrontMatter =
    u"---\n"
    u"title: \"Hallo\\tWelt\"\n"
    u"date: 2026-11-07\n"
    u"done: true\n"
    u"tags: [rot, grün]\n"
    u"map: {a: 1}\n"
    u"# Kommentar\n"
    u"---\n"
    u"Text";
}  // namespace

TEST(S028_FindInFile, PropertiesAreSearchedAsKeysAndValues) {
    EXPECT_EQ(find(kFrontMatter, u"Hallo Welt").sources(), (Strings{"Hallo\\tWelt"}));
    const auto s = find(kFrontMatter, u"Hallo Welt");
    ASSERT_EQ(s.matches.size(), 1U);
    EXPECT_EQ(s.matches[0].start, (DocPos{1, 8}));
    EXPECT_EQ(s.matches[0].end, (DocPos{1, 19}));
    EXPECT_EQ(find(kFrontMatter, u"2026-11-07").sources(), (Strings{"2026-11-07"}));
    EXPECT_EQ(find(kFrontMatter, u"true").sources(), (Strings{"true"}));
    EXPECT_EQ(find(kFrontMatter, u"grün").sources(), (Strings{"grün"}));
    EXPECT_EQ(find(kFrontMatter, u"rot grün").sources(), (Strings{"rot, grün"}));
    EXPECT_EQ(find(kFrontMatter, u"title Hallo").sources(), (Strings{"title: \"Hallo"}));
    EXPECT_EQ(find(kFrontMatter, u"a 1").sources(), (Strings{"a: 1"}));
}

TEST(S028_FindInFile, YamlAndJsonSyntaxDoesNotCount) {
    for (const std::u16string_view absent :
         {u"title: \"Hallo", u"\"a\":1", u"{a", u"Properties", u"Kommentar", u"---", u"[rot", u"date:"}) {
        EXPECT_TRUE(find(kFrontMatter, absent).matches.empty()) << utf8(absent);
    }
}

TEST(S028_FindInFile, APropertiesBlockHasHardEdges) {
    EXPECT_TRUE(find(kFrontMatter, u"1 Text").matches.empty());
    EXPECT_EQ(find(kFrontMatter, u"Text").sources(), (Strings{"Text"}));
}

TEST(S028_FindInFile, EscapesAndFoldingMapToTheirWholeSource) {
    const std::u16string doc =
        u"---\n"
        u"a: \"x\\u00e4y\"\n"
        u"b: 'it''s'\n"
        u"c: >\n"
        u"  eins\n"
        u"  zwei\n"
        u"d: eins\n"
        u"  zwei drei\n"
        u"---";
    EXPECT_EQ(find(doc, u"ä").sources(), (Strings{"\\u00e4"}));
    EXPECT_EQ(find(doc, u"it's").sources(), (Strings{"it''s"}));
    EXPECT_EQ(find(doc, u"eins zwei").sources(), (Strings{"eins\n  zwei", "eins\n  zwei"}));
}

TEST(S028_FindInFile, FrontMatterAsTextWhenInvalidOrSwitchedOff) {
    EXPECT_EQ(find(kFrontMatter, u"title: \"Hallo", {}, {.frontMatterAsProperties = false}).sources(),
              (Strings{"title: \"Hallo"}));
    const std::u16string invalid = u"---\na: b: c\n---\nText";
    EXPECT_EQ(find(invalid, u"a: b").sources(), (Strings{"a: b"}));
    // Shown as text, its delimiters are text too.
    EXPECT_EQ(find(invalid, u"c --- Text").sources(), (Strings{"c\n---\nText"}));
}

TEST(S028_FindInFile, PropertyUnitsAreEmptyForInvalidYaml) {
    const std::vector<std::u16string> yaml{u"a: b: c"};
    EXPECT_TRUE(propertyUnits(yaml, 1).empty());
    const std::vector<std::u16string> ok{u"k: v", u"l: [x, y]"};
    const auto units = propertyUnits(ok, 1);
    ASSERT_EQ(units.size(), 5U);
    EXPECT_EQ(units[0].text, u"k");
    EXPECT_EQ(units[0].before, FindBoundary::Hard);
    EXPECT_EQ(units[1].text, u"v");
    EXPECT_EQ(units[1].before, FindBoundary::Soft);
    EXPECT_EQ(units[4].text, u"y");
    EXPECT_EQ(units[4].segments.front().source, (DocPos{2, 7}));
}

// --- The reference -----------------------------------------------------------------------------

TEST(S028_FindInFile, DocumentUnitsFollowRenderDocument) {
    const std::vector<std::u16string> lines =
        splitLines(u"# A *b*\n\nText `c` und [d](e)\n- f\n\n```\ng\n```\nh\n---\n\ni");
    const auto renders = registry().renderDocument(lines);
    const auto units = documentUnits(registry(), lines);
    std::vector<std::u16string> lineTexts;
    for (const auto& u : units) {
        if (u.kind == FindUnitKind::Line) {
            lineTexts.push_back(u.text);
        }
    }
    ASSERT_EQ(lineTexts.size(), lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(utf8(lineTexts[i]), utf8(concealedReference(lines[i], renders[i]))) << i;
    }
}

TEST(S028_FindInFile, RandomDocumentsFindWhatTheyConceal) {
    std::mt19937 rng(2811);
    const std::vector<std::u16string> pieces{u"# ",   u"- ",   u"**", u"*", u"`", u"[",  u"](", u")",   u"\\", u"~~",
                                             u"Wort", u"wort", u"Ä",  u"ä", u" ", u"\t", u"|",  u"---", u"x"};
    for (int round = 0; round < 300; ++round) {
        std::u16string doc;
        const int lineCount = 1 + static_cast<int>(rng() % 6);
        for (int l = 0; l < lineCount; ++l) {
            const int n = static_cast<int>(rng() % 8);
            for (int p = 0; p < n; ++p) {
                doc += pieces[rng() % pieces.size()];
            }
            doc += u'\n';
        }
        const auto lines = splitLines(doc);
        const auto matches = findInDocument(registry(), lines, u"wort");
        // In document order; matches outside destinations never overlap (a destination match
        // highlights the link text, where a match of the text may lie too).
        DocPos lastStart{};
        DocPos lastEnd{};
        for (const auto& m : matches) {
            EXPECT_LE(lastStart, m.start) << utf8(doc);
            EXPECT_LT(m.start, m.end) << utf8(doc);
            lastStart = m.start;
            if (!m.inDestination) {
                EXPECT_LE(lastEnd, m.start) << utf8(doc);
                lastEnd = m.end;
            }
        }
        // Every concealed "wort" of a line unit is found (no whitespace in the search text).
        std::size_t expected = 0;
        for (const auto& u : documentUnits(registry(), lines)) {
            std::u16string folded;
            for (const char16_t c : u.text) {
                folded += static_cast<char16_t>(foldCaseSimple(c));
            }
            for (std::size_t at = folded.find(u"wort"); at != std::u16string::npos; at = folded.find(u"wort", at + 4)) {
                ++expected;
            }
        }
        EXPECT_EQ(matches.size(), expected) << utf8(doc);
    }
}

// Added with the implementation: the UI scans the units in pieces (a line at a time, keeping only
// what a pending match needs); that finds exactly what one scan finds.
TEST(S028_FindInFile, ScanningInPiecesFindsWhatOneScanFinds) {
    std::mt19937 rng(28112);
    const std::vector<std::u16string> pieces{u"a", u"b", u" ", u"\t", u"**", u"[", u"](", u")", u"|", u"- ", u"\n"};
    for (int round = 0; round < 400; ++round) {
        std::u16string doc;
        const int n = 5 + static_cast<int>(rng() % 30);
        for (int p = 0; p < n; ++p) {
            doc += pieces[rng() % pieces.size()];
        }
        const auto lines = splitLines(doc);
        const auto units = documentUnits(registry(), lines);
        for (const std::u16string_view text : {u"a", u"a b", u" a", u"b ", u"a  b a", u"ab"}) {
            const FindQuery query(text, {});
            const auto whole = findInUnits(units, query);
            std::vector<FindMatch> pieces2;
            std::vector<FindUnit> buffer;
            std::size_t resumeUnit = 0;
            std::uint32_t resumeOffset = 0;
            for (std::size_t u = 0; u < units.size(); ++u) {
                buffer.push_back(units[u]);
                const FindScan scan = scanUnits(buffer, resumeUnit, resumeOffset, query, u + 1 < units.size());
                pieces2.insert(pieces2.end(), scan.matches.begin(), scan.matches.end());
                const std::size_t keep = std::min(scan.resumeUnit, buffer.size());
                buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(keep));
                resumeUnit = 0;
                resumeOffset = buffer.empty() ? 0 : scan.resumeOffset;
            }
            std::stable_sort(pieces2.begin(), pieces2.end(),
                             [](const FindMatch& a, const FindMatch& b) { return a.start < b.start; });
            ASSERT_EQ(pieces2.size(), whole.size()) << utf8(doc) << " / " << utf8(text);
            for (std::size_t i = 0; i < whole.size(); ++i) {
                EXPECT_EQ(pieces2[i].start, whole[i].start) << utf8(doc);
                EXPECT_EQ(pieces2[i].end, whole[i].end) << utf8(doc);
                EXPECT_EQ(pieces2[i].inDestination, whole[i].inDestination) << utf8(doc);
            }
        }
    }
}

// Code review O1: a match across a line end does not swallow the destinations of that line.
TEST(S028_FindInFile, ADestinationSkippedByAMatchAcrossALineEndIsStillSearched) {
    EXPECT_EQ(find(u"x [y](<a a>) a\na", u"a a").sources(), (Strings{"@y", "a\na"}));
}

// Code review O2: the context read to a screen reader leaves out destinations the match skipped.
TEST(S028_FindInFile, MatchContextLeavesOutSkippedDestinations) {
    const std::vector<std::u16string> lines{u"Absatz [Doku](https://example.com/handbuch) hier", u"weiter geht es"};
    const auto units = documentUnits(registry(), lines);
    const auto matches = findInUnits(units, FindQuery(u"hier weiter", {}));
    ASSERT_EQ(matches.size(), 1U);
    EXPECT_EQ(matchContext(units, matches[0]), u"Absatz Doku hier weiter geht es");
}

TEST(S028_FindInFile, MatchContextForScreenReaders) {
    const std::vector<std::u16string> lines{u"Die **Agenten** arbeiten heute"};
    const auto units = documentUnits(registry(), lines);
    const auto matches = findInUnits(units, FindQuery(u"Agenten", {}));
    ASSERT_EQ(matches.size(), 1U);
    EXPECT_EQ(matchContext(units, matches[0]), u"Die Agenten arbeiten heute");
    const std::vector<std::u16string> longLine{std::u16string(100, u'a') + u" Ziel " + std::u16string(100, u'b')};
    const auto longUnits = documentUnits(registry(), longLine);
    const auto longMatches = findInUnits(longUnits, FindQuery(u"Ziel", {}));
    ASSERT_EQ(longMatches.size(), 1U);
    const std::u16string context = matchContext(longUnits, longMatches[0]);
    EXPECT_LE(context.size(), 80U);
    EXPECT_NE(context.find(u"Ziel"), std::u16string::npos);
    EXPECT_EQ(context.front(), u'…');
    EXPECT_EQ(context.back(), u'…');
}

// --- Theming baseline ---------------------------------------------------------------------------

TEST(S028_FindInFile, FindTokensFallBackAsDocumented) {
    const Theme& light = builtinTheme(ColorScheme::Light);
    const FindStyle l = findStyle(light);
    EXPECT_EQ(l.background, light.surface);
    EXPECT_EQ(l.border, light.border);
    EXPECT_EQ(l.text, light.textMuted);
    EXPECT_EQ(l.fieldBackground, light.background);
    EXPECT_EQ(l.fieldText, light.text);
    EXPECT_EQ(l.fieldBorder, light.border);
    EXPECT_EQ(l.fieldFocusBorder, light.primary);
    EXPECT_EQ(l.icon, light.text);
    EXPECT_EQ(l.hoverBackground, light.border);
    EXPECT_EQ(l.activeBackground, light.selection);
    EXPECT_EQ(l.activeIcon, light.selectionText);
    EXPECT_EQ(l.noMatchBackground, (Color{0xf8, 0xd7, 0xda, 255}));
    EXPECT_EQ(l.currentBackground, (Color{0xff, 0xd4, 0x3b, 255}));
    EXPECT_EQ(l.currentText, (Color{0x1a, 0x1a, 0x1a, 255}));
    EXPECT_EQ(l.matchBackground, (Color{0xff, 0xf3, 0xbf, 255}));
    EXPECT_EQ(l.matchText, (Color{0x1a, 0x1a, 0x1a, 255}));
    EXPECT_EQ(l.borderWidth, 1);
    EXPECT_EQ(l.padding, light.spacing);
    EXPECT_EQ(l.spacing, light.spacing);
    EXPECT_EQ(l.fontScale, 1);
    EXPECT_EQ(l.fieldBorderWidth, 1);
    EXPECT_EQ(l.fieldRadius, light.radius);
    EXPECT_EQ(l.buttonSize, 24);
    EXPECT_EQ(l.buttonRadius, light.radius);
    EXPECT_EQ(l.fontFamily, "");

    const FindStyle d = findStyle(builtinTheme(ColorScheme::Dark));
    EXPECT_EQ(d.noMatchBackground, (Color{0x5c, 0x2b, 0x2e, 255}));
    EXPECT_EQ(d.currentBackground, (Color{0xc9, 0x9a, 0x06, 255}));
    EXPECT_EQ(d.currentText, (Color{0x1a, 0x1a, 0x1a, 255}));
    EXPECT_EQ(d.matchBackground, (Color{0x5c, 0x4a, 0x12, 255}));
    EXPECT_EQ(d.matchText, (Color{0xf1, 0xf1, 0xf1, 255}));
}

TEST(S028_FindInFile, FindTokensAreParsed) {
    const auto parsed = parseTheme(
        "[find]\n"
        "background = \"#010101\"\nborder = \"#020202\"\nborder-width = 2\npadding = 3\nspacing = 5\n"
        "text = \"#030303\"\nfont-family = \"Mono\"\nfont-scale = 1.5\nfield-background = \"#040404\"\n"
        "field-text = \"#050505\"\nfield-border = \"#060606\"\nfield-focus-border = \"#070707\"\n"
        "field-border-width = 3\nfield-radius = 9\nno-match-background = \"#080808\"\nicon = \"#090909\"\n"
        "button-size = 30\nbutton-radius = 4\nhover-background = \"#0a0a0a\"\nactive-background = \"#0b0b0b\"\n"
        "active-icon = \"#0c0c0c\"\ncurrent-background = \"#0d0d0d\"\ncurrent-text = \"#0e0e0e\"\n"
        "match-background = \"#0f0f0f\"\nmatch-text = \"#101010\"\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {  // explicit for bugprone-unchecked-optional-access
        FAIL() << "theme not parsed";
    }
    const FindStyle s = findStyle(*parsed.theme);
    EXPECT_EQ(s.background, (Color{1, 1, 1, 255}));
    EXPECT_EQ(s.border, (Color{2, 2, 2, 255}));
    EXPECT_EQ(s.borderWidth, 2);
    EXPECT_EQ(s.padding, 3);
    EXPECT_EQ(s.spacing, 5);
    EXPECT_EQ(s.text, (Color{3, 3, 3, 255}));
    EXPECT_EQ(s.fontFamily, "Mono");
    EXPECT_EQ(s.fontScale, 1.5);
    EXPECT_EQ(s.fieldBackground, (Color{4, 4, 4, 255}));
    EXPECT_EQ(s.fieldText, (Color{5, 5, 5, 255}));
    EXPECT_EQ(s.fieldBorder, (Color{6, 6, 6, 255}));
    EXPECT_EQ(s.fieldFocusBorder, (Color{7, 7, 7, 255}));
    EXPECT_EQ(s.fieldBorderWidth, 3);
    EXPECT_EQ(s.fieldRadius, 9);
    EXPECT_EQ(s.noMatchBackground, (Color{8, 8, 8, 255}));
    EXPECT_EQ(s.icon, (Color{9, 9, 9, 255}));
    EXPECT_EQ(s.buttonSize, 30);
    EXPECT_EQ(s.buttonRadius, 4);
    EXPECT_EQ(s.hoverBackground, (Color{10, 10, 10, 255}));
    EXPECT_EQ(s.activeBackground, (Color{11, 11, 11, 255}));
    EXPECT_EQ(s.activeIcon, (Color{12, 12, 12, 255}));
    EXPECT_EQ(s.currentBackground, (Color{13, 13, 13, 255}));
    EXPECT_EQ(s.currentText, (Color{14, 14, 14, 255}));
    EXPECT_EQ(s.matchBackground, (Color{15, 15, 15, 255}));
    EXPECT_EQ(s.matchText, (Color{16, 16, 16, 255}));
}

TEST(S028_FindInFile, BuiltInHighlightsKeepTheirContrast) {
    for (const ColorScheme scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const FindStyle s = findStyle(builtinTheme(scheme));
        EXPECT_GE(contrast(s.currentText, s.currentBackground), 4.5);
        EXPECT_GE(contrast(s.matchText, s.matchBackground), 4.5);
    }
}
