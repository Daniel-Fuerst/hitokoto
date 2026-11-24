// SPDX-License-Identifier: GPL-3.0-or-later
// S048: the section headings of the settings flyout, in the accent colour and followed by a line.
#include "floating_toolbar.hpp"
#include "hitokoto/core/theme.hpp"
#include "qt_bridge.hpp"
#include "settings_menu.hpp"
#include "ui_test_support.hpp"

#include <QComboBox>
#include <QLabel>
#include <QToolButton>

#include <cmath>

using namespace hitokoto;

namespace {

void openSettings(test::Fixture& f) {
    f.editor()->setThemeConfigDir(f.dir.path());
    if (!f.editor()->settingsMenu()->isVisible()) {
        auto* button = f.editor()->floatingToolbar()->findChild<QToolButton*>(QStringLiteral("openSettingsButton"));
        QTest::mouseClick(button, Qt::LeftButton);
    }
    f.settle();
}

QLabel* label(const test::Fixture& f, const char* name) {
    return f.editor()->settingsMenu()->findChild<QLabel*>(QString::fromLatin1(name));
}

QWidget* rule(const test::Fixture& f, const char* name) {
    return f.editor()->settingsMenu()->findChild<QWidget*>(QString::fromLatin1(name));
}

QPoint inMenu(const QWidget* w, const QPoint& p) {
    return w->mapTo(w->window(), p);
}

/// The rows of the rule's middle column that show `color`, in logical pixels: first row and count.
std::pair<int, int> ruleRows(QWidget* ruleWidget, const QColor& color) {
    const QImage image = ruleWidget->grab().toImage();
    const qreal dpr = image.devicePixelRatio();
    const int x = static_cast<int>(std::lround(ruleWidget->width() / 2.0 * dpr));
    int first = -1;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        if (image.pixelColor(x, y).name() == color.name()) {
            if (first < 0) {
                first = y;
            }
            ++count;
        }
    }
    return {static_cast<int>(std::lround(first / dpr)), static_cast<int>(std::lround(count / dpr))};
}

void expectHeadingRow(const test::Fixture& f, const char* labelName, const char* ruleName,
                      const core::SettingsSectionStyle& style) {
    QLabel* heading = label(f, labelName);
    QVERIFY(heading != nullptr);
    QCOMPARE(heading->palette().color(QPalette::WindowText), ui::toQColor(style.text));
    QCOMPARE(heading->font().weight(), static_cast<QFont::Weight>(style.weight));

    QWidget* line = rule(f, ruleName);
    QVERIFY(line != nullptr);
    QVERIFY(line->isVisible());
    // To the right of the heading, by the gap, and as far right as the controls of the rows reach.
    const QPoint headingRight = inMenu(heading, QPoint(heading->width(), 0));
    const QPoint lineLeft = inMenu(line, QPoint(0, 0));
    QVERIFY(std::abs(lineLeft.x() - headingRight.x() - static_cast<int>(std::lround(style.ruleGap))) <= 1);
    auto* combo = f.editor()->settingsMenu()->themeSelector();
    QCOMPARE(inMenu(line, QPoint(line->width(), 0)).x(), inMenu(combo, QPoint(combo->width(), 0)).x());

    // The line is drawn in its colour and width, centred vertically on the heading.
    const auto [top, rows] = ruleRows(line, ui::toQColor(style.rule));
    QCOMPARE(rows, static_cast<int>(std::lround(style.ruleWidth)));
    const double lineMiddle = inMenu(line, QPoint(0, top)).y() + rows / 2.0;
    const double headingMiddle = inMenu(heading, QPoint(0, 0)).y() + heading->height() / 2.0;
    QVERIFY2(std::abs(lineMiddle - headingMiddle) <= 1.0,
             qPrintable(QStringLiteral("line at %1, heading at %2").arg(lineMiddle).arg(headingMiddle)));
}

}  // namespace

class tst_s048_settings_section_headings : public QObject {
    Q_OBJECT
private slots:
    void headingsStandApartFromRowLabels() {
        test::Fixture f;
        openSettings(f);
        const core::Theme& theme = core::builtinTheme(core::ColorScheme::Light);
        const core::SettingsSectionStyle style = core::settingsSectionStyle(theme);
        QCOMPARE(style.text, theme.accent);
        expectHeadingRow(f, "appearanceLabel", "appearanceRule", style);
        expectHeadingRow(f, "feelLabel", "feelRule", style);

        // The row labels keep the muted colour and the normal weight.
        QLabel* row = label(f, "smoothScrollingLabel");
        QVERIFY(row != nullptr);
        QCOMPARE(row->palette().color(QPalette::WindowText), ui::toQColor(theme.textMuted));
        QVERIFY(row->font().weight() < label(f, "feelLabel")->font().weight());
    }

    void headingsFollowTheirThemeTokens() {
        test::Fixture f;
        openSettings(f);
        const auto parsed = core::parseTheme(
            "[settings-section]\ntext = \"#112233\"\nweight = 800\nrule = \"#445566\"\nrule-width = 3\n"
            "rule-gap = 14\n",
            core::builtinTheme(core::ColorScheme::Light));
        if (!parsed.theme) {
            QFAIL("the [settings-section] tokens do not parse");
        }
        f.window->applyTheme(*parsed.theme);
        f.settle();
        const core::SettingsSectionStyle style = core::settingsSectionStyle(*parsed.theme);
        QCOMPARE(style.text.toHex(), std::string("#112233"));
        expectHeadingRow(f, "appearanceLabel", "appearanceRule", style);
        expectHeadingRow(f, "feelLabel", "feelRule", style);
    }
};

QTEST_MAIN(tst_s048_settings_section_headings)
#include "tst_s048_settings_section_headings.moc"
