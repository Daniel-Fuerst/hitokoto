// SPDX-License-Identifier: GPL-3.0-or-later
// S006/S007: end-to-end performance harness.
//
//   hitokoto-bench [--runs N] [--output results.json] [--baseline baseline.json]
//                  [--tolerance 0.2] [--fail-on-budget] [--app path/to/hitokoto]
//
// Measures cold start (separate processes), opening 1 MB and 10 MB files, key-to-paint
// latency (typing, Enter, moving across headings) and saving. Compares against a baseline
// (regression gate for CI) and reports the brief's budgets (section 4).
#include "document_session.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "main_window.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

using namespace hitokoto;

namespace {

struct Stats {
    double median = 0;
    double p95 = 0;
    double max = 0;
};

Stats stats(std::vector<double> v) {
    if (v.empty())
        return {};
    std::sort(v.begin(), v.end());
    const auto at = [&](double q) { return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))]; };
    return {at(0.5), at(0.95), v.back()};
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
                QTest::keyClick(e, Qt::Key_Return);
                break;
            case 23:
                QTest::keyClick(e, Qt::Key_Up);
                break;
            case 24:
                QTest::keyClick(e, Qt::Key_Down);
                break;
            default:
                QTest::keyClick(e, text.at(i % 22).toLatin1());
                break;
        }
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    return samples;
}

struct SaveTiming {
    double uiBlockedMs = 0;
    double totalMs = 0;
};

SaveTiming measureSave(ui::MainWindow& w, const QString& path) {
    QSignalSpy done(w.session(), &ui::DocumentSession::saveFinished);
    QElapsedTimer t;
    t.start();
    w.session()->save(path);
    SaveTiming r;
    r.uiBlockedMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
    (void)QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000);
    r.totalMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
    return r;
}

struct Budget {
    const char* key;
    double limit;
    const char* text;
};

// Brief, section 4.
const Budget kBudgets[] = {
    {"startup_median_ms", 1000, "cold start < 1000 ms"},
    {"open_1mb_loaded_ms", 500, "open 1 MB < 500 ms"},
    {"open_10mb_first_chunk_ms", 3000, "10 MB usable < 3 s"},
    {"typing_small_p95_ms", 16, "key to screen <= 16 ms"},
    {"typing_10mb_p95_ms", 16, "key to screen <= 16 ms (10 MB)"},
    {"save_10mb_ui_blocked_ms", 16, "saving never blocks the UI"},
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption runsOpt(QStringLiteral("runs"), QStringLiteral("Startup runs."), QStringLiteral("n"),
                                     QStringLiteral("10"));
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
    r["typing_small_max_ms"] = typingSmall.max;
    w.session()->newDocument();

    const auto open1 = measureOpen(w, oneMb);
    r["open_1mb_loaded_ms"] = open1.loadedMs;
    w.session()->newDocument();

    const auto open10 = measureOpen(w, tenMb);
    r["open_10mb_first_chunk_ms"] = open10.firstChunkMs;
    r["open_10mb_loaded_ms"] = open10.loadedMs;
    r["open_10mb_styled_ms"] = open10.styledMs;
    const auto typing10 = stats(measureTyping(w, 200));
    r["typing_10mb_median_ms"] = typing10.median;
    r["typing_10mb_p95_ms"] = typing10.p95;
    r["typing_10mb_max_ms"] = typing10.max;
    const auto save10 = measureSave(w, dir.filePath(QStringLiteral("10mb-copy.md")));
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
    std::printf("\nBudgets (brief, section 4; meaningful on reference hardware only):\n");
    for (const auto& b : kBudgets) {
        const double v = r[b.key];
        const bool ok = v <= b.limit;
        std::printf("  %-4s %-34s %8.2f / %.0f ms\n", ok ? "ok" : "OVER", b.text, v, b.limit);
        if (!ok && parser.isSet(budgetOpt))
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
                std::printf("  %-4s %-28s %8.2f -> %8.2f\n", regressed ? "SLOW" : "ok", qPrintable(it.key()), was, now);
                if (regressed)
                    exitCode = 1;
            }
        }
    }
    return exitCode;
}
