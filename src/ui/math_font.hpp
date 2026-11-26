// SPDX-License-Identifier: GPL-3.0-or-later
// The bundled math font (S045, ADR 0014): STIX Two Math, compiled into the program as a Qt
// resource, so typesetting never depends on a font the user installed; and its glyph metrics for
// the core's math layout.
#pragma once

#include "hitokoto/core/math.hpp"

#include <QByteArray>
#include <QRawFont>

#include <unordered_map>

namespace hitokoto::ui {

/// The bytes of the bundled font file. Read on the first call, not at start-up (brief, section 4:
/// nothing the first frame does not need); the same bytes afterwards.
const QByteArray& bundledMathFontData();

/// The bundled font at `pixelSize`, for painting glyphs. Invalid only if the resource is missing,
/// which a test guards.
QRawFont bundledMathFont(qreal pixelSize);

/// The bundled font's MATH table, read once; empty constants if the font had none.
const core::MathTable& bundledMathTable();

/// The glyph metrics of a raw font for the core, in design units (the font loaded at one pixel per
/// unit). Characters are looked up once and kept.
class QtMathFont final : public core::MathFont {
public:
    explicit QtMathFont(const QByteArray& fontData);

    [[nodiscard]] double unitsPerEm() const override;
    [[nodiscard]] std::uint16_t glyph(char32_t character) const override;
    [[nodiscard]] double advance(std::uint16_t glyph) const override;
    [[nodiscard]] core::GlyphBounds bounds(std::uint16_t glyph) const override;

private:
    QRawFont font_;
    mutable std::unordered_map<char32_t, std::uint16_t> glyphs_;
};

/// The bundled font's metrics, made on the first call.
const QtMathFont& bundledMathMetrics();

}  // namespace hitokoto::ui
