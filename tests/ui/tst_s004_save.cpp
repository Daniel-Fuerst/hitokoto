// SPDX-License-Identifier: GPL-3.0-or-later
// S004: save and save-as write the text byte for byte, through the real editor widget.
#include "ui_test_support.hpp"

#include "hitokoto/core/markdown.hpp"

#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QInputMethodEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTableView>

#include <new>
#include <random>
#include <type_traits>

using namespace hitokoto;

namespace {

// Rework of 2026-11-21: half of a character (a lone surrogate) does not enter the text.
const QString kHigh(QChar(0xD83D));  // the first half of U+1F600
const QString kLow(QChar(0xDE00));   // its second half
const QString kFffd(QChar(0xFFFD));
const QString kEmoji = kHigh + kLow;  // U+1F600 as a whole

/// Whether `text` holds half of a character, which UTF-8 cannot hold.
bool holdsHalf(const QString& text) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i).isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            ++i;
        } else if (text.at(i).isSurrogate()) {
            return true;
        }
    }
    return false;
}

/// Opens `markdown` from a file and waits until it is styled.
bool openText(test::Fixture& f, const QString& markdown) {
    if (!f.open(f.writeFile(QStringLiteral("doc.md"), markdown.toUtf8()))) {
        return false;
    }
    f.settle();
    return true;
}

/// Saves to a new file; true when it was written.
bool saves(test::Fixture& f) {
    QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
    QSignalSpy failed(f.session(), &ui::DocumentSession::saveFailed);
    f.session()->save(f.dir.filePath(QStringLiteral("saved.md")));
    return QTest::qWaitFor([&] { return !done.isEmpty() || !failed.isEmpty(); }, 30000) && !done.isEmpty();
}

/// The text with the caret in the second cell of the body row of `| a | b |`, with the cell's
/// editor open and its text selected; the editor, or nullptr.
QPlainTextEdit* openCell(test::Fixture& f) {
    QTableView* view = f.editor()->tableViewAt(0);
    if (view == nullptr) {
        return nullptr;
    }
    view->setCurrentIndex(view->model()->index(1, 0));
    view->edit(view->model()->index(1, 0));
    QCoreApplication::processEvents();
    auto* cell = qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
    if (cell != nullptr) {
        cell->selectAll();
    }
    return cell;
}

void commitInputMethod(QWidget* target, const QString& commit) {
    QInputMethodEvent event;
    event.setCommitString(commit);
    QCoreApplication::sendEvent(target, &event);
    QCoreApplication::processEvents();
}

/// A drag event at `at`: Qt 6.12 adds constructors taking QPointF and deprecates those taking
/// QPoint; Qt 6.8 has only the latter (as in tst_s031_paste_tables).
template <typename Event>
Event dragEvent(QPointF at, const QMimeData* data) {
    if constexpr (std::is_constructible_v<Event, QPointF, Qt::DropActions, const QMimeData*, Qt::MouseButtons,
                                          Qt::KeyboardModifiers>) {
        return Event(at, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
    } else {
        return Event(at.toPoint(), Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
    }
}

}  // namespace

class tst_s004_save : public QObject {
    Q_OBJECT
private slots:
    void unchangedFilesRoundTripThroughTheEditor_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("lf") << QByteArray("# Title\n\n- a\n- b\n");
        QTest::newRow("crlf+bom") << QByteArray("\xEF\xBB\xBF# T\r\n- x\r\n");
        QTest::newRow("no final newline") << QByteArray("a\nb");
        QTest::newRow("cr") << QByteArray("a\rb\r");
        QTest::newRow("mixed") << QByteArray("a\r\nb\nc\rd\r\n");
        QTest::newRow("nbsp, tab, U+2028, NUL") << QByteArray(
            "a\xC2\xA0"
            "b\tc\xE2\x80\xA8"
            "d\x00"
            "e\n",
            13);
        QTest::newRow("only newlines") << QByteArray("\n\n\n");
        QTest::newRow("empty") << QByteArray();
    }
    void unchangedFilesRoundTripThroughTheEditor() {
        QFETCH(QByteArray, bytes);
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("doc.md"), bytes);
        QVERIFY(f.open(path));
        f.settle();
        // Move the caret around so that markers get revealed and concealed again.
        for (int i = 0; i < f.editor()->document()->blockCount(); ++i)
            f.placeCaret(i);
        const QString copy = f.dir.filePath(QStringLiteral("copy.md"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), bytes);
    }

    void editedFileKeepsBomAndCrlfWithoutRenderingArtifacts() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("doc.md"), "\xEF\xBB\xBF# T\r\n- x\r\n");
        QVERIFY(f.open(path));
        f.placeCaret(2);
        test::keyClicks(f.editor(), QStringLiteral("## New"));
        test::keyClick(f.editor(), Qt::Key_Return);
        test::keyClicks(f.editor(), QStringLiteral("- y"));
        f.placeCaret(0);
        QVERIFY(f.saveAndWait(path));
        QCOMPARE(test::Fixture::readFile(path), QByteArray("\xEF\xBB\xBF# T\r\n- x\r\n## New\r\n- y"));
        QVERIFY(!f.editor()->document()->isModified());
    }

    // ADR 0002: mixed files keep each line's ending. Only endings that differ from the majority are
    // stored per line; every other line saves with the majority ending anyway.
    void mixedEndingsStoreOnlyMinorityEndings() {
        test::Fixture f;
        const QByteArray bytes("a\r\nb\nc\nd\re\nf\r\n\ng\n");
        const QString path = f.writeFile(QStringLiteral("doc.md"), bytes);
        QVERIFY(f.open(path));
        f.settle();
        QCOMPARE(test::blocksWithUserData(f.editor()->document()), (QList<int>{0, 3, 5}));

        const QString copy = f.dir.filePath(QStringLiteral("copy.md"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), bytes);

        // A new line takes the majority ending; the minority line it was split from keeps its own.
        f.placeCaret(0, 1);
        test::keyClick(f.editor(), Qt::Key_Return);
        test::keyClicks(f.editor(), QStringLiteral("z"));
        QVERIFY(f.saveAndWait(copy));
        QCOMPARE(test::Fixture::readFile(copy), QByteArray("a\r\nz\nb\nc\nd\re\nf\r\n\ng\n"));
    }

    void saveAsChangesPathAndTitle() {
        test::Fixture f;
        test::keyClicks(f.editor(), QStringLiteral("hello"));
        const QString target = f.dir.filePath(QStringLiteral("new.md"));
        f.window->setPathPicker([&](bool forSaving, const QString&) { return forSaving ? target : QString(); });
        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        QVERIFY(f.window->saveAs());
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(test::Fixture::readFile(target), QByteArray("hello"));
        QCOMPARE(f.session()->path(), target);
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("new.md")));
        // Later saves go to the new path.
        test::keyClicks(f.editor(), QStringLiteral("!"));
        QVERIFY(f.window->save());
        QTRY_COMPARE(done.size(), 2);
        QCOMPARE(test::Fixture::readFile(target), QByteArray("hello!"));
    }

    // Keystrokes after the snapshot was taken (S017) are not in the file: the document stays
    // modified. Keystrokes before that go into the file.
    void typingDuringSaveKeepsModifiedFlag() {
        test::Fixture f;
        const QString path = f.dir.filePath(QStringLiteral("t.md"));
        test::keyClicks(f.editor(), QStringLiteral("one"));
        QSignalSpy snapshot(f.session(), &ui::DocumentSession::snapshotTaken);
        QSignalSpy done(f.session(), &ui::DocumentSession::saveFinished);
        f.session()->save(path);
        QTRY_COMPARE(snapshot.size(), 1);
        test::keyClicks(f.editor(), QStringLiteral(" two"));  // before the result arrives
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(test::Fixture::readFile(path), QByteArray("one"));
        QVERIFY(f.editor()->document()->isModified());
    }

    // Property: random valid Markdown-ish text survives open -> edit-free save unchanged.
    // Running out of memory while the save encodes its copy of the text, on the worker thread
    // (save) and on the calling thread (saveNow, what quitting does).
    void runningOutOfMemoryWhileSavingKeepsTextAndFile_data() {
        QTest::addColumn<bool>("now");
        QTest::newRow("save") << false;
        QTest::newRow("saveNow") << true;
    }

    void runningOutOfMemoryWhileSavingKeepsTextAndFile() {
        QFETCH(bool, now);
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("notes.md"), "# old\r\n");
        QVERIFY(f.open(path));
        test::keyClicks(f.editor(), QStringLiteral("new "));
        const QString text = f.editor()->toPlainText();
        ui::DocumentSession::WorkerHooks hooks;
        hooks.beforeEncode = [] { throw std::bad_alloc(); };
        f.session()->setWorkerHooksForTesting(hooks);
        QSignalSpy failed(f.session(), &ui::DocumentSession::saveFailed);
        QSignalSpy finished(f.session(), &ui::DocumentSession::saveFinished);
        if (now) {
            QString error;
            QVERIFY(!f.session()->saveNow(path, &error));
            QVERIFY2(error.contains(QStringLiteral("not enough memory")), qPrintable(error));
        } else {
            f.session()->save(path);
            QTRY_COMPARE(failed.size(), 1);
            QVERIFY2(failed.first().at(1).toString().contains(QStringLiteral("not enough memory")),
                     qPrintable(failed.first().at(1).toString()));
        }
        QCOMPARE(failed.size(), 1);
        QCOMPARE(finished.size(), 0);
        QVERIFY(!f.session()->isSaving());
        QCOMPARE(f.editor()->toPlainText(), text);
        QVERIFY(f.editor()->document()->isModified());
        QVERIFY(!f.editor()->isReadOnly());
        QCOMPARE(test::Fixture::readFile(path), QByteArray("# old\r\n"));
        QCOMPARE(QDir(f.dir.path()).entryList(QDir::Files | QDir::Hidden), QStringList{QStringLiteral("notes.md")});
        // The next save works.
        f.session()->setWorkerHooksForTesting({});
        QVERIFY(f.saveAndWait(path));
        QCOMPARE(test::Fixture::readFile(path), QByteArray("new # old\r\n"));
        QVERIFY(!f.editor()->document()->isModified());
    }

    void propertyRandomFilesRoundTrip() {
        std::mt19937 rng(1024);
        const char* pieces[] = {"# ",       "## ", "- ",   "* ", "+ ",       "text",         " ",
                                "\t",       "\n",  "\r\n", "\r", "\xC3\xA4", "\xE4\xB8\x80", "\xF0\x9F\x98\x80",
                                "\xC2\xA0", "#",   "-",    "---"};
        test::Fixture f;
        for (int iteration = 0; iteration < 60; ++iteration) {
            QByteArray bytes = (rng() % 3 == 0) ? QByteArray("\xEF\xBB\xBF") : QByteArray();
            const int n = static_cast<int>(rng() % 40);
            for (int k = 0; k < n; ++k)
                bytes += pieces[rng() % std::size(pieces)];
            const QString path = f.writeFile(QStringLiteral("p%1.md").arg(iteration), bytes);
            QVERIFY(f.open(path));
            const QString out = f.dir.filePath(QStringLiteral("p%1.out").arg(iteration));
            QVERIFY(f.saveAndWait(out));
            QCOMPARE(test::Fixture::readFile(out), bytes);
        }
    }

    // Rework of 2026-11-21. Scenario: half of a character does not enter the text. Each way text
    // comes into the editor inserts U+FFFD for it; whole characters, emoji included, stay.
    void halfACharacterIsPastedAsTheReplacementCharacter_data() {
        QTest::addColumn<QString>("way");
        QTest::newRow("Ctrl+V") << QStringLiteral("paste");
        QTest::newRow("Paste as Plain Text") << QStringLiteral("plain");
        QTest::newRow("drop") << QStringLiteral("drop");
    }

    void halfACharacterIsPastedAsTheReplacementCharacter() {
        QFETCH(QString, way);
        const QString markdown = QStringLiteral("One.\n\nTwo.\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        const QString pasted = QStringLiteral("a") + kHigh + QStringLiteral("x") + kLow + QStringLiteral("b") + kEmoji;
        const QString expected =
            QStringLiteral("a") + kFffd + QStringLiteral("x") + kFffd + QStringLiteral("b") + kEmoji;
        f.placeCaret(1);
        if (way == QStringLiteral("drop")) {
            QMimeData data;
            data.setText(pasted);
            const QPoint at = f.editor()->cursorRect(f.editor()->textCursor()).center();
            QWidget* viewport = f.editor()->viewport();
            auto enter = dragEvent<QDragEnterEvent>(QPointF(at), &data);
            QCoreApplication::sendEvent(viewport, &enter);
            auto move = dragEvent<QDragMoveEvent>(QPointF(at), &data);
            QCoreApplication::sendEvent(viewport, &move);
            QDropEvent drop(QPointF(at), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &drop);
        } else {
            auto* data = new QMimeData;
            data->setText(pasted);
            QApplication::clipboard()->setMimeData(data);
            if (way == QStringLiteral("plain")) {
                test::keyClick(f.editor(), Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
            } else {
                test::keyClick(f.editor(), Qt::Key_V, Qt::ControlModifier);
            }
        }
        f.settle();
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("One.\n") + expected + QStringLiteral("\nTwo.\n"));
        QVERIFY(saves(f));
    }

    // A table pasted from HTML (a character reference to half of one) or from Excel's plain text.
    void halfACharacterInAPastedTableIsTheReplacementCharacter_data() {
        QTest::addColumn<QString>("html");
        QTest::addColumn<QString>("plain");
        QTest::newRow("HTML") << QStringLiteral(
                                     "<table><tr><td>a</td><td>b</td></tr>"
                                     "<tr><td>c&#xD83D;d</td><td>e&#xDE00;</td></tr></table>")
                              << QStringLiteral("a\tb\ncd\te\n");
        QTest::newRow("Excel's plain text")
            << QString() << QStringLiteral("a\tb\nc") + kHigh + QStringLiteral("d\te") + kLow + QStringLiteral("\n");
    }

    void halfACharacterInAPastedTableIsTheReplacementCharacter() {
        QFETCH(QString, html);
        QFETCH(QString, plain);
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("One.\n\nTwo.\n")));
        f.placeCaret(1);
        auto* data = new QMimeData;
        if (!html.isEmpty()) {
            data->setHtml(html);
        }
        data->setText(plain);
        QApplication::clipboard()->setMimeData(data);
        test::keyClick(f.editor(), Qt::Key_V, Qt::ControlModifier);
        f.settle();
        const QString text = f.editor()->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("| c") + kFffd + QStringLiteral("d | e") + kFffd + QStringLiteral(" |")),
                 qPrintable(text.toUtf8().toPercentEncoding(" |\n")));
        QVERIFY(!holdsHalf(text));
        QVERIFY(saves(f));
    }

    // A key's text, also a key that brings only half a character: its text is typed with U+FFFD
    // for the half (Qt alone types nothing for a text that starts with a half).
    void halfACharacterIsTypedAsTheReplacementCharacter_data() {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("expected");
        QTest::newRow("after a letter") << QStringLiteral("a") + kHigh << QStringLiteral("a") + kFffd;
        QTest::newRow("a second half") << kLow << kFffd;
        QTest::newRow("a whole emoji stays") << kEmoji << kEmoji;
    }

    void halfACharacterIsTypedAsTheReplacementCharacter() {
        QFETCH(QString, typed);
        QFETCH(QString, expected);
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("One.\n")));
        f.placeCaret(0, 3);
        test::typeKey(f.editor(), typed);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("One") + expected + QStringLiteral(".\n"));
        QVERIFY(saves(f));
    }

    void halfACharacterIsCommittedByAnInputMethodAsTheReplacementCharacter() {
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("One.\n")));
        f.placeCaret(0, 3);
        commitInputMethod(f.editor(), kLow + QStringLiteral("x") + kHigh + kEmoji);
        QCOMPARE(f.editor()->toPlainText(),
                 QStringLiteral("One") + kFffd + QStringLiteral("x") + kFffd + kEmoji + QStringLiteral(".\n"));
        QVERIFY(saves(f));
    }

    // In a cell of a shown table (S018), by each way into the cell, and a value the cell hands to
    // the table: the cell's line gets U+FFFD.
    void halfACharacterInACellIsTheReplacementCharacter_data() {
        QTest::addColumn<QString>("way");
        QTest::newRow("Ctrl+V") << QStringLiteral("paste");
        QTest::newRow("Paste as Plain Text") << QStringLiteral("plain");
        QTest::newRow("key") << QStringLiteral("key");
        QTest::newRow("input method") << QStringLiteral("input method");
        QTest::newRow("value") << QStringLiteral("value");
    }

    void halfACharacterInACellIsTheReplacementCharacter() {
        QFETCH(QString, way);
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nafter\n")));
        const QString input = QStringLiteral("x") + kHigh + QStringLiteral("y") + kEmoji;
        const QString expected = QStringLiteral("x") + kFffd + QStringLiteral("y") + kEmoji;
        if (way == QStringLiteral("value")) {
            QTableView* view = f.editor()->tableViewAt(0);
            QVERIFY(view != nullptr);
            QVERIFY(view->model()->setData(view->model()->index(1, 0), input, Qt::EditRole));
        } else {
            QPlainTextEdit* cell = openCell(f);
            QVERIFY(cell != nullptr);
            if (way == QStringLiteral("paste") || way == QStringLiteral("plain")) {
                auto* data = new QMimeData;
                data->setText(input);
                QApplication::clipboard()->setMimeData(data);
                test::keyClick(cell, Qt::Key_V,
                               way == QStringLiteral("plain") ? Qt::ControlModifier | Qt::ShiftModifier
                                                              : Qt::KeyboardModifiers(Qt::ControlModifier));
            } else if (way == QStringLiteral("key")) {
                test::typeKey(cell, input);
            } else {
                commitInputMethod(cell, input);
            }
            QCoreApplication::processEvents();
            QCOMPARE(cell->toPlainText(), expected);
            test::keyClick(cell, Qt::Key_Backtab);  // commits, as in S018
        }
        f.settle();
        const std::u16string row =
            core::replaceTableCell(u"| 1 | 2 |", 0, core::tableCellFromText(expected.toStdU16String()));
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("| a | b |\n| --- | --- |\n") +
                                                QString::fromStdU16String(row) + QStringLiteral("\n\nafter\n"));
        QVERIFY(saves(f));
    }

    // The caret moves over a character outside the BMP as a whole, and Backspace deletes it whole:
    // an insertion never stands inside a pair (the reason a half at an insertion's edge has no
    // partner, text_codec.hpp).
    void theCaretNeverStandsInsideAPair() {
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("a") + kEmoji + kEmoji + QStringLiteral("b\n")));
        f.placeCaret(0, 1);
        test::keyClick(f.editor(), Qt::Key_Right);
        QCOMPARE(f.editor()->textCursor().position(), 3);
        test::keyClick(f.editor(), Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(f.editor()->textCursor().position(), 5);
        test::keyClick(f.editor(), Qt::Key_Left);
        test::keyClick(f.editor(), Qt::Key_Backspace);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("a") + kEmoji + QStringLiteral("b\n"));
        test::keyClick(f.editor(), Qt::Key_Delete);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("ab\n"));
    }

    // Scenario: a save that still meets half of a character (a way in that no one knows of) names
    // its line and column; text and file stay as they are.
    void aSaveThatMeetsHalfACharacterNamesItsLineAndColumn_data() {
        QTest::addColumn<bool>("now");
        QTest::newRow("save") << false;
        QTest::newRow("saveNow") << true;
    }

    void aSaveThatMeetsHalfACharacterNamesItsLineAndColumn() {
        QFETCH(bool, now);
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("notes.md"),
                                         "# old\r\n\r\nab\xF0\x9F\x98\x80"
                                         "cde\r\n");
        QVERIFY(f.open(path));
        f.settle();
        QTextCursor cursor(f.editor()->document()->findBlockByNumber(2));
        cursor.setPosition(cursor.block().position() + 7);  // after "ab", the emoji and "cde"
        cursor.insertText(kHigh);                           // past every way in, as a test can
        const QString text = f.editor()->toPlainText();
        QSignalSpy failed(f.session(), &ui::DocumentSession::saveFailed);
        QString message;
        if (now) {
            QVERIFY(!f.session()->saveNow(path, &message));
        } else {
            f.session()->save(path);
            QTRY_COMPARE(failed.size(), 1);
            message = failed.first().at(1).toString();
        }
        QVERIFY2(message.contains(QStringLiteral("line 3, column 7")), qPrintable(message));
        QVERIFY2(message.startsWith(QStringLiteral("notes.md: ")), qPrintable(message));
        QCOMPARE(f.editor()->toPlainText(), text);
        QVERIFY(f.editor()->document()->isModified());
        QCOMPARE(test::Fixture::readFile(path), QByteArray("# old\r\n\r\nab\xF0\x9F\x98\x80"
                                                           "cde\r\n"));
    }
};

QTEST_MAIN(tst_s004_save)
#include "tst_s004_save.moc"
