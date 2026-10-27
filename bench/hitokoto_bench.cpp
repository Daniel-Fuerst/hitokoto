// SPDX-License-Identifier: GPL-3.0-or-later
// S006/S007: end-to-end performance harness.
//
//   hitokoto-bench [--runs N] [--output results.json] [--baseline baseline.json]
//                  [--tolerance 0.2] [--fail-on-budget] [--app path/to/hitokoto]
//
// Measures cold start (separate processes), opening 1 MB and 10 MB files, key-to-paint
// latency (typing, Enter, moving across headings), opening and typing in large tables (S018) and
// saving (longest UI stall and total, median of --runs saves). Compares against a baseline (regression gate for CI) and
// reports the brief's budgets (section 4); hard budgets fail the run on any machine.
#include "document_session.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "key_input.hpp"
#include "main_window.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <map>
#include <string_view>
#include <vector>

using namespace hitokoto;

namespace {

struct Stats {
    double median = 0;
    double p95 = 0;
    double p99 = 0;  ///< the maximum after dropping the worst 1 % (2 of 200 samples)
    double max = 0;
    double over16 = 0;  ///< how many samples exceeded the 16 ms typing budget
};

Stats stats(std::vector<double> v) {
    if (v.empty())
        return {};
    std::sort(v.begin(), v.end());
    const auto at = [&](double q) { return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))]; };
    const auto over16 = static_cast<double>(std::count_if(v.begin(), v.end(), [](double ms) { return ms > 16.0; }));
    return {at(0.5), at(0.95), at(0.99), v.back(), over16};
}

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

/// A paragraph, then a GFM table of `rows` body rows with three columns, then a paragraph (S018).
QByteArray tableDocument(int rows) {
    QByteArray out = "Text before the table.\n\n| Name | Value | Note |\n| :--- | ---: | --- |\n";
    for (int i = 1; i <= rows; ++i) {
        out += "| row " + QByteArray::number(i) + " | " + QByteArray::number(i * 7) + " | *note* with `code` |\n";
    }
    out += "\nText after the table.\n";
    return out;
}

QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes) {
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qPrintable(path));
    f.write(bytes);
    return path;
}

std::vector<double> measureStartup(const QString& app, const QString& file, int runs) {
    std::vector<double> results;
    for (int i = 0; i < runs; ++i) {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("HITOKOTO_PROBE_FIRST_FRAME"), QStringLiteral("1"));
        p.setProcessEnvironment(env);
        QElapsedTimer t;
        t.start();
        p.start(app, file.isEmpty() ? QStringList{} : QStringList{file});
        QByteArray err;
        double wall = -1;
        while (t.elapsed() < 20000) {
            const bool gotData = p.waitForReadyRead(50);
            err += p.readAllStandardError();
            if (err.contains("hitokoto-probe first-frame-ms=")) {
                wall = static_cast<double>(t.nsecsElapsed()) / 1e6;
                break;
            }
            if (!gotData && p.state() == QProcess::NotRunning)
                break;
        }
        if (!p.waitForFinished(5000))
            p.kill();
        if (wall < 0) {
            std::fprintf(stderr, "startup run %d failed: %s\n", i, err.constData());
            continue;
        }
        results.push_back(wall);
    }
    return results;
}

struct OpenTiming {
    double firstChunkMs = 0;
    double loadedMs = 0;
    double styledMs = 0;
};

OpenTiming measureOpen(ui::MainWindow& w, const QString& path) {
    QSignalSpy first(w.session(), &ui::DocumentSession::firstChunkShown);
    QSignalSpy done(w.session(), &ui::DocumentSession::loadFinished);
    QElapsedTimer t;
    t.start();
    OpenTiming r;
    w.openFile(path);
    (void)QTest::qWaitFor([&] { return !first.isEmpty(); }, 120000);
    w.editor()->viewport()->repaint();
    r.firstChunkMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
    (void)QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000);
    r.loadedMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
    (void)QTest::qWaitFor([&] { return w.editor()->styler().isComplete(); }, 120000);
    r.styledMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
    return r;
}

// Opening is measured this many times, each from an empty document, and the medians compared: one
// open of 10 MB on shared runners swung by more than 25 % without a code change (first chunk 51
// and 89 ms in two runs, decoding alone 12 and 24 ms; S018).
constexpr int kOpenRuns = 5;

OpenTiming medianOpen(ui::MainWindow& w, const QString& path) {
    std::vector<double> first;
    std::vector<double> loaded;
    std::vector<double> styled;
    for (int i = 0; i < kOpenRuns; ++i) {
        w.session()->newDocument();
        const OpenTiming t = measureOpen(w, path);
        first.push_back(t.firstChunkMs);
        loaded.push_back(t.loadedMs);
        styled.push_back(t.styledMs);
    }
    return {stats(first).median, stats(loaded).median, stats(styled).median};
}

std::vector<double> measureTyping(ui::MainWindow& w, int keys) {
    ui::EditorWidget* e = w.editor();
    QTextCursor c(e->document()->findBlockByNumber(e->document()->blockCount() / 2));
    e->setTextCursor(c);
    std::vector<double> samples;
    const QString text = QStringLiteral("## typed heading line ");
    for (int i = 0; i < keys; ++i) {
        QElapsedTimer t;
        t.start();
        switch (i % 25) {
            case 22:
                test::keyClick(e, Qt::Key_Return);
                break;
            case 23:
                test::keyClick(e, Qt::Key_Up);
                break;
            case 24:
                test::keyClick(e, Qt::Key_Down);
                break;
            default:
                test::keyClick(e, text.at(i % 22).toLatin1());
                break;
        }
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    return samples;
}

/// S018: from openFile() until the table view shows the table.
double measureTableOpen(ui::MainWindow& w, const QString& path) {
    QElapsedTimer t;
    t.start();
    (void)measureOpen(w, path);
    (void)QTest::qWaitFor([&] { return w.editor()->tableViewAt(2) != nullptr; }, 120000);
    w.editor()->viewport()->repaint();
    return static_cast<double>(t.nsecsElapsed()) / 1e6;
}

/// Median of kOpenRuns table opens, each from an empty document.
double medianTableOpen(ui::MainWindow& w, const QString& path) {
    std::vector<double> samples;
    for (int i = 0; i < kOpenRuns; ++i) {
        w.session()->newDocument();
        samples.push_back(measureTableOpen(w, path));
    }
    return stats(samples).median;
}

/// S018: key-to-paint in a table cell. Puts the caret into a cell in the middle of the table and
/// types; every keystroke writes the cell into its Markdown line, so each sample includes the
/// document change, re-styling and re-fitting the view; every 25th key is Tab to the next cell.
std::vector<double> measureTableTyping(ui::MainWindow& w, int keys) {
    ui::EditorWidget* e = w.editor();
    QTableView* view = e->tableViewAt(2);
    if (view == nullptr) {
        std::fprintf(stderr, "no table view to type in\n");
        return {};
    }
    const QModelIndex start = view->model()->index(view->model()->rowCount() / 2, 2);
    view->scrollTo(start);
    view->setCurrentIndex(start);
    view->edit(start);
    QCoreApplication::processEvents();
    std::vector<double> samples;
    const QString text = QStringLiteral("typed cell text ");
    for (int i = 0; i < keys; ++i) {
        QWidget* target = QApplication::focusWidget();
        if (target == nullptr) {
            target = view;
        }
        QElapsedTimer t;
        t.start();
        if (i % 25 == 24) {
            test::keyClick(target, Qt::Key_Tab);
        } else {
            test::keyClick(target, text.at(i % text.size()).toLatin1());
        }
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    test::keyClick(QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : view, Qt::Key_Return);
    return samples;
}

struct SaveTiming {
    double uiBlockedMs = 0;  ///< longest stall of the UI thread while saving
    double totalMs = 0;      ///< until the file is on disk (saveFinished)
};

/// One save: the longest gap between 1 ms timer ticks until saveFinished (S017: the snapshot is
/// taken in slices, so the stall is no longer the save() call itself), and the total time. The
/// wait runs in an event loop without sleeps; QTest::qWaitFor() would add its own 10 ms gaps.
SaveTiming measureSave(ui::MainWindow& w, const QString& path) {
    QEventLoop untilSaved;
    QObject::connect(w.session(), &ui::DocumentSession::saveFinished, &untilSaved, &QEventLoop::quit);
    QObject::connect(w.session(), &ui::DocumentSession::saveFailed, &untilSaved, &QEventLoop::quit);
    QTimer::singleShot(120000, &untilSaved, &QEventLoop::quit);
    qint64 longestGapNs = 0;
    QElapsedTimer gap;
    QTimer ticker;
    ticker.setInterval(1);
    QObject::connect(&ticker, &QTimer::timeout, [&] {
        longestGapNs = std::max(longestGapNs, gap.nsecsElapsed());
        gap.restart();
    });
    QElapsedTimer total;
    total.start();
    gap.start();
    ticker.start();
    w.session()->save(path);
    untilSaved.exec();
    ticker.stop();
    SaveTiming r;
    r.uiBlockedMs = static_cast<double>(longestGapNs) / 1e6;
    r.totalMs = static_cast<double>(total.nsecsElapsed()) / 1e6;
    return r;
}

/// `runs` saves of the open document; the median of each value.
SaveTiming measureSaves(ui::MainWindow& w, const QString& path, int runs) {
    // Measure saving alone: let the styler and layout finish the work the typing left behind.
    (void)QTest::qWaitFor([&] { return w.editor()->styler().isComplete(); }, 120000);
    QCoreApplication::processEvents();
    std::vector<double> blocked;
    std::vector<double> total;
    for (int i = 0; i < std::max(runs, 1); ++i) {
        const auto one = measureSave(w, path);
        blocked.push_back(one.uiBlockedMs);
        total.push_back(one.totalMs);
    }
    return {stats(blocked).median, stats(total).median};
}

struct Budget {
    const char* key;
    double limit;
    const char* text;
    bool hard = false;  ///< fails the run on every machine, not only with --fail-on-budget
};

// Compared with the baseline but never failing the baseline comparison: the time until the saved
// file is on disk includes fsync before the atomic rename (ADR 0002) and swings with the shared
// CI runners' disks (58.6 -> 78.1 ms between two runs without a code change). The hard budget
// below catches real regressions instead (S017).
// Extremes of few samples are one scheduler hiccup away from failing on a shared runner: the
// maximum of 200 key-to-paint samples (1.47 -> 23.32 ms in one run) and the p95 of 10 cold
// starts (31.6 -> 127.2 ms on a Windows runner, the median unchanged). Medians, and p95/p99 of
// the 200 typing samples, are compared; extremes are reported, and where the brief names an
// absolute limit they have a hard budget below. The counts of samples over 16 ms are information.
constexpr std::string_view kReportOnly[] = {
    "save_10mb_total_ms",     "typing_small_max_ms",   "typing_10mb_max_ms",          "s018_table_typing_max_ms",
    "typing_small_over_16ms", "typing_10mb_over_16ms", "s018_table_typing_over_16ms", "startup_p95_ms"};

// Brief, section 4. Most budgets are meaningful on reference hardware only (S011) and fail the run
// with --fail-on-budget; the hard ones hold on any machine, CI runners included (S017): a save
// must never stall the UI, 10 MB must be on disk within a second, and no key event may take
// longer than 50 ms (S007).
const Budget kBudgets[] = {
    {"startup_median_ms", 1000, "cold start < 1000 ms"},
    {"open_1mb_loaded_ms", 500, "open 1 MB < 500 ms"},
    {"open_10mb_first_chunk_ms", 3000, "10 MB usable < 3 s"},
    {"typing_small_p95_ms", 16, "key to screen <= 16 ms"},
    {"typing_10mb_p95_ms", 16, "key to screen <= 16 ms (10 MB)"},
    {"typing_small_p99_ms", 16, "key to screen <= 16 ms, p99"},
    {"typing_10mb_p99_ms", 16, "key to screen <= 16 ms, p99 (10 MB)"},
    {"typing_small_max_ms", 50, "no key event above 50 ms (hard)", true},
    {"typing_10mb_max_ms", 50, "no key event above 50 ms, 10 MB (hard)", true},
    {"s018_table_open_ms", 500, "open a 1000-row table < 500 ms"},
    {"s018_table_typing_p95_ms", 16, "key to screen <= 16 ms (200-row table)"},
    {"s018_table_typing_p99_ms", 16, "key to screen <= 16 ms, p99 (200-row table)"},
    {"s018_table_typing_max_ms", 50, "no key event above 50 ms, table (hard)", true},
    {"save_10mb_ui_blocked_ms", 16, "saving never stalls the UI (hard)", true},
    {"save_10mb_total_ms", 1000, "10 MB on disk within 1 s (hard)", true},
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption runsOpt(QStringLiteral("runs"), QStringLiteral("Startup runs and save runs."),
                                     QStringLiteral("n"), QStringLiteral("10"));
    const QCommandLineOption outOpt(QStringLiteral("output"), QStringLiteral("Write results as JSON."),
                                    QStringLiteral("file"));
    const QCommandLineOption baseOpt(QStringLiteral("baseline"), QStringLiteral("Compare with baseline JSON."),
                                     QStringLiteral("file"));
    const QCommandLineOption tolOpt(QStringLiteral("tolerance"), QStringLiteral("Allowed slowdown (0.2 = 20 %)."),
                                    QStringLiteral("x"), QStringLiteral("0.2"));
    const QCommandLineOption budgetOpt(QStringLiteral("fail-on-budget"),
                                       QStringLiteral("Exit 1 if a budget is missed."));
    const QCommandLineOption appOpt(QStringLiteral("app"), QStringLiteral("hitokoto executable."),
                                    QStringLiteral("path"), QStringLiteral(HITOKOTO_APP_PATH));
    parser.addOptions({runsOpt, outOpt, baseOpt, tolOpt, budgetOpt, appOpt});
    parser.process(app);

    QTemporaryDir dir;
    const QString small = writeFile(dir, QStringLiteral("small.md"), largeDocument(qsizetype{10} * 1024));
    const QString oneMb = writeFile(dir, QStringLiteral("1mb.md"), largeDocument(qsizetype{1024} * 1024));
    const QString tenMb = writeFile(dir, QStringLiteral("10mb.md"), largeDocument(qsizetype{10} * 1024 * 1024));
    const QString table200 = writeFile(dir, QStringLiteral("table200.md"), tableDocument(200));
    const QString table1000 = writeFile(dir, QStringLiteral("table1000.md"), tableDocument(1000));

    std::map<std::string, double> r;
    const auto startup = stats(measureStartup(parser.value(appOpt), small, parser.value(runsOpt).toInt()));
    r["startup_median_ms"] = startup.median;
    r["startup_p95_ms"] = startup.p95;

    static const core::ElementRegistry registry = core::ElementRegistry::withBuiltins();
    ui::applyThemeToApplication(app, core::builtinTheme(core::ColorScheme::Light));
    ui::MainWindow w(registry);
    w.applyTheme(core::builtinTheme(core::ColorScheme::Light));
    w.resize(1000, 800);
    w.show();
    (void)QTest::qWaitForWindowExposed(&w);

    (void)measureOpen(w, small);
    const auto typingSmall = stats(measureTyping(w, 200));
    r["typing_small_median_ms"] = typingSmall.median;
    r["typing_small_p95_ms"] = typingSmall.p95;
    r["typing_small_p99_ms"] = typingSmall.p99;
    r["typing_small_max_ms"] = typingSmall.max;
    r["typing_small_over_16ms"] = typingSmall.over16;
    w.session()->newDocument();

    r["s018_table_open_ms"] = medianTableOpen(w, table1000);
    w.session()->newDocument();
    (void)measureTableOpen(w, table200);
    const auto typingTable = stats(measureTableTyping(w, 200));
    r["s018_table_typing_median_ms"] = typingTable.median;
    r["s018_table_typing_p95_ms"] = typingTable.p95;
    r["s018_table_typing_p99_ms"] = typingTable.p99;
    r["s018_table_typing_max_ms"] = typingTable.max;
    r["s018_table_typing_over_16ms"] = typingTable.over16;
    w.editor()->document()->setModified(false);
    w.session()->newDocument();

    const auto open1 = medianOpen(w, oneMb);
    r["open_1mb_loaded_ms"] = open1.loadedMs;
    w.session()->newDocument();

    const auto open10 = medianOpen(w, tenMb);
    r["open_10mb_first_chunk_ms"] = open10.firstChunkMs;
    r["open_10mb_loaded_ms"] = open10.loadedMs;
    r["open_10mb_styled_ms"] = open10.styledMs;
    const auto typing10 = stats(measureTyping(w, 200));
    r["typing_10mb_median_ms"] = typing10.median;
    r["typing_10mb_p95_ms"] = typing10.p95;
    r["typing_10mb_p99_ms"] = typing10.p99;
    r["typing_10mb_max_ms"] = typing10.max;
    r["typing_10mb_over_16ms"] = typing10.over16;
    const auto save10 = measureSaves(w, dir.filePath(QStringLiteral("10mb-copy.md")), parser.value(runsOpt).toInt());
    r["save_10mb_ui_blocked_ms"] = save10.uiBlockedMs;
    r["save_10mb_total_ms"] = save10.totalMs;
    w.editor()->document()->setModified(false);

    QJsonObject json;
    for (const auto& [k, v] : r) {
        json.insert(QString::fromStdString(k), v);
        std::printf("%-28s %10.2f\n", k.c_str(), v);
    }
    if (parser.isSet(outOpt)) {
        QFile out(parser.value(outOpt));
        if (out.open(QIODevice::WriteOnly))
            out.write(QJsonDocument(json).toJson());
    }

    int exitCode = 0;
    std::printf("\nBudgets (brief, section 4; hard ones hold on any machine, the rest on reference hardware):\n");
    for (const auto& b : kBudgets) {
        const double v = r[b.key];
        const bool ok = v <= b.limit;
        std::printf("  %-4s %-34s %8.2f / %.0f ms\n", ok ? "ok" : "OVER", b.text, v, b.limit);
        if (!ok && (b.hard || parser.isSet(budgetOpt)))
            exitCode = 1;
    }

    if (parser.isSet(baseOpt)) {
        QFile f(parser.value(baseOpt));
        if (!f.open(QIODevice::ReadOnly)) {
            std::printf("\nNo baseline at %s; nothing compared.\n", qPrintable(parser.value(baseOpt)));
        } else {
            const QJsonObject base = QJsonDocument::fromJson(f.readAll()).object();
            const double tol = parser.value(tolOpt).toDouble();
            std::printf("\nRegression check (tolerance %.0f %%):\n", tol * 100);
            for (auto it = base.begin(); it != base.end(); ++it) {
                const auto found = r.find(it.key().toStdString());
                if (found == r.end())
                    continue;
                const double was = it.value().toDouble();
                const double now = found->second;
                // Ignore differences below 2 ms: timer and scheduler noise.
                const bool regressed = now > was * (1 + tol) && now - was > 2.0;
                const bool reportOnly = std::find(std::begin(kReportOnly), std::end(kReportOnly),
                                                  it.key().toStdString()) != std::end(kReportOnly);
                const char* status = !regressed ? "ok" : reportOnly ? "info" : "SLOW";
                std::printf("  %-4s %-28s %8.2f -> %8.2f\n", status, qPrintable(it.key()), was, now);
                if (regressed && !reportOnly)
                    exitCode = 1;
            }
        }
    }
    return exitCode;
}
