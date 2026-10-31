// SPDX-License-Identifier: GPL-3.0-or-later
// S006/S007: end-to-end performance harness.
//
//   hitokoto-bench [--runs N] [--output results.json] [--baseline baseline.json]
//                  [--tolerance 0.2] [--fail-on-budget] [--app path/to/hitokoto]
//
// Measures cold start (separate processes), opening 1 MB and 10 MB files, key-to-paint
// latency (typing, Enter, moving across headings), opening and typing in large tables (S018) and
// saving (longest UI stall and total, median of --runs saves), moving, entering and typing in a
// 10 MB code block (S021), and opening and typing in a document of thematic breaks, setext
// headings and long hard-wrapped paragraphs (S023). Compares against a baseline (regression gate for CI) and
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
#include <array>
#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>
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

/// S021: a paragraph, one fenced code block of about `target` bytes, a paragraph. Line 2 is the
/// opening fence, with a blank line above it; a blank line follows the closing fence.
QByteArray codeBlockDocument(qsizetype target) {
    QByteArray out = "Text before the block.\n\n```cpp\n";
    out.reserve(target + 512);
    int i = 0;
    while (out.size() < target) {
        ++i;
        out += "    const int value" + QByteArray::number(i) + " = compute(" + QByteArray::number(i) +
               "); // # not a heading, *not* emphasis\n";
    }
    out += "```\n\nText after the block.\n";
    return out;
}

/// S023 (review O9): about `target` bytes of sections with setext headings, hard-wrapped paragraphs,
/// thematic breaks and list items continued lazily, with one paragraph of `longLines` lines without
/// a blank line in the middle (its first line starts with "Long paragraph"; a blank line follows).
QByteArray s023Document(qsizetype target, int longLines) {
    QByteArray out = "A setext title\nover two lines\n===\n\n";
    out.reserve(target + 512);
    const auto section = [&](int i) {
        out += "Section " + QByteArray::number(i) + "\n-------\n\n";
        for (int line = 0; line < 12; ++line) {
            out += "hard-wrapped text of section " + QByteArray::number(i) + ", line " + QByteArray::number(line) +
                   ", with *emphasis* and words\n";
        }
        out += "\n---\n- an item with text\nlazily continued line of the item\n  continued in the item\n***\n\n";
        out += "Text directly above a break\n___\n\n";
    };
    int i = 0;
    while (out.size() < target / 2) {
        section(++i);
    }
    for (int line = 0; line < longLines; ++line) {
        out += (line == 0 ? "Long paragraph" : "long paragraph") + QByteArray(" line ") + QByteArray::number(line) +
               " of hard-wrapped text without a blank line\n";
    }
    out += "\n";
    while (out.size() < target) {
        section(++i);
    }
    return out;
}

/// S024: front matter of `keys` top-level properties of every type, or (`nested`) one key whose
/// value is a list of `keys` single-key mappings, followed by `body`.
QByteArray s024Document(int keys, bool nested, const QByteArray& body) {
    QByteArray out = "---\n";
    if (nested) {
        out += "deep:\n";
        for (int i = 0; i < keys; ++i) {
            out += "  - k" + QByteArray::number(i) + ": " + QByteArray::number(i) + "\n";
        }
    } else {
        for (int i = 0; i < keys; ++i) {
            const QByteArray key = "key" + QByteArray::number(i) + ": ";
            switch (i % 6) {
                case 0:
                    out += key + "text value " + QByteArray::number(i) + " with some words\n";
                    break;
                case 1:
                    out += key + QByteArray::number(i * 3) + "\n";
                    break;
                case 2:
                    out += key + (i % 4 == 0 ? "true" : "false") + "\n";
                    break;
                case 3:
                    out += key + "2026-10-30\n";
                    break;
                case 4:
                    out += key + "[alpha, beta, gamma]\n";
                    break;
                default:
                    out += key + "\n  - one\n  - two\n";
                    break;
            }
        }
    }
    out += "---\n\n" + body;
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
    QSignalSpy done(w.session(), &ui::DocumentSession::loadFinished);
    QElapsedTimer t;
    OpenTiming r;
    // The first screen is painted and timed in the signal itself. Waiting for it with qWaitFor
    // added the wait's own polling and the further slices of text that processEvents inserted in
    // the meantime: 15-40 ms, and the share swung between runs (S018).
    bool shown = false;
    const QMetaObject::Connection first = QObject::connect(w.session(), &ui::DocumentSession::firstChunkShown, &w, [&] {
        w.editor()->viewport()->repaint();
        r.firstChunkMs = static_cast<double>(t.nsecsElapsed()) / 1e6;
        shown = true;
    });
    t.start();
    w.openFile(path);
    (void)QTest::qWaitFor([&] { return shown; }, 120000);
    QObject::disconnect(first);
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

/// Prints every sample of a median, in measuring order, so a CI log shows how they spread.
void printSamples(const char* name, const std::vector<double>& samples) {
    std::printf("samples %-28s", name);
    for (const double ms : samples) {
        std::printf(" %8.2f", ms);
    }
    std::printf("\n");
}

OpenTiming medianOpen(ui::MainWindow& w, const QString& path, const char* name) {
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
    printSamples((std::string(name) + "_first_chunk_ms").c_str(), first);
    printSamples((std::string(name) + "_loaded_ms").c_str(), loaded);
    printSamples((std::string(name) + "_styled_ms").c_str(), styled);
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
    printSamples("s018_table_open_ms", samples);
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

/// S021: key-to-paint of caret moves inside a large code block (Down, Up, End, Home, Right, Left).
std::vector<double> measureCodeBlockCaret(ui::MainWindow& w, int keys) {
    ui::EditorWidget* e = w.editor();
    e->setTextCursor(QTextCursor(e->document()->findBlockByNumber(e->document()->blockCount() / 2)));
    QCoreApplication::processEvents();
    constexpr std::array kKeys = {Qt::Key_Down, Qt::Key_Down,  Qt::Key_Down, Qt::Key_Up,   Qt::Key_End,
                                  Qt::Key_Home, Qt::Key_Right, Qt::Key_Left, Qt::Key_Down, Qt::Key_Up};
    std::vector<double> samples;
    for (int i = 0; i < keys; ++i) {
        QElapsedTimer t;
        t.start();
        test::keyClick(e, kKeys.at(static_cast<std::size_t>(i) % kKeys.size()));
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    return samples;
}

/// S021: key-to-paint of entering and leaving a large code block, from below onto the closing
/// fence and from above onto the opening fence; each round also scrolls to the other end.
std::vector<double> measureCodeBlockEnter(ui::MainWindow& w, int rounds) {
    ui::EditorWidget* e = w.editor();
    QTextDocument* doc = e->document();
    const auto press = [&](Qt::Key key, std::vector<double>& out) {
        QElapsedTimer t;
        t.start();
        test::keyClick(e, key);
        e->viewport()->repaint();
        out.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    };
    std::vector<double> samples;
    for (int i = 0; i < rounds; ++i) {
        e->setTextCursor(QTextCursor(doc->findBlockByNumber(doc->blockCount() - 3)));  // blank line below
        QCoreApplication::processEvents();
        press(Qt::Key_Up, samples);                                // onto the closing fence
        press(Qt::Key_Down, samples);                              // out again
        e->setTextCursor(QTextCursor(doc->findBlockByNumber(1)));  // blank line above
        QCoreApplication::processEvents();
        press(Qt::Key_Down, samples);  // onto the opening fence
        press(Qt::Key_Up, samples);    // out again
    }
    return samples;
}

/// Runs the event loop without sleeping until the styler has nothing left to do (S023). Returns the
/// time it took.
double waitUntilStyled(ui::EditorWidget* e) {
    QElapsedTimer t;
    t.start();
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(1);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (e->styler().isComplete() || t.elapsed() > 120000) {
            loop.quit();
        }
    });
    poll.start();
    loop.exec();
    return static_cast<double>(t.nsecsElapsed()) / 1e6;
}

/// The first block at or after the middle of the document whose text is `text`, or the first one.
QTextBlock blockWithText(QTextDocument* doc, const QString& text, bool startsWith = false) {
    for (QTextBlock b = doc->findBlockByNumber(doc->blockCount() / 2); b.isValid(); b = b.next()) {
        if (startsWith ? b.text().startsWith(text) : b.text() == text) {
            return b;
        }
    }
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (startsWith ? b.text().startsWith(text) : b.text() == text) {
            return b;
        }
    }
    return doc->begin();
}

/// S023: key-to-paint of typing on a thematic break in the middle of the document: a character at
/// its end and Backspace in turn, so the line changes between a break and paragraph text on every
/// key.
std::vector<double> measureBreakTyping(ui::MainWindow& w, int keys) {
    ui::EditorWidget* e = w.editor();
    QTextCursor c(blockWithText(e->document(), QStringLiteral("---")));
    c.movePosition(QTextCursor::EndOfBlock);
    e->setTextCursor(c);
    QCoreApplication::processEvents();
    std::vector<double> samples;
    for (int i = 0; i < keys; ++i) {
        QElapsedTimer t;
        t.start();
        if (i % 2 == 0) {
            test::keyClick(e, 'x');
        } else {
            test::keyClick(e, Qt::Key_Backspace);
        }
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    return samples;
}

struct BulletTiming {
    std::vector<double> keys;     ///< key-to-paint of every key
    std::vector<double> settled;  ///< after "- " and after removing it: until everything is restyled
};

/// S023 (review O9): typing "- " before the first line of a long hard-wrapped paragraph changes
/// the content column of every line of it; the rest is restyled in idle slices. Then Backspace
/// twice. Key-to-paint of each key, and the time until the styler is done.
BulletTiming measureBulletBeforeParagraph(ui::MainWindow& w, int rounds) {
    ui::EditorWidget* e = w.editor();
    const QTextBlock first = blockWithText(e->document(), QStringLiteral("Long paragraph"), true);
    BulletTiming r;
    const auto press = [&](auto key) {
        QElapsedTimer t;
        t.start();
        test::keyClick(e, key);
        e->viewport()->repaint();
        r.keys.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    };
    for (int i = 0; i < rounds; ++i) {
        e->setTextCursor(QTextCursor(first));
        QCoreApplication::processEvents();
        press('-');
        press(' ');
        r.settled.push_back(waitUntilStyled(e));
        press(Qt::Key_Backspace);
        press(Qt::Key_Backspace);
        r.settled.push_back(waitUntilStyled(e));
    }
    return r;
}

/// S023 (review O2): typing "-" on the blank line below the long paragraph makes all of it a setext
/// heading; Backspace makes it a paragraph again. Key-to-paint of each key, and the time until the
/// styler is done after each.
BulletTiming measureUnderlineBelowParagraph(ui::MainWindow& w, int rounds) {
    ui::EditorWidget* e = w.editor();
    QTextBlock below = blockWithText(e->document(), QStringLiteral("Long paragraph"), true);
    while (below.isValid() && !below.text().isEmpty()) {
        below = below.next();
    }
    BulletTiming r;
    const auto press = [&](auto key) {
        QElapsedTimer t;
        t.start();
        test::keyClick(e, key);
        e->viewport()->repaint();
        r.keys.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    };
    for (int i = 0; i < rounds; ++i) {
        e->setTextCursor(QTextCursor(below));
        QCoreApplication::processEvents();
        press('-');
        r.settled.push_back(waitUntilStyled(e));
        press(Qt::Key_Backspace);
        r.settled.push_back(waitUntilStyled(e));
    }
    return r;
}

/// S024: until the properties block shows after opening `path` (or 120 s).
double measurePropertiesOpen(ui::MainWindow& w, const QString& path, bool& firstScreen) {
    QSignalSpy done(w.session(), &ui::DocumentSession::loadFinished);
    QElapsedTimer t;
    firstScreen = false;
    bool shown = false;
    const QMetaObject::Connection first = QObject::connect(w.session(), &ui::DocumentSession::firstChunkShown, &w, [&] {
        w.editor()->viewport()->repaint();
        firstScreen = w.editor()->propertiesView() != nullptr;
        shown = true;
    });
    t.start();
    w.openFile(path);
    (void)QTest::qWaitFor([&] { return w.editor()->propertiesView() != nullptr; }, 120000);
    const double ms = static_cast<double>(t.nsecsElapsed()) / 1e6;
    (void)QTest::qWaitFor([&] { return shown; }, 120000);
    QObject::disconnect(first);
    (void)QTest::qWaitFor([&] { return !done.isEmpty(); }, 120000);
    (void)waitUntilStyled(w.editor());
    return ms;
}

/// S024: key-to-paint of typing on the first line below the front matter (the heading of the body).
std::vector<double> measureTypingBelowProperties(ui::MainWindow& w, int line, int keys) {
    ui::EditorWidget* e = w.editor();
    QTextCursor c(e->document()->findBlockByNumber(line));
    c.movePosition(QTextCursor::EndOfBlock);
    e->setTextCursor(c);
    QCoreApplication::processEvents();
    std::vector<double> samples;
    for (int i = 0; i < keys; ++i) {
        QElapsedTimer t;
        t.start();
        if (i % 2 == 0) {
            test::keyClick(e, 'x');
        } else {
            test::keyClick(e, Qt::Key_Backspace);
        }
        e->viewport()->repaint();
        samples.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    }
    return samples;
}

/// S024: Up from the first line below the block into its YAML and Down out again, `rounds` times;
/// `edit`: a character typed and removed on the closing line before leaving, so that the block is
/// read and built again. Key-to-paint of the keys, and the time until the block shows again.
BulletTiming measurePropertiesEnterLeave(ui::MainWindow& w, int line, int rounds, bool edit) {
    ui::EditorWidget* e = w.editor();
    BulletTiming r;
    const auto press = [&](auto key) {
        QElapsedTimer t;
        t.start();
        test::keyClick(e, key);
        e->viewport()->repaint();
        r.keys.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
    };
    for (int i = 0; i < rounds; ++i) {
        e->setTextCursor(QTextCursor(e->document()->findBlockByNumber(line)));
        (void)waitUntilStyled(e);
        press(Qt::Key_Up);
        if (edit) {
            test::keyClick(e, 'x');
            test::keyClick(e, Qt::Key_Backspace);
        }
        QElapsedTimer settled;
        settled.start();
        press(Qt::Key_Down);
        (void)QTest::qWaitFor([&] { return e->propertiesView() != nullptr; }, 120000);
        r.settled.push_back(static_cast<double>(settled.nsecsElapsed()) / 1e6);
    }
    return r;
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
constexpr std::string_view kReportOnly[] = {"save_10mb_total_ms",
                                            "typing_small_max_ms",
                                            "typing_10mb_max_ms",
                                            "s018_table_typing_max_ms",
                                            "s021_codeblock_caret_max_ms",
                                            "s021_codeblock_enter_max_ms",
                                            "s021_codeblock_typing_max_ms",
                                            "typing_small_over_16ms",
                                            "typing_10mb_over_16ms",
                                            "s018_table_typing_over_16ms",
                                            "s021_codeblock_caret_over_16ms",
                                            "s021_codeblock_typing_over_16ms",
                                            "s023_break_typing_max_ms",
                                            "s023_break_typing_over_16ms",
                                            "s023_bullet_typing_max_ms",
                                            "s023_bullet_settled_ms",
                                            "s023_underline_typing_max_ms",
                                            "s023_underline_settled_ms",
                                            "s024_typing_max_ms",
                                            "s024_typing_over_16ms",
                                            "s024_enter_leave_max_ms",
                                            "s024_leave_5000_keys_max_ms",
                                            "s024_leave_5000_nested_max_ms",
                                            "startup_p95_ms"};

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
    {"s021_codeblock_caret_p95_ms", 16, "key to screen <= 16 ms (10 MB code block)"},
    {"s021_codeblock_caret_max_ms", 50, "no caret move above 50 ms, code block (hard)", true},
    {"s021_codeblock_enter_max_ms", 50, "entering a 10 MB code block <= 50 ms (hard)", true},
    {"s021_codeblock_typing_p95_ms", 16, "key to screen <= 16 ms (10 MB code block)"},
    {"s021_codeblock_typing_max_ms", 50, "no key event above 50 ms, code block (hard)", true},
    {"s023_break_typing_p95_ms", 16, "key to screen <= 16 ms (on a thematic break)"},
    {"s023_break_typing_max_ms", 50, "no key event above 50 ms, break (hard)", true},
    {"s023_bullet_typing_max_ms", 50, "no key event above 50 ms, \"- \" before a paragraph (hard)", true},
    {"s023_underline_typing_max_ms", 50, "no key event above 50 ms, underline below a paragraph (hard)", true},
    {"s024_open_10mb_properties_ms", 3000, "10 MB with front matter: properties shown < 3 s"},
    {"s024_typing_p95_ms", 16, "key to screen <= 16 ms (below front matter)"},
    {"s024_typing_max_ms", 50, "no key event above 50 ms, below front matter (hard)", true},
    {"s024_enter_leave_max_ms", 50, "entering and leaving front matter <= 50 ms (hard)", true},
    {"s024_leave_5000_keys_max_ms", 50, "leaving 5,000 edited properties <= 50 ms (hard)", true},
    {"s024_leave_5000_nested_max_ms", 50, "leaving a nested value of 5,000 lines <= 50 ms (hard)", true},
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
    const QString codeBlock10 =
        writeFile(dir, QStringLiteral("codeblock10mb.md"), codeBlockDocument(qsizetype{10} * 1024 * 1024));
    const QString breaks10 =
        writeFile(dir, QStringLiteral("s023-10mb.md"), s023Document(qsizetype{10} * 1024 * 1024, 20000));
    const QString properties10 = writeFile(dir, QStringLiteral("s024-10mb.md"),
                                           s024Document(200, false, largeDocument(qsizetype{10} * 1024 * 1024)));
    const QString keys5000 = writeFile(dir, QStringLiteral("s024-keys.md"), s024Document(5000, false, "# Body\n"));
    const QString nested5000 = writeFile(dir, QStringLiteral("s024-nested.md"), s024Document(5000, true, "# Body\n"));

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

    const auto open1 = medianOpen(w, oneMb, "open_1mb");
    r["open_1mb_loaded_ms"] = open1.loadedMs;
    w.session()->newDocument();

    const auto open10 = medianOpen(w, tenMb, "open_10mb");
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
    w.session()->newDocument();

    // S021: a 10 MB document that is one code block. Caret steps, entering and typing.
    (void)measureOpen(w, codeBlock10);
    const auto caretCode = stats(measureCodeBlockCaret(w, 200));
    r["s021_codeblock_caret_median_ms"] = caretCode.median;
    r["s021_codeblock_caret_p95_ms"] = caretCode.p95;
    r["s021_codeblock_caret_p99_ms"] = caretCode.p99;
    r["s021_codeblock_caret_max_ms"] = caretCode.max;
    r["s021_codeblock_caret_over_16ms"] = caretCode.over16;
    const auto enterSamples = measureCodeBlockEnter(w, 10);
    printSamples("s021_codeblock_enter_ms", enterSamples);
    const auto enterCode = stats(enterSamples);
    r["s021_codeblock_enter_median_ms"] = enterCode.median;
    r["s021_codeblock_enter_max_ms"] = enterCode.max;
    const auto typingCode = stats(measureTyping(w, 200));
    r["s021_codeblock_typing_median_ms"] = typingCode.median;
    r["s021_codeblock_typing_p95_ms"] = typingCode.p95;
    r["s021_codeblock_typing_p99_ms"] = typingCode.p99;
    r["s021_codeblock_typing_max_ms"] = typingCode.max;
    r["s021_codeblock_typing_over_16ms"] = typingCode.over16;
    w.editor()->document()->setModified(false);
    w.session()->newDocument();

    // S023 (review O9): 10 MB of setext headings, breaks, lazily continued items and hard-wrapped
    // paragraphs. Opening and styling all of it (the cold walk), typing on a break, and "- "
    // before a paragraph of 20,000 lines.
    const auto openBreaks = medianOpen(w, breaks10, "s023_open_10mb");
    r["s023_open_10mb_first_chunk_ms"] = openBreaks.firstChunkMs;
    r["s023_open_10mb_styled_ms"] = openBreaks.styledMs;
    const auto typingBreak = stats(measureBreakTyping(w, 200));
    r["s023_break_typing_median_ms"] = typingBreak.median;
    r["s023_break_typing_p95_ms"] = typingBreak.p95;
    r["s023_break_typing_p99_ms"] = typingBreak.p99;
    r["s023_break_typing_max_ms"] = typingBreak.max;
    r["s023_break_typing_over_16ms"] = typingBreak.over16;
    (void)waitUntilStyled(w.editor());
    const auto bullet = measureBulletBeforeParagraph(w, 10);
    printSamples("s023_bullet_settled_ms", bullet.settled);
    const auto bulletKeys = stats(bullet.keys);
    r["s023_bullet_typing_median_ms"] = bulletKeys.median;
    r["s023_bullet_typing_max_ms"] = bulletKeys.max;
    r["s023_bullet_settled_ms"] = stats(bullet.settled).median;
    const auto underline = measureUnderlineBelowParagraph(w, 10);
    printSamples("s023_underline_settled_ms", underline.settled);
    const auto underlineKeys = stats(underline.keys);
    r["s023_underline_typing_median_ms"] = underlineKeys.median;
    r["s023_underline_typing_max_ms"] = underlineKeys.max;
    r["s023_underline_settled_ms"] = stats(underline.settled).median;
    w.editor()->document()->setModified(false);
    w.session()->newDocument();

    // S024: 10 MB with 200 properties in front: opening until the block shows, typing in the body
    // below it, entering and leaving it; then leaving an edited block of 5,000 keys and of a nested
    // value of 5,000 lines (review O7, O8).
    std::vector<double> propertiesOpen;
    bool firstScreen = true;
    for (int i = 0; i < kOpenRuns; ++i) {
        w.session()->newDocument();
        bool shown = false;
        propertiesOpen.push_back(measurePropertiesOpen(w, properties10, shown));
        firstScreen = firstScreen && shown;
    }
    printSamples("s024_open_10mb_properties_ms", propertiesOpen);
    std::printf("s024: the first screen %s the properties block\n", firstScreen ? "shows" : "does not show");
    r["s024_open_10mb_properties_ms"] = stats(propertiesOpen).median;
    const int below = w.editor()->styler().frontMatterClose() + 2;  // the heading after the blank line
    const auto typingProperties = stats(measureTypingBelowProperties(w, below, 200));
    r["s024_typing_median_ms"] = typingProperties.median;
    r["s024_typing_p95_ms"] = typingProperties.p95;
    r["s024_typing_p99_ms"] = typingProperties.p99;
    r["s024_typing_max_ms"] = typingProperties.max;
    r["s024_typing_over_16ms"] = typingProperties.over16;
    const auto enterLeave = measurePropertiesEnterLeave(w, w.editor()->styler().frontMatterClose() + 1, 10, false);
    printSamples("s024_enter_leave_ms", enterLeave.keys);
    r["s024_enter_leave_median_ms"] = stats(enterLeave.keys).median;
    r["s024_enter_leave_max_ms"] = stats(enterLeave.keys).max;
    w.editor()->document()->setModified(false);
    for (const auto& [path, name] : {std::pair{keys5000, std::string("s024_leave_5000_keys")},
                                     std::pair{nested5000, std::string("s024_leave_5000_nested")}}) {
        w.session()->newDocument();
        bool shown = false;
        (void)measurePropertiesOpen(w, path, shown);
        const auto leave = measurePropertiesEnterLeave(w, w.editor()->styler().frontMatterClose() + 1, 5, true);
        printSamples((name + "_keys_ms").c_str(), leave.keys);
        printSamples((name + "_built_ms").c_str(), leave.settled);
        r[name + "_max_ms"] = stats(leave.keys).max;
        r[name + "_built_ms"] = stats(leave.settled).median;
        w.editor()->document()->setModified(false);
    }

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
