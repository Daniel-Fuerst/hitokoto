// SPDX-License-Identifier: GPL-3.0-or-later
// S000: random edits on the window with fixed seeds. Each seed drives a fixed number of steps that
// mix what a user does in any order: typing, keys, the caret, undo and redo, pastes of text and
// tables, find while the document changes, the source view of a table, resizing, scrolling and the
// theme. The stories test each of these on its own; this test meets their combinations, which no
// story lists. It passes when no step crashes (the CI runs it with ASan and UBSan as well) and
// the document saves back byte for byte afterwards. A failing seed is reproduced with
//   tst_s000_random_edits randomEditsKeepTheEditorWhole:<seed>
#include "find_bar.hpp"
#include "find_controller.hpp"
#include "ui_test_support.hpp"

#include <QClipboard>
#include <QLineEdit>
#include <QMimeData>
#include <QRandomGenerator>
#include <QScrollBar>

#include <array>

using namespace hitokoto;

namespace {

// Steps per seed: enough to meet tables, find and undo together within seconds, few enough that
// the run under the sanitizers ends within the time limit of the test.
constexpr int kSteps = 600;

/// What gets inserted at random places: every element the editor renders, and pieces of them.
const QStringList& snippets() {
    static const QStringList list = {
        QStringLiteral("# heading\n"),
        QStringLiteral("- item\n"),
        QStringLiteral("  - nested\n"),
        QStringLiteral("text **bold** _it_ `code` ~~gone~~\n"),
        QStringLiteral("| a | b |\n|---|---|\n| 1 | 2 |\n"),
        QStringLiteral("| x |\n"),
        QStringLiteral("|---|\n"),
        QStringLiteral("```\ncode\n```\n"),
        QStringLiteral("---\n"),
        QStringLiteral("---\nkey: value\ntags: [a, b]\n---\n"),
        QStringLiteral("[link](http://example.com \"title\")"),
        QStringLiteral("![picture](missing.png)"),
        QStringLiteral("\n"),
        QStringLiteral("\n\n"),
        QStringLiteral("> quote\n"),
        QStringLiteral("1. one\n"),
        QStringLiteral("===\n"),
        QStringLiteral("a\\|b"),
        QStringLiteral("<br>"),
        QStringLiteral("| c1 <br> c2 | ![i](missing.png) |\n"),
        QStringLiteral("***"),
        QStringLiteral("___"),
        QStringLiteral("`"),
        QStringLiteral("``"),
        QStringLiteral("["),
        QStringLiteral("]("),
        QStringLiteral("\\"),
        QStringLiteral("|"),
        QStringLiteral("~~"),
        QStringLiteral("    indented\n"),
        QStringLiteral("\t"),
        QStringLiteral("k: {a: [1, 2]}\n"),
        QString::fromUtf8("\xF0\x9F\x98\x80"),  // an emoji (a surrogate pair)
        QString::fromUtf8("\xC3\xA4"),          // a letter outside ASCII
        QString::fromUtf8("\xE2\x80\xAE"),      // right-to-left override
        QStringLiteral(" "),
    };
    return list;
}

int bounded(QRandomGenerator& rng, qsizetype n) {
    return static_cast<int>(rng.bounded(static_cast<quint32>(n)));
}

/// The widget that has the focus, or the editor.
QWidget* focused(const test::Fixture& f) {
    QWidget* w = QApplication::focusWidget();
    return w != nullptr ? w : f.editor();
}

/// Lets the event loop run for up to `ms` milliseconds (timers, lazy styling, find slices).
void pump(int ms) {
    if (ms <= 0) {
        QCoreApplication::processEvents();
        return;
    }
    QTest::qWait(ms);
}

void step(test::Fixture& f, QRandomGenerator& rng) {
    ui::EditorWidget* editor = f.editor();
    QTextDocument* doc = editor->document();
    const int chars = std::max(1, doc->characterCount());
    const int op = bounded(rng, 100);
    if (op < 22) {
        QTextCursor c(doc);
        c.setPosition(bounded(rng, chars));
        c.insertText(snippets().at(bounded(rng, snippets().size())));
    } else if (op < 34) {
        QTextCursor c(doc);
        const int at = bounded(rng, chars);
        c.setPosition(at);
        c.setPosition(std::min(chars - 1, at + bounded(rng, 40)), QTextCursor::KeepAnchor);
        c.removeSelectedText();
    } else if (op < 46) {
        QTextCursor c = editor->textCursor();
        c.setPosition(bounded(rng, chars));
        if (bounded(rng, 3) == 0) {
            c.setPosition(bounded(rng, chars), QTextCursor::KeepAnchor);
        }
        editor->setTextCursor(c);
    } else if (op < 60) {
        static constexpr std::array keys = {Qt::Key_Return,  Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_Tab,
                                            Qt::Key_Backtab, Qt::Key_Up,        Qt::Key_Down,   Qt::Key_Left,
                                            Qt::Key_Right,   Qt::Key_Home,      Qt::Key_End,    Qt::Key_PageDown,
                                            Qt::Key_PageUp,  Qt::Key_Escape};
        const Qt::Key key = keys.at(static_cast<std::size_t>(bounded(rng, keys.size())));
        test::keyClick(focused(f), key, bounded(rng, 5) == 0 ? Qt::ShiftModifier : Qt::NoModifier);
    } else if (op < 70) {
        static const QString typed = QStringLiteral("a|-#*_`[]()\\ >~1.");
        test::typeKey(focused(f), QString(typed.at(bounded(rng, typed.size()))));
    } else if (op < 76) {
        if (bounded(rng, 2) == 0) {
            editor->undo();
        } else {
            editor->redo();
        }
    } else if (op < 81) {
        auto* mime = new QMimeData;
        if (bounded(rng, 2) == 0) {
            mime->setHtml(
                QStringLiteral("<table><tr><td>a|b</td><td colspan=2>c<br>d</td></tr>"
                               "<tr><td>1</td><td>2</td><td>3</td></tr></table>"));
        } else {
            mime->setText(QStringLiteral("x\ty\n1\t2\n"));
        }
        QApplication::clipboard()->setMimeData(mime);
        editor->setFocus();
        editor->paste();
    } else if (op < 86) {
        if (bounded(rng, 3) == 0) {
            const QKeyCombination find = QKeySequence(QKeySequence::Find)[0];
            test::keyClick(focused(f), find.key(), find.keyboardModifiers());
        }
        static const QStringList queries = {QStringLiteral("a"),
                                            QStringLiteral("b c"),
                                            QStringLiteral("1"),
                                            QStringLiteral("|"),
                                            QStringLiteral("key"),
                                            QStringLiteral("x  y"),
                                            QString()};
        f.window->findBar()->searchField()->setText(queries.at(bounded(rng, queries.size())));
        test::keyClick(focused(f), bounded(rng, 2) == 0 ? Qt::Key_Return : Qt::Key_Escape);
        editor->setFocus();
    } else if (op < 90) {
        f.window->resize(300 + bounded(rng, 900), 200 + bounded(rng, 700));
    } else if (op < 94) {
        QScrollBar* bar = editor->verticalScrollBar();
        bar->setValue(bounded(rng, std::max(1, bar->maximum() + 1)));
    } else if (op < 96) {
        test::keyClick(focused(f), Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);  // source of a table
    } else if (op < 98) {
        f.window->applyTheme(
            core::builtinTheme(bounded(rng, 2) == 0 ? core::ColorScheme::Dark : core::ColorScheme::Light));
    } else if (bounded(rng, 4) == 0) {
        QTextCursor c(doc);
        c.select(QTextCursor::Document);
        c.removeSelectedText();
    }
    if (bounded(rng, 3) == 0) {
        pump(bounded(rng, 30));
    } else {
        QCoreApplication::processEvents();
    }
}

}  // namespace

class tst_s000_random_edits : public QObject {
    Q_OBJECT

private slots:
    void randomEditsKeepTheEditorWhole_data() {
        QTest::addColumn<quint32>("seed");
        for (const quint32 seed : {1U, 2U, 3U, 4U, 5U, 6U}) {
            QTest::newRow(QByteArray::number(seed).constData()) << seed;
        }
    }

    void randomEditsKeepTheEditorWhole() {
        QFETCH(quint32, seed);
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("random.md"), QByteArray());
        QVERIFY(f.open(path));
        f.editor()->setFocus();
        QRandomGenerator rng(seed);
        for (int i = 0; i < kSteps; ++i) {
            step(f, rng);
        }
        f.settle();
        // After it all the document is still one the editor can write: it saves and reads back
        // byte for byte (ADR 0002).
        QString text = f.editor()->document()->toRawText();
        text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        QVERIFY(f.saveAndWait(path));
        QCOMPARE(test::Fixture::readFile(path), text.toUtf8());
    }
};

QTEST_MAIN(tst_s000_random_edits)
#include "tst_s000_random_edits.moc"
