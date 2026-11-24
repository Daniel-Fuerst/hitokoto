// SPDX-License-Identifier: GPL-3.0-or-later
// S045, ADR 0014: the math font is inside the program, so typesetting needs no font the user
// installed, and its licence travels with it.
#include "math_font.hpp"

#include <QFile>
#include <QRawFont>
#include <QTest>
#include <QtEndian>

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

    void theLicenceTravelsWithTheFont() {
        QFile licence(QStringLiteral(":/fonts/OFL.txt"));
        QVERIFY(licence.open(QIODevice::ReadOnly));
        QVERIFY(licence.readAll().contains("SIL OPEN FONT LICENSE Version 1.1"));
    }
};

QTEST_MAIN(tst_s045_bundled_math_font)
#include "tst_s045_bundled_math_font.moc"
