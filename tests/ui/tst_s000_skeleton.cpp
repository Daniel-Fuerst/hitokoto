// SPDX-License-Identifier: GPL-3.0-or-later
// S000: the app starts with an empty, focused editor window.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s000_skeleton : public QObject {
    Q_OBJECT
private slots:
    void startsWithEmptyFocusedEditor() {
        test::Fixture f;
        QVERIFY(f.window->isVisible());
        QTRY_VERIFY(f.editor()->hasFocus());
        QVERIFY(f.editor()->document()->isEmpty());
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Untitled")));
    }
};

QTEST_MAIN(tst_s000_skeleton)
#include "tst_s000_skeleton.moc"
