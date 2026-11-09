// SPDX-License-Identifier: GPL-3.0-or-later
// S029: hitokoto-bench-gate decides from files whether the Linux benchmark job is green (exit 0) or
// fails (1); 2 means wrong arguments. The decisions are in the gate library (standard library only);
// this file reads files and JSON and writes the report, also when a round has no complete results.
//
//   hitokoto-bench-gate --kind pr --base-rounds a.json,b.json --pr-rounds a.json,b.json
//                       [--class <runner class>] [--changed-paths paths.txt] [--qt-changed]
//                       [--image-changed] [--accepted] [--report report.md]
//   hitokoto-bench-gate --kind main --results e2e-bench.json [--class <runner class>]
//                       [--report report.md]
//   hitokoto-bench-gate --print-class --cpuinfo /proc/cpuinfo --cpus N --l2 512K --l3 32768K
//                       [--output bench-class.json]
#include "hitokoto/bench_gate/gate.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

#include <cstdio>
#include <optional>

using namespace hitokoto::bench_gate;

namespace {

constexpr int kExitArguments = 2;

std::optional<QJsonObject> readObject(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return doc.object();
}

Values toValues(const QJsonObject& object) {
    Values v;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isDouble()) {
            v[it.key().toStdString()] = it.value().toDouble();
        }
    }
    return v;
}

std::optional<Values> readValues(const QString& path) {
    const auto object = readObject(path);
    if (!object) {
        return std::nullopt;
    }
    return toValues(*object);
}

/// Two rounds from "a.json,b.json"; an error text if a file is missing or no JSON object.
std::optional<std::array<Values, 2>> readRounds(const QString& list, const char* side, QString& error) {
    const QStringList files = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (files.size() != 2) {
        error = QStringLiteral("%1 rounds: two files expected").arg(QLatin1String(side));
        return std::nullopt;
    }
    std::array<Values, 2> rounds;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto v = readValues(files[static_cast<int>(i)]);
        if (!v) {
            error = QStringLiteral("%1 round %2 has no complete results (%3)")
                        .arg(QLatin1String(side))
                        .arg(i + 1)
                        .arg(files[static_cast<int>(i)]);
            return std::nullopt;
        }
        rounds[i] = *v;
    }
    return rounds;
}

/// An error text if a round lacks a hard-budget key other than `except`.
bool checkComplete(const std::array<Values, 2>& rounds, const QString& list, const char* side,
                   const std::set<std::string, std::less<>>& except, QString& error) {
    const QStringList files = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (std::size_t i = 0; i < 2; ++i) {
        if (!isCompleteResult(rounds[i], except)) {
            error = QStringLiteral("%1 round %2 has no complete results (%3)")
                        .arg(QLatin1String(side))
                        .arg(i + 1)
                        .arg(files.value(static_cast<int>(i)));
            return false;
        }
    }
    return true;
}

std::vector<std::string> readLines(const QString& path) {
    std::vector<std::string> lines;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (!line.isEmpty()) {
                lines.push_back(line.toStdString());
            }
        }
    }
    return lines;
}

const char* statusText(KeyStatus s) {
    switch (s) {
        case KeyStatus::Ok:
            return "ok";
        case KeyStatus::Slow:
            return "SLOW";
        case KeyStatus::Info:
            return "slower (report only)";
        case KeyStatus::OneSide:
            return "on one side only";
    }
    return "";
}

QString number(double v) {
    return QString::number(v, 'f', 2);
}

void writeHeader(QTextStream& out, const Inputs& in, const QString& runnerClass) {
    out << "## Benchmarks (Linux, " << (in.kind == RunKind::Main ? "run of main" : "pull request") << ")\n\n";
    if (!runnerClass.isEmpty()) {
        out << "Runner class: `" << runnerClass << "` (information)\n\n";
    }
}

void writeKeys(QTextStream& out, const std::vector<KeyResult>& keys) {
    int notOk = 0;
    for (const auto& k : keys) {
        notOk += k.status == KeyStatus::Ok ? 0 : 1;
    }
    out << "\n### Comparison with main on this runner (" << keys.size() << " keys, "
        << (keys.size() - static_cast<std::size_t>(notOk)) << " ok)\n";
    if (notOk == 0) {
        return;
    }
    out << "\n| Key | Base 1 | PR 1 | PR 2 | Base 2 | Base (mean) | PR (mean) | Status |\n"
           "|---|---:|---:|---:|---:|---:|---:|---|\n";
    for (const auto& k : keys) {
        if (k.status == KeyStatus::Ok) {
            continue;
        }
        out << "| `" << QString::fromStdString(k.key) << "` | ";
        if (k.status == KeyStatus::OneSide) {
            out << " |  |  |  |  |  | " << statusText(k.status) << " |\n";
            continue;
        }
        for (const double r : k.rounds) {
            out << number(r) << " | ";
        }
        out << number(k.was) << " | " << number(k.now) << " | " << statusText(k.status) << " |\n";
    }
}

QString report(const Inputs& in, const Decision& d, const QString& runnerClass) {
    QString text;
    QTextStream out(&text);
    writeHeader(out, in, runnerClass);
    out << "Outcome: " << (d.outcome == Outcome::Green ? "green" : "fails") << "\n";
    if (d.acceptedByLabel) {
        out << "\nSlower keys accepted by label `bench-accepted`.\n";
    }
    for (const auto& n : d.notices) {
        out << "\nNotice: " << QString::fromStdString(n) << "\n";
    }
    if (!d.mixedPaths.empty()) {
        out << "\n### Toolchain changes belong in their own pull request\n\n"
               "Besides the toolchain the pull request changes:\n\n";
        for (const auto& p : d.mixedPaths) {
            out << "- `" << QString::fromStdString(p) << "`\n";
        }
    }
    if (!d.hardBudgetMisses.empty()) {
        out << "\n### Hard budgets missed\n\n";
        for (const auto& b : d.hardBudgetMisses) {
            out << "- `" << QString::fromStdString(b.key) << "`: " << QString::fromStdString(b.text) << ", limit "
                << b.limit << " ms\n";
        }
    }
    if (!d.baseBudgetMisses.empty()) {
        out << "\n### Hard budgets missed by the base (information)\n\n";
        for (const auto& m : d.baseBudgetMisses) {
            out << "- `" << QString::fromStdString(m.budget.key) << "`: base round " << m.round << ", "
                << number(m.value) << " ms, limit " << m.budget.limit << " ms\n";
        }
    }
    if (in.kind == RunKind::PullRequest && !d.keys.empty()) {
        writeKeys(out, d.keys);
    }
    return text;
}

/// The report of a run that cannot be compared (a round without complete results).
QString errorReport(const Inputs& in, const QString& runnerClass, const QString& error) {
    QString text;
    QTextStream out(&text);
    writeHeader(out, in, runnerClass);
    out << "Outcome: fails, " << error << "\n";
    return text;
}

int finish(const QString& text, const QString& reportPath, Outcome outcome) {
    std::fputs(qPrintable(text), stdout);
    if (!reportPath.isEmpty()) {
        QFile f(reportPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(text.toUtf8());
        }
    }
    return static_cast<int>(outcome);
}

/// --print-class: the runner class and its context, printed and written as JSON.
int printClass(const QString& cpuinfoPath, const QString& cpus, const QString& l2, const QString& l3,
               const QString& output) {
    QFile f(cpuinfoPath);
    const QByteArray cpuinfo = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    const std::string text = cpuinfo.toStdString();
    QJsonObject o;
    o[QStringLiteral("class")] =
        QString::fromStdString(runnerClass(text, cpus.toInt(), l2.toStdString(), l3.toStdString()));
    o[QStringLiteral("model_name")] = QString::fromStdString(modelName(text));
    o[QStringLiteral("cpus")] = cpus;
    o[QStringLiteral("l2")] = l2;
    o[QStringLiteral("l3")] = l3;
    const QByteArray json = QJsonDocument(o).toJson();
    std::printf("%s\n%s", qPrintable(o[QStringLiteral("class")].toString()), json.constData());
    if (!output.isEmpty()) {
        QFile out(output);
        if (!out.open(QIODevice::WriteOnly) || out.write(json) != json.size()) {
            std::fprintf(stderr, "hitokoto-bench-gate: cannot write %s\n", qPrintable(output));
            return static_cast<int>(Outcome::Fails);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("S029 benchmark gate"));
    parser.addHelpOption();
    const QCommandLineOption kindOpt(QStringLiteral("kind"), QStringLiteral("pr or main."), QStringLiteral("kind"));
    const QCommandLineOption classOpt(QStringLiteral("class"), QStringLiteral("Runner class, for the report."),
                                      QStringLiteral("class"));
    const QCommandLineOption resultsOpt(QStringLiteral("results"), QStringLiteral("e2e-bench.json of a run of main."),
                                        QStringLiteral("file"));
    const QCommandLineOption baseOpt(QStringLiteral("base-rounds"), QStringLiteral("Two result files of the base."),
                                     QStringLiteral("a,b"));
    const QCommandLineOption prOpt(QStringLiteral("pr-rounds"), QStringLiteral("Two result files of the pull request."),
                                   QStringLiteral("a,b"));
    const QCommandLineOption changedOpt(QStringLiteral("changed-paths"),
                                        QStringLiteral("Paths changed by the pull request, one per line."),
                                        QStringLiteral("file"));
    const QCommandLineOption qtOpt(QStringLiteral("qt-changed"),
                                   QStringLiteral("The Qt version of the bench job differs from the base's."));
    const QCommandLineOption imageOpt(QStringLiteral("image-changed"),
                                      QStringLiteral("The runner image of the bench job differs from the base's."));
    const QCommandLineOption acceptedOpt(QStringLiteral("accepted"),
                                         QStringLiteral("The pull request carries bench-accepted."));
    const QCommandLineOption reportOpt(QStringLiteral("report"), QStringLiteral("Write the report (Markdown) here."),
                                       QStringLiteral("file"));
    const QCommandLineOption printClassOpt(QStringLiteral("print-class"),
                                           QStringLiteral("Print the runner class and its context."));
    const QCommandLineOption cpuinfoOpt(QStringLiteral("cpuinfo"), QStringLiteral("Path of /proc/cpuinfo."),
                                        QStringLiteral("file"), QStringLiteral("/proc/cpuinfo"));
    const QCommandLineOption cpusOpt(QStringLiteral("cpus"), QStringLiteral("CPUs the job may use (nproc)."),
                                     QStringLiteral("n"));
    const QCommandLineOption l2Opt(QStringLiteral("l2"), QStringLiteral("L2 size as in sysfs."),
                                   QStringLiteral("size"));
    const QCommandLineOption l3Opt(QStringLiteral("l3"), QStringLiteral("L3 size as in sysfs."),
                                   QStringLiteral("size"));
    const QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("File to write."),
                                       QStringLiteral("file"));
    parser.addOptions({kindOpt, classOpt, resultsOpt, baseOpt, prOpt, changedOpt, qtOpt, imageOpt, acceptedOpt,
                       reportOpt, printClassOpt, cpuinfoOpt, cpusOpt, l2Opt, l3Opt, outputOpt});
    parser.process(app);

    if (parser.isSet(printClassOpt)) {
        return printClass(parser.value(cpuinfoOpt), parser.value(cpusOpt), parser.value(l2Opt), parser.value(l3Opt),
                          parser.value(outputOpt));
    }

    const QString kind = parser.value(kindOpt);
    const bool isMain = kind == QLatin1String("main");
    if ((kind != QLatin1String("pr") && !isMain) || (isMain && !parser.isSet(resultsOpt)) ||
        (!isMain && (!parser.isSet(baseOpt) || !parser.isSet(prOpt)))) {
        std::fprintf(stderr,
                     "hitokoto-bench-gate: --kind main needs --results, --kind pr needs --base-rounds and "
                     "--pr-rounds\n");
        return kExitArguments;
    }

    Inputs in;
    in.kind = isMain ? RunKind::Main : RunKind::PullRequest;
    const QString runnerClass = parser.value(classOpt);
    const QString reportPath = parser.value(reportOpt);
    if (isMain) {
        const auto results = readValues(parser.value(resultsOpt));
        if (!results || !isCompleteResult(*results)) {
            const QString error = QStringLiteral("%1 has no complete results").arg(parser.value(resultsOpt));
            return finish(errorReport(in, runnerClass, error), reportPath, Outcome::Fails);
        }
        in.results = *results;
    } else {
        QString error;
        QString pullRequestError;
        in.baseRounds = readRounds(parser.value(baseOpt), "base", error);
        in.pullRequestRounds = readRounds(parser.value(prOpt), "pull request", pullRequestError);
        // S031: a hard key new in the pull request is not measured by the base yet.
        const auto newKeys = in.baseRounds && in.pullRequestRounds ? newHardKeys(*in.baseRounds, *in.pullRequestRounds)
                                                                   : std::set<std::string, std::less<>>{};
        if (error.isEmpty() && in.baseRounds &&
            !checkComplete(*in.baseRounds, parser.value(baseOpt), "base", newKeys, error)) {
            in.baseRounds.reset();
        }
        if (error.isEmpty()) {
            error = pullRequestError;
        }
        if (error.isEmpty() && in.pullRequestRounds &&
            !checkComplete(*in.pullRequestRounds, parser.value(prOpt), "pull request", {}, error)) {
            in.pullRequestRounds.reset();
        }
        if (!error.isEmpty()) {
            return finish(errorReport(in, runnerClass, error), reportPath, Outcome::Fails);
        }
        if (parser.isSet(changedOpt)) {
            in.changedPaths = readLines(parser.value(changedOpt));
        }
        in.qtVersionChanged = parser.isSet(qtOpt);
        in.imageChanged = parser.isSet(imageOpt);
        in.accepted = parser.isSet(acceptedOpt);
    }
    const Decision d = decide(in);
    return finish(report(in, d, runnerClass), reportPath, d.outcome);
}
