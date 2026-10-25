// SPDX-License-Identifier: GPL-3.0-or-later
// S016: the running application and its window expose the hitokoto icon.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s016_app_icon : public QObject {
    Q_OBJECT
private slots:
    void setsApplicationAndWindowIcons() {
        test::Fixture f;
        QVERIFY(!QApplication::windowIcon().isNull());
        QVERIFY(!f.window->windowIcon().isNull());
        QVERIFY(!f.window->windowIcon().pixmap(64, 64).isNull());
    }
};

QTEST_MAIN(tst_s016_app_icon)
#include "tst_s016_app_icon.moc"
