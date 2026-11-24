// SPDX-License-Identifier: GPL-3.0-or-later
// S045: reading the OpenType MATH table (https://learn.microsoft.com/typography/opentype/spec/math)
// and finding a table in a font file. Every read is bounds-checked; an offset outside the table is
// skipped, never followed, so a malformed font gives less data, never a crash.
#include "hitokoto/core/math.hpp"

#include <array>

namespace hitokoto::core {
namespace {

/// Big-endian reads over a span; a read outside it gives 0 and marks the reader bad.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint16_t u16(std::size_t at) {
        if (at + 2 > bytes_.size()) {
            bad_ = true;
            return 0;
        }
        return static_cast<std::uint16_t>((static_cast<unsigned>(bytes_[at]) << 8U) | bytes_[at + 1]);
    }
    [[nodiscard]] std::int16_t i16(std::size_t at) { return static_cast<std::int16_t>(u16(at)); }
    [[nodiscard]] std::uint32_t u32(std::size_t at) {
        return (static_cast<std::uint32_t>(u16(at)) << 16U) | u16(at + 2);
    }
    /// The sub-table at `offset` from `base`, or an empty reader when the offset is 0 or outside.
    [[nodiscard]] Reader at(std::size_t base, std::size_t offset) const {
        if (offset == 0 || base + offset >= bytes_.size()) {
            return Reader({});
        }
        return Reader(bytes_.subspan(base + offset));
    }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }
    [[nodiscard]] bool bad() const noexcept { return bad_; }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

private:
    std::span<const std::uint8_t> bytes_;
    bool bad_ = false;
};

/// The glyphs a Coverage table lists, in coverage-index order (formats 1 and 2).
std::vector<std::uint16_t> coverage(Reader r) {
    std::vector<std::uint16_t> glyphs;
    if (r.empty()) {
        return glyphs;
    }
    const std::uint16_t format = r.u16(0);
    const std::uint16_t count = r.u16(2);
    if (format == 1) {
        if (4 + std::size_t{count} * 2 > r.size()) {
            return glyphs;
        }
        glyphs.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            glyphs.push_back(r.u16(4 + i * 2));
        }
    } else if (format == 2) {
        if (4 + std::size_t{count} * 6 > r.size()) {
            return glyphs;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint16_t start = r.u16(4 + i * 6);
            const std::uint16_t end = r.u16(6 + i * 6);
            const std::uint16_t index = r.u16(8 + i * 6);
            if (end < start) {
                continue;
            }
            const std::size_t need = std::size_t{index} + (end - start) + 1U;
            if (glyphs.size() < need) {
                glyphs.resize(need, 0);
            }
            for (std::uint32_t g = start; g <= end; ++g) {
                glyphs[index + (g - start)] = static_cast<std::uint16_t>(g);
            }
        }
    }
    return glyphs;
}

void readConstants(Reader r, MathConstants& c) {
    if (r.size() < 214) {
        return;
    }
    c.scriptPercentScaleDown = r.i16(0);
    c.scriptScriptPercentScaleDown = r.i16(2);
    c.delimitedSubFormulaMinHeight = r.u16(4);
    c.displayOperatorMinHeight = r.u16(6);
    // The MathValueRecords (value, device offset) in the order of the specification.
    const std::array<double MathConstants::*, 51> records{
        &MathConstants::mathLeading,
        &MathConstants::axisHeight,
        &MathConstants::accentBaseHeight,
        &MathConstants::flattenedAccentBaseHeight,
        &MathConstants::subscriptShiftDown,
        &MathConstants::subscriptTopMax,
        &MathConstants::subscriptBaselineDropMin,
        &MathConstants::superscriptShiftUp,
        &MathConstants::superscriptShiftUpCramped,
        &MathConstants::superscriptBottomMin,
        &MathConstants::superscriptBaselineDropMax,
        &MathConstants::subSuperscriptGapMin,
        &MathConstants::superscriptBottomMaxWithSubscript,
        &MathConstants::spaceAfterScript,
        &MathConstants::upperLimitGapMin,
        &MathConstants::upperLimitBaselineRiseMin,
        &MathConstants::lowerLimitGapMin,
        &MathConstants::lowerLimitBaselineDropMin,
        &MathConstants::stackTopShiftUp,
        &MathConstants::stackTopDisplayStyleShiftUp,
        &MathConstants::stackBottomShiftDown,
        &MathConstants::stackBottomDisplayStyleShiftDown,
        &MathConstants::stackGapMin,
        &MathConstants::stackDisplayStyleGapMin,
        &MathConstants::stretchStackTopShiftUp,
        &MathConstants::stretchStackBottomShiftDown,
        &MathConstants::stretchStackGapAboveMin,
        &MathConstants::stretchStackGapBelowMin,
        &MathConstants::fractionNumeratorShiftUp,
        &MathConstants::fractionNumeratorDisplayStyleShiftUp,
        &MathConstants::fractionDenominatorShiftDown,
        &MathConstants::fractionDenominatorDisplayStyleShiftDown,
        &MathConstants::fractionNumeratorGapMin,
        &MathConstants::fractionNumDisplayStyleGapMin,
        &MathConstants::fractionRuleThickness,
        &MathConstants::fractionDenominatorGapMin,
        &MathConstants::fractionDenomDisplayStyleGapMin,
        &MathConstants::skewedFractionHorizontalGap,
        &MathConstants::skewedFractionVerticalGap,
        &MathConstants::overbarVerticalGap,
        &MathConstants::overbarRuleThickness,
        &MathConstants::overbarExtraAscender,
        &MathConstants::underbarVerticalGap,
        &MathConstants::underbarRuleThickness,
        &MathConstants::underbarExtraDescender,
        &MathConstants::radicalVerticalGap,
        &MathConstants::radicalDisplayStyleVerticalGap,
        &MathConstants::radicalRuleThickness,
        &MathConstants::radicalExtraAscender,
        &MathConstants::radicalKernBeforeDegree,
        &MathConstants::radicalKernAfterDegree,
    };
    for (std::size_t i = 0; i < records.size(); ++i) {
        c.*records[i] = r.i16(8 + i * 4);
    }
    c.radicalDegreeBottomRaisePercent = r.i16(8 + records.size() * 4);
}

void readItalics(Reader glyphInfo, std::unordered_map<std::uint16_t, double>& out) {
    const Reader italics = glyphInfo.at(0, glyphInfo.u16(0));
    if (italics.empty()) {
        return;
    }
    Reader r = italics;
    const std::vector<std::uint16_t> glyphs = coverage(r.at(0, r.u16(0)));
    const std::uint16_t count = r.u16(2);
    for (std::size_t i = 0; i < count && i < glyphs.size(); ++i) {
        const std::int16_t value = r.i16(4 + i * 4);
        if (r.bad()) {
            break;
        }
        out[glyphs[i]] = value;
    }
}

GlyphConstruction construction(Reader r) {
    GlyphConstruction c;
    if (r.empty()) {
        return c;
    }
    const std::uint16_t assemblyOffset = r.u16(0);
    const std::uint16_t variantCount = r.u16(2);
    for (std::size_t i = 0; i < variantCount; ++i) {
        const std::uint16_t glyph = r.u16(4 + i * 4);
        const std::uint16_t advance = r.u16(6 + i * 4);
        if (r.bad()) {
            break;
        }
        c.variants.push_back({glyph, static_cast<double>(advance)});
    }
    Reader a = r.at(0, assemblyOffset);
    if (!a.empty()) {
        const std::uint16_t partCount = a.u16(4);  // after the italics correction record
        for (std::size_t i = 0; i < partCount; ++i) {
            const std::size_t at = 6 + i * 10;
            GlyphPart part{a.u16(at), static_cast<double>(a.u16(at + 2)), static_cast<double>(a.u16(at + 4)),
                           static_cast<double>(a.u16(at + 6)), (a.u16(at + 8) & 1U) != 0};
            if (a.bad()) {
                c.assembly.clear();
                break;
            }
            c.assembly.push_back(part);
        }
    }
    return c;
}

void readVariants(Reader r, MathTable& table) {
    if (r.empty()) {
        return;
    }
    table.minConnectorOverlap = r.u16(0);
    const std::vector<std::uint16_t> vertical = coverage(r.at(0, r.u16(2)));
    const std::vector<std::uint16_t> horizontal = coverage(r.at(0, r.u16(4)));
    const std::uint16_t vertCount = r.u16(6);
    const std::uint16_t horizCount = r.u16(8);
    for (std::size_t i = 0; i < vertCount && i < vertical.size(); ++i) {
        const std::uint16_t offset = r.u16(10 + i * 2);
        if (r.bad()) {
            return;
        }
        table.vertical[vertical[i]] = construction(r.at(0, offset));
    }
    for (std::size_t i = 0; i < horizCount && i < horizontal.size(); ++i) {
        const std::uint16_t offset = r.u16(10 + (std::size_t{vertCount} + i) * 2);
        if (r.bad()) {
            return;
        }
        table.horizontal[horizontal[i]] = construction(r.at(0, offset));
    }
}

}  // namespace

std::span<const std::uint8_t> openTypeTable(std::span<const std::uint8_t> font, std::string_view tag) {
    Reader r(font);
    if (tag.size() != 4) {
        return {};
    }
    const std::uint16_t count = r.u16(4);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t record = 12 + i * 16;
        if (record + 16 > font.size()) {
            return {};
        }
        const bool match = font[record] == static_cast<std::uint8_t>(tag[0]) &&
                           font[record + 1] == static_cast<std::uint8_t>(tag[1]) &&
                           font[record + 2] == static_cast<std::uint8_t>(tag[2]) &&
                           font[record + 3] == static_cast<std::uint8_t>(tag[3]);
        if (!match) {
            continue;
        }
        const std::uint32_t offset = r.u32(record + 8);
        const std::uint32_t length = r.u32(record + 12);
        if (offset > font.size() || length > font.size() - offset) {
            return {};
        }
        return font.subspan(offset, length);
    }
    return {};
}

std::optional<MathTable> MathTable::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    if (bytes.size() < 10 || r.u16(0) != 1) {
        return std::nullopt;
    }
    MathTable table;
    readConstants(r.at(0, r.u16(4)), table.constants);
    readItalics(r.at(0, r.u16(6)), table.italicCorrections);
    readVariants(r.at(0, r.u16(8)), table);
    return table;
}

}  // namespace hitokoto::core
