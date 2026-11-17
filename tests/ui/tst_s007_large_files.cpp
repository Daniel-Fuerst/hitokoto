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

    // Rework of 2026-11-16: a line above the bounds (core::kInlineBounds) shows as text. Nothing of
    // it is concealed or styled, lines within the bounds beside it look as ever, and typing in it
    // and saving keep every byte (ADR 0002).
    void aLineAboveTheBoundsShowsAsTextAndIsSavedByteForByte() {
        test::Fixture f;
        const QByteArray within = "**bold** and `code`";
        QByteArray dense;  // more syntax characters than the bound allows
        for (std::size_t i = 0; i <= core::kInlineBounds.syntaxCharacters / 2; ++i) {
            dense += "*a* ";
        }
        const QByteArray longLine =  // longer than the bound allows
            "**bold** [link](https://example.com) " +
            QByteArray(static_cast<qsizetype>(core::kInlineBounds.lineLength), 'x');
        const QByteArray bytes = within + "\r\n\r\n" + dense + "\n\n" + longLine + "\n\nlast line\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), bytes)));
        f.settle();
        const QTextDocument* document = f.editor()->document();
        QCOMPARE(document->blockCount(), 8);
        f.placeCaret(6);
        f.settle();

        const QTextBlock first = document->findBlockByNumber(0);
        QVERIFY(test::isCollapsed(first, 0));
        QCOMPARE(test::formatAt(first, 2).fontWeight(), QFont::Bold);
        for (const int number : {2, 4}) {
            const QTextBlock block = document->findBlockByNumber(number);
            QVERIFY2(block.layout()->formats().isEmpty(), qPrintable(QString::number(number)));
            QVERIFY(!test::isCollapsed(block, 0));
        }

        // The caret on the line, a key typed in it: still text.
        f.placeCaret(4, 3);
        test::keyClick(f.editor(), 'y');
        f.settle();
        const QTextBlock typed = document->findBlockByNumber(4);
        QVERIFY(typed.text().startsWith(QStringLiteral("**by")));
        QVERIFY(typed.layout()->formats().isEmpty());
        f.placeCaret(6);
        f.settle();
        QVERIFY(document->findBlockByNumber(4).layout()->formats().isEmpty());

        QByteArray expected = bytes;
        expected.insert(within.size() + 4 + dense.size() + 2 + 3, 'y');
        const QString copy = f.dir.filePath(QStringLiteral("copy.md"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), expected);
    }
};

QTEST_MAIN(tst_s007_large_files)
#include "tst_s007_large_files.moc"
