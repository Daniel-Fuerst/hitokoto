// SPDX-License-Identifier: GPL-3.0-or-later
// S008: unsaved changes are never lost silently.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s008_unsaved : public QObject {
    Q_OBJECT
private slots:
    void modifiedMarkerFollowsEditsAndSaves() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("a.md"), "x");
        QVERIFY(f.open(path));
        QVERIFY(!f.window->isWindowModified());
        QTest::keyClick(f.editor(), Qt::Key_End);
        QTest::keyClicks(f.editor(), QStringLiteral("y"));
        QVERIFY(f.window->isWindowModified());
        QVERIFY(f.saveAndWait(path));
        QVERIFY(!f.window->isWindowModified());
    }

    void undoBackToSavedStateClearsMarker() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("a.md"), "x")));
        QTest::keyClicks(f.editor(), QStringLiteral("abc"));
        QVERIFY(f.window->isWindowModified());
        f.editor()->undo();
        QVERIFY(!f.window->isWindowModified());
    }

    void styleChangesDoNotCountAsModifications() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("a.md"), "# a\n- b\n## c\n")));
        for (int i = 0; i < 3; ++i)
            f.placeCaret(i);
        f.settle();
        QVERIFY(!f.editor()->document()->isModified());
    }

    void cancelKeepsWindowOpenAndText() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("draft"));
        int asked = 0;
        f.window->setAskSave([&](const QString&) {
            ++asked;
            return ui::SaveChoice::Cancel;
        });
        QVERIFY(!f.window->close());
        QCOMPARE(asked, 1);
        QVERIFY(f.window->isVisible());
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("draft"));
    }

    void discardCloses() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("draft"));
        f.window->setAskSave([](const QString&) { return ui::SaveChoice::Discard; });
        QVERIFY(f.window->close());
    }

    void saveChoiceWritesBeforeClosing() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("a.md"), "");
        QVERIFY(f.open(path));
        QTest::keyClicks(f.editor(), QStringLiteral("keep"));
        f.window->setAskSave([](const QString&) { return ui::SaveChoice::Save; });
        QVERIFY(f.window->close());
        QCOMPARE(test::Fixture::readFile(path), QByteArray("keep"));
    }

    void newDuringLoadKeepsUndoWorking() {
        test::Fixture f;
        QByteArray big;
        for (int i = 0; i < 200000; ++i)
            big += "- line of a large list\n";
        f.session()->load(f.writeFile(QStringLiteral("big.md"), big));
        QTRY_VERIFY(f.editor()->document()->blockCount() > 1);  // first chunk is in
        f.window->newFile();
        QVERIFY(!f.session()->isLoading());
        QVERIFY(f.editor()->document()->isEmpty());
        QTest::keyClicks(f.editor(), QStringLiteral("abc"));
        QVERIFY(f.editor()->document()->isUndoAvailable());
        f.editor()->undo();
        QVERIFY(f.editor()->document()->isEmpty());
        QTest::qWait(100);
        QVERIFY2(f.editor()->document()->isEmpty(), "no late chunks of the abandoned load");
    }

    void openingAnotherFileAsksFirst() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("draft"));
        int asked = 0;
        f.window->setAskSave([&](const QString&) {
            ++asked;
            return ui::SaveChoice::Cancel;
        });
        f.window->openFile(f.writeFile(QStringLiteral("other.md"), "other"));
        QCOMPARE(asked, 1);
        QTest::qWait(50);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("draft"));
    }
};

QTEST_MAIN(tst_s008_unsaved)
#include "tst_s008_unsaved.moc"
