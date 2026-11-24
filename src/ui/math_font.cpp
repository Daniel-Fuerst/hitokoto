// SPDX-License-Identifier: GPL-3.0-or-later
#include "math_font.hpp"

#include <QFile>

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

}  // namespace hitokoto::ui
