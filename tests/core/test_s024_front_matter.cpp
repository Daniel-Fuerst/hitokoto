// SPDX-License-Identifier: GPL-3.0-or-later
// S024: front matter shown as properties. The YAML is read by a parser of our own for a subset of
// YAML 1.2 (ADR 0007). Expected values checked on 2026-10-30 with js-yaml 4.3.2 and ruamel.yaml
// 0.19.1 (both YAML 1.2), JSON numbers with Node 22; where the story decides otherwise, the test
// says so.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/properties.hpp"
#include "hitokoto/core/text_codec.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

std::string narrow(std::u16string_view text) {
    std::string out;
    for (const char16_t c : text) {
        out += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return out;
}

/// The lines of `text` (split at '\n'; a final '\n' ends the last line).
std::vector<std::u16string> linesOf(std::u16string_view text) {
    std::vector<std::u16string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find(u'\n', start), text.size());
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

Properties parse(std::u16string_view yaml) {
    return parseProperties(linesOf(yaml));
}

/// The row with `key`, or a failure.
const Property* row(const Properties& p, std::u16string_view key) {
    for (const Property& r : p.rows) {
        if (r.key == key) {
            return &r;
        }
    }
    ADD_FAILURE() << "no row " << narrow(key);
    return nullptr;
}

std::vector<std::u16string> keys(const Properties& p) {
    std::vector<std::u16string> out;
    out.reserve(p.rows.size());
    for (const Property& r : p.rows) {
        out.push_back(r.key);
    }
    return out;
}

/// Armin's example (2026-10-30): the YAML of the review record 022-inline-links-code-recheck3.md,
/// between its `---` lines (document lines 1 and 27, counted from 1).
constexpr std::u16string_view kReviewRecord =
    uR"(mode: code
spec: docs/stories/022-inline-links.md
branch: feature/links_references  # Daniel's branch, pull request #21
commit_range: d5e959f..60263e3  # third follow-up: 3ecbe77, 6357d45, 60263e3
reviewed_on: 2026-10-30
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark)
objections:
  - id: O19
    category: failure
    severity: major
    title: On a revealed line link text overlaps emphasis and escape markers; fuzz_render aborts (since bb344f0)
    disposition: pending
    disposition_rationale: null
  - id: O20
    category: failure
    severity: major
    title: A code span no longer lets a link form across it; the new test pins this against CommonMark
    disposition: pending
    disposition_rationale: null
  - id: O21
    category: operational
    severity: minor
    title: Fix committed before its tests
    disposition: pending
    disposition_rationale: null
)";

void expectError(std::u16string_view yaml, PropertiesErrorReason reason, std::size_t line) {
    const Properties p = parse(yaml);
    if (!p.error) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "no error: " << narrow(yaml);
    }
    EXPECT_EQ(p.error->reason, reason) << narrow(yaml);
    EXPECT_EQ(p.error->line, line) << narrow(yaml);
    EXPECT_TRUE(p.rows.empty()) << narrow(yaml);
}
}  // namespace

// ---- Armin's example ------------------------------------------------------------------------

TEST(S024_FrontMatter, ReviewRecordShowsItsProperties) {
    const Properties p = parse(kReviewRecord);
    ASSERT_FALSE(p.error.has_value());
    EXPECT_EQ(keys(p), (std::vector<std::u16string>{u"mode", u"spec", u"branch", u"commit_range", u"reviewed_on",
                                                    u"environment", u"objections"}));
    if (const Property* r = row(p, u"branch")) {
        EXPECT_EQ(r->type, PropertyType::Text);
        EXPECT_EQ(r->text, u"feature/links_references") << "the comment is no part of the value";
        EXPECT_EQ(r->keyLine, 2U);
        EXPECT_EQ(r->valueLine, 2U);
        EXPECT_EQ(r->valueColumn, 8U);
    }
    if (const Property* r = row(p, u"commit_range")) {
        EXPECT_EQ(r->text, u"d5e959f..60263e3");
    }
    if (const Property* r = row(p, u"reviewed_on")) {
        EXPECT_EQ(r->type, PropertyType::Date);
        EXPECT_EQ(r->text, u"2026-10-30");
        EXPECT_EQ(r->date, (PropertyDate{2026, 10, 30, -1, -1, -1}));
    }
    if (const Property* r = row(p, u"environment")) {
        EXPECT_EQ(r->type, PropertyType::Text);
        EXPECT_EQ(r->text,
                  u"Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations "
                  u"checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark)");
    }
    if (const Property* r = row(p, u"objections")) {
        EXPECT_EQ(r->type, PropertyType::Unsupported);
        // JSON.stringify of js-yaml's value, byte for byte.
        EXPECT_EQ(
            r->text,
            uR"json([{"id":"O19","category":"failure","severity":"major","title":"On a revealed line link text overlaps emphasis and escape markers; fuzz_render aborts (since bb344f0)","disposition":"pending","disposition_rationale":null},{"id":"O20","category":"failure","severity":"major","title":"A code span no longer lets a link form across it; the new test pins this against CommonMark","disposition":"pending","disposition_rationale":null},{"id":"O21","category":"operational","severity":"minor","title":"Fix committed before its tests","disposition":"pending","disposition_rationale":null}])json");
        EXPECT_EQ(r->keyLine, 6U);
        EXPECT_EQ(r->valueLine, 7U) << "a value below its key: the first of its lines";
        EXPECT_EQ(r->valueColumn, 2U) << "at the value's first character";
    }
}

// ---- Types (S024, "Types") ------------------------------------------------------------------

TEST(S024_FrontMatter, TypesFollowFromValues) {
    const Properties p = parse(
        u"text: hello world\n"
        u"b1: true\n"
        u"b2: False\n"
        u"b3: TRUE\n"
        u"word: yes\n"  // YAML 1.2: text (PyYAML, YAML 1.1, would say true)
        u"nul: null\n"
        u"tilde: ~\n"
        u"empty:\n"
        u"qnum: '42'\n"  // quoted: always text
        u"qdate: \"2026-10-30\"\n"
        u"time: 16:28\n"  // YAML 1.2: no sexagesimal numbers
        u"url: https://x.org/a?b=c:d\n");
    ASSERT_FALSE(p.error.has_value());
    const auto expect = [&](std::u16string_view key, PropertyType type, std::u16string_view text) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, type) << narrow(key);
            EXPECT_EQ(r->text, text) << narrow(key);
        }
    };
    expect(u"text", PropertyType::Text, u"hello world");
    expect(u"b1", PropertyType::Checkbox, u"true");
    expect(u"b2", PropertyType::Checkbox, u"False");
    expect(u"b3", PropertyType::Checkbox, u"TRUE");
    expect(u"word", PropertyType::Text, u"yes");
    expect(u"nul", PropertyType::Text, u"");
    expect(u"tilde", PropertyType::Text, u"");
    expect(u"empty", PropertyType::Text, u"");
    expect(u"qnum", PropertyType::Text, u"42");
    expect(u"qdate", PropertyType::Text, u"2026-10-30");
    expect(u"time", PropertyType::Text, u"16:28");
    expect(u"url", PropertyType::Text, u"https://x.org/a?b=c:d");
    if (const Property* r = row(p, u"b1")) {
        EXPECT_TRUE(r->checked);
    }
    if (const Property* r = row(p, u"b2")) {
        EXPECT_FALSE(r->checked);
    }
    if (const Property* r = row(p, u"empty")) {
        EXPECT_EQ(r->valueLine, 7U);
        EXPECT_EQ(r->valueColumn, 6U) << "a null: the end of the key line";
    }
}

TEST(S024_FrontMatter, NumbersOfTheCoreSchema) {
    const Properties p = parse(
        u"int: 42\nneg: -3.5\nexp: 1e3\nhex: 0x1A\noct: 0o17\nplus: +12\nver: 0.31\nlead: 007\n"
        u"inf: .inf\nninf: -.Inf\nnan: .NaN\n"
        u"bin: 0b101\nunder: 1_000\nhexword: 0xZZ\n");
    ASSERT_FALSE(p.error.has_value());
    for (const std::u16string_view key :
         {u"int", u"neg", u"exp", u"hex", u"oct", u"plus", u"ver", u"lead", u"inf", u"ninf", u"nan"}) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, PropertyType::Number) << narrow(key);
        }
    }
    if (const Property* r = row(p, u"lead")) {
        EXPECT_EQ(r->text, u"007") << "shown as written";
    }
    // Not in the core schema: js-yaml and ruamel.yaml read 0b101 as 5, ruamel.yaml 1_000 as 1000.
    for (const std::u16string_view key : {u"bin", u"under", u"hexword"}) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, PropertyType::Text) << narrow(key);
        }
    }
}

TEST(S024_FrontMatter, DatesAndTimes) {
    const Properties p = parse(
        u"d: 2026-10-30\n"
        u"leap: 2024-02-29\n"
        u"dt: 2026-10-30T16:28\n"
        u"dts: 2026-10-30T16:28:05\n"
        u"badday: 2026-02-30\n"  // js-yaml rolls it over to 2026-03-02; the story keeps it text
        u"nonleap: 2025-02-29\n"
        u"badhour: 2026-10-30T24:00\n"
        u"space: 2026-10-30 16:28:00\n"  // a YAML timestamp, but not how Obsidian stores dates
        u"zone: 2026-10-30T16:28:00Z\n"
        u"offset: 2026-10-30T16:28:00+01:00\n"
        u"fraction: 2026-10-30T16:28:00.5\n"
        u"short: 2026-1-3\n");
    ASSERT_FALSE(p.error.has_value());
    const auto expect = [&](std::u16string_view key, PropertyType type, PropertyDate date) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, type) << narrow(key);
            if (type != PropertyType::Text) {
                EXPECT_EQ(r->date, date) << narrow(key);
            }
        }
    };
    expect(u"d", PropertyType::Date, {2026, 10, 30, -1, -1, -1});
    expect(u"leap", PropertyType::Date, {2024, 2, 29, -1, -1, -1});
    expect(u"dt", PropertyType::DateTime, {2026, 10, 30, 16, 28, -1});
    expect(u"dts", PropertyType::DateTime, {2026, 10, 30, 16, 28, 5});
    for (const std::u16string_view key :
         {u"badday", u"nonleap", u"badhour", u"space", u"zone", u"offset", u"fraction", u"short"}) {
        expect(key, PropertyType::Text, {});
    }
}

TEST(S024_FrontMatter, ListsAndTags) {
    const Properties p = parse(
        u"list:\n"
        u"  - a\n"
        u"  - b\n"
        u"flat:\n"
        u"- c\n"  // not indented below the key
        u"flow: [x, \"y, z\", 'w']\n"
        u"none: []\n"
        u"tags: one\n"
        u"aliases:\n"
        u"cssclasses: [wide]\n"
        u"single: x\n");
    ASSERT_FALSE(p.error.has_value());
    const auto expect = [&](std::u16string_view key, PropertyType type, const std::vector<std::u16string>& items) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, type) << narrow(key);
            EXPECT_EQ(r->items, items) << narrow(key);
        }
    };
    expect(u"list", PropertyType::List, {u"a", u"b"});
    expect(u"flat", PropertyType::List, {u"c"});
    expect(u"flow", PropertyType::List, {u"x", u"y, z", u"w"});
    expect(u"none", PropertyType::List, {});
    expect(u"tags", PropertyType::Tags, {u"one"});  // Obsidian may split "a, b"; not verified (S024)
    expect(u"aliases", PropertyType::List, {});
    expect(u"cssclasses", PropertyType::List, {u"wide"});
    if (const Property* r = row(p, u"single")) {
        EXPECT_EQ(r->type, PropertyType::Text) << "only tags, aliases and cssclasses turn a scalar into a list";
    }
    if (const Property* r = row(p, u"list")) {
        EXPECT_EQ(r->valueLine, 1U);
        EXPECT_EQ(r->valueColumn, 2U);
    }

    const Properties tags = parse(u"tags:\n  - s022\n  - review\n");
    ASSERT_EQ(tags.rows.size(), 1U);
    EXPECT_EQ(tags.rows[0].type, PropertyType::Tags);
    EXPECT_EQ(tags.rows[0].items, (std::vector<std::u16string>{u"s022", u"review"}));

    const Properties nested = parse(u"nested:\n- [1, 2]\nmapping:\n  name: Armin\n  mail: a@b\nmixed: [a, {b: 1}]\n");
    ASSERT_FALSE(nested.error.has_value());
    const auto unsupported = [&](std::u16string_view key, std::u16string_view json) {
        if (const Property* r = row(nested, key)) {
            EXPECT_EQ(r->type, PropertyType::Unsupported) << narrow(key);
            EXPECT_EQ(r->text, json) << narrow(key);
        }
    };
    unsupported(u"nested", u"[[1,2]]");
    unsupported(u"mapping", uR"({"name":"Armin","mail":"a@b"})");
    unsupported(u"mixed", uR"(["a",{"b":1}])");
}

// ---- Scalars --------------------------------------------------------------------------------

TEST(S024_FrontMatter, QuotedFoldedAndBlockScalars) {
    const Properties p = parse(
        u"sq: 'it''s'\n"
        u"dq: \"a\\tb\\\"c\\u00e4\"\n"
        u"plain: first\n"
        u"  second\n"
        u"\n"
        u"  third\n"
        u"lit: |\n"
        u"  line 1\n"
        u"  line 2\n"
        u"strip: |-\n"
        u"  x\n"
        u"fold: >\n"
        u"  a\n"
        u"  b\n"
        u"keep: |+\n"
        u"  x\n"
        u"\n"
        u"comment: plain # a comment\n"
        u"hash: a#b\n"
        u"qhash: 'x # y'\n"
        u"\"my key\": 1\n"
        u"'k2': 2\n");
    ASSERT_FALSE(p.error.has_value());
    const auto expect = [&](std::u16string_view key, std::u16string_view text) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->type, PropertyType::Text) << narrow(key);
            EXPECT_EQ(r->text, text) << narrow(key);
        }
    };
    expect(u"sq", u"it's");
    expect(u"dq", u"a\tb\"c\u00e4");
    expect(u"plain", u"first second\nthird");
    expect(u"lit", u"line 1\nline 2\n");
    expect(u"strip", u"x");
    expect(u"fold", u"a b\n");
    expect(u"keep", u"x\n\n");
    expect(u"comment", u"plain");
    expect(u"hash", u"a#b");
    expect(u"qhash", u"x # y");
    if (const Property* r = row(p, u"my key")) {
        EXPECT_EQ(r->type, PropertyType::Number);
    }
    EXPECT_NE(row(p, u"k2"), nullptr);
}

// Code review O7: a click on a block scalar puts the caret at its first content line, at the
// content's indentation; on a null directly after the colon, also before a comment.
TEST(S024_FrontMatter, ClickPositionsOfBlockScalarsAndNulls) {
    const Properties p = parse(u"lit: |\n\n    first\n    second\nfold: >-\n  x\nempty: |\nnote:   # a note\nlast:\n");
    ASSERT_FALSE(p.error.has_value());
    const auto expectAt = [&](std::u16string_view key, std::size_t line, std::size_t column) {
        if (const Property* r = row(p, key)) {
            EXPECT_EQ(r->valueLine, line) << narrow(key);
            EXPECT_EQ(r->valueColumn, column) << narrow(key);
        }
    };
    expectAt(u"lit", 2, 4);
    expectAt(u"fold", 5, 2);
    expectAt(u"empty", 6, 7);  // no content: after the header
    expectAt(u"note", 7, 5);
    expectAt(u"last", 8, 5);
}

TEST(S024_FrontMatter, CommentsAndBlankLinesShowNothing) {
    for (const std::u16string_view yaml : {u"", u"\n\n", u"# only a comment\n\n  # indented\n"}) {
        const Properties p = parse(yaml);
        EXPECT_FALSE(p.error.has_value()) << narrow(yaml);
        EXPECT_TRUE(p.rows.empty()) << narrow(yaml);
    }
    const Properties p = parse(u"a: 1\n\n# comment\nb: 2\n");
    ASSERT_FALSE(p.error.has_value());
    EXPECT_EQ(keys(p), (std::vector<std::u16string>{u"a", u"b"}));
    if (const Property* r = row(p, u"b")) {
        EXPECT_EQ(r->keyLine, 3U);
    }
}

// ---- JSON of unsupported values (JSON.stringify) --------------------------------------------

TEST(S024_FrontMatter, JsonAsJsonStringifyWritesIt) {
    const Properties p = parse(
        u"m:\n"
        u"  s: \"quote \\\" and \\\\ and \\u00e4 and tab\\t\"\n"
        u"  n: 007\n"
        u"  e: 1e3\n"
        u"  h: 0x1A\n"
        u"  big: 1e20\n"
        u"  huge: 1e21\n"
        u"  small: 1e-6\n"
        u"  tiny: 1e-7\n"
        u"  inf: .inf\n"
        u"  nan: .nan\n"
        u"  t: true\n"
        u"  z: null\n"
        u"  d: 2026-10-30\n"
        u"  l: [a, {b: 1}]\n");
    ASSERT_EQ(p.rows.size(), 1U);
    EXPECT_EQ(p.rows[0].type, PropertyType::Unsupported);
    // Node 22: JSON.stringify of js-yaml 4.3.2's value, except "d": js-yaml makes a Date of it
    // ("2026-10-30T00:00:00.000Z"); the story keeps dates inside JSON as written.
    EXPECT_EQ(
        p.rows[0].text,
        uR"({"s":"quote \" and \\ and )"
        u"\u00e4"
        uR"( and tab\t","n":7,"e":1000,"h":26,"big":100000000000000000000,"huge":1e+21,"small":0.000001,"tiny":1e-7,"inf":null,"nan":null,"t":true,"z":null,"d":"2026-10-30","l":["a",{"b":1}]})");
    const Properties control = parse(u"m: {c: \"\\x01\\b\\n\"}\n");
    ASSERT_EQ(control.rows.size(), 1U);
    EXPECT_EQ(control.rows[0].text, uR"({"c":"\u0001\b\n"})");
}

// Code review O5: the tooltip of an unsupported value, as JSON.stringify(value, null, 2) writes it
// (Node 22), at most a number of lines.
TEST(S024_FrontMatter, PrettyJsonForTheTooltip) {
    const std::u16string json = uR"({"s":"a,b:{\"[","l":[1,[],{}],"o":{"x":null}})";
    const std::u16string pretty =
        u"{\n"
        uR"(  "s": "a,b:{\"[",)"
        u"\n"
        uR"(  "l": [)"
        u"\n    1,\n    [],\n    {}\n  ],\n"
        uR"(  "o": {)"
        u"\n"
        uR"(    "x": null)"
        u"\n  }\n}";
    EXPECT_EQ(prettyJson(json, 12), pretty);
    EXPECT_EQ(prettyJson(json, 100), pretty);
    EXPECT_EQ(prettyJson(json, 5),
              u"{\n"
              uR"(  "s": "a,b:{\"[",)"
              u"\n"
              uR"(  "l": [)"
              u"\n    1,\n    [],\n…");
    EXPECT_EQ(prettyJson(u"[]", 20), u"[]");
    EXPECT_EQ(prettyJson(u"\"x\"", 20), u"\"x\"");
    // A long value: only the prefix of 20 lines is read and written.
    std::u16string big = u"[";
    for (int i = 0; i < 100000; ++i) {
        big += (i == 0 ? u"" : u",") + std::u16string(u"{\"k\":1}");
    }
    big += u"]";
    const std::u16string capped = prettyJson(big, 20);
    EXPECT_EQ(std::count(capped.begin(), capped.end(), u'\n'), 20);
    EXPECT_TRUE(capped.ends_with(u"\n…"));
}

// ---- Invalid and unsupported YAML (S024, "Invalid") ----------------------------------------

// Code review O6, Armin: the badge names the mistakes writers make most often; "bad indentation"
// is only indentation.
TEST(S024_FrontMatter, InvalidYamlNamesTheMistake) {
    using R = PropertiesErrorReason;
    expectError(u"title: Note: draft\n", R::ColonInPlainValue, 0);
    expectError(u"a: b:\n", R::ColonInPlainValue, 0);
    expectError(u"a:\n  - x: y: z\n", R::ColonInPlainValue, 1);
    expectError(u"a: : x\n", R::ColonInPlainValue, 0);
    expectError(u"a: \"C:\\path\"\n", R::UnknownEscape, 0);
    expectError(u"a: [\"\\q\"]\n", R::UnknownEscape, 0);
    expectError(u"a: \"x\" y\n", R::TextAfterValue, 0);
    expectError(u"a: 'x' y\n", R::TextAfterValue, 0);
    expectError(u"a: [x] y\n", R::TextAfterValue, 0);
    expectError(u"a: [\"x\" y]\n", R::TextAfterValue, 0);
    expectError(u"a: | text\n  b\n", R::TextAfterValue, 0);
    expectError(u"{a: 1}\nb: 2\n", R::TextAfterValue, 1);
    expectError(u"author: @armin\n", R::ReservedCharacter, 0);
    expectError(u"a: `x`\n", R::ReservedCharacter, 0);
    expectError(u"a: %x\n", R::ReservedCharacter, 0);
    expectError(u"a: ]\n", R::ReservedCharacter, 0);
    expectError(u"a: [@x]\n", R::ReservedCharacter, 0);
    // Indentation stays "bad indentation", also a block collection on its key's line.
    expectError(u"a: - x\n", R::BadIndentation, 0);
    expectError(u"a:\n  b: 1\n   c: 2\n", R::BadIndentation, 2);
    expectError(u"a: 1\n- x\n", R::BadIndentation, 1);
    for (const R reason : {R::ColonInPlainValue, R::UnknownEscape, R::TextAfterValue, R::ReservedCharacter}) {
        EXPECT_FALSE(isUnsupported(reason));
    }
    EXPECT_EQ(reasonText(R::ColonInPlainValue), "colon in a plain value (quote it)");
    EXPECT_EQ(reasonText(R::UnknownEscape), "unknown escape");
    EXPECT_EQ(reasonText(R::TextAfterValue), "text after a value");
    EXPECT_EQ(reasonText(R::ReservedCharacter), "reserved character (quote it)");
}

TEST(S024_FrontMatter, InvalidYamlNamesLineAndReason) {
    using R = PropertiesErrorReason;
    expectError(u"a: 1\na: 2\n", R::DuplicateKey, 1);
    expectError(u"a: 1\n\"a\": 2\n", R::DuplicateKey, 1);
    expectError(u"m:\n  x: 1\n  x: 2\n", R::DuplicateKey, 2);
    expectError(u"a:\n  - x\n - y\n", R::BadIndentation, 2);
    expectError(u"a: 1\n  b: 2\n", R::BadIndentation, 1);
    expectError(u"a:\n\t- x\n", R::TabInIndentation, 1);
    expectError(u"a: \"abc\n", R::UnclosedQuote, 0);
    expectError(u"a: 'abc\nb: 1\n", R::UnclosedQuote, 0);
    expectError(u"a: [1, 2\n", R::UnclosedFlowCollection, 0);
    expectError(u"- a\n- b\n", R::NotAMapping, 0);
    expectError(u"just text\n", R::NotAMapping, 0);
    expectError(u"a: 1\n...\n", R::DocumentEnd, 1);
    expectError(u"a: " + std::u16string(100, u'[') + std::u16string(100, u']') + u"\n", R::TooDeep, 0);
    EXPECT_FALSE(parse(u"a: " + std::u16string(40, u'[') + std::u16string(40, u']') + u"\n").error.has_value())
        << "40 levels are fine";
    for (const R reason : {R::DuplicateKey, R::BadIndentation, R::TabInIndentation, R::UnclosedQuote,
                           R::UnclosedFlowCollection, R::NotAMapping, R::DocumentEnd, R::TooDeep}) {
        EXPECT_FALSE(isUnsupported(reason));
    }
    EXPECT_EQ(reasonText(R::DuplicateKey), "duplicate key");
    EXPECT_EQ(reasonText(R::BadIndentation), "bad indentation");
    EXPECT_EQ(reasonText(R::TabInIndentation), "tab in indentation");
    EXPECT_EQ(reasonText(R::UnclosedQuote), "unclosed quote");
    EXPECT_EQ(reasonText(R::UnclosedFlowCollection), "unclosed flow collection");
    EXPECT_EQ(reasonText(R::NotAMapping), "not a mapping");
    EXPECT_EQ(reasonText(R::DocumentEnd), "document end");
    EXPECT_EQ(reasonText(R::TooDeep), "too deep");
}

// Valid YAML (js-yaml and ruamel.yaml accept it) outside the subset of ADR 0007.
TEST(S024_FrontMatter, YamlOutsideTheSubsetIsUnsupported) {
    using R = PropertiesErrorReason;
    expectError(u"a: &x 1\nb: *x\n", R::AnchorOrAlias, 0);
    expectError(u"a: 1\nb: *x\n", R::AnchorOrAlias, 1);
    expectError(u"a: !!str 1\n", R::Tag, 0);
    expectError(u"? a\n: b\n", R::ComplexKey, 0);
    expectError(u"%YAML 1.2\n", R::Directive, 0);
    expectError(u"a: |2\n   x\n", R::IndentationIndicator, 0);
    for (const R reason : {R::AnchorOrAlias, R::Tag, R::ComplexKey, R::Directive, R::IndentationIndicator}) {
        EXPECT_TRUE(isUnsupported(reason));
    }
    EXPECT_EQ(reasonText(R::AnchorOrAlias), "anchor or alias");
    EXPECT_EQ(reasonText(R::Tag), "tag");
    EXPECT_EQ(reasonText(R::ComplexKey), "complex key");
    EXPECT_EQ(reasonText(R::Directive), "directive");
    EXPECT_EQ(reasonText(R::IndentationIndicator), "indentation indicator");
    // Inside quotes these characters are text.
    const Properties quoted = parse(u"a: '&x *y !!z'\n");
    ASSERT_FALSE(quoted.error.has_value());
    ASSERT_EQ(quoted.rows.size(), 1U);
    EXPECT_EQ(quoted.rows[0].text, u"&x *y !!z");
}

// Cases from the differential run against js-yaml 4.3.2 (2026-10-31, ADR 0007), checked with
// ruamel.yaml 0.19.1 too.
TEST(S024_FrontMatter, CasesFromTheDifferentialRun) {
    using R = PropertiesErrorReason;
    // Both references give 0o and 0x numbers a sign, although the core schema has none.
    const Properties signs = parse(u"a: -0x1A\nb: +0o17\nm: [-0x1A, +0o17]\n");
    ASSERT_FALSE(signs.error.has_value());
    for (const std::u16string_view key : {u"a", u"b"}) {
        if (const Property* r = row(signs, key)) {
            EXPECT_EQ(r->type, PropertyType::Number) << narrow(key);
        }
    }
    if (const Property* r = row(signs, u"m")) {
        EXPECT_EQ(r->type, PropertyType::List);
    }
    const Properties signedJson = parse(u"m: [[-0x1A, +0o17]]\n");
    ASSERT_EQ(signedJson.rows.size(), 1U);
    EXPECT_EQ(signedJson.rows[0].text, u"[[-26,15]]");
    // A `#` right after a closing quote or bracket starts a comment (both references).
    const Properties hash = parse(u"a: \"x\"#c\nb: [1]#c\n");
    ASSERT_FALSE(hash.error.has_value());
    if (const Property* r = row(hash, u"a")) {
        EXPECT_EQ(r->text, u"x");
    }
    if (const Property* r = row(hash, u"b")) {
        EXPECT_EQ(r->items, std::vector<std::u16string>{u"1"});
    }
    // An unknown escape is invalid (js-yaml: "unknown escape sequence"); code review O6 names it.
    expectError(u"a: \"\\q\"\n", R::UnknownEscape, 0);
    // A tab before a compact mapping in a sequence entry is indentation (both reject it).
    expectError(u"a:\n-\tk: v\n", R::TabInIndentation, 1);
    // An implicit key in a flow mapping and its `:` stay on one line (both reject it).
    expectError(u"a: {k\n  : 1}\n", R::TextAfterValue, 1);
    // Keys keep their order in JSON, as in ruamel.yaml; JavaScript puts integer-like keys first.
    const Properties order = parse(u"m: {2: a, 1: b, x: c}\n");
    ASSERT_EQ(order.rows.size(), 1U);
    EXPECT_EQ(order.rows[0].text, uR"({"2":"a","1":"b","x":"c"})");
}

// ADR 0007: linear in the length of the YAML (best of three, ten times the input).
TEST(S024_FrontMatter, ReadingIsLinear) {
    const auto seconds = [](const std::vector<std::u16string>& lines) {
        double best = 1e9;
        for (int run = 0; run < 3; ++run) {
            const auto start = std::chrono::steady_clock::now();
            const Properties p = parseProperties(lines);
            const std::chrono::duration<double> took = std::chrono::steady_clock::now() - start;
            EXPECT_FALSE(p.error.has_value());
            best = std::min(best, took.count());
        }
        return best;
    };
    const auto keysDocument = [](int n) {
        std::vector<std::u16string> lines;
        for (int i = 0; i < n; ++i) {
            std::u16string line = u"k";
            for (const char c : std::to_string(i)) {
                line += static_cast<char16_t>(c);
            }
            lines.push_back(line + u": value with some words and [x, y]");
        }
        return lines;
    };
    const auto flowDocument = [](int n) {
        std::u16string line = u"a: [";
        for (int i = 0; i < n; ++i) {
            line += u"x, ";
        }
        return std::vector<std::u16string>{line + u"x]"};
    };
    const double keysSmall = seconds(keysDocument(5'000));
    const double keysLarge = seconds(keysDocument(50'000));
    EXPECT_LT(keysLarge, 30 * std::max(keysSmall, 1e-4)) << keysSmall << " s, " << keysLarge << " s";
    const double flowSmall = seconds(flowDocument(20'000));
    const double flowLarge = seconds(flowDocument(200'000));
    EXPECT_LT(flowLarge, 30 * std::max(flowSmall, 1e-4)) << flowSmall << " s, " << flowLarge << " s";
    EXPECT_FALSE(keysDocument(1).empty());
    EXPECT_EQ(parseProperties(keysDocument(3)).rows.size(), 3U);
}

// ---- Front matter in the whole-document reference (ADR 0006) --------------------------------

TEST(S024_FrontMatter, LinesOfClosedFrontMatterAreMarked) {
    const std::vector<std::u16string> lines = {u"---", u"title: x", u"# not a heading", u"---", u"# Heading", u"text"};
    for (const LineRange revealed : {LineRange{}, LineRange{1, 1}, LineRange{0, 3}}) {
        const auto r = registry().renderDocument(lines, revealed);
        for (std::size_t i = 0; i < 4; ++i) {
            EXPECT_TRUE(r[i].frontMatter) << i;
            EXPECT_EQ(r[i].headingLevel, 0) << i;
            EXPECT_FALSE(r[i].thematicBreak) << i;
            EXPECT_TRUE(r[i].runs.empty()) << i;
        }
        EXPECT_FALSE(r[4].frontMatter);
        EXPECT_EQ(r[4].headingLevel, 1);
        EXPECT_FALSE(r[5].frontMatter);
    }
    const std::vector<std::u16string> unclosed = {u"---", u"title: x"};
    for (const LineRender& r : registry().renderDocument(unclosed)) {
        EXPECT_FALSE(r.frontMatter);
    }
    const std::vector<std::u16string> later = {u"text", u"", u"---", u"a: 1", u"---"};
    for (const LineRender& r : registry().renderDocument(later)) {
        EXPECT_FALSE(r.frontMatter);
    }
    const std::vector<std::u16string> blankFirst = {u"", u"---", u"a: 1", u"---"};
    for (const LineRender& r : registry().renderDocument(blankFirst)) {
        EXPECT_FALSE(r.frontMatter);
    }
}

TEST(S024_FrontMatter, ByteOrderMarkBeforeTheDelimiter) {
    const std::string bytes = "\xEF\xBB\xBF---\r\ntitle: x\r\n---\r\nbody\r\n";
    const auto decoded = decodeUtf8(bytes);
    if (!decoded.value) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "not decoded";
    }
    const DecodedText& text = *decoded.value;
    const auto lines = linesOf(text.text);
    ASSERT_GE(lines.size(), 4U);
    const auto r = registry().renderDocument(lines);
    EXPECT_TRUE(r[0].frontMatter);
    EXPECT_TRUE(r[2].frontMatter);
    EXPECT_FALSE(r[3].frontMatter);
    const Properties p = parseProperties(std::vector<std::u16string>(lines.begin() + 1, lines.begin() + 2));
    EXPECT_EQ(keys(p), (std::vector<std::u16string>{u"title"}));
    const auto encoded = encodeUtf8(text.text, text.hasBom, text.uniformEnding, text.perBreak);
    if (!encoded) {  // explicit, so that bugprone-unchecked-optional-access sees the check
        FAIL() << "not encoded";
    }
    EXPECT_EQ(*encoded, bytes);
}

// Review O9, known consequence (as in Obsidian): a document that starts with a break and later has
// text underlined with `---` is front matter up to that line; if the text is a valid mapping it
// shows as properties, its `# ` lines dropped as comments (js-yaml 4.3.2 agrees on the mapping).
TEST(S024_FrontMatter, YamlLookingBodyTextBecomesProperties) {
    const std::vector<std::u16string> lines = {
        u"---",           u"Status: draft", u"",    u"# Chapter one", u"", u"Note: the plan is simple", u"",
        u"# Chapter two", u"Summary: none", u"---", u"body"};
    const auto r = registry().renderDocument(lines);
    for (std::size_t i = 0; i <= 9; ++i) {
        EXPECT_TRUE(r[i].frontMatter) << i;
    }
    const Properties p = parseProperties(std::vector<std::u16string>(lines.begin() + 1, lines.begin() + 9));
    ASSERT_FALSE(p.error.has_value());
    EXPECT_EQ(keys(p), (std::vector<std::u16string>{u"Status", u"Note", u"Summary"}));
}

// ---- Theming baseline (ADR 0004) ------------------------------------------------------------

TEST(S024_FrontMatter, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[properties]\n"
        "background = \"#010203\"\ntext = \"#040506\"\nkey-text = \"#070809\"\ntitle-text = \"#0a0b0c\"\n"
        "icon = \"#0d0e0f\"\nunsupported-text = \"#101112\"\nbadge-text = \"#131415\"\n"
        "badge-font-family = \"Badge Sans\"\nbadge-font-scale = 0.75\ncheckbox = \"#161718\"\n"
        "checkbox-background = \"#191a1b\"\nchip-background = \"#1c1d1e\"\nchip-text = \"#1f2021\"\n"
        "chip-border = \"#222324\"\nchip-border-width = 2\nchip-radius = 5\nchip-padding = 6\n"
        "border = \"#252627\"\nborder-width = 1\nradius = 7\npadding = 9\nmargin = 11\nrow-spacing = 4\n"
        "key-width = 200\nfont-family = \"Props Sans\"\nfont-scale = 0.9\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << (r.diagnostics.empty() ? std::string() : r.diagnostics.front().toString());
    const PropertiesStyle s = propertiesStyle(r.theme);
    EXPECT_EQ(s.background, (Color{1, 2, 3, 255}));
    EXPECT_EQ(s.text, (Color{4, 5, 6, 255}));
    EXPECT_EQ(s.keyText, (Color{7, 8, 9, 255}));
    EXPECT_EQ(s.titleText, (Color{10, 11, 12, 255}));
    EXPECT_EQ(s.icon, (Color{13, 14, 15, 255}));
    EXPECT_EQ(s.unsupportedText, (Color{16, 17, 18, 255}));
    EXPECT_EQ(s.badgeText, (Color{19, 20, 21, 255}));
    EXPECT_EQ(s.badgeFontFamily, "Badge Sans");
    EXPECT_DOUBLE_EQ(s.badgeFontScale, 0.75);
    EXPECT_EQ(s.checkbox, (Color{22, 23, 24, 255}));
    EXPECT_EQ(s.checkboxBackground, (Color{25, 26, 27, 255}));
    EXPECT_EQ(s.chipBackground, (Color{28, 29, 30, 255}));
    EXPECT_EQ(s.chipText, (Color{31, 32, 33, 255}));
    EXPECT_EQ(s.chipBorder, (Color{34, 35, 36, 255}));
    EXPECT_DOUBLE_EQ(s.chipBorderWidth, 2);
    EXPECT_DOUBLE_EQ(s.chipRadius, 5);
    EXPECT_DOUBLE_EQ(s.chipPadding, 6);
    EXPECT_EQ(s.border, (Color{37, 38, 39, 255}));
    EXPECT_DOUBLE_EQ(s.borderWidth, 1);
    EXPECT_DOUBLE_EQ(s.radius, 7);
    EXPECT_DOUBLE_EQ(s.padding, 9);
    EXPECT_DOUBLE_EQ(s.margin, 11);
    EXPECT_DOUBLE_EQ(s.rowSpacing, 4);
    EXPECT_DOUBLE_EQ(s.keyWidth, 200);
    EXPECT_EQ(s.fontFamily, "Props Sans");
    EXPECT_DOUBLE_EQ(s.fontScale, 0.9);

    for (const char* bad :
         {"border-width = 9", "chip-border-width = -1", "radius = 33", "chip-radius = 33", "padding = 65",
          "chip-padding = 33", "margin = 65", "row-spacing = 33", "key-width = 39", "key-width = 1001",
          "font-scale = 0.4", "badge-font-scale = 2.1", "add-text = \"#000000\""}) {
        EXPECT_TRUE(loadTheme(std::string("[properties]\n") + bad + "\n", ColorScheme::Light).usedFallback) << bad;
    }
}

TEST(S024_FrontMatter, ThemeTokensFallBack) {
    for (const auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& t = builtinTheme(scheme);
        const PropertiesStyle s = propertiesStyle(t);
        EXPECT_EQ(s.background, t.background);
        EXPECT_EQ(s.text, t.text);
        EXPECT_EQ(s.keyText, t.textMuted);
        EXPECT_EQ(s.titleText, t.textMuted);
        EXPECT_EQ(s.icon, t.textMuted);
        EXPECT_EQ(s.unsupportedText, t.accent);
        EXPECT_EQ(s.badgeText, t.accent);
        EXPECT_EQ(s.badgeFontFamily, "");
        EXPECT_DOUBLE_EQ(s.badgeFontScale, 1);
        EXPECT_EQ(s.checkbox, t.accent);
        EXPECT_EQ(s.checkboxBackground, t.background);
        EXPECT_EQ(s.chipBackground, t.surface);
        EXPECT_EQ(s.chipText, t.text);
        EXPECT_EQ(s.chipBorder, t.border);
        EXPECT_DOUBLE_EQ(s.chipBorderWidth, 0);
        EXPECT_DOUBLE_EQ(s.chipRadius, t.radius);
        EXPECT_DOUBLE_EQ(s.chipPadding, 4);
        EXPECT_EQ(s.border, t.border);
        EXPECT_DOUBLE_EQ(s.borderWidth, 0);
        EXPECT_DOUBLE_EQ(s.radius, t.radius);
        EXPECT_DOUBLE_EQ(s.padding, t.spacing);
        EXPECT_DOUBLE_EQ(s.margin, 3);
        EXPECT_DOUBLE_EQ(s.rowSpacing, 2);
        EXPECT_DOUBLE_EQ(s.keyWidth, 160);
        EXPECT_EQ(s.fontFamily, "");
        EXPECT_DOUBLE_EQ(s.fontScale, 1);
    }
    // Fallbacks resolve when used, and element tokens fall back on each other.
    const auto r =
        loadTheme("[colors]\naccent = \"#123456\"\n[properties]\ntext = \"#abcdef\"\nbackground = \"#fedcba\"\n",
                  ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback);
    const PropertiesStyle s = propertiesStyle(r.theme);
    EXPECT_EQ(s.unsupportedText, (Color{0x12, 0x34, 0x56, 255}));
    EXPECT_EQ(s.badgeText, (Color{0x12, 0x34, 0x56, 255}));
    EXPECT_EQ(s.chipText, (Color{0xab, 0xcd, 0xef, 255}));
    EXPECT_EQ(s.checkboxBackground, (Color{0xfe, 0xdc, 0xba, 255}));
}

TEST(S024_FrontMatter, TokensAreDocumented) {
    std::ifstream in(std::string(HITOKOTO_SOURCE_DIR) + "/docs/theming.md", std::ios::binary);
    ASSERT_TRUE(in) << "docs/theming.md not found";
    std::stringstream doc;
    doc << in.rdbuf();
    for (const std::string_view key : {"background",
                                       "text",
                                       "key-text",
                                       "title-text",
                                       "icon",
                                       "unsupported-text",
                                       "badge-text",
                                       "badge-font-family",
                                       "badge-font-scale",
                                       "checkbox",
                                       "checkbox-background",
                                       "chip-background",
                                       "chip-text",
                                       "chip-border",
                                       "chip-border-width",
                                       "chip-radius",
                                       "chip-padding",
                                       "border",
                                       "border-width",
                                       "radius",
                                       "padding",
                                       "margin",
                                       "row-spacing",
                                       "key-width",
                                       "font-family",
                                       "font-scale"}) {
        bool found = false;
        for (const auto& info : themeTokens()) {
            found = found || (info.section == "properties" && info.key == key);
        }
        EXPECT_TRUE(found) << key;
        EXPECT_NE(doc.str().find("`properties." + std::string(key) + "`"), std::string::npos) << key;
    }
}
