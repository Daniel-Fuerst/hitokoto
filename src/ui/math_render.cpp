// SPDX-License-Identifier: GPL-3.0-or-later
#include "math_render.hpp"

#include "math_font.hpp"

#include <QGlyphRun>
#include <QList>

#include <cmath>
#include <cstring>

namespace hitokoto::ui {

namespace {

/// The cache key: the source, then the size and style as code units after a separator that a
/// source cannot hold in the same place (the key is never parsed, only compared).
std::u16string keyOf(std::u16string_view source, double pixelSize, bool display) {
    std::u16string key(source);
    key.push_back(u'\0');
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(pixelSize));
    std::memcpy(&bits, &pixelSize, sizeof(bits));
    for (int i = 0; i < 4; ++i) {
        key.push_back(static_cast<char16_t>((bits >> (16U * static_cast<unsigned>(i))) & 0xFFFFU));
    }
    key.push_back(display ? u'D' : u'T');
    return key;
}

const QRawFont& rawFontAt(double size) {
    static std::map<double, QRawFont> fonts;
    auto it = fonts.find(size);
    if (it == fonts.end()) {
        if (fonts.size() > 64) {
            fonts.clear();  // sizes come from themes and zoom; a handful are in use at a time
        }
        it = fonts.emplace(size, bundledMathFont(size)).first;
    }
    return it->second;
}

}  // namespace

const core::MathLayout& MathCache::layout(std::u16string_view source, double pixelSize, bool display) {
    std::u16string key = keyOf(source, pixelSize, display);
    if (const auto it = layouts_.find(key); it != layouts_.end()) {
        return it->second;
    }
    if (layouts_.size() >= kCapacity) {
        layouts_.clear();
    }
    ++made_;
    core::MathLayout made = core::layoutMath(source, bundledMathMetrics(), bundledMathTable(), pixelSize, display);
    return layouts_.emplace(std::move(key), std::move(made)).first->second;
}

void paintMathLayout(QPainter& painter, const core::MathLayout& layout, QPointF origin, const QColor& color) {
    if (layout.error) {
        return;
    }
    std::map<double, std::pair<QList<quint32>, QList<QPointF>>> bySize;
    for (const core::MathGlyph& g : layout.glyphs) {
        auto& [ids, points] = bySize[g.size];
        ids.append(g.glyph);
        points.append(QPointF(g.x, g.y));
    }
    painter.save();
    painter.setPen(color);
    for (const auto& [size, run] : bySize) {
        QGlyphRun glyphs;
        glyphs.setRawFont(rawFontAt(size));
        glyphs.setGlyphIndexes(run.first);
        glyphs.setPositions(run.second);
        painter.drawGlyphRun(origin, glyphs);
    }
    for (const core::MathRule& r : layout.rules) {
        painter.fillRect(QRectF(origin + QPointF(r.x, r.y), QSizeF(r.width, r.height)), color);
    }
    painter.restore();
}

}  // namespace hitokoto::ui
