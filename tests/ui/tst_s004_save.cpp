// SPDX-License-Identifier: GPL-3.0-or-later
// S004: save and save-as write the text byte for byte, through the real editor widget.
#include "ui_test_support.hpp"

#include <random>

using namespace hitokoto;

class tst_s004_save : public QObject {
    Q_OBJECT
private slots:
    void unchangedFilesRoundTripThroughTheEditor_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("lf") << QByteArray("# Title\n\n- a\n- b\n");
        QTest::newRow("crlf+bom") << QByteArray("\xEF\xBB\xBF# T\r\n- x\r\n");
        QTest::newRow("no final newline") << QByteArray("a\nb");
        QTest::newRow("cr") << QByteArray("a\rb\r");
        QTest::newRow("mixed") << QByteArray("a\r\nb\nc\rd\r\n");
        QTest::newRow("nbsp, tab, U+2028, NUL") << QByteArray(
            "a\xC2\xA0"
            "b\tc\xE2\x80\xA8"
            "d\x00"
            "e\n",
            13);
        QTest::newRow("only newlines") << QByteArray("\n\n\n");
        QTest::newRow("empty") << QByteArray();
    }
    void unchangedFilesRoundTripThroughTheEditor() {
        QFETCH(QByteArray, bytes);
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("doc.md"), bytes);
        QVERIFY(f.open(path));
        f.settle();
        // Move the caret around so that markers get revealed and concealed again.
        for (int i = 0; i < f.editor()->document()->blockCount(); ++i)
            f.placeCaret(i);
        const QString copy = f.dir.filePath(QStringLiteral("copy.md"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), bytes);
    }

    void editedFileKeepsBomAndCrlfWithoutRenderingArtifacts() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("doc.md"), "\xEF\xBB\xBF# T\r\n- x\r\n");
        QVERIFY(f.open(path));
        f.placeCaret(2);
        QTest::keyClicks(f.editor(), QStringLiteral("## New"));
        QTest::keyClick(f.editor(), Qt::Key_Return);
        QTest::keyClicks(f.editor(), QStringLiteral("- y"));
        f.placeCaret(0);
        QVERIFY(f.saveAndWait(path));
        QCOMPARE(test::Fixture::readFile(path), QByteArray("\xEF\xBB\xBF# T\r\n- x\r\n## New\r\n- y"));
        QVERIFY(!f.editor()->document()->isModified());
    }

    // ADR 0002: mixed files keep each line's ending. Only endings that differ from the majority are
    // stored per line; every other line saves with the majority ending anyway.
    void mixedEndingsStoreOnlyMinorityEndings() {
        test::Fixture f;
        const QByteArray bytes("a\r\nb\nc\nd\re\nf\r\n\ng\n");
        const QString path = f.writeFile(QStringLiteral("doc.md"), bytes);
        QVERIFY(f.open(path));
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{0, 3, 5}));

        const QString copy = f.dir.filePath(QStringLiteral("copy.md"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), bytes);

        // A new line takes the majority ending; the minority line it was split from keeps its own.
        f.placeCaret(0, 1);
        QTest::keyClick(f.editor(), Qt::Key_Return);
        QTest::keyClicks(f.editor(), QStringLiteral("z"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), QByteArray("a\r\nz\nb\nc\nd\re\nf\r\n\ng\n"));
    }

    void saveAsChangesPathAndTitle() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("hello"));
        const QString target = f.dir.filePath(QStringLiteral("new.md"));
        f.window->setPathPicker([&](bool forSaving, const QString&) { return forSaving ? target : QString(); });
        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        QVERIFY(f.window->saveAs());
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(test::Fixture::readFile(target), QByteArray("hello"));
        QCOMPARE(f.session()->path(), target);
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("new.md")));
        // Later saves go to the new path.
        QTest::keyClicks(f.editor(), QStringLiteral("!"));
        QVERIFY(f.window->save());
        QTRY_COMPARE(done.size(), 2);
        QCOMPARE(test::Fixture::readFile(target), QByteArray("hello!"));
    }

    void typingDuringSaveKeepsModifiedFlag() {
        test::Fixture f;
        const QString path = f.dir.filePath(QStringLiteral("t.md"));
        QTest::keyClicks(f.editor(), QStringLiteral("one"));
        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        f.session()->save(path);
        QTest::keyClicks(f.editor(), QStringLiteral(" two"));  // before the result arrives
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(f.editor()->document()->isModified());
    }

    // Property: random valid Markdown-ish text survives open -> edit-free save unchanged.
    void propertyRandomFilesRoundTrip() {
        std::mt19937 rng(1024);
        const char* pieces[] = {"# ",       "## ", "- ",   "* ", "+ ",       "text",         " ",
                                "\t",       "\n",  "\r\n", "\r", "\xC3\xA4", "\xE4\xB8\x80", "\xF0\x9F\x98\x80",
                                "\xC2\xA0", "#",   "-",    "---"};
        test::Fixture f;
        for (int iteration = 0; iteration < 60; ++iteration) {
            QByteArray bytes = (rng() % 3 == 0) ? QByteArray("\xEF\xBB\xBF") : QByteArray();
            const int n = static_cast<int>(rng() % 40);
            for (int k = 0; k < n; ++k)
                bytes += pieces[rng() % std::size(pieces)];
            const QString path = f.writeFile(QStringLiteral("p%1.md").arg(iteration), bytes);
            QVERIFY(f.open(path));
            const QString out = f.dir.filePath(QStringLiteral("p%1.out").arg(iteration));
            QVERIFY(f.saveAndWait(out));
            QCOMPARE(test::Fixture::readFile(out), bytes);
        }
    }
};

QTEST_MAIN(tst_s004_save)
#include "tst_s004_save.moc"
