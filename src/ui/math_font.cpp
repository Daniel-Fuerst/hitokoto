// SPDX-License-Identifier: GPL-3.0-or-later
#include "math_font.hpp"

#include <QFile>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <span>

namespace hitokoto::ui {

const QByteArray& bundledMathFontData() {
    static const QByteArray data = [] {
        QFile file(QStringLiteral(":/fonts/STIXTwoMath-Regular.otf"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    return data;
}

QRawFont bundledMathFont(qreal pixelSize) {
    return QRawFont(bundledMathFontData(), pixelSize);
}

const core::MathTable& bundledMathTable() {
    static const core::MathTable table = [] {
        const QByteArray& data = bundledMathFontData();
        const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(data.constData()),
                                                  static_cast<std::size_t>(data.size()));
        return core::MathTable::parse(core::openTypeTable(bytes, "MATH")).value_or(core::MathTable{});
    }();
    return table;
}

QtMathFont::QtMathFont(const QByteArray& fontData) {
    // Loaded once at its own units per em, so that Qt's pixel metrics are design units.
    const QRawFont probe(fontData, 1000);
    const qreal upem = probe.isValid() ? probe.unitsPerEm() : 1000;
    font_ = QRawFont(fontData, upem);
}

double QtMathFont::unitsPerEm() const {
    return font_.isValid() ? font_.unitsPerEm() : 1000;
}

std::uint16_t QtMathFont::glyph(char32_t character) const {
    if (const auto it = glyphs_.find(character); it != glyphs_.end()) {
        return it->second;
    }
    std::uint16_t id = 0;
    if (font_.isValid() && font_.supportsCharacter(character)) {
        const char32_t text[] = {character};
        const QList<quint32> indexes = font_.glyphIndexesForString(QString::fromUcs4(text, 1));
        if (indexes.size() == 1) {
            id = static_cast<std::uint16_t>(indexes.front());
        }
    }
    glyphs_.emplace(character, id);
    return id;
}

double QtMathFont::advance(std::uint16_t glyph) const {
    const QList<QPointF> advances = font_.advancesForGlyphIndexes(QList<quint32>{glyph});
    return advances.isEmpty() ? 0 : advances.front().x();
}

core::GlyphBounds QtMathFont::bounds(std::uint16_t glyph) const {
    const QRectF r = font_.boundingRect(glyph);  // y downwards
    return {r.left(), -r.bottom(), r.right(), -r.top()};
}

const QtMathFont& bundledMathMetrics() {
    static const QtMathFont metrics(bundledMathFontData());
    return metrics;
}

}  // namespace hitokoto::ui
