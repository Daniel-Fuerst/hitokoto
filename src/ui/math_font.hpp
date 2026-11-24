// SPDX-License-Identifier: GPL-3.0-or-later
// The bundled math font (S045, ADR 0014): STIX Two Math, compiled into the program as a Qt
// resource, so typesetting never depends on a font the user installed.
#pragma once

#include <QByteArray>
#include <QRawFont>

namespace hitokoto::ui {

/// The bytes of the bundled font file. Read on the first call, not at start-up (brief, section 4:
/// nothing the first frame does not need); the same bytes afterwards.
const QByteArray& bundledMathFontData();

/// The bundled font at `pixelSize`, for laying out and painting glyphs. Invalid only if the
/// resource is missing, which a test guards.
QRawFont bundledMathFont(qreal pixelSize);

}  // namespace hitokoto::ui
