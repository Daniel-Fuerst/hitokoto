// SPDX-License-Identifier: GPL-3.0-or-later
// S029: hitokoto-bench-gate decides from files whether the Linux benchmark job is green (exit 0),
// fails (1) or needs the main stage (3); 2 means wrong arguments. The decisions are in the gate
// library (standard library only); this file reads files and JSON and writes the report.
//
//   hitokoto-bench-gate --kind pr|main --class <runner class> --results e2e-bench.json
//                       --data <checkout of bench-data> [--base-rounds a.json,b.json]
//                       [--pr-rounds a.json,b.json] [--changed-paths paths.txt] [--accepted]
//                       [--report report.md]
//   hitokoto-bench-gate --print-class --cpuinfo /proc/cpuinfo --cpus N --l2 512K --l3 32768K
//                       [--output bench-class.json]
//   hitokoto-bench-gate --make-sample --results e2e-bench.json --context context.json
//                       --output sample.json
//
// bench-data holds linux/<class>/<sample>.json ({"start_time": ..., "values": {...}, context}) and
// linux/accepted/<marker>.json ({"start_time": ...}).
#include "hitokoto/bench_gate/gate.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
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

/// The samples of a class; a file that cannot be read becomes an unreadable sample.
std::vector<Sample> readSamples(const QDir& data, const QString& runnerClass) {
    std::vector<Sample> samples;
    const QDir dir(data.filePath(QStringLiteral("linux/") + runnerClass));
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        Sample s;
        s.file = (QStringLiteral("linux/") + runnerClass + QLatin1Char('/') + name).toStdString();
        const auto object = readObject(dir.filePath(name));
        if (!object || !(*object)[QStringLiteral("values")].isObject() ||
            (*object)[QStringLiteral("start_time")].toString().isEmpty()) {
            s.readable = false;
        } else {
            s.startTime = (*object)[QStringLiteral("start_time")].toString().toStdString();
            s.values = toValues((*object)[QStringLiteral("values")].toObject());
        }
        samples.push_back(std::move(s));
    }
    return samples;
}

std::vector<std::string> readMarkers(const QDir& data, std::vector<std::string>& warnings) {
    std::vector<std::string> markers;
    const QDir dir(data.filePath(QStringLiteral("linux/accepted")));
    for (const QString& name : dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        const auto object = readObject(dir.filePath(name));
        const QString time = object ? (*object)[QStringLiteral("start_time")].toString() : QString();
        if (time.isEmpty()) {
            warnings.push_back("unreadable acceptance marker skipped: linux/accepted/" + name.toStdString());
        } else {
            markers.push_back(time.toStdString());
        }
    }
    return markers;
}

/// Two rounds from "a.json,b.json"; an error text if a round has no complete results.
std::optional<std::array<Values, 2>> readRounds(const QString& list, const char* side, QString& error) {
    const QStringList files = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (files.size() != 2) {
        error = QStringLiteral("%1 rounds: two files expected").arg(QLatin1String(side));
        return std::nullopt;
    }
    std::array<Values, 2> rounds;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto v = readValues(files[static_cast<int>(i)]);
        if (!v || !isCompleteResult(*v)) {
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
            return "info";
        case KeyStatus::NoBaseline:
            return "no class baseline yet";
        case KeyStatus::OneSide:
            return "on one side only";
    }
    return "";
}

void writeKeys(QTextStream& out, const char* title, const char* was, const char* now,
               const std::vector<KeyResult>& keys) {
    int notOk = 0;
    for (const auto& k : keys) {
        notOk += k.status == KeyStatus::Ok ? 0 : 1;
    }
    out << "\n### " << title << " (" << keys.size() << " keys, " << (keys.size() - static_cast<std::size_t>(notOk))
        << " ok)\n";
    if (notOk == 0) {
        return;
    }
    out << "\n| Key | " << was << " | " << now << " | Status |\n|---|---:|---:|---|\n";
    for (const auto& k : keys) {
        if (k.status != KeyStatus::Ok) {
            out << "| `" << QString::fromStdString(k.key) << "` | " << QString::number(k.was, 'f', 2) << " | "
                << QString::number(k.now, 'f', 2) << " | " << statusText(k.status) << " |\n";
        }
    }
}

QString report(const Inputs& in, const Decision& d) {
    QString text;
    QTextStream out(&text);
    std::size_t readable = 0;
    for (const auto& s : in.samples) {
        readable += s.readable ? 1 : 0;
    }
    const bool established = isEstablished(in.runnerClass, in.samples);
    out << "## Benchmarks (Linux, " << (in.kind == RunKind::Main ? "run of main" : "pull request") << ")\n\n";
    out << "Runner class: `" << QString::fromStdString(in.runnerClass) << "`, " << readable << " samples, "
        << (established ? "established" : "not established") << "\n\n";
    const char* stage = d.stage == Stage::Class ? "class" : d.stage == Stage::Main ? "main" : "none";
    out << "Stage: " << stage << "\n\n";
    if (!established && in.kind == RunKind::PullRequest) {
        if (isUnknownClass(in.runnerClass)) {
            out << "The runner reports no CPU model (unknown class): compared with main on this runner.\n\n";
        } else {
            out << "The class has " << readable << " of " << kMinSamples
                << " samples: compared with main on this runner.\n\n";
        }
    }
    const char* outcome = d.outcome == Outcome::Green           ? "green"
                          : d.outcome == Outcome::NeedMainStage ? "run the main stage (build the base and measure)"
                                                                : "fails";
    out << "Outcome: " << outcome << "\n";
    if (d.acceptedByLabel) {
        out << "\nSlower keys accepted by label `bench-accepted`.\n";
    }
    for (const auto& n : d.notices) {
        out << "\nNotice: " << QString::fromStdString(n) << "\n";
    }
    for (const auto& w : d.warnings) {
        out << "\nWarning: " << QString::fromStdString(w) << "\n";
    }
    if (!d.hardBudgetMisses.empty()) {
        out << "\n### Hard budgets missed\n\n";
        for (const auto& b : d.hardBudgetMisses) {
            out << "- `" << QString::fromStdString(b.key) << "`: " << QString::fromStdString(b.text) << ", limit "
                << b.limit << " ms\n";
        }
    }
    if (!d.classKeys.empty()) {
        writeKeys(out, "Class stage", "Class baseline", "This run", d.classKeys);
    }
    if (!d.mainKeys.empty()) {
        writeKeys(out, "Main stage", "Base (mean)", "Pull request (mean)", d.mainKeys);
    }
    if (!d.driftKeys.empty()) {
        writeKeys(out, "Drift from the anchor", "Anchor", "Class baseline", d.driftKeys);
    }
    return text;
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

/// --make-sample: a sample from complete results with every context field, or a refusal (exit 1).
int makeSample(const QString& resultsPath, const QString& contextPath, const QString& output) {
    const auto results = readObject(resultsPath);
    const auto context = readObject(contextPath);
    const Values values = results ? toValues(*results) : Values{};
    std::map<std::string, std::string, std::less<>> fields;
    if (context) {
        for (auto it = context->begin(); it != context->end(); ++it) {
            fields[it.key().toStdString()] = it.value().toVariant().toString().toStdString();
        }
    }
    QStringList missing;
    for (const auto& b : hardBudgets()) {
        if (values.count(b.key) == 0) {
            missing << QString::fromStdString(b.key);
        }
    }
    for (const auto& field : missingContextFields(fields)) {
        missing << QString::fromStdString(field);
    }
    if (!missing.isEmpty() || output.isEmpty() || !results || !context) {
        std::printf("sample refused: missing %s\n", qPrintable(missing.join(QStringLiteral(", "))));
        return static_cast<int>(Outcome::Fails);
    }
    QJsonObject sample = *context;
    sample[QStringLiteral("values")] = *results;
    QFile out(output);
    const QByteArray json = QJsonDocument(sample).toJson();
    if (!out.open(QIODevice::WriteOnly) || out.write(json) != json.size()) {
        std::fprintf(stderr, "hitokoto-bench-gate: cannot write %s\n", qPrintable(output));
        return static_cast<int>(Outcome::Fails);
    }
    std::printf("sample written: %s\n", qPrintable(output));
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("S029 benchmark gate"));
    parser.addHelpOption();
    const QCommandLineOption kindOpt(QStringLiteral("kind"), QStringLiteral("pr or main."), QStringLiteral("kind"));
    const QCommandLineOption classOpt(QStringLiteral("class"), QStringLiteral("Runner class."),
                                      QStringLiteral("class"));
    const QCommandLineOption resultsOpt(QStringLiteral("results"), QStringLiteral("e2e-bench.json of this run."),
                                        QStringLiteral("file"));
    const QCommandLineOption dataOpt(QStringLiteral("data"), QStringLiteral("Checkout of bench-data."),
                                     QStringLiteral("dir"));
    const QCommandLineOption baseOpt(QStringLiteral("base-rounds"), QStringLiteral("Two result files of the base."),
                                     QStringLiteral("a,b"));
    const QCommandLineOption prOpt(QStringLiteral("pr-rounds"), QStringLiteral("Two result files of the pull request."),
                                   QStringLiteral("a,b"));
    const QCommandLineOption changedOpt(QStringLiteral("changed-paths"),
                                        QStringLiteral("Paths changed by the pull request, one per line."),
                                        QStringLiteral("file"));
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
    const QCommandLineOption makeSampleOpt(QStringLiteral("make-sample"),
                                           QStringLiteral("Write a sample from --results and --context."));
    const QCommandLineOption contextOpt(QStringLiteral("context"), QStringLiteral("Context of the run (JSON)."),
                                        QStringLiteral("file"));
    const QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("File to write."),
                                       QStringLiteral("file"));
    parser.addOptions({kindOpt, classOpt, resultsOpt, dataOpt, baseOpt, prOpt, changedOpt, acceptedOpt, reportOpt,
                       printClassOpt, cpuinfoOpt, cpusOpt, l2Opt, l3Opt, makeSampleOpt, contextOpt, outputOpt});
    parser.process(app);

    if (parser.isSet(printClassOpt)) {
        return printClass(parser.value(cpuinfoOpt), parser.value(cpusOpt), parser.value(l2Opt), parser.value(l3Opt),
                          parser.value(outputOpt));
    }
    if (parser.isSet(makeSampleOpt)) {
        return makeSample(parser.value(resultsOpt), parser.value(contextOpt), parser.value(outputOpt));
    }

    const QString kind = parser.value(kindOpt);
    if ((kind != QLatin1String("pr") && kind != QLatin1String("main")) || !parser.isSet(classOpt) ||
        !parser.isSet(resultsOpt)) {
        std::fprintf(stderr, "hitokoto-bench-gate: --kind pr|main, --class and --results are required\n");
        return kExitArguments;
    }

    Inputs in;
    in.kind = kind == QLatin1String("main") ? RunKind::Main : RunKind::PullRequest;
    in.runnerClass = parser.value(classOpt).toStdString();
    const auto results = readValues(parser.value(resultsOpt));
    if (!results || !isCompleteResult(*results)) {
        std::fprintf(stderr, "hitokoto-bench-gate: %s has no complete results\n", qPrintable(parser.value(resultsOpt)));
        return static_cast<int>(Outcome::Fails);
    }
    in.results = *results;

    std::vector<std::string> warnings;
    const QDir data(parser.value(dataOpt));
    if (!parser.isSet(dataOpt) || !data.exists()) {
        warnings.emplace_back("bench-data is not available: the class counts as not established");
    } else {
        in.samples = readSamples(data, parser.value(classOpt));
        in.markers = readMarkers(data, warnings);
    }
    if (parser.isSet(baseOpt) != parser.isSet(prOpt)) {
        std::fprintf(stderr, "hitokoto-bench-gate: --base-rounds and --pr-rounds go together\n");
        return kExitArguments;
    }
    if (parser.isSet(baseOpt)) {
        QString error;
        in.baseRounds = readRounds(parser.value(baseOpt), "base", error);
        if (in.baseRounds) {
            in.pullRequestRounds = readRounds(parser.value(prOpt), "pull request", error);
        }
        if (!error.isEmpty()) {
            std::fprintf(stderr, "hitokoto-bench-gate: %s\n", qPrintable(error));
            return static_cast<int>(Outcome::Fails);
        }
    }
    if (parser.isSet(changedOpt)) {
        in.changedPaths = readLines(parser.value(changedOpt));
    }
    in.accepted = parser.isSet(acceptedOpt);

    Decision d = decide(in);
    d.warnings.insert(d.warnings.begin(), warnings.begin(), warnings.end());
    const QString text = report(in, d);
    std::fputs(qPrintable(text), stdout);
    if (parser.isSet(reportOpt)) {
        QFile f(parser.value(reportOpt));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(text.toUtf8());
        }
    }
    return static_cast<int>(d.outcome);
}
