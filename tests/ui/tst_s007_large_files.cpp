// SPDX-License-Identifier: GPL-3.0-or-later
// S007: large files load without blocking the UI and stay editable. Absolute budgets are
// measured by bench/hitokoto-bench on real machines; this test guards the mechanism.
#include "ui_test_support.hpp"

#include <QElapsedTimer>
#include <QTimer>

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
}  // namespace

class tst_s007_large_files : public QObject {
    Q_OBJECT
private slots:
    void tenMegabytesLoadInSlicesAndRoundTrip() {
        test::Fixture f;
        const QByteArray bytes = largeDocument(qsizetype{10} * 1024 * 1024);
        const QString path = f.writeFile(QStringLiteral("big.md"), bytes);

        // Watch the event loop: the longest gap between timer ticks while loading.
        qint64 longestGapMs = 0;
        QElapsedTimer gap;
        gap.start();
        QTimer ticker;
        ticker.setInterval(1);
        QObject::connect(&ticker, &QTimer::timeout, [&] {
            longestGapMs = std::max(longestGapMs, gap.elapsed());
            gap.restart();
        });
        QSignalSpy first(f.session(), &ui::DocumentSession::firstChunkShown);
        QElapsedTimer total;
        total.start();
        ticker.start();
        QVERIFY(f.open(path, 120000));
        const qint64 loadMs = total.elapsed();
        f.settle();
        ticker.stop();
        qInfo("10 MB: loaded in %lld ms, styled after %lld ms, longest UI gap %lld ms", loadMs,
              static_cast<long long>(total.elapsed()), longestGapMs);
        QCOMPARE(first.size(), 1);
        // Generous bound for slow CI machines; the budget itself is checked by the benchmark.
        QVERIFY2(longestGapMs < 1000, qPrintable(QString::number(longestGapMs)));

        // Typing in the middle stays responsive.
        f.placeCaret(f.editor()->document()->blockCount() / 2);
        QElapsedTimer typing;
        typing.start();
        test::keyClicks(f.editor(), QStringLiteral("## typed heading"));
        f.editor()->viewport()->repaint();
        qInfo("16 keys + repaint: %lld ms", static_cast<long long>(typing.elapsed()));

        QVERIFY(f.saveAndWait(f.dir.filePath(QStringLiteral("copy.md"))));
        QCOMPARE(test::Fixture::readFile(f.dir.filePath(QStringLiteral("copy.md"))).size(), bytes.size() + 16);
    }
};

QTEST_MAIN(tst_s007_large_files)
#include "tst_s007_large_files.moc"
