// SPDX-License-Identifier: GPL-3.0-or-later
// S045, ADR 0014: the math font is inside the program, so typesetting needs no font the user
// installed, and its licence travels with it.
#include "math_font.hpp"

#include <QFile>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QTest>
#include <QTextStream>
#include <QtEndian>

#include <map>

using namespace hitokoto;

namespace {

/// Whether an OpenType file's table directory lists `tag`.
bool hasTable(const QByteArray& font, const char* tag) {
    if (font.size() < 12) {
        return false;
    }
    const auto count = qFromBigEndian<quint16>(font.constData() + 4);
    for (int i = 0; i < count; ++i) {
        const qsizetype at = 12 + qsizetype{16} * i;
        if (at + 16 > font.size()) {
            return false;
        }
        if (font.mid(at, 4) == QByteArray(tag)) {
            return true;
        }
    }
    return false;
}

/// The formulas of the sample, display blocks joined (`$$` lines dropped), inline ones as written.
QList<QPair<QString, bool>> sampleFormulas() {
    QFile file(QStringLiteral(HITOKOTO_SOURCE_DIR "/tests/data/s044-latex.md"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QList<QPair<QString, bool>> out;
    QString block;
    bool inBlock = false;
    for (const QString& line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        if (line.trimmed() == QStringLiteral("$$")) {
            if (inBlock) {
                out.append({block, true});
                block.clear();
            }
            inBlock = !inBlock;
            continue;
        }
        if (inBlock) {
            block += line + QLatin1Char('\n');
        }
    }
    out.append({QStringLiteral("\\alpha_1, \\beta_2, \\gamma_3, \\Omega, \\omega, \\phi, \\varphi"), false});
    out.append({QStringLiteral("\\frac{\\sqrt{x^2 + y^2}}{z_0^2}"), false});
    out.append({QStringLiteral("\\binom{n}{k} = \\frac{n!}{k!(n-k)!}"), false});
    return out;
}

/// Paints a layout with its baseline origin at `origin` (for the preview).
void paint(QPainter& painter, const core::MathLayout& layout, QPointF origin) {
    std::map<double, std::pair<QList<quint32>, QList<QPointF>>> bySize;
    for (const core::MathGlyph& g : layout.glyphs) {
        auto& [ids, points] = bySize[g.size];
        ids.append(g.glyph);
        points.append(origin + QPointF(g.x, g.y));
    }
    for (const auto& [size, run] : bySize) {
        QGlyphRun glyphs;
        glyphs.setRawFont(ui::bundledMathFont(size));
        glyphs.setGlyphIndexes(run.first);
        glyphs.setPositions(run.second);
        painter.drawGlyphRun(QPointF(), glyphs);
    }
    for (const core::MathRule& r : layout.rules) {
        painter.fillRect(QRectF(origin + QPointF(r.x, r.y), QSizeF(r.width, r.height)), painter.pen().color());
    }
}

}  // namespace

class tst_s045_bundled_math_font : public QObject {
    Q_OBJECT
private slots:
    void theFontIsInsideTheProgram() {
        const QByteArray& data = ui::bundledMathFontData();
        QVERIFY2(!data.isEmpty(), "the resource :/fonts/STIXTwoMath-Regular.otf is missing");
        QVERIFY(hasTable(data, "MATH"));
        QCOMPARE(&ui::bundledMathFontData(), &data);  // read once
    }

    void theFontLoadsAndHasMathGlyphs() {
        const QRawFont font = ui::bundledMathFont(20);
        QVERIFY(font.isValid());
        QCOMPARE(font.familyName(), QStringLiteral("STIX Two Math"));
        QVERIFY(!font.fontTable("MATH").isEmpty());
        // A sum, nabla, partial and an integral, as the sample of S044 writes them.
        for (const char32_t c : {U'∑', U'∇', U'∂', U'∫', U'α'}) {
            QVERIFY2(font.supportsCharacter(c), qPrintable(QString::number(c, 16)));
        }
    }

    void theSampleLaysOutWithTheBundledFont() {
        const auto& font = ui::bundledMathMetrics();
        const auto& table = ui::bundledMathTable();
        QVERIFY(table.constants.axisHeight > 0);
        const auto formulas = sampleFormulas();
        QCOMPARE(formulas.size(), 7);
        QImage preview(900, 1100, QImage::Format_ARGB32);
        preview.fill(Qt::white);
        QPainter painter(&preview);
        painter.setPen(Qt::black);
        double y = 20;
        for (const auto& [source, display] : formulas) {
            const core::MathLayout layout = core::layoutMath(source.toStdU16String(), font, table, 28, display);
            QVERIFY2(!layout.error, qPrintable(source + QStringLiteral(": ") +
                                               QString::fromStdString(layout.error ? layout.error->message : "")));
            QVERIFY(layout.width > 0 && layout.ascent > 0);
            // Real ink: every glyph has an outline (no missing glyph boxes).
            for (const core::MathGlyph& g : layout.glyphs) {
                QVERIFY(g.glyph != 0);
            }
            y += layout.ascent + 10;
            paint(painter, layout, QPointF(20, y));
            y += layout.descent + 20;
        }
        painter.end();
        // A picture to look at: HITOKOTO_MATH_PREVIEW=<file.png> writes it.
        if (const QByteArray out = qgetenv("HITOKOTO_MATH_PREVIEW"); !out.isEmpty()) {
            QVERIFY(preview.save(QString::fromLocal8Bit(out)));
        }
    }

    void theLicenceTravelsWithTheFont() {
        QFile licence(QStringLiteral(":/fonts/OFL.txt"));
        QVERIFY(licence.open(QIODevice::ReadOnly));
        QVERIFY(licence.readAll().contains("SIL OPEN FONT LICENSE Version 1.1"));
    }
};

QTEST_MAIN(tst_s045_bundled_math_font)
#include "tst_s045_bundled_math_font.moc"
