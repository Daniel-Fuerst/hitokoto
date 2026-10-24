// SPDX-License-Identifier: GPL-3.0-or-later
// S005: a theme file changes the look; broken themes fall back to the built-in theme.
#include "theme_manager.hpp"
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s005_theme : public QObject {
    Q_OBJECT
private slots:
    void customThemeChangesPaletteFontsAndHeadings() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("theme.toml"),
                                         "base = \"dark\"\n"
                                         "[colors]\naccent = \"#ff8800\"\nbackground = \"#101010\"\n"
                                         "[fonts]\neditor-size = 17\n"
                                         "[headings]\nh1-scale = 3\n");
        ui::ThemeManager themes(*qApp, path);
        QObject::connect(&themes, &ui::ThemeManager::themeChanged, f.window.get(), &ui::MainWindow::applyTheme);
        QSignalSpy problems(&themes, &ui::ThemeManager::problemsFound);
        themes.reload();
        QCOMPARE(problems.size(), 0);
        QCOMPARE(QApplication::palette().color(QPalette::Base), QColor(0x10, 0x10, 0x10));
        QCOMPARE(QApplication::palette().color(QPalette::Accent), QColor(0xff, 0x88, 0x00));
        QCOMPARE(QApplication::palette().color(QPalette::Text),
                 QColor(QString::fromStdString(core::builtinTheme(core::ColorScheme::Dark).text.toHex())));
        QCOMPARE(f.editor()->document()->defaultFont().pointSizeF(), 17.0);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("#ff8800")));

        QVERIFY(f.open(f.writeFile(QStringLiteral("h.md"), "# Big\nx\n")));
        f.placeCaret(1);
        QCOMPARE(test::formatAt(f.editor()->document()->firstBlock(), 2).fontPointSize(), 51.0);
    }

    void brokenThemeFallsBackAndReportsLineAndColumn() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("theme.toml"), "[colors]\naccent = \"nope\"\n");
        ui::ThemeManager themes(*qApp, path);
        QSignalSpy problems(&themes, &ui::ThemeManager::problemsFound);
        themes.reload();
        QCOMPARE(problems.size(), 1);
        const QStringList lines = problems.first().at(1).toStringList();
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.first().startsWith(QStringLiteral("line 2, column 10")));
        QCOMPARE(themes.current(), core::builtinTheme(ui::ThemeManager::systemScheme()));
    }

    void missingThemeFileMeansBuiltinWithoutComplaints() {
        test::Fixture f;
        ui::ThemeManager themes(*qApp, f.dir.filePath(QStringLiteral("absent.toml")));
        QSignalSpy problems(&themes, &ui::ThemeManager::problemsFound);
        themes.reload();
        QCOMPARE(problems.size(), 0);
        QCOMPARE(themes.current(), core::builtinTheme(ui::ThemeManager::systemScheme()));
    }

    void builtinThemesRenderDifferently() {
        test::Fixture f;
        f.editor()->setPlainText(QStringLiteral("# Light and dark\n- item\ntext"));
        f.window->applyTheme(core::builtinTheme(core::ColorScheme::Light));
        ui::applyThemeToApplication(*qApp, core::builtinTheme(core::ColorScheme::Light));
        const QImage light = f.window->grab().toImage();
        f.window->applyTheme(core::builtinTheme(core::ColorScheme::Dark));
        ui::applyThemeToApplication(*qApp, core::builtinTheme(core::ColorScheme::Dark));
        const QImage dark = f.window->grab().toImage();
        QVERIFY(light != dark);
        const QPoint corner(light.width() - 5, light.height() - 5);
        QVERIFY(light.pixelColor(corner).lightness() > dark.pixelColor(corner).lightness());
    }
};

QTEST_MAIN(tst_s005_theme)
#include "tst_s005_theme.moc"
