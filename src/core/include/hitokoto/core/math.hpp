// SPDX-License-Identifier: GPL-3.0-or-later
// S045, ADR 0014: laying out LaTeX formulas the way TeX does, driven by the OpenType MATH table of
// one font. The core reads the formula and the table and places glyphs and rules; the UI supplies
// the font's glyph metrics (MathFont) and paints the result. Never throws for any input.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hitokoto::core {

/// The bytes of the table `tag` (four characters, e.g. "MATH") inside an OpenType font file, or an
/// empty span if the file has no such table or is malformed.
[[nodiscard]] std::span<const std::uint8_t> openTypeTable(std::span<const std::uint8_t> font, std::string_view tag);

/// Ink bounds of a glyph in design units, y upwards from the baseline.
struct GlyphBounds {
    double xMin = 0;
    double yMin = 0;
    double xMax = 0;
    double yMax = 0;
    friend bool operator==(const GlyphBounds&, const GlyphBounds&) = default;
};

/// The glyph metrics of the math font, in design units. The UI implements it with Qt; tests use a
/// font of their own.
class MathFont {
public:
    MathFont() = default;
    MathFont(const MathFont&) = delete;
    MathFont& operator=(const MathFont&) = delete;
    virtual ~MathFont() = default;

    [[nodiscard]] virtual double unitsPerEm() const = 0;
    /// The glyph for a character, 0 if the font has none.
    [[nodiscard]] virtual std::uint16_t glyph(char32_t character) const = 0;
    [[nodiscard]] virtual double advance(std::uint16_t glyph) const = 0;
    [[nodiscard]] virtual GlyphBounds bounds(std::uint16_t glyph) const = 0;
};

/// The constants of a MATH table (OpenType, MathConstants), in design units; the two script
/// scales and the degree raise in percent.
struct MathConstants {
    double scriptPercentScaleDown = 70;
    double scriptScriptPercentScaleDown = 50;
    double delimitedSubFormulaMinHeight = 0;
    double displayOperatorMinHeight = 0;
    double mathLeading = 0;
    double axisHeight = 0;
    double accentBaseHeight = 0;
    double flattenedAccentBaseHeight = 0;
    double subscriptShiftDown = 0;
    double subscriptTopMax = 0;
    double subscriptBaselineDropMin = 0;
    double superscriptShiftUp = 0;
    double superscriptShiftUpCramped = 0;
    double superscriptBottomMin = 0;
    double superscriptBaselineDropMax = 0;
    double subSuperscriptGapMin = 0;
    double superscriptBottomMaxWithSubscript = 0;
    double spaceAfterScript = 0;
    double upperLimitGapMin = 0;
    double upperLimitBaselineRiseMin = 0;
    double lowerLimitGapMin = 0;
    double lowerLimitBaselineDropMin = 0;
    double stackTopShiftUp = 0;
    double stackTopDisplayStyleShiftUp = 0;
    double stackBottomShiftDown = 0;
    double stackBottomDisplayStyleShiftDown = 0;
    double stackGapMin = 0;
    double stackDisplayStyleGapMin = 0;
    double stretchStackTopShiftUp = 0;
    double stretchStackBottomShiftDown = 0;
    double stretchStackGapAboveMin = 0;
    double stretchStackGapBelowMin = 0;
    double fractionNumeratorShiftUp = 0;
    double fractionNumeratorDisplayStyleShiftUp = 0;
    double fractionDenominatorShiftDown = 0;
    double fractionDenominatorDisplayStyleShiftDown = 0;
    double fractionNumeratorGapMin = 0;
    double fractionNumDisplayStyleGapMin = 0;
    double fractionRuleThickness = 0;
    double fractionDenominatorGapMin = 0;
    double fractionDenomDisplayStyleGapMin = 0;
    double skewedFractionHorizontalGap = 0;
    double skewedFractionVerticalGap = 0;
    double overbarVerticalGap = 0;
    double overbarRuleThickness = 0;
    double overbarExtraAscender = 0;
    double underbarVerticalGap = 0;
    double underbarRuleThickness = 0;
    double underbarExtraDescender = 0;
    double radicalVerticalGap = 0;
    double radicalDisplayStyleVerticalGap = 0;
    double radicalRuleThickness = 0;
    double radicalExtraAscender = 0;
    double radicalKernBeforeDegree = 0;
    double radicalKernAfterDegree = 0;
    double radicalDegreeBottomRaisePercent = 0;
    friend bool operator==(const MathConstants&, const MathConstants&) = default;
};

/// One part of a glyph assembly (a stretchy delimiter built from pieces), bottom to top or left to
/// right, in design units.
struct GlyphPart {
    std::uint16_t glyph = 0;
    double startConnector = 0;
    double endConnector = 0;
    double fullAdvance = 0;
    bool extender = false;
    friend bool operator==(const GlyphPart&, const GlyphPart&) = default;
};

/// A pre-built larger version of a glyph and its size along the stretch direction.
struct GlyphVariant {
    std::uint16_t glyph = 0;
    double advance = 0;
    friend bool operator==(const GlyphVariant&, const GlyphVariant&) = default;
};

/// How a glyph grows in one direction: larger variants, smallest first, and an assembly for sizes
/// beyond them (empty if the glyph has none).
struct GlyphConstruction {
    std::vector<GlyphVariant> variants;
    std::vector<GlyphPart> assembly;
};

/// What layout needs of a MATH table.
struct MathTable {
    MathConstants constants;
    std::unordered_map<std::uint16_t, double> italicCorrections;
    std::unordered_map<std::uint16_t, GlyphConstruction> vertical;
    std::unordered_map<std::uint16_t, GlyphConstruction> horizontal;
    double minConnectorOverlap = 0;

    /// Reads the bytes of a MATH table (`openTypeTable(font, "MATH")`); nullopt if they are not one.
    /// Offsets outside the table are skipped, never followed.
    [[nodiscard]] static std::optional<MathTable> parse(std::span<const std::uint8_t> bytes);
};

/// A glyph placed by the layout: its baseline origin, x to the right and y downwards (as the UI
/// paints), relative to the formula's origin on its baseline, at `size` pixels per em.
struct MathGlyph {
    std::uint16_t glyph = 0;
    double x = 0;
    double y = 0;
    double size = 0;
    friend bool operator==(const MathGlyph&, const MathGlyph&) = default;
};

/// A filled rectangle (fraction bar, radical overbar), its top-left corner, y downwards.
struct MathRule {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    friend bool operator==(const MathRule&, const MathRule&) = default;
};

/// Why a formula cannot be laid out: the position in its source (UTF-16) and the reason.
struct MathError {
    std::uint32_t position = 0;
    std::string message;
    friend bool operator==(const MathError&, const MathError&) = default;
};

/// A laid-out formula in pixels: its advance width, how far it reaches above and below the
/// baseline, and what to paint; or the error that stopped it (then nothing to paint).
struct MathLayout {
    double width = 0;
    double ascent = 0;
    double descent = 0;
    std::vector<MathGlyph> glyphs;
    std::vector<MathRule> rules;
    std::optional<MathError> error;
};

/// Lays out the LaTeX `source` of one formula at `pixelSize` pixels per em, in display style
/// (`$$`) or text style (`$`). The subset is S045's; anything else gives an error. Linear in the
/// source, at most 64 levels of nesting.
[[nodiscard]] MathLayout layoutMath(std::u16string_view source, const MathFont& font, const MathTable& table,
                                    double pixelSize, bool display);

}  // namespace hitokoto::core
