// SPDX-License-Identifier: GPL-3.0-or-later
// S020: backslash escapes render without backslashes when concealed, byte-exact source preserved.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}
}  // namespace

class tst_s020_backslash_escapes : public QObject {
    Q_OBJECT
private slots:
    void userExampleConcealsBackslashesWhenNotOnLine() {
        constexpr auto content =
            "\\\\ \\` \\* \\_ \\{ \\} \\[ \\] \\< \\> \\( \\) \\# \\+ \\- \\. \\! \\| \\~ \\$\nsecond line\n";
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("escapes.md"), content);
        QVERIFY(f.open(path));
        // Place caret on the second line so the first line is concealed.
        f.placeCaret(1);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        for (int i = 0; i < 20; ++i) {
            const int pos = i * 3;
            QVERIFY2(test::isCollapsed(block0, pos),
                     qPrintable(QStringLiteral("backslash at %1 should be collapsed").arg(pos)));
            QVERIFY2(!test::isCollapsed(block0, pos + 1),
                     qPrintable(QStringLiteral("escaped char at %1 should be visible").arg(pos + 1)));
        }

        // Saving preserves the exact original Markdown with all backslashes.
        const QString savePath = f.dir.filePath(QStringLiteral("saved.md"));
        QVERIFY(f.saveAndWait(savePath));
        QCOMPARE(test::Fixture::readFile(savePath), QByteArray(content));
    }

    void revealedLineShowsBackslashesWithMarkerColor() {
        constexpr auto content =
            "\\\\ \\` \\* \\_ \\{ \\} \\[ \\] \\< \\> \\( \\) \\# \\+ \\- \\. \\! \\| \\~ \\$\nsecond line\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("escapes.md"), content)));
        // Place caret on the first line to reveal syntax markers.
        f.placeCaret(0);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        for (int i = 0; i < 20; ++i) {
            const int pos = i * 3;
            QVERIFY2(!test::isCollapsed(block0, pos),
                     qPrintable(QStringLiteral("backslash at %1 should be revealed").arg(pos)));
            QCOMPARE(test::formatAt(block0, pos).foreground().color(), qcolor(theme.marker));
        }
    }

    void customThemeEscapeMarkerApplies() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("escape.md"), "\\*escaped\\*\nsecond\n")));
        f.placeCaret(0);

        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.escape.marker = core::Color{0xaa, 0x11, 0xbb, 0xff};
        f.window->applyTheme(theme);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        QCOMPARE(test::formatAt(block0, 0).foreground().color(), qcolor(*theme.escape.marker));
        QCOMPARE(test::formatAt(block0, 9).foreground().color(), qcolor(*theme.escape.marker));
    }
};

QTEST_MAIN(tst_s020_backslash_escapes)
#include "tst_s020_backslash_escapes.moc"
