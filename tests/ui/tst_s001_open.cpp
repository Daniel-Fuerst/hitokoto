// SPDX-License-Identifier: GPL-3.0-or-later
// S001: opening a Markdown file shows its content.
#include "ui_test_support.hpp"

using namespace hitokoto;

class tst_s001_open : public QObject {
    Q_OBJECT
private slots:
    void showsExactTextAndTitle() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("notes.md"), "# Title\r\n\r\n- \xC3\xA4pfel\r\nlast");
        QVERIFY(f.open(path));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("# Title\n\n- \u00E4pfel\nlast"));
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("notes.md")));
        QCOMPARE(f.editor()->textCursor().position(), 0);
        QVERIFY(!f.editor()->document()->isModified());
        QVERIFY(!f.editor()->isReadOnly());
    }

    void invalidUtf8IsRefusedAndDocumentUnchanged() {
        test::Fixture f;
        f.editor()->setPlainText(QStringLiteral("keep me"));
        const QString path = f.writeFile(QStringLiteral("bad.md"), "ok\xFF\xFE");
        QVERIFY(!f.open(path));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("keep me"));
        QCOMPARE(f.messages.size(), 1);
        QVERIFY(f.messages.first().contains(QStringLiteral("invalid UTF-8")));
        QVERIFY(f.messages.first().contains(QStringLiteral("byte 2")));
        QCOMPARE(test::Fixture::readFile(path), QByteArray("ok\xFF\xFE"));
    }

    void missingFileIsReported() {
        test::Fixture f;
        QVERIFY(!f.open(f.dir.filePath(QStringLiteral("nope.md"))));
        QCOMPARE(f.messages.size(), 1);
        QVERIFY(f.messages.first().contains(QStringLiteral("does not exist")));
    }

    void editorIsReadOnlyWhileLoadingAndSavingIsRefused() {
        test::Fixture f;
        QTest::keyClicks(f.editor(), QStringLiteral("old"));
        f.editor()->document()->setModified(false);
        const QString path = f.writeFile(QStringLiteral("n.md"), "new content");
        QSignalSpy done(f.session(), &ui::DocumentSession::loadFinished);
        QSignalSpy refused(f.session(), &ui::DocumentSession::saveFailed);
        f.session()->load(path);
        QVERIFY(f.editor()->isReadOnly());
        QTest::keyClicks(f.editor(), QStringLiteral("lost?"));
        f.session()->save(f.dir.filePath(QStringLiteral("other.md")));
        QCOMPARE(refused.size(), 1);
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!f.editor()->isReadOnly());
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("new content"));
        QCOMPARE(f.session()->path(), path);
    }

    void failedLoadLeavesDocumentEditable() {
        test::Fixture f;
        QVERIFY(!f.open(f.dir.filePath(QStringLiteral("missing.md"))));
        QVERIFY(!f.editor()->isReadOnly());
    }

    void emptyFileOpens() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("empty.md"), "")));
        QVERIFY(f.editor()->document()->isEmpty());
    }
};

QTEST_MAIN(tst_s001_open)
#include "tst_s001_open.moc"
