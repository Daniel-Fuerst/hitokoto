// SPDX-License-Identifier: GPL-3.0-or-later
// S017: saving takes its snapshot in slices, so the UI never stalls and the file is always a
// state the document had. Absolute budgets are measured by bench/hitokoto-bench; this test
// guards the mechanism.
#include "ui_test_support.hpp"

#include <QElapsedTimer>
#include <QEventLoop>
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
/// `run()` must keep the event loop running without sleeping: QTest::qWaitFor() sleeps 10 ms
/// between its passes, which would show up as stalls.
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

/// The longest UI stall of one save to `path`, in an event loop without sleeps.
qint64 saveStallMs(test::Fixture& f, const QString& path) {
    QEventLoop untilSaved;
    QObject::connect(f.session(), &ui::DocumentSession::saveFinished, &untilSaved, &QEventLoop::quit);
    QTimer::singleShot(120000, &untilSaved, &QEventLoop::quit);
    return longestStallMs([&] {
        f.session()->save(path);
        untilSaved.exec();
    });
}

/// The smallest of three saves' longest stalls: the VM running the tests preempts the thread
/// for 20 ms and more now and then (seen inside one QTextCursor::selectedText() call), which no
/// mechanism can avoid; a stall the mechanism causes shows up in every run.
qint64 bestOfThreeSaveStallMs(test::Fixture& f, const QString& path) {
    qint64 best = saveStallMs(f, path);
    for (int i = 0; i < 2; ++i) {
        best = std::min(best, saveStallMs(f, path));
    }
    return best;
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
        // stay well below it. On a fast machine the one-shot copy itself is short, so the bound
        // never drops below the 16 ms budget (one slice is a few milliseconds). The budget itself
        // is checked by the benchmark.
        QElapsedTimer copy;
        copy.start();
        const QString whole = f.editor()->document()->toRawText();
        const qint64 copyMs = copy.elapsed();
        QVERIFY(whole.size() > 0);

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString path = f.dir.filePath(QStringLiteral("copy.md"));
        const qint64 stall = bestOfThreeSaveStallMs(f, path);
        QCOMPARE(done.size(), 3);
        qInfo("10 MB: one-shot copy %lld ms, longest UI stall while saving %lld ms (best of 3)", copyMs, stall);
        QVERIFY2(stall < std::max<qint64>(copyMs / 2, 16), qPrintable(QString::number(stall)));
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
        QVERIFY(QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000));
        QTest::qWait(200);
        QCOMPARE(done.size(), 1);
        QVERIFY2(!QFile::exists(first), "the dropped request wrote a file");
        QCOMPARE(test::Fixture::readFile(second), bytes);
        QCOMPARE(f.session()->path(), second);
    }

    // Review (docs/reviews/017-save-without-stall-code.md, O1): the styler's markContentsDirty()
    // emits contentsChange for every block it styles; those are not edits and must not set the
    // copy back (eight of them would make the rest copy in one go: the stall S017 removes).
    void stylerWorkDuringTheCopyIsNotAnEdit() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        const QString path = f.dir.filePath(QStringLiteral("copy.md"));
        quint64 setBacks = 0;
        qint64 stall = 0;
        for (int i = 0; i < 3; ++i) {
            f.editor()->styler().invalidateAll();  // restyles every block lazily, as after a theme change
            const qint64 s = saveStallMs(f, path);
            stall = i == 0 ? s : std::min(stall, s);
            setBacks += f.session()->copySetBacks();
        }
        // The stall here is the styler's own catch-up (docs/performance.md, "Known gaps"), not the
        // copy's: reported, not asserted. savingTenMegabytesDoesNotStallTheEventLoop bounds the copy.
        qInfo("styler working: %llu set-backs, longest UI stall %lld ms (best of 3)", setBacks, stall);
        QCOMPARE(setBacks, quint64{0});
        QCOMPARE(test::Fixture::readFile(path), bytes);
    }

    // Review O3: a save whose copy is still running is not lost when the window closes or
    // another document replaces this one; before S017 the snapshot was taken at once, so the
    // write always happened.
    void pendingSaveIsWrittenWhenTheWindowCloses() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();

        const QString saveAs = f.dir.filePath(QStringLiteral("save-as.md"));
        f.session()->save(saveAs);  // Save As without edits: the window is not modified
        QCoreApplication::processEvents();
        QVERIFY(!f.window->isWindowModified());
        QVERIFY(f.window->close());
        QCOMPARE(test::Fixture::readFile(saveAs), bytes);
    }

    void pendingSaveIsWrittenWhenAnotherFileIsOpened() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        QVERIFY(f.open(f.writeFile(QStringLiteral("big.md"), bytes), 120000));
        f.settle();
        const QString other = f.writeFile(QStringLiteral("other.md"), "other\n");

        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        const QString saveAs = f.dir.filePath(QStringLiteral("save-as.md"));
        f.session()->save(saveAs);
        QCoreApplication::processEvents();
        QVERIFY(f.open(other));
        QVERIFY(QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000));
        QCOMPARE(test::Fixture::readFile(saveAs), bytes);
        QCOMPARE(f.session()->path(), other);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("other\n"));
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
        // The pending copy was completed and written first (review O3: never dropped), then the
        // synchronous save; the file holds the later state.
        QCOMPARE(done.size(), 2);
        QCOMPARE(QString::fromUtf8(test::Fixture::readFile(path)), f.editor()->document()->toPlainText());
    }
};

QTEST_MAIN(tst_s017_save_without_stall)
#include "tst_s017_save_without_stall.moc"
