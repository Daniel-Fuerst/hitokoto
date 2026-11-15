// SPDX-License-Identifier: GPL-3.0-or-later
// S038: subtle top menu bar without a dividing line.
#include "menu_bar.hpp"
#include "qt_bridge.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QMenu>

using namespace hitokoto;

class tst_s038_subtle_menu_bar : public QObject {
    Q_OBJECT
private slots:
    void menuBarSitsAtTopLeftWithCoreMenusAndNoDividingLine() {
        test::Fixture f;
        ui::MenuBar* bar = f.window->topMenuBar();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar, qobject_cast<ui::MenuBar*>(f.window->menuBar()));
        QVERIFY(bar->isVisible());
        QCOMPARE(bar->y(), 0);

        const QList<QAction*> actions = bar->actions();
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions[0]->text().remove(u'&'), QStringLiteral("File"));
        QCOMPARE(actions[1]->text().remove(u'&'), QStringLiteral("Edit"));
        QCOMPARE(actions[2]->text().remove(u'&'), QStringLiteral("View"));
        for (QAction* a : actions) {
            QVERIFY(a->menu() != nullptr);
            QVERIFY(a->isVisible());
        }

        const QRect fileRect = bar->actionGeometry(actions[0]);
        const QRect editRect = bar->actionGeometry(actions[1]);
        const QRect viewRect = bar->actionGeometry(actions[2]);
        QVERIFY(fileRect.left() < editRect.left());
        QVERIFY(editRect.left() < viewRect.left());
        QVERIFY(fileRect.left() >= 6);
        QVERIFY(fileRect.top() >= 6);
        QCOMPARE(editRect.left() - fileRect.right() - 1, 4);

        const core::Theme theme = core::builtinTheme(core::ColorScheme::Light);
        const QImage img = bar->grab().toImage();
        const int bottomY = bar->height() - 1;
        QCOMPARE(img.pixelColor(bar->width() / 2, bottomY), ui::toQColor(theme.background));
        QCOMPARE(img.pixelColor(fileRect.center().x(), bottomY), ui::toQColor(theme.background));
        QVERIFY(img.pixelColor(bar->width() / 2, bottomY) != ui::toQColor(theme.border));
    }

    void idleItemsAreSubtleAndHoverOrActiveShowsRoundedPill() {
        test::Fixture f;
        ui::MenuBar* bar = f.window->topMenuBar();
        QVERIFY(bar != nullptr);
        const QList<QAction*> actions = bar->actions();
        QCOMPARE(actions.size(), 3);
        QAction* fileAction = actions[0];
        const QRect fileRect = bar->actionGeometry(fileAction);
        const QRect editRect = bar->actionGeometry(actions[1]);
        const core::MenuBarStyle style = core::menuBarStyle(core::builtinTheme(core::ColorScheme::Light));

        const QImage idleImg = bar->grab().toImage();
        const QPoint filePillProbe(fileRect.left() + 3, fileRect.center().y());
        const QPoint editPillProbe(editRect.left() + 3, editRect.center().y());
        QCOMPARE(idleImg.pixelColor(filePillProbe), ui::toQColor(style.background));
        QCOMPARE(idleImg.pixelColor(editPillProbe), ui::toQColor(style.background));

        // Idle text is muted (lighter than full-contrast active/hover text on light theme) and present.
        int darkestLightness = 255;
        for (int y = fileRect.top(); y <= fileRect.bottom(); ++y) {
            for (int x = fileRect.left(); x <= fileRect.right(); ++x) {
                darkestLightness = std::min(darkestLightness, idleImg.pixelColor(x, y).lightness());
            }
        }
        QVERIFY(darkestLightness < ui::toQColor(style.background).lightness());
        QVERIFY(darkestLightness >= ui::toQColor(style.text).lightness() - 5);

        // Hover over File draws its rounded pill while Edit stays idle.
        QTest::mouseMove(bar, fileRect.center());
        QCoreApplication::processEvents();
        const QImage hoverImg = bar->grab().toImage();
        QCOMPARE(hoverImg.pixelColor(filePillProbe), ui::toQColor(style.hoverBackground));
        QCOMPARE(hoverImg.pixelColor(editPillProbe), ui::toQColor(style.background));

        // Open menu uses activeBackground pill.
        fileAction->menu()->popup(bar->mapToGlobal(fileRect.bottomLeft()));
        QCoreApplication::processEvents();
        const QImage activeImg = bar->grab().toImage();
        QCOMPARE(activeImg.pixelColor(filePillProbe), ui::toQColor(style.activeBackground));
        fileAction->menu()->close();
        QCoreApplication::processEvents();
    }

    void themedMenuBarUsesEveryToken() {
        test::Fixture f;
        constexpr core::Color background{0x18, 0x19, 0x1c, 0xff};
        constexpr core::Color border{0xd4, 0x42, 0x55, 0xff};
        constexpr core::Color text{0x90, 0x95, 0xa0, 0xff};
        constexpr core::Color hoverText{0xff, 0xff, 0xff, 0xff};
        constexpr core::Color activeText{0xff, 0xe0, 0x80, 0xff};
        constexpr core::Color hoverBackground{0x2c, 0x2f, 0x36, 0xff};
        constexpr core::Color activeBackground{0x3e, 0x44, 0x52, 0xff};

        auto theme = core::builtinTheme(core::ColorScheme::Dark);
        theme.menuBar.background = background;
        theme.menuBar.border = border;
        theme.menuBar.borderWidth = 2;
        theme.menuBar.padding = 8;
        theme.menuBar.spacing = 6;
        theme.menuBar.text = text;
        theme.menuBar.hoverText = hoverText;
        theme.menuBar.activeText = activeText;
        theme.menuBar.hoverBackground = hoverBackground;
        theme.menuBar.activeBackground = activeBackground;
        theme.menuBar.itemRadius = 10;
        theme.menuBar.itemPaddingHorizontal = 12;
        theme.menuBar.itemPaddingVertical = 6;
        theme.menuBar.fontFamily = "Menu Test Family";
        theme.menuBar.fontScale = 1.25;
        f.window->applyTheme(theme);
        f.settle();

        ui::MenuBar* bar = f.window->topMenuBar();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->menuBarStyle(), core::menuBarStyle(theme));
        QCOMPARE(bar->font().families().first(), QStringLiteral("Menu Test Family"));

        const QList<QAction*> actions = bar->actions();
        const QRect fileRect = bar->actionGeometry(actions[0]);
        const QRect editRect = bar->actionGeometry(actions[1]);
        QVERIFY(fileRect.left() >= 8);
        QVERIFY(fileRect.top() >= 8);
        QCOMPARE(editRect.left() - fileRect.right() - 1, 6);

        const QImage img = bar->grab().toImage();
        QCOMPARE(img.pixelColor(bar->width() / 2, 2), ui::toQColor(background));
        QCOMPARE(img.pixelColor(bar->width() / 2, bar->height() - 1), ui::toQColor(border));
    }
};

QTEST_MAIN(tst_s038_subtle_menu_bar)
#include "tst_s038_subtle_menu_bar.moc"
