// SPDX-License-Identifier: GPL-3.0-or-later
// S038: settings menu flyout, live theme switching, and persistent configuration.
#include "floating_toolbar.hpp"
#include "hitokoto/core/settings.hpp"
#include "hitokoto/core/theme.hpp"
#include "settings_menu.hpp"
#include "ui_test_support.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QTemporaryDir>
#include <QToolButton>

#include <filesystem>

using namespace hitokoto;

namespace {
QToolButton* toolbarButton(ui::EditorWidget* editor, const char* name) {
    return editor->floatingToolbar()->findChild<QToolButton*>(QString::fromLatin1(name));
}
}  // namespace

class tst_s038_settings_menu : public QObject {
    Q_OBJECT
private slots:
    void settingsButtonOpensAndClosesSettingsMenuWithDebugInfo() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("notes.md"), "one\ntwo\nthree")));
        f.settle();
        f.editor()->setFocus();

        ui::FloatingToolbar* bar = f.editor()->floatingToolbar();
        ui::SettingsMenu* menu = f.editor()->settingsMenu();
        QToolButton* settingsBtn = toolbarButton(f.editor(), "openSettingsButton");
        QVERIFY(bar != nullptr);
        QVERIFY(menu != nullptr);
        QVERIFY(settingsBtn != nullptr);
        QVERIFY(settingsBtn->isCheckable());
        QVERIFY(!menu->isVisible());
        QVERIFY(!settingsBtn->isChecked());

        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();
        QVERIFY(menu->isVisible());
        QVERIFY(settingsBtn->isChecked());
        QVERIFY(f.editor()->hasFocus());
        QVERIFY(menu->geometry().right() < bar->x());
        QVERIFY(f.editor()->rect().contains(menu->geometry()));

        QCOMPARE(menu->findChild<QLabel*>(QStringLiteral("debugAppValue")), nullptr);
        QCOMPARE(menu->findChild<QLabel*>(QStringLiteral("debugQtValue")), nullptr);
        QCOMPARE(menu->findChild<QLabel*>(QStringLiteral("debugPlatformValue")), nullptr);
        QCOMPARE(menu->findChild<QLabel*>(QStringLiteral("debugThemeValue")), nullptr);
        QCOMPARE(menu->findChild<QLabel*>(QStringLiteral("debugDocumentValue")), nullptr);

        auto* versionLabel = menu->findChild<QLabel*>(QStringLiteral("settingsVersionLabel"));
        auto* titleLabel = menu->findChild<QLabel*>(QStringLiteral("settingsTitleLabel"));
        QVERIFY(versionLabel != nullptr);
        QVERIFY(titleLabel != nullptr);
        QVERIFY(!versionLabel->text().isEmpty());
        QVERIFY(versionLabel->text().startsWith(QLatin1Char('v')));
        QVERIFY(versionLabel->font().pointSizeF() < titleLabel->font().pointSizeF());

        // Typing continues uninterrupted while the menu is open.
        f.placeCaret(2, 5);
        test::keyClick(f.editor(), Qt::Key_Return);
        test::keyClicks(f.editor(), QStringLiteral("four"));
        f.settle();
        QCOMPARE(f.editor()->document()->blockCount(), 4);

        // Escape closes the menu without stealing focus.
        test::keyClick(f.editor(), Qt::Key_Escape);
        f.settle();
        QVERIFY(!menu->isVisible());
        QVERIFY(!settingsBtn->isChecked());
        QVERIFY(f.editor()->hasFocus());

        // Clicking the settings button opens and then toggles the menu closed.
        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();
        QVERIFY(menu->isVisible());
        QVERIFY(settingsBtn->isChecked());

        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();
        QVERIFY(!menu->isVisible());
        QVERIFY(!settingsBtn->isChecked());
    }

    void settingsMenuListsAndSelectsThemesFromConfigFolder() {
        test::Fixture f;

        auto writeThemeFile = [&](const QString& relativeName, const QByteArray& content) {
            QFile file(f.dir.filePath(relativeName));
            QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
            QCOMPARE(file.write(content), content.size());
        };

        writeThemeFile(QStringLiteral("berry-crush.toml"),
                       "name = \"Berry Crush\"\n"
                       "base = \"dark\"\n\n"
                       "[colors]\n"
                       "background = \"#2a1220\"\n"
                       "accent = \"#f06292\"\n");
        writeThemeFile(QStringLiteral("ember.toml"),
                       "name = \"Ember\"\n"
                       "base = \"light\"\n\n"
                       "[colors]\n"
                       "background = \"#fdf3e7\"\n"
                       "accent = \"#d9480f\"\n");

        f.editor()->setThemeConfigDir(f.dir.path());

        auto* toolbar = f.editor()->floatingToolbar();
        auto* settingsBtn = toolbar->findChild<QToolButton*>(QStringLiteral("openSettingsButton"));
        auto* menu = f.editor()->settingsMenu();
        QVERIFY(settingsBtn != nullptr);
        QVERIFY(menu != nullptr);

        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();
        QVERIFY(menu->isVisible());

        auto* combo = menu->findChild<QComboBox*>(QStringLiteral("themeSelector"));
        QVERIFY2(combo != nullptr, "SettingsMenu must expose a themeSelector QComboBox");

        const int lightIdx = combo->findText(QStringLiteral("hitokoto Light"));
        const int darkIdx = combo->findText(QStringLiteral("hitokoto Dark"));
        const int berryIdx = combo->findText(QStringLiteral("Berry Crush"));
        const int emberIdx = combo->findText(QStringLiteral("Ember"));
        QVERIFY2(lightIdx >= 0, "hitokoto Light should appear in themeSelector");
        QVERIFY2(darkIdx >= 0, "hitokoto Dark should appear in themeSelector");
        QVERIFY2(berryIdx >= 0, "Berry Crush from config folder should appear in themeSelector");
        QVERIFY2(emberIdx >= 0, "Ember from config folder should appear in themeSelector");

        // Selecting Ember applies its light colors immediately.
        combo->setCurrentIndex(emberIdx);
        f.settle();
        QCOMPARE(QApplication::palette().color(QPalette::Base), QColor(QStringLiteral("#fdf3e7")));
        QCOMPARE(QApplication::palette().color(QPalette::Accent), QColor(QStringLiteral("#d9480f")));
        QCOMPARE(QString::fromStdString(f.editor()->currentTheme().name), QStringLiteral("Ember"));

        // Selecting Berry Crush switches to its dark colors immediately.
        combo->setCurrentIndex(berryIdx);
        f.settle();
        QCOMPARE(QApplication::palette().color(QPalette::Base), QColor(QStringLiteral("#2a1220")));
        QCOMPARE(QApplication::palette().color(QPalette::Accent), QColor(QStringLiteral("#f06292")));
        QCOMPARE(QString::fromStdString(f.editor()->currentTheme().name), QStringLiteral("Berry Crush"));

        // Selecting built-in Dark restores builtinTheme(ColorScheme::Dark).
        const auto builtinDark = hitokoto::core::builtinTheme(hitokoto::core::ColorScheme::Dark);
        combo->setCurrentIndex(darkIdx);
        f.settle();
        QCOMPARE(QApplication::palette().color(QPalette::Base),
                 QColor(builtinDark.background.r, builtinDark.background.g, builtinDark.background.b,
                        builtinDark.background.a));
        QCOMPARE(QApplication::palette().color(QPalette::Accent),
                 QColor(builtinDark.accent.r, builtinDark.accent.g, builtinDark.accent.b, builtinDark.accent.a));
        QCOMPARE(QString::fromStdString(f.editor()->currentTheme().name), QStringLiteral("hitokoto Dark"));
    }

    void selectedThemePersistsInConfigFileAcrossRestart() {
        test::Fixture f;

        auto writeThemeFile = [&](const QString& relativeName, const QByteArray& content) {
            QFile file(f.dir.filePath(relativeName));
            QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
            QCOMPARE(file.write(content), content.size());
        };

        writeThemeFile(QStringLiteral("berry-crush.toml"),
                       "name = \"Berry Crush\"\n"
                       "base = \"dark\"\n\n"
                       "[colors]\n"
                       "background = \"#2a1220\"\n"
                       "accent = \"#f06292\"\n");

        f.editor()->setThemeConfigDir(f.dir.path());

        auto* settingsBtn = toolbarButton(f.editor(), "openSettingsButton");
        auto* menu = f.editor()->settingsMenu();
        QVERIFY(settingsBtn != nullptr);
        QVERIFY(menu != nullptr);

        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();

        auto* combo = menu->findChild<QComboBox*>(QStringLiteral("themeSelector"));
        QVERIFY(combo != nullptr);
        const int berryIdx = combo->findText(QStringLiteral("Berry Crush"));
        QVERIFY(berryIdx >= 0);

        combo->setCurrentIndex(berryIdx);
        f.settle();

        const QString configPath = f.dir.filePath(QStringLiteral("config.toml"));
        QVERIFY2(QFile::exists(configPath), "Selecting a theme must save config.toml in the config directory");

        // config.toml itself must not appear as a selectable theme entry.
        f.editor()->refreshAvailableThemes();
        QCOMPARE(combo->findText(QStringLiteral("config")), -1);
        QCOMPARE(combo->findText(QStringLiteral("config.toml")), -1);

        // Simulate restarting the application with a fresh MainWindow pointing at the same config dir.
        ui::applyThemeToApplication(*qApp, core::builtinTheme(core::ColorScheme::Light));
        ui::MainWindow restarted(test::registry());
        restarted.resize(900, 700);
        restarted.show();
        QVERIFY(QTest::qWaitForWindowExposed(&restarted));
        restarted.editor()->setThemeConfigDir(f.dir.path());
        QCoreApplication::processEvents();

        QCOMPARE(QString::fromStdString(restarted.editor()->currentTheme().name), QStringLiteral("Berry Crush"));
        QCOMPARE(QApplication::palette().color(QPalette::Base), QColor(QStringLiteral("#2a1220")));
        QCOMPARE(QApplication::palette().color(QPalette::Accent), QColor(QStringLiteral("#f06292")));

        auto* restartedSettingsBtn = toolbarButton(restarted.editor(), "openSettingsButton");
        QVERIFY(restartedSettingsBtn != nullptr);
        QTest::mouseClick(restartedSettingsBtn, Qt::LeftButton);
        QCoreApplication::processEvents();
        auto* restartedCombo =
            restarted.editor()->settingsMenu()->findChild<QComboBox*>(QStringLiteral("themeSelector"));
        QVERIFY(restartedCombo != nullptr);
        QCOMPARE(restartedCombo->currentText(), QStringLiteral("Berry Crush"));

        // Switch to built-in Dark, restart again, and confirm hitokoto Dark is restored.
        const int darkIdx = restartedCombo->findText(QStringLiteral("hitokoto Dark"));
        QVERIFY(darkIdx >= 0);
        restartedCombo->setCurrentIndex(darkIdx);
        QCoreApplication::processEvents();

        ui::applyThemeToApplication(*qApp, core::builtinTheme(core::ColorScheme::Light));
        ui::MainWindow restartedDark(test::registry());
        restartedDark.resize(900, 700);
        restartedDark.show();
        QVERIFY(QTest::qWaitForWindowExposed(&restartedDark));
        restartedDark.editor()->setThemeConfigDir(f.dir.path());
        QCoreApplication::processEvents();

        const auto builtinDark = hitokoto::core::builtinTheme(hitokoto::core::ColorScheme::Dark);
        QCOMPARE(QString::fromStdString(restartedDark.editor()->currentTheme().name), QStringLiteral("hitokoto Dark"));
        QCOMPARE(QApplication::palette().color(QPalette::Base),
                 QColor(builtinDark.background.r, builtinDark.background.g, builtinDark.background.b,
                        builtinDark.background.a));
    }

    void editorOptionsInConfigFileLoadAndSurviveThemeSelection() {
        test::Fixture f;

        QVERIFY(f.dir.path().size() > 0);
        const QString themesSubdir = f.dir.filePath(QStringLiteral("themes"));
        QVERIFY(QDir().mkpath(themesSubdir));
        QFile themeFile(QDir(themesSubdir).filePath(QStringLiteral("ember.toml")));
        QVERIFY(themeFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        themeFile.write("name = \"Ember\"\nbase = \"light\"\n[colors]\nbackground = \"#fdf3e7\"\n");
        themeFile.close();

        const QString configPath = f.dir.filePath(QStringLiteral("config.toml"));
        QFile cfgFile(configPath);
        QVERIFY(cfgFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        cfgFile.write(
            "[appearance]\n"
            "theme = \"themes/ember.toml\"\n"
            "[editor]\n"
            "smooth-scrolling = false\n"
            "front-matter-as-properties = false\n"
            "report-blank-lines-added = true\n");
        cfgFile.close();

        f.editor()->setThemeConfigDir(f.dir.path());
        f.settle();

        QCOMPARE(QString::fromStdString(f.editor()->currentTheme().name), QStringLiteral("Ember"));
        QVERIFY(!f.editor()->isSmoothScrollingEnabled());
        QVERIFY(!f.editor()->frontMatterAsProperties());
        QVERIFY(!f.window->options().smoothScrolling);
        QVERIFY(!f.window->options().frontMatterAsProperties);
        QVERIFY(f.window->options().reportBlankLinesAdded);

        // Selecting a different theme must preserve the existing [editor] options in config.toml.
        auto* settingsBtn = toolbarButton(f.editor(), "openSettingsButton");
        QVERIFY(settingsBtn != nullptr);
        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();
        auto* combo = f.editor()->settingsMenu()->findChild<QComboBox*>(QStringLiteral("themeSelector"));
        QVERIFY(combo != nullptr);
        const int darkIdx = combo->findText(QStringLiteral("hitokoto Dark"));
        QVERIFY(darkIdx >= 0);
        combo->setCurrentIndex(darkIdx);
        f.settle();

        const core::SettingsLoadResult reloaded =
            core::loadSettingsFile(std::filesystem::path(configPath.toStdU16String()));
        QVERIFY(reloaded.diagnostics.empty());
        QCOMPARE(reloaded.settings.theme, std::string("dark"));
        QVERIFY(!reloaded.settings.smoothScrolling);
        QVERIFY(!reloaded.settings.frontMatterAsProperties);
        QVERIFY(reloaded.settings.reportBlankLinesAdded);
    }

    void themeDiscoveryRejectsNonRegularOrOversizedFiles() {
        test::Fixture f;
        f.editor()->setThemeConfigDir(f.dir.path());

        const QDir configDir(f.dir.path());
        QVERIFY(configDir.mkpath(QStringLiteral("themes")));
        const QDir themesDir(configDir.filePath(QStringLiteral("themes")));

        // Normal theme must be discovered and listed.
        QFile normalFile(themesDir.filePath(QStringLiteral("normal.toml")));
        QVERIFY(normalFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        normalFile.write("name = \"Normal Theme\"\nbase = \"light\"\n");
        normalFile.close();

        // Oversized theme (> 1 MiB) must be skipped.
        const QString oversizedPath = themesDir.filePath(QStringLiteral("oversized.toml"));
        QFile oversizedFile(oversizedPath);
        QVERIFY(oversizedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        oversizedFile.write("name = \"Oversized Theme\"\nbase = \"light\"\n");
        oversizedFile.close();
        std::filesystem::resize_file(std::filesystem::path(oversizedPath.toStdU16String()), 1024 * 1024 + 1024);

#if !defined(Q_OS_WIN)
        // Symlink to /dev/zero must be skipped and not block or exhaust memory.
        std::error_code ec;
        std::filesystem::create_symlink("/dev/zero", themesDir.filePath(QStringLiteral("x.toml")).toStdString(), ec);
        QVERIFY(!ec);
#endif

        auto* settingsBtn = toolbarButton(f.editor(), "openSettingsButton");
        QVERIFY(settingsBtn != nullptr);
        QTest::mouseClick(settingsBtn, Qt::LeftButton);
        f.settle();

        auto* menu = f.editor()->settingsMenu();
        QVERIFY(menu != nullptr);
        QVERIFY(menu->isVisible());

        auto* combo = menu->findChild<QComboBox*>(QStringLiteral("themeSelector"));
        QVERIFY(combo != nullptr);
        QVERIFY(combo->findText(QStringLiteral("Normal Theme")) >= 0);
        QCOMPARE(combo->findText(QStringLiteral("Oversized Theme")), -1);
#if !defined(Q_OS_WIN)
        QCOMPARE(combo->findText(QStringLiteral("x")), -1);
        QCOMPARE(combo->findText(QStringLiteral("x.toml")), -1);
#endif
    }

    void messageBoxUsesPlainTextFormat() {
        QTemporaryDir tempDir;
        ui::MainWindow window(test::registry());
        window.editor()->setThemeConfigDir(tempDir.path());

        window.showMessage(QStringLiteral("Theme error"),
                           QStringLiteral("Could not load \"<img src='x'>.toml\":\nsyntax error"));
        auto* box = window.findChild<QMessageBox*>();
        QVERIFY(box != nullptr);
        QCOMPARE(box->textFormat(), Qt::PlainText);
    }
};

QTEST_MAIN(tst_s038_settings_menu)
#include "tst_s038_settings_menu.moc"
