// SPDX-License-Identifier: GPL-3.0-or-later
// S017: saving takes its snapshot in slices, so the UI never stalls and the file is always a
// state the document had. Absolute budgets are measured by bench/hitokoto-bench; this test
// guards the mechanism.
#include "ui_test_support.hpp"

#include <QElapsedTimer>
#include <QTimer>

#include <algorithm>

using namespace hitokoto;

namespace {
QByteArray largeDocument(qsizetype target) {
    QByteArray out;
    out.reserve(target + 512);
    int i = 0;
    while (out.size() < target) {
        ++i;
        out += QByteArray(1 + i % 3, '#') + " Section " + QByteArray::number(i) + "\n\n";
        out +=
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore.\n\n";
        out += "- first item of the list\n- second item of the list\n\n";
    }
    return out;
}

/// The longest gap between 1 ms timer ticks while `run()` executes (the UI thread's longest stall).
template <typename F>
qint64 longestStallMs(F&& run) {
    qint64 longest = 0;
    QElapsedTimer gap;
    gap.start();
    QTimer ticker;
    ticker.setInterval(1);
    QObject::connect(&ticker, &QTimer::timeout, [&] {
        longest = std::max(longest, gap.elapsed());
        gap.restart();
    });
    ticker.start();
    run();
    ticker.stop();
    return longest;
}

void insertAtBlock(ui::EditorWidget* editor, int blockNumber, const QString& text) {
    QTextCursor c(editor->document()->findBlockByNumber(blockNumber));
    c.insertText(text);
}
}  // namespace

class tst_s017_save_without_stall : public QObject {
    Q_OBJECT
private slots:
    void savingTenMegabytesDoesNotStallTheEventLoop() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        // What one synchronous copy of the text costs on this machine; the sliced snapshot must
        // stay well below it. Slices are 4 ms or 1 MB of text, whichever ends first; on a fast
        // machine the one-shot copy itself is short, so a floor of 12 ms keeps the bound above
        // one slice plus timer noise. The 16 ms budget is checked by the benchmark.
        QElapsedTimer copy;
        copy.start();
        const QString whole = f.editor()->document()->toRawText();
        const qint64 copyMs = copy.elapsed();
        QVERIFY(whole.size() > 0);

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString path = f.dir.filePath(QStringLiteral("copy.md"));
        const qint64 stall = longestStallMs([&] {
            f.session()->save(path);
            QVERIFY(QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000));
        });
        qInfo("10 MB: one-shot copy %lld ms, longest UI stall while saving %lld ms", copyMs, stall);
        QVERIFY2(stall < std::max<qint64>(copyMs / 2, 12), qPrintable(QString::number(stall)));
        QCOMPARE(test::Fixture::readFile(path), bytes);
    }

    void fileIsAStateTheDocumentHad() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();
        auto* doc = f.editor()->document();
        const int blocks = doc->blockCount();

        // Edit before, inside and after the copied part while the copy runs, with and without
        // changing the number of blocks; stop when the snapshot is complete and remember the text
        // the document had at that moment.
        QString expected;
        bool snapshotTaken = false;
        connect(f.session(), &ui::DocumentSession::snapshotTaken, this, [&] {
            snapshotTaken = true;
            expected = doc->toPlainText();
        });
        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString path = f.dir.filePath(QStringLiteral("copy.md"));
        f.session()->save(path);
        int edits = 0;
        while (!snapshotTaken && edits < 10000) {
            const int at = (edits * 7919) % blocks;  // all over the document
            switch (edits % 4) {
                case 0:
                    insertAtBlock(f.editor(), at, QStringLiteral("x"));
                    break;
                case 1:
                    insertAtBlock(f.editor(), at, QStringLiteral("new line\n"));
                    break;
                case 2: {
                    QTextCursor c(doc->findBlockByNumber(at));
                    c.movePosition(QTextCursor::EndOfBlock);
                    c.deleteChar();  // joins with the next block
                    break;
                }
                default:
                    insertAtBlock(f.editor(), 0, QStringLiteral("top\n"));
                    break;
            }
            ++edits;
            QCoreApplication::processEvents();
        }
        QVERIFY2(snapshotTaken, "the save never took its snapshot");
        QVERIFY2(edits > 3, "the copy finished before the test could edit; make the document larger");
        QVERIFY(QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000));
        qInfo("%d edits while the snapshot was taken", edits);
        QCOMPARE(QString::fromUtf8(test::Fixture::readFile(path)), expected);
        QVERIFY2(!doc->isModified(), "no edit after the snapshot: the document is saved");
    }

    void savingFinishesDespiteContinuousInserts() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        bool snapshotTaken = false;
        connect(f.session(), &ui::DocumentSession::snapshotTaken, this, [&] { snapshotTaken = true; });
        f.session()->save(f.dir.filePath(QStringLiteral("copy.md")));
        QElapsedTimer t;
        t.start();
        while (!snapshotTaken && t.elapsed() < 10000) {
            insertAtBlock(f.editor(), 0, QStringLiteral("top\n"));  // sets the copy back every time
            QCoreApplication::processEvents();
        }
        QVERIFY2(snapshotTaken, "continuous inserts at the top starved the save");
        QVERIFY2(t.elapsed() < 5000, qPrintable(QString::number(t.elapsed())));
        QVERIFY(QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000));
    }

    void newerSaveRequestWins() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString first = f.dir.filePath(QStringLiteral("first.md"));
        const QString second = f.dir.filePath(QStringLiteral("second.md"));
        f.session()->save(first);
        QCoreApplication::processEvents();  // the copy is under way
        f.session()->save(second);          // Save As while the first copy runs
        QVERIFY(QTest::qWaitFor([&] { return done.size() >= 1; }, 120000));
        QTest::qWait(200);
        QCOMPARE(done.size(), 1);
        QVERIFY2(!QFile::exists(first), "the dropped request wrote a file");
        QCOMPARE(test::Fixture::readFile(second), bytes);
        QCOMPARE(f.session()->path(), second);
    }

    void quitSavesSynchronouslyWhileACopyRuns() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString path = f.dir.filePath(QStringLiteral("copy.md"));
        f.session()->save(path);
        QCoreApplication::processEvents();
        insertAtBlock(f.editor(), 0, QStringLiteral("late\n"));
        QVERIFY(f.session()->saveNow(path));  // what quitting does
        QCOMPARE(QString::fromUtf8(test::Fixture::readFile(path)), f.editor()->document()->toPlainText());
        QVERIFY(!f.editor()->document()->isModified());
        QTest::qWait(200);
        QCOMPARE(done.size(), 1);
        QCOMPARE(QString::fromUtf8(test::Fixture::readFile(path)), f.editor()->document()->toPlainText());
    }
};

QTEST_MAIN(tst_s017_save_without_stall)
#include "tst_s017_save_without_stall.moc"
