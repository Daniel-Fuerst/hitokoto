// SPDX-License-Identifier: GPL-3.0-or-later
// S043: toggle switches in the settings flyout and the switch "Smooth scrolling" in the section Feel.
#include "floating_toolbar.hpp"
#include "hitokoto/core/settings.hpp"
#include "hitokoto/core/theme.hpp"
#include "settings_menu.hpp"
#include "smooth_scroller.hpp"
#include "toggle_switch.hpp"
#include "ui_test_support.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QScrollBar>
#include <QToolButton>

#include <filesystem>

using namespace hitokoto;

namespace {

QString configPath(const test::Fixture& f) {
    return f.dir.filePath(QStringLiteral("config.toml"));
}

void writeConfig(const test::Fixture& f, const QByteArray& bytes) {
    QFile file(configPath(f));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qFatal("cannot write config.toml");
    }
    file.write(bytes);
}

/// Points the editor at the fixture's folder (never the user's own configuration) and opens the flyout.
void openSettings(test::Fixture& f) {
    f.editor()->setThemeConfigDir(f.dir.path());
    if (!f.editor()->settingsMenu()->isVisible()) {
        auto* button = f.editor()->floatingToolbar()->findChild<QToolButton*>(QStringLiteral("openSettingsButton"));
        QTest::mouseClick(button, Qt::LeftButton);
    }
    f.settle();
}

ui::ToggleSwitch* smoothSwitch(const test::Fixture& f) {
    return f.editor()->settingsMenu()->findChild<ui::ToggleSwitch*>(QStringLiteral("smoothScrollingSwitch"));
}

bool loadedSmoothScrolling(const test::Fixture& f) {
    return core::loadSettingsFile(std::filesystem::path(configPath(f).toStdU16String())).settings.smoothScrolling;
}

}  // namespace

class tst_s043_smooth_scrolling_toggle : public QObject {
    Q_OBJECT
private slots:
    void feelSectionShowsTheSwitchInItsSavedState() {
        test::Fixture f;
        writeConfig(f, "[editor]\nsmooth-scrolling = false\n");
        f.editor()->setFocus();
        openSettings(f);

        auto* feel = f.editor()->settingsMenu()->findChild<QLabel*>(QStringLiteral("feelLabel"));
        QVERIFY(feel != nullptr);
        QCOMPARE(feel->text(), QStringLiteral("Feel"));
        auto* appearance = f.editor()->settingsMenu()->findChild<QLabel*>(QStringLiteral("appearanceLabel"));
        QVERIFY(appearance != nullptr);
        QVERIFY(feel->geometry().top() > appearance->geometry().bottom());

        ui::ToggleSwitch* sw = smoothSwitch(f);
        QVERIFY(sw != nullptr);
        QVERIFY(sw->isVisible());
        QVERIFY(sw->isCheckable());
        QVERIFY(!sw->isChecked());
        QCOMPARE(sw->accessibleName(), QStringLiteral("Smooth scrolling"));
        QCOMPARE(sw->focusPolicy(), Qt::NoFocus);
        QCOMPARE(sw->knobPosition(), 0.0);

        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(f.editor()->hasFocus());

        // Without the key the switch shows the default of S027: on.
        test::Fixture g;
        writeConfig(g, "[appearance]\ntheme = \"light\"\n");
        openSettings(g);
        QVERIFY(smoothSwitch(g)->isChecked());
        QCOMPARE(smoothSwitch(g)->knobPosition(), 1.0);
    }

    void flippingTheSwitchTurnsSmoothScrollingOnAndOff() {
        test::Fixture f;
        openSettings(f);
        ui::ToggleSwitch* sw = smoothSwitch(f);
        QVERIFY(sw != nullptr);
        QVERIFY(sw->isChecked());
        QVERIFY(f.editor()->isSmoothScrollingEnabled());

        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(!sw->isChecked());
        QVERIFY(!f.editor()->isSmoothScrollingEnabled());
        QVERIFY(!f.window->options().smoothScrolling);

        // The editor now scrolls in plain steps, without animation.
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), QByteArray("line\n").repeated(400))));
        f.settle();
        QWheelEvent wheel(QPointF(50, 50), f.editor()->viewport()->mapToGlobal(QPointF(50, 50)), QPoint(),
                          QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(f.editor()->viewport(), &wheel);
        QVERIFY(!f.editor()->smoothScroller()->isRunning());
        QVERIFY(f.editor()->verticalScrollBar()->value() > 0);

        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(sw->isChecked());
        QVERIFY(f.editor()->isSmoothScrollingEnabled());
        QVERIFY(f.window->options().smoothScrolling);
    }

    void theKnobMovesWithAnEaseInOutAnimation() {
        test::Fixture f;
        writeConfig(f, "[editor]\nsmooth-scrolling = false\n");
        openSettings(f);
        ui::ToggleSwitch* sw = smoothSwitch(f);
        QVERIFY(sw != nullptr);
        QCOMPARE(sw->knobPosition(), 0.0);

        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(sw->isAnimating());
        QVERIFY(sw->knobPosition() < 1.0);  // no jump

        // Ease in: early on the knob is behind where a linear movement would put it.
        QVERIFY(QTest::qWaitFor([&] { return sw->linearProgress() >= 0.2; }, 2000));
        const double early = sw->linearProgress();
        if (early < 0.45) {
            QVERIFY(sw->knobPosition() < early);
        }
        // Ease out: near the end it is ahead of a linear movement.
        QVERIFY(QTest::qWaitFor([&] { return sw->linearProgress() >= 0.7 || !sw->isAnimating(); }, 2000));
        if (sw->isAnimating() && sw->linearProgress() < 1.0) {
            QVERIFY(sw->knobPosition() > sw->linearProgress());
        }
        QVERIFY(QTest::qWaitFor([&] { return !sw->isAnimating(); }, 2000 * test::kTimeScale));
        QCOMPARE(sw->knobPosition(), 1.0);
        QCOMPARE(sw->trackColor(), sw->switchStyle().checkedTrack);

        // A second click while moving turns the knob back from where it is.
        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(QTest::qWaitFor([&] { return sw->knobPosition() < 0.9; }, 2000));
        const double turning = sw->knobPosition();
        QVERIFY(turning > 0.0);
        QTest::mouseClick(sw, Qt::LeftButton);
        QVERIFY(sw->isChecked());
        QVERIFY(qAbs(sw->knobPosition() - turning) < 0.2);  // continues from there, no jump to an end
        QVERIFY(QTest::qWaitFor([&] { return !sw->isAnimating(); }, 2000 * test::kTimeScale));
        QCOMPARE(sw->knobPosition(), 1.0);
    }

    void theChoiceIsSavedAndRestored() {
        test::Fixture f;
        const QByteArray original =
            "\xEF\xBB\xBF# mine\r\n[appearance]\r\ntheme = \"light\"\r\n\r\n[editor]\r\n"
            "front-matter-as-properties = true # keep\r\nsmooth-scrolling = true\r\n";
        writeConfig(f, original);
        openSettings(f);
        QTest::mouseClick(smoothSwitch(f), Qt::LeftButton);
        QCOMPARE(test::Fixture::readFile(configPath(f)),
                 QByteArray("\xEF\xBB\xBF# mine\r\n[appearance]\r\ntheme = \"light\"\r\n\r\n[editor]\r\n"
                            "front-matter-as-properties = true # keep\r\nsmooth-scrolling = false\r\n"));
        QVERIFY(f.messages.isEmpty());

        // A new window with the same folder restores it.
        test::Fixture g;
        g.editor()->setThemeConfigDir(f.dir.path());
        QVERIFY(!g.editor()->isSmoothScrollingEnabled());
        QVERIFY(!g.window->options().smoothScrolling);
        auto* button = g.editor()->floatingToolbar()->findChild<QToolButton*>(QStringLiteral("openSettingsButton"));
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(!smoothSwitch(g)->isChecked());

        // Without a file one is written.
        test::Fixture h;
        openSettings(h);
        QVERIFY(!QFileInfo::exists(configPath(h)));
        QTest::mouseClick(smoothSwitch(h), Qt::LeftButton);
        QVERIFY(QFileInfo::exists(configPath(h)));
        QVERIFY(!loadedSmoothScrolling(h));
    }

    void aChoiceThatCannotBeSavedIsReported() {
        test::Fixture f;
        const QByteArray original = "editor.smooth-scrolling = true\n";
        writeConfig(f, original);
        openSettings(f);
        f.messages.clear();  // the start reports the unknown dotted key (S039)
        QTest::mouseClick(smoothSwitch(f), Qt::LeftButton);
        QVERIFY(!f.editor()->isSmoothScrollingEnabled());
        QCOMPARE(test::Fixture::readFile(configPath(f)), original);
        QCOMPARE(f.messages.size(), 1);
        QVERIFY(f.messages.first().startsWith(QStringLiteral("Setting not saved: ")));
        QVERIFY(f.messages.first().contains(QDir::toNativeSeparators(configPath(f))) ||
                f.messages.first().contains(configPath(f)));

        // A folder named config.toml.
        test::Fixture g;
        QVERIFY(QDir().mkpath(configPath(g)));
        openSettings(g);
        g.messages.clear();
        QTest::mouseClick(smoothSwitch(g), Qt::LeftButton);
        QVERIFY(!g.editor()->isSmoothScrollingEnabled());
        QCOMPARE(g.messages.size(), 1);
        QVERIFY(g.messages.first().startsWith(QStringLiteral("Setting not saved: ")));
    }

    void switchFollowsItsThemeTokens() {
        test::Fixture f;
        openSettings(f);
        const auto parsed = core::parseTheme(
            "[switch]\ntrack = \"#112233\"\nchecked-track = \"#445566\"\nknob = \"#778899\"\n"
            "checked-knob = \"#aabbcc\"\nborder = \"#ddeeff\"\nborder-width = 1\nwidth = 50\nheight = 26\n"
            "radius = 4\nknob-margin = 3\n",
            core::builtinTheme(core::ColorScheme::Light));
        if (!parsed.theme) {
            QFAIL("the [switch] tokens do not parse");
        }
        f.window->applyTheme(*parsed.theme);
        ui::ToggleSwitch* sw = smoothSwitch(f);
        QCOMPARE(sw->switchStyle(), core::switchStyle(*parsed.theme));
        QCOMPARE(sw->size(), QSize(50, 26));
        QCOMPARE(sw->trackColor(), sw->switchStyle().checkedTrack);  // on by default

        // What it paints: the checked track colour beside the knob, the checked knob colour in its middle.
        const QImage image = sw->grab().toImage();
        QCOMPARE(image.pixelColor(QPoint(8, 13) * image.devicePixelRatio()).name(), QStringLiteral("#445566"));
        QCOMPARE(image.pixelColor(QPoint(50 - 3 - 10, 13) * image.devicePixelRatio()).name(),
                 QStringLiteral("#aabbcc"));
    }
};

QTEST_MAIN(tst_s043_smooth_scrolling_toggle)
#include "tst_s043_smooth_scrolling_toggle.moc"
