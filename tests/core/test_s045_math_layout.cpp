// SPDX-License-Identifier: GPL-3.0-or-later
// S045: the math layout engine. Geometry is checked with a font of fixed metrics, so every
// distance follows from the MATH constants; the bundled STIX Two Math checks the table reader and
// that every formula of the sample lays out.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/math.hpp"

#include "support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {

constexpr double kPx = 20;  // pixels per em in these tests; the fake font has 1000 units per em

/// Every character has a glyph (except the private-use U+E000), 500 units wide, from -100 to 650.
class FakeFont final : public MathFont {
public:
    [[nodiscard]] double unitsPerEm() const override { return 1000; }
    [[nodiscard]] std::uint16_t glyph(char32_t c) const override {
        if (c == U'') {
            return 0;
        }
        const auto it = ids_.find(c);
        if (it != ids_.end()) {
            return it->second;
        }
        const auto id = static_cast<std::uint16_t>(ids_.size() + 1);
        ids_[c] = id;
        return id;
    }
    [[nodiscard]] double advance(std::uint16_t g) const override {
        return extra_.contains(g) ? extra_.at(g).first : 500;
    }
    [[nodiscard]] GlyphBounds bounds(std::uint16_t g) const override {
        return extra_.contains(g) ? extra_.at(g).second : GlyphBounds{0, -100, 450, 650};
    }
    /// A glyph of its own size (variants and parts).
    std::uint16_t add(double advance, GlyphBounds bounds) {
        const auto id = static_cast<std::uint16_t>(30000 + extra_.size());
        extra_[id] = {advance, bounds};
        return id;
    }

private:
    mutable std::map<char32_t, std::uint16_t> ids_;
    std::map<std::uint16_t, std::pair<double, GlyphBounds>> extra_;
};

MathTable fakeTable() {
    MathTable t;
    MathConstants& c = t.constants;
    c.scriptPercentScaleDown = 70;
    c.scriptScriptPercentScaleDown = 50;
    c.displayOperatorMinHeight = 1300;
    c.axisHeight = 250;
    c.subscriptShiftDown = 150;
    c.subscriptTopMax = 400;
    c.subscriptBaselineDropMin = 50;
    c.superscriptShiftUp = 360;
    c.superscriptShiftUpCramped = 290;
    c.superscriptBottomMin = 120;
    c.superscriptBaselineDropMax = 250;
    c.subSuperscriptGapMin = 160;
    c.superscriptBottomMaxWithSubscript = 400;
    c.spaceAfterScript = 40;
    c.upperLimitGapMin = 150;
    c.upperLimitBaselineRiseMin = 300;
    c.lowerLimitGapMin = 150;
    c.lowerLimitBaselineDropMin = 600;
    c.stackTopShiftUp = 470;
    c.stackTopDisplayStyleShiftUp = 660;
    c.stackBottomShiftDown = 385;
    c.stackBottomDisplayStyleShiftDown = 690;
    c.stackGapMin = 150;
    c.stackDisplayStyleGapMin = 300;
    c.stretchStackTopShiftUp = 800;
    c.stretchStackBottomShiftDown = 600;
    c.stretchStackGapAboveMin = 150;
    c.stretchStackGapBelowMin = 150;
    c.fractionNumeratorShiftUp = 390;
    c.fractionNumeratorDisplayStyleShiftUp = 680;
    c.fractionDenominatorShiftDown = 340;
    c.fractionDenominatorDisplayStyleShiftDown = 690;
    c.fractionNumeratorGapMin = 40;
    c.fractionNumDisplayStyleGapMin = 120;
    c.fractionRuleThickness = 60;
    c.fractionDenominatorGapMin = 40;
    c.fractionDenomDisplayStyleGapMin = 120;
    c.radicalVerticalGap = 70;
    c.radicalDisplayStyleVerticalGap = 160;
    c.radicalRuleThickness = 60;
    c.radicalExtraAscender = 60;
    c.radicalKernBeforeDegree = 280;
    c.radicalKernAfterDegree = -550;
    c.radicalDegreeBottomRaisePercent = 60;
    t.minConnectorOverlap = 20;
    return t;
}

double units(double designUnits, double scale = 1) {
    return designUnits * kPx / 1000 * scale;
}

std::vector<std::uint8_t> readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

const std::vector<std::uint8_t>& stixBytes() {
    static const std::vector<std::uint8_t> bytes =
        readBytes(std::string(HITOKOTO_SOURCE_DIR) + "/fonts/stix-two-math/STIXTwoMath-Regular.otf");
    return bytes;
}

std::uint16_t be16(std::span<const std::uint8_t> b, std::size_t at) {
    return at + 2 <= b.size() ? static_cast<std::uint16_t>((static_cast<unsigned>(b[at]) << 8U) | b[at + 1]) : 0;
}
std::uint32_t be32(std::span<const std::uint8_t> b, std::size_t at) {
    return (static_cast<std::uint32_t>(be16(b, at)) << 16U) | be16(b, at + 2);
}

/// The real font's characters (cmap format 12) and advances (hmtx); ink bounds are not read from
/// the CFF outlines, so they are taken as the advance from -200 to 700 units: enough for the layout
/// to run on the real MATH table, not for exact geometry (the UI test does that with Qt).
class FileFont final : public MathFont {
public:
    explicit FileFont(std::span<const std::uint8_t> font) {
        const auto head = openTypeTable(font, "head");
        upem_ = be16(head, 18);
        const auto hhea = openTypeTable(font, "hhea");
        metrics_ = be16(hhea, 34);
        hmtx_ = openTypeTable(font, "hmtx");
        const auto cmap = openTypeTable(font, "cmap");
        const std::uint16_t count = be16(cmap, 2);
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t offset = be32(cmap, 4 + i * 8 + 4);
            if (be16(cmap, offset) != 12) {
                continue;
            }
            const std::uint32_t groups = be32(cmap, offset + 12);
            for (std::uint32_t g = 0; g < groups; ++g) {
                const std::size_t at = offset + 16 + g * 12;
                const std::uint32_t start = be32(cmap, at);
                const std::uint32_t end = be32(cmap, at + 4);
                const std::uint32_t glyph = be32(cmap, at + 8);
                for (std::uint32_t c = start; c <= end && c - start < 0x10000; ++c) {
                    chars_[c] = static_cast<std::uint16_t>(glyph + (c - start));
                }
            }
            break;
        }
    }
    [[nodiscard]] double unitsPerEm() const override { return upem_; }
    [[nodiscard]] std::uint16_t glyph(char32_t c) const override {
        const auto it = chars_.find(c);
        return it == chars_.end() ? 0 : it->second;
    }
    [[nodiscard]] double advance(std::uint16_t g) const override {
        const std::size_t index = std::min<std::size_t>(g, metrics_ > 0 ? metrics_ - 1U : 0);
        return be16(hmtx_, index * 4);
    }
    [[nodiscard]] GlyphBounds bounds(std::uint16_t g) const override { return {0, -200, advance(g), 700}; }
    [[nodiscard]] bool empty() const { return chars_.empty(); }

private:
    double upem_ = 1000;
    std::uint16_t metrics_ = 0;
    std::span<const std::uint8_t> hmtx_;
    std::map<char32_t, std::uint16_t> chars_;
};

/// The one fake font the tests share, so that glyph ids agree between a layout and a test.
const FakeFont& fakeFont() {
    static const FakeFont font;
    return font;
}

MathLayout lay(std::u16string_view source, bool display = false) {
    static const MathTable table = fakeTable();
    return layoutMath(source, fakeFont(), table, kPx, display);
}

std::string errorOf(const MathLayout& layout) {
    return layout.error ? layout.error->message : std::string();
}

std::uint32_t errorPosition(const MathLayout& layout) {
    return layout.error ? layout.error->position : 0xFFFFFFFFU;
}

/// The formulas of the sample: inline ones as they are, display blocks joined line by line.
std::vector<std::pair<std::u16string, bool>> sampleFormulas() {
    std::ifstream in(std::string(HITOKOTO_SOURCE_DIR) + "/tests/data/s044-latex.md");
    std::vector<std::u16string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.emplace_back(line.begin(), line.end());
    }
    const auto shown = hitokoto::test::registry().renderDocument(lines, {});
    std::vector<std::pair<std::u16string, bool>> out;
    std::u16string block;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        for (const MathSpan& m : shown[i].math) {
            const std::u16string source = lines[i].substr(m.source.start, m.source.length);
            if (shown[i].mathBlock) {
                block += source + u"\n";
            } else {
                out.emplace_back(source, m.display);
            }
        }
        if (shown[i].mathBlock && shown[i].mathDelimiter && !shown[i].blockState.active()) {
            out.emplace_back(block, true);
            block.clear();
        }
    }
    return out;
}

}  // namespace

TEST(S045_MathLayout, TheMathTableOfTheBundledFontIsRead) {
    const auto& bytes = stixBytes();
    ASSERT_FALSE(bytes.empty());
    const auto mathBytes = openTypeTable(bytes, "MATH");
    ASSERT_FALSE(mathBytes.empty());
    const auto table = MathTable::parse(mathBytes);
    ASSERT_TRUE(table.has_value());
    if (!table) {
        FAIL();
    }
    const MathConstants& c = table->constants;
    EXPECT_GT(c.scriptPercentScaleDown, 50);
    EXPECT_LT(c.scriptPercentScaleDown, 90);
    EXPECT_GT(c.axisHeight, 200);
    EXPECT_LT(c.axisHeight, 300);
    EXPECT_GT(c.fractionRuleThickness, 0);
    EXPECT_GT(c.displayOperatorMinHeight, 1000);
    EXPECT_FALSE(table->italicCorrections.empty());
    EXPECT_GT(table->minConnectorOverlap, 0);

    const FileFont font(bytes);
    ASSERT_FALSE(font.empty());
    const auto paren = table->vertical.find(font.glyph(U'('));
    ASSERT_NE(paren, table->vertical.end());
    EXPECT_GT(paren->second.variants.size(), 2U);
    ASSERT_FALSE(paren->second.assembly.empty());
    EXPECT_TRUE(std::any_of(paren->second.assembly.begin(), paren->second.assembly.end(),
                            [](const GlyphPart& p) { return p.extender; }));
    EXPECT_NE(table->vertical.find(font.glyph(U'√')), table->vertical.end()) << "the radical sign stretches";
    EXPECT_NE(table->horizontal.find(font.glyph(U'→')), table->horizontal.end()) << "the arrow stretches";
}

TEST(S045_MathLayout, MalformedTablesGiveNothingOrLess) {
    EXPECT_FALSE(MathTable::parse({}).has_value());
    const std::vector<std::uint8_t> garbage{0, 1, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const auto table = MathTable::parse(garbage);
    if (!table) {
        FAIL() << "a header with offsets outside the table is read as empty";
    }
    EXPECT_TRUE(table->vertical.empty());
    EXPECT_TRUE(openTypeTable(garbage, "MATH").empty());
    // Every prefix of the real table parses without reading outside it.
    const auto mathBytes = openTypeTable(stixBytes(), "MATH");
    for (std::size_t length = 0; length < mathBytes.size(); length += 997) {
        (void)MathTable::parse(mathBytes.first(length));
    }
}

TEST(S045_MathLayout, EveryFormulaOfTheSampleLaysOutWithTheBundledFont) {
    const auto& bytes = stixBytes();
    const FileFont font(bytes);
    const auto table = MathTable::parse(openTypeTable(bytes, "MATH"));
    ASSERT_TRUE(table.has_value());
    if (!table) {
        FAIL();
    }
    const auto formulas = sampleFormulas();
    EXPECT_EQ(formulas.size(), 9U) << "five inline formulas and four display blocks";
    for (const auto& [source, display] : formulas) {
        const MathLayout layout = layoutMath(source, font, *table, kPx, display);
        EXPECT_FALSE(layout.error.has_value()) << hitokoto::test::narrow(source) << ": " << errorOf(layout);
        EXPECT_GT(layout.width, 0) << hitokoto::test::narrow(source);
        EXPECT_FALSE(layout.glyphs.empty());
    }
}

TEST(S045_MathLayout, LettersAreItalicDigitsUpright) {
    const FakeFont& font = fakeFont();
    const MathLayout layout = lay(u"x2");
    ASSERT_EQ(layout.glyphs.size(), 2U);
    EXPECT_EQ(layout.glyphs[0].glyph, font.glyph(U'\U0001D465')) << "mathematical italic x";
    EXPECT_EQ(layout.glyphs[1].glyph, font.glyph(U'2'));
    EXPECT_EQ(lay(u"\\mathbf{E}").glyphs[0].glyph, font.glyph(U'\U0001D404')) << "bold E";
    EXPECT_EQ(lay(u"\\mathrm{d}").glyphs[0].glyph, font.glyph(U'd'));
    EXPECT_EQ(lay(u"\\alpha").glyphs[0].glyph, font.glyph(U'\U0001D6FC')) << "italic alpha";
    EXPECT_EQ(lay(u"\\Omega").glyphs[0].glyph, font.glyph(U'Ω')) << "upright capital omega";
    EXPECT_EQ(lay(u"\\varepsilon").glyphs[0].glyph, font.glyph(U'\U0001D700'));
    EXPECT_EQ(lay(u"\\epsilon").glyphs[0].glyph, font.glyph(U'\U0001D716'));
    EXPECT_EQ(lay(u"h").glyphs[0].glyph, font.glyph(U'ℎ')) << "the italic h is Planck's constant";
}

TEST(S045_MathLayout, AtomsAreSpacedAsTeXSpacesThem) {
    const double advance = units(500);
    const double mu = kPx / 18;
    EXPECT_DOUBLE_EQ(lay(u"ab").width, 2 * advance);
    EXPECT_DOUBLE_EQ(lay(u"a+b").width, 3 * advance + 2 * 4 * mu) << "medium spaces around a binary operator";
    EXPECT_DOUBLE_EQ(lay(u"a=b").width, 3 * advance + 2 * 5 * mu) << "thick spaces around a relation";
    EXPECT_DOUBLE_EQ(lay(u"-a").width, 2 * advance) << "a unary minus is ordinary";
    EXPECT_DOUBLE_EQ(lay(u"a,b").width, 3 * advance + 3 * mu) << "a thin space after punctuation";
    EXPECT_DOUBLE_EQ(lay(u"a\\quad b").width, 2 * advance + kPx);
    EXPECT_DOUBLE_EQ(lay(u"a\\,b").width, 2 * advance + 3 * mu);
    EXPECT_DOUBLE_EQ(lay(u"\\sin x").width, 4 * advance + 3 * mu) << "a thin space after an operator name";
}

TEST(S045_MathLayout, ScriptsFollowTheMathConstants) {
    const MathLayout sup = lay(u"x^2");
    ASSERT_EQ(sup.glyphs.size(), 2U);
    EXPECT_DOUBLE_EQ(sup.glyphs[1].size, kPx * 0.7);
    // A single character as base: the shift is SuperscriptShiftUp, or more for a deep script.
    const double shift = std::max(units(360), units(100, 0.7) + units(120));
    EXPECT_DOUBLE_EQ(-sup.glyphs[1].y, shift);
    EXPECT_DOUBLE_EQ(sup.glyphs[1].x, units(500));

    const MathLayout sub = lay(u"a_k");
    EXPECT_DOUBLE_EQ(sub.glyphs[1].y, std::max(units(150), units(650, 0.7) - units(400)));

    const MathLayout both = lay(u"a_k^2");
    const double supBottom = -both.glyphs[1].y - units(100, 0.7);
    const double subTop = -both.glyphs[2].y + units(650, 0.7);
    EXPECT_GE(supBottom - subTop, units(160) - 1e-9) << "SubSuperscriptGapMin between the scripts";
}

TEST(S045_MathLayout, LargeOperatorsTakeLimitsInDisplayStyle) {
    const MathLayout display = lay(u"\\sum_{k=1}^n", true);
    const MathLayout inlineSum = lay(u"\\sum_{k=1}^n", false);
    ASSERT_FALSE(display.error.has_value());
    // In display style the limits stand above and below the operator, centred on it.
    const MathGlyph& op = display.glyphs[0];
    const MathGlyph& upper = display.glyphs[1];
    EXPECT_LT(upper.y, op.y - units(650)) << "the upper limit stands above the operator";
    EXPECT_LT(display.width, inlineSum.width) << "limits beside the operator need more width";
    // In text style the scripts stand beside it.
    EXPECT_GE(inlineSum.glyphs[1].x, units(500));
}

TEST(S045_MathLayout, AFractionCentresItsRuleOnTheAxis) {
    const MathLayout f = lay(u"\\frac{a}{b}", true);
    ASSERT_FALSE(f.error.has_value());
    ASSERT_EQ(f.rules.size(), 1U);
    const MathRule& rule = f.rules[0];
    EXPECT_DOUBLE_EQ(rule.height, units(60));
    EXPECT_DOUBLE_EQ(rule.y + rule.height / 2, -units(250)) << "the rule's middle is on the math axis";
    const MathGlyph& num = f.glyphs[0];
    const MathGlyph& den = f.glyphs[1];
    EXPECT_LE(num.y + units(100, 1), rule.y + 1e-9) << "the numerator stays above the rule";
    EXPECT_GE(den.y - units(650), rule.y + rule.height - 1e-9) << "the denominator below it";
    EXPECT_DOUBLE_EQ(-num.y, std::max(units(680), units(250) + units(30) + units(120) + units(100)));
    // Text style: numerator and denominator in script size.
    EXPECT_DOUBLE_EQ(lay(u"\\frac{a}{b}").glyphs[0].size, kPx * 0.7);
    // \binom has no rule and is wrapped in parentheses.
    const MathLayout binom = lay(u"\\binom{n}{k}");
    EXPECT_TRUE(binom.rules.empty());
    EXPECT_EQ(binom.glyphs.size(), 4U);
}

TEST(S045_MathLayout, ARadicalCoversItsRadicand) {
    const MathLayout r = lay(u"\\sqrt{x}");
    ASSERT_FALSE(r.error.has_value());
    ASSERT_EQ(r.rules.size(), 1U);
    const MathRule& rule = r.rules[0];
    EXPECT_DOUBLE_EQ(rule.height, units(60));
    EXPECT_DOUBLE_EQ(rule.width, units(500)) << "the bar spans the radicand";
    EXPECT_LE(rule.y + rule.height, -units(650) - units(70) + 1e-9) << "RadicalVerticalGap above the radicand";
    const MathLayout degree = lay(u"\\sqrt[3]{x}");
    ASSERT_FALSE(degree.error.has_value());
    EXPECT_EQ(degree.glyphs.size(), 3U);
    EXPECT_DOUBLE_EQ(degree.glyphs[0].size, kPx * 0.5) << "the degree in scriptscript size";
}

TEST(S045_MathLayout, DelimitersGrowToTheirContent) {
    FakeFont font;
    MathTable table = fakeTable();
    const std::uint16_t paren = font.glyph(U'(');
    const std::uint16_t big = font.add(600, {0, -900, 550, 1500});
    const std::uint16_t bottom = font.add(600, {0, 0, 550, 800});
    const std::uint16_t extender = font.add(600, {0, 0, 550, 500});
    const std::uint16_t top = font.add(600, {0, 0, 550, 800});
    table.vertical[paren] =
        GlyphConstruction{{{paren, 750}, {big, 2400}},
                          {{bottom, 0, 200, 800, false}, {extender, 200, 200, 500, true}, {top, 200, 0, 800, false}}};
    const auto tallest = [&](std::u16string_view source) {
        const MathLayout l = layoutMath(source, font, table, kPx, true);
        EXPECT_FALSE(l.error.has_value()) << errorOf(l);
        return l;
    };
    EXPECT_EQ(tallest(u"\\left( x \\right.").glyphs[0].glyph, paren) << "small content keeps the glyph";
    EXPECT_EQ(tallest(u"\\left( \\frac{a}{b} \\right.").glyphs[0].glyph, big) << "a variant for a fraction";
    const MathLayout assembled =
        tallest(u"\\left( \\begin{matrix} a \\\\ b \\\\ c \\\\ d \\\\ e \\end{matrix} \\right.");
    ASSERT_GE(assembled.glyphs.size(), 3U);
    EXPECT_EQ(assembled.glyphs[0].glyph, bottom) << "an assembly, bottom part first";
    const double contentHeight = assembled.ascent + assembled.descent;
    double low = 1e9;
    double high = -1e9;
    for (const MathGlyph& g : assembled.glyphs) {
        if (g.glyph == bottom || g.glyph == extender || g.glyph == top) {
            low = std::min(low, g.y);
            high = std::max(high, g.y);
        }
    }
    EXPECT_GT(high - low, contentHeight * 0.5) << "the parts spread over the content";
    // The null delimiter takes only \nulldelimiterspace.
    EXPECT_DOUBLE_EQ(lay(u"\\left. x \\right.").width, units(500) + 2 * 0.12 * kPx);
}

TEST(S045_MathLayout, MatricesAndAlignedEquations) {
    const MathLayout m = lay(u"\\begin{matrix} a & b \\\\ c & d \\end{matrix}");
    ASSERT_FALSE(m.error.has_value()) << errorOf(m);
    ASSERT_EQ(m.glyphs.size(), 4U);
    EXPECT_DOUBLE_EQ(m.glyphs[1].x - m.glyphs[0].x, units(500) + kPx) << "a quad between columns";
    EXPECT_DOUBLE_EQ(m.glyphs[2].x, m.glyphs[0].x);
    EXPECT_GT(m.glyphs[2].y, m.glyphs[0].y) << "the second row below the first";
    EXPECT_NEAR((m.ascent - m.descent) / 2, units(250), 1e-9) << "centred on the axis";

    // The relations of an aligned block stand under each other.
    const MathLayout a = lay(u"\\begin{aligned} ab &= c \\\\ d &= efg \\end{aligned}", true);
    ASSERT_FALSE(a.error.has_value()) << errorOf(a);
    const FakeFont& font = fakeFont();
    std::vector<double> equals;
    for (const MathGlyph& g : a.glyphs) {
        if (g.glyph == font.glyph(U'=')) {
            equals.push_back(g.x);
        }
    }
    ASSERT_EQ(equals.size(), 2U);
    EXPECT_DOUBLE_EQ(equals[0], equals[1]);

    const MathLayout cases = lay(u"f = \\begin{cases} 1 & x > 0 \\\\ 0 & \\text{else} \\end{cases}");
    EXPECT_FALSE(cases.error.has_value()) << errorOf(cases);
    const MathLayout pmatrix = lay(u"\\begin{pmatrix} a \\\\ b \\end{pmatrix}");
    EXPECT_FALSE(pmatrix.error.has_value()) << errorOf(pmatrix);
    EXPECT_EQ(pmatrix.glyphs.size(), 4U) << "two cells and two parentheses";
}

TEST(S045_MathLayout, ARaggedMatrixStaysLinearInItsSource) {
    // S044 code review O2: short rows are padded to the widest one, so one wide row and many short
    // rows would make a grid quadratic in the source; such a grid is refused, ragged real ones not.
    const std::size_t n = 3000;
    std::u16string source = u"\\begin{matrix}";
    source += std::u16string(n, u'&');
    for (std::size_t i = 0; i < n; ++i) {
        source += u"\\\\";
    }
    source += u"\\end{matrix}";
    const MathLayout wide = lay(source);
    EXPECT_EQ(errorOf(wide), "matrix too large");
    EXPECT_TRUE(wide.glyphs.empty());
    const MathLayout cases = lay(u"\\begin{cases} 1 & x > 0 \\\\ 0 \\end{cases}");
    EXPECT_FALSE(cases.error.has_value()) << errorOf(cases);
    const MathLayout ragged = lay(u"\\begin{matrix} a & b & c \\\\ d \\end{matrix}");
    EXPECT_FALSE(ragged.error.has_value()) << errorOf(ragged);
    EXPECT_EQ(ragged.glyphs.size(), 4U);
}

TEST(S045_MathLayout, DfracAndTfracForceTheirStyle) {
    // S044 code review O3: \dfrac sets its parts in display style wherever it stands, \tfrac in
    // text style; \dbinom and \tbinom likewise.
    const double numeratorInline = lay(u"\\frac{a}{b}").glyphs[0].size;
    EXPECT_DOUBLE_EQ(numeratorInline, kPx * 0.7);
    EXPECT_DOUBLE_EQ(lay(u"\\dfrac{a}{b}").glyphs[0].size, kPx) << "display style: parts in text size";
    EXPECT_DOUBLE_EQ(lay(u"\\tfrac{a}{b}", true).glyphs[0].size, kPx * 0.7) << "text style in a display";
    EXPECT_DOUBLE_EQ(lay(u"\\frac{a}{b}", true).glyphs[0].size, kPx);
    EXPECT_GT(lay(u"\\dfrac{a}{b}").ascent, lay(u"\\frac{a}{b}").ascent);
    EXPECT_EQ(lay(u"\\dfrac{a}{b}", true).glyphs, lay(u"\\frac{a}{b}", true).glyphs);
    EXPECT_EQ(lay(u"\\tfrac{a}{b}").glyphs, lay(u"\\frac{a}{b}").glyphs);
    // In a superscript (script style) \dfrac comes back to full size.
    const MathLayout script = lay(u"x^{\\dfrac{a}{b}}");
    ASSERT_FALSE(script.error.has_value()) << errorOf(script);
    EXPECT_DOUBLE_EQ(script.glyphs[1].size, kPx);
    // The binomials: the parentheses and both entries in the forced style.
    const MathLayout dbinom = lay(u"\\dbinom{n}{k}");
    ASSERT_FALSE(dbinom.error.has_value()) << errorOf(dbinom);
    EXPECT_EQ(dbinom.glyphs, lay(u"\\binom{n}{k}", true).glyphs);
    EXPECT_EQ(lay(u"\\tbinom{n}{k}", true).glyphs, lay(u"\\binom{n}{k}").glyphs);
}

TEST(S045_MathLayout, ExtensibleArrowsCarryTheirLabels) {
    const MathLayout x = lay(u"A \\xrightarrow{h\\nu} B");
    ASSERT_FALSE(x.error.has_value()) << errorOf(x);
    const MathLayout wide = lay(u"A \\xrightarrow{\\text{a long label}} B");
    EXPECT_GT(wide.width, x.width);
    EXPECT_FALSE(lay(u"\\xrightarrow[below]{above}").error.has_value());
}

TEST(S045_MathLayout, ErrorsNameTheReasonAndPosition) {
    EXPECT_EQ(errorOf(lay(u"\\frac{a")), "missing }");
    EXPECT_EQ(errorPosition(lay(u"x + \\frac{a")), 9U);
    EXPECT_EQ(errorOf(lay(u"\\notacommand x")), "unknown command \\notacommand");
    EXPECT_EQ(errorPosition(lay(u"a \\notacommand")), 2U);
    EXPECT_EQ(errorOf(lay(u"x^2^3")), "double superscript");
    EXPECT_EQ(errorOf(lay(u"x_2_3")), "double subscript");
    EXPECT_EQ(errorOf(lay(u"\\left( x")), "\\left without \\right");
    EXPECT_EQ(errorOf(lay(u"x \\right)")), "\\right without \\left");
    EXPECT_EQ(errorOf(lay(u"a }")), "unexpected }");
    EXPECT_EQ(errorOf(lay(u"a & b")), "& outside an environment");
    EXPECT_EQ(errorOf(lay(u"\\begin{foo} a \\end{foo}")), "unknown environment");
    EXPECT_EQ(errorOf(lay(u"\\begin{matrix} a \\end{pmatrix}")), "\\end does not match \\begin");
    EXPECT_EQ(errorOf(lay(u"\\begin{matrix} a")), "missing \\end");
    EXPECT_EQ(errorOf(lay(u"\\frac")), "missing argument");
    EXPECT_EQ(errorOf(lay(u"")), "the math font has no glyph for U+E000");
    EXPECT_EQ(errorOf(lay(std::u16string(100, u'{') + std::u16string(100, u'}'))), "too deeply nested");
    EXPECT_FALSE(lay(std::u16string(60, u'{') + u"x" + std::u16string(60, u'}')).error.has_value());
}

TEST(S045_MathLayout, TheSubsetOfTheStoryLaysOut) {
    const std::u16string maxwell = std::u16string(u"\\nabla \\times \\mathbf{B} = \\mu_0\\left(\\mathbf{J} + ") +
                                   u"\\varepsilon_0 \\frac{\\partial \\mathbf{E}}{\\partial t}\\right)";
    const std::u16string chemistry = std::u16string(u"\\text{6 CO}_2 + \\text{6 H}_2\\text{O} \\xrightarrow{h\\nu} ") +
                                     u"\\text{C}_6\\text{H}_{12}\\text{O}_6 \\uparrow";
    for (const std::u16string_view source : std::initializer_list<std::u16string_view>{
             u"\\alpha_1, \\beta_2, \\gamma_3, \\Omega, \\omega, \\phi, \\varphi, \\epsilon, \\varepsilon",
             u"\\frac{\\sqrt{x^2 + y^2}}{z_0^2}",
             u"\\binom{n}{k} = \\frac{n!}{k!(n-k)!}",
             u"\\left( \\sum_{k=1}^n a_k b_k \\right)^2 \\leq \\left( \\sum_{k=1}^n a_k^2 \\right)",
             maxwell,
             u"\\vdots & \\ddots & \\cdots",
             chemistry,
             u"\\int_0^\\infty e^{-x} \\, dx = 1",
             u"\\lim_{x \\to 0} \\frac{\\sin x}{x} = 1",
             u"f'(x) + f''(x)",
             u"\\sqrt[n]{a} \\cdot \\pm \\infty",
             u"\\LaTeX",
             u"\\begin{bmatrix} 1 & 0 \\\\ 0 & 1 \\end{bmatrix} \\begin{vmatrix} a \\end{vmatrix}",
         }) {
        const bool inArray = source.find(u'&') != std::u16string_view::npos && source.find(u"begin") == source.npos;
        const std::u16string wrapped =
            inArray ? u"\\begin{matrix}" + std::u16string(source) + u"\\end{matrix}" : std::u16string(source);
        const MathLayout l = lay(wrapped, true);
        EXPECT_FALSE(l.error.has_value()) << hitokoto::test::narrow(source) << ": " << errorOf(l);
    }
}

// Arbitrary input never crashes, never throws and stays within the nesting bound (S045, fuzzing).
TEST(S045_MathLayout, ArbitraryInputNeverCrashes) {
    const std::u16string alphabet = u"ab1{}^_&\\[]()|.' ~=+-";
    const std::vector<std::u16string> words{
        u"\\frac",           u"\\sqrt",         u"\\left",  u"\\right", u"\\begin{matrix}",
        u"\\end{matrix}",    u"\\\\",           u"\\text{", u"\\sum",   u"\\xrightarrow",
        u"\\begin{aligned}", u"\\end{aligned}", u"\\binom", u"\\mathbf"};
    std::mt19937 random(45);
    for (int round = 0; round < 3000; ++round) {
        std::u16string source;
        const int length = static_cast<int>(random() % 40);
        for (int i = 0; i < length; ++i) {
            if (random() % 4 == 0) {
                source += words[random() % words.size()];
            } else {
                source += alphabet[random() % alphabet.size()];
            }
        }
        const MathLayout l = lay(source, round % 2 == 0);
        if (!l.error) {
            EXPECT_GE(l.width, -1e6);
        }
    }
}
