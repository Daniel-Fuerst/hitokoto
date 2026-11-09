// SPDX-License-Identifier: GPL-3.0-or-later
// S031: pasting a table from Outlook, Excel or a web page as a Markdown table (ADR 0011). Pastes go
// through the clipboard with the samples of tests/data/s031/samples: HTML as Qt hands it to the
// editor on Windows and the plain text, so the tests run alike on every platform.
#include "paste_samples.hpp"
#include "table_model.hpp"
#include "ui_test_support.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTextCursor>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>

using namespace hitokoto;

namespace {

QString text(const test::Fixture& f) {
    return f.editor()->toPlainText();
}

// The tables the samples become (S031, checked with cmark-gfm and markdown-it-py).
const QString kExample = QStringLiteral(
    "| Product | Supplier | Price per kg |\n"
    "| --- | --- | --- |\n"
    "| Green tea | Tea House North<br>Tea House South | 12,50<br>13,20 |\n"
    "| Black tea | Tea House West | 9,80 |");

const QString kOrder = QStringLiteral(
    "| Product | Boxes |\n"
    "| --- | --- |\n"
    "| Oolong | 4 |\n"
    "| Rooibos | 2 |");

const QString kExcelExample = QStringLiteral(
    "| Product | Note | Price per kg |\n"
    "| --- | --- | --- |\n"
    "| Green tea | first line<br>second line | 12,50 |\n"
    "| Black tea | 5\" tin | 9,80 |\n"
    "| Oolong | loose leaf | 15,00 |\n"
    "| Rooibos |  | 7,25 |");

const QString kExcelQuotes = QStringLiteral(
    "| Case | Text |\n"
    "| --- | --- |\n"
    "| start | \"Leaf\" Co |\n"
    "| inside | say \"hi\" |\n"
    "| break | a \"b\"<br>c |\n"
    "| \"a<br>b\" |  |");

const QString kEmpty = QStringLiteral(
    "| Product | Origin |  | Stock |\n"
    "| --- | --- | --- | --- |\n"
    "| Green tea | Japan |  | 12 |\n"
    "|  |  |  |  |\n"
    "| Black tea | India |  | 7 |");

const QString kWeb = QStringLiteral(
    "| Tea | Shop | Rating | Picture |\n"
    "| --- | --- | --- | --- |\n"
    "| Sencha | Leaf and Cup | good, fresh |  |\n"
    "| Assam | Open daily<br>9 to 18 | strong | none |");

const QString kMerged = QStringLiteral(
    "| Region | Spring | Summer | Autumn |\n"
    "| --- | --- | --- | --- |\n"
    "| North | Closed |  | 40 |\n"
    "| South | 25 | Holiday break |  |\n"
    "|  | 30 |  |  |");

const QString kEscapes = QStringLiteral(
    "| Text 1 | Text 2 | Text 3 | Text 4 |\n"
    "| --- | --- | --- | --- |\n"
    "| a\\|b | \\*x\\* | C:\\\\Dev | \\`c\\` |\n"
    "| R&D | \\&amp; | \\[l\\](u) | x\\<br>y |\n"
    "| A & B | \\&#38; | \\&foo; | \\~t\\~ \\_u\\_ |");

const QString kLines = QStringLiteral(
    "| Case | Cell |\n"
    "| --- | --- |\n"
    "| spaces around | padded |\n"
    "| empty line inside | first<br><br>third |\n"
    "| empty paragraph inside | first<br><br>third |\n"
    "| empty lines around | middle |\n"
    "| empty cell |  |\n"
    "| no-break spaces | no break |");

const QString kTwoTables = QStringLiteral(
    "| Tea | Cups |\n"
    "| --- | --- |\n"
    "| Sencha | 3 |\n"
    "\n"
    "Text between the tables.\n"
    "\n"
    "| Shop | Town |\n"
    "| --- | --- |\n"
    "| Leaf and Cup | Riverside |");

/// `doc` with `insert` at `offset`: what a plain paste gives.
QString inserted(const QString& doc, qsizetype offset, const QString& insert) {
    return doc.left(offset) + insert + doc.mid(offset);
}

/// Opens `markdown` as a file and waits until it is styled.
bool openText(test::Fixture& f, const QString& markdown) {
    if (!f.open(f.writeFile(QStringLiteral("paste.md"), markdown.toUtf8()))) {
        return false;
    }
    f.settle();
    return true;
}

/// Presses the platform's paste keys in the editor (QKeySequence::Paste).
void pressPaste(QWidget* target) {
    test::keyClick(target, Qt::Key_V, Qt::ControlModifier);
    QCoreApplication::processEvents();
}

/// Paste as Plain Text (Ctrl+Shift+V, Cmd+Shift+V on macOS).
void pressPastePlain(QWidget* target) {
    test::keyClick(target, Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
    QCoreApplication::processEvents();
}

/// Presses the first key combination of a standard key sequence.
void pressStandardKey(QWidget* target, QKeySequence::StandardKey key) {
    const QKeyCombination combination = QKeySequence(key)[0];
    test::keyClick(target, combination.key(), combination.keyboardModifiers());
    QCoreApplication::processEvents();
}

/// The action of the main window's menus whose text without mnemonics is `name`.
QAction* menuAction(const test::Fixture& f, const QString& name) {
    for (QAction* menu : f.window->menuBar()->actions()) {
        if (menu->menu() == nullptr) {
            continue;
        }
        for (QAction* a : menu->menu()->actions()) {
            if (a->text().remove(u'&') == name) {
                return a;
            }
        }
    }
    return nullptr;
}

/// The entries of the context menu `widget` shows at `point`, with their enabled state.
QList<std::pair<QString, bool>> contextMenuOf(QWidget* widget, QPoint point) {
    auto entries = std::make_shared<QList<std::pair<QString, bool>>>();
    QTimer::singleShot(0, [entries] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            for (const QAction* entry : menu->actions()) {
                if (!entry->isSeparator()) {
                    *entries << std::pair{entry->text().remove(u'&').section(u'\t', 0, 0), entry->isEnabled()};
                }
            }
            menu->close();
        }
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, widget->mapToGlobal(point));
    QCoreApplication::sendEvent(widget, &event);
    QCoreApplication::processEvents();
    return *entries;
}

std::optional<bool> entryEnabled(const QList<std::pair<QString, bool>>& entries, const QString& name) {
    for (const auto& [text, enabled] : entries) {
        if (text == name) {
            return enabled;
        }
    }
    return std::nullopt;
}

/// The position of the end of line `blockNumber`.
int endOfLine(const test::Fixture& f, int blockNumber) {
    const QTextBlock b = f.editor()->document()->findBlockByNumber(blockNumber);
    return b.position() + b.length() - 1;
}

/// A drag event at `at`: Qt 6.12 adds constructors taking QPointF and deprecates those taking
/// QPoint; Qt 6.8 has only the latter.
template <typename Event>
Event dragEvent(QPointF at, const QMimeData* data) {
    if constexpr (std::is_constructible_v<Event, QPointF, Qt::DropActions, const QMimeData*, Qt::MouseButtons,
                                          Qt::KeyboardModifiers>) {
        return Event(at, Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
    } else {
        return Event(at.toPoint(), Qt::CopyAction, data, Qt::LeftButton, Qt::NoModifier);
    }
}

/// Records the override cursor at every change of the document while it is alive.
class CursorDuringChanges {
public:
    explicit CursorDuringChanges(QTextDocument* document) {
        connection_ = QObject::connect(document, &QTextDocument::contentsChange, [this](int, int, int) {
            const QCursor* c = QGuiApplication::overrideCursor();
            shapes_ << (c != nullptr ? c->shape() : Qt::BitmapCursor);
        });
    }
    ~CursorDuringChanges() { QObject::disconnect(connection_); }
    CursorDuringChanges(const CursorDuringChanges&) = delete;
    CursorDuringChanges& operator=(const CursorDuringChanges&) = delete;

    /// Whether there were changes and the wait cursor showed during each of them.
    [[nodiscard]] bool waitCursorThroughout() const {
        return !shapes_.isEmpty() &&
               std::all_of(shapes_.begin(), shapes_.end(), [](Qt::CursorShape s) { return s == Qt::WaitCursor; });
    }

private:
    QMetaObject::Connection connection_;
    QList<Qt::CursorShape> shapes_;
};

}  // namespace

class tst_s031_paste_tables : public QObject {
    Q_OBJECT
private slots:
    void init() { QApplication::clipboard()->clear(); }

    void theSamplesAreThere() {
        // Guards the resources: every sample of the README has its HTML and its plain text.
        for (const char* name :
             {"word-t1-example", "word-t3-merged", "outlook-t1-example", "outlook-t2-text-around",
              "outlook-t2-text-around-break", "outlook-t4-escapes", "outlook-t5-lines", "excel-t6-example",
              "excel-t7-empty", "excel-quotes", "firefox-t4-escapes", "firefox-t8-web", "firefox-t9-two-tables",
              "firefox-t10-one-row", "firefox-t11-one-column", "firefox-t12-nested"}) {
            QVERIFY2(!test::s031Html(QString::fromLatin1(name)).isEmpty(), name);
            QVERIFY2(!test::s031Text(QString::fromLatin1(name)).isEmpty(), name);
        }
    }

    // Scenario: a table from Outlook becomes a Markdown table.
    void aTableFromOutlookBecomesAMarkdownTable() {
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("Para one.\n\nPara two.\n")));
        f.placeCaret(1);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), QStringLiteral("Para one.\n\n") + kExample + QStringLiteral("\n\nPara two.\n"));

        // Shown as a table (S018), the two-line cells on two lines.
        QTableView* view = f.editor()->tableViewAt(2);
        QVERIFY(view != nullptr);
        QAbstractItemModel* model = view->model();
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(model->columnCount(), 3);
        QCOMPARE(model->data(model->index(1, 1)).toString(), QStringLiteral("Tea House North\nTea House South"));
        QCOMPARE(model->data(model->index(1, 2)).toString(), QStringLiteral("12,50\n13,20"));
    }

    void wordGivesTheSameTableAsOutlook() {
        test::Fixture f;
        test::putSampleOnClipboard(QStringLiteral("word-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), kExample);
    }

    void newLinesTakeTheMajorityLineEnding() {
        // ADR 0002: the paste is the only change; the file is saved byte for byte with CR LF.
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("crlf.md"), "Para one.\r\n\r\nPara two.\r\n")));
        f.settle();
        f.placeCaret(1);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        const QString path = f.dir.filePath(QStringLiteral("crlf-saved.md"));
        QVERIFY(f.saveAndWait(path));
        QString expected = QStringLiteral("Para one.\n\n") + kExample + QStringLiteral("\n\nPara two.\n");
        expected.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        QCOMPARE(test::Fixture::readFile(path), expected.toUtf8());
    }

    // Scenario: text before and after a table from Outlook.
    void textBeforeAndAfterATableFromOutlook_data() {
        QTest::addColumn<QString>("sample");
        QTest::newRow("paragraph after") << QStringLiteral("outlook-t2-text-around");
        QTest::newRow("line after, no paragraph") << QStringLiteral("outlook-t2-text-around-break");
    }

    void textBeforeAndAfterATableFromOutlook() {
        QFETCH(QString, sample);
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("Para one.\n\nPara two.\n")));
        f.placeCaret(1);
        test::putSampleOnClipboard(sample);
        pressPaste(f.editor());
        f.settle();
        // The text before goes on the caret's line and the text after continues it, as a plain
        // paste would; one blank line on each side of the table.
        QCOMPARE(text(f), QStringLiteral("Para one.\nThe spring order is below.\n\n") + kOrder +
                              QStringLiteral("\n\nPlease confirm by Friday.\nPara two.\n"));
        QVERIFY(f.editor()->tableViewAt(3) != nullptr);
    }

    // Scenario: a table from Excel, with and without HTML.
    void aTableFromExcel_data() {
        QTest::addColumn<QString>("sample");
        QTest::addColumn<bool>("withHtml");
        QTest::addColumn<QString>("expected");
        QTest::newRow("example, HTML") << QStringLiteral("excel-t6-example") << true << kExcelExample;
        QTest::newRow("example, plain text only") << QStringLiteral("excel-t6-example") << false << kExcelExample;
        QTest::newRow("empty column and row, HTML") << QStringLiteral("excel-t7-empty") << true << kEmpty;
        QTest::newRow("empty column and row, plain text only") << QStringLiteral("excel-t7-empty") << false << kEmpty;
        QTest::newRow("quotes, HTML") << QStringLiteral("excel-quotes") << true << kExcelQuotes;
        QTest::newRow("quotes, plain text only") << QStringLiteral("excel-quotes") << false << kExcelQuotes;
    }

    void aTableFromExcel() {
        QFETCH(QString, sample);
        QFETCH(bool, withHtml);
        QFETCH(QString, expected);
        test::Fixture f;
        test::putOnClipboard(withHtml ? test::s031Html(sample) : QString(), test::s031Text(sample));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), expected);
        QVERIFY(f.editor()->tableViewAt(0) != nullptr);
    }

    // Scenario: a table from a web page and from Word; escaping; lines in cells.
    void samplesBecomeTheirTables_data() {
        QTest::addColumn<QString>("sample");
        QTest::addColumn<QString>("expected");
        QTest::newRow("Firefox: th, link, bold, image") << QStringLiteral("firefox-t8-web") << kWeb;
        QTest::newRow("Word: merged cells") << QStringLiteral("word-t3-merged") << kMerged;
        QTest::newRow("Outlook: escapes") << QStringLiteral("outlook-t4-escapes") << kEscapes;
        QTest::newRow("Firefox: escapes") << QStringLiteral("firefox-t4-escapes") << kEscapes;
        QTest::newRow("Outlook: lines and spaces") << QStringLiteral("outlook-t5-lines") << kLines;
        QTest::newRow("Firefox: two tables, text between") << QStringLiteral("firefox-t9-two-tables") << kTwoTables;
    }

    void samplesBecomeTheirTables() {
        QFETCH(QString, sample);
        QFETCH(QString, expected);
        test::Fixture f;
        test::putSampleOnClipboard(sample);
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), expected);
    }

    // Scenario: escaping keeps the text as copied, with and without the caret in the cell.
    void escapingKeepsTheTextAsCopied() {
        test::Fixture f;
        test::putSampleOnClipboard(QStringLiteral("outlook-t4-escapes"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), kEscapes);
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QAbstractItemModel* model = view->model();
        // Cells without the caret show the copied text: Qt renders the cell's Markdown with the
        // escapes resolved (TableDelegate, S018), in Qt 6.8 as in 6.12.
        const QStringList copied = {QStringLiteral("a|b"),    QStringLiteral("*x*"),   QStringLiteral("C:\\Dev"),
                                    QStringLiteral("`c`"),    QStringLiteral("R&D"),   QStringLiteral("&amp;"),
                                    QStringLiteral("[l](u)"), QStringLiteral("x<br>y")};
        for (int i = 0; i < copied.size(); ++i) {
            const QModelIndex index = model->index(1 + i / 4, i % 4);
            QTextDocument shown;
            shown.setMarkdown(model->data(index, ui::TableModel::MarkdownRole)
                                  .toString()
                                  .replace(QStringLiteral("\\|"), QStringLiteral("|")),
                              QTextDocument::MarkdownDialectGitHub);
            QCOMPARE(shown.toPlainText(), copied.at(i));
        }
        // The cell with the caret shows its Markdown, backslashes included; x<br>y has no break.
        auto* cell = qobject_cast<QPlainTextEdit*>([&] {
            view->setCurrentIndex(model->index(2, 3));
            view->edit(model->index(2, 3));
            QCoreApplication::processEvents();
            return QApplication::focusWidget();
        }());
        QVERIFY(cell != nullptr);
        QCOMPARE(cell->toPlainText(), QStringLiteral("x\\<br>y"));
        QCOMPARE(model->data(model->index(2, 3)).toString(), QStringLiteral("x\\<br>y"));
    }

    // Scenario: the table lands as its own block.
    void theTableLandsAsItsOwnBlock_data() {
        QTest::addColumn<QString>("document");
        QTest::addColumn<int>("anchor");
        QTest::addColumn<int>("position");
        QTest::addColumn<QString>("expected");
        QTest::newRow("middle of a line") << QStringLiteral("Hello world\n") << 6 << 6
                                          << QStringLiteral("Hello \n\n") + kExample + QStringLiteral("\n\nworld\n");
        QTest::newRow("end of a line") << QStringLiteral("Hello\n") << 5 << 5
                                       << QStringLiteral("Hello\n\n") + kExample + QStringLiteral("\n");
        QTest::newRow("end of a line before text")
            << QStringLiteral("Hello\nnext\n") << 5 << 5
            << QStringLiteral("Hello\n\n") + kExample + QStringLiteral("\n\nnext\n");
        QTest::newRow("start of the document")
            << QStringLiteral("Hello\n") << 0 << 0 << kExample + QStringLiteral("\n\nHello\n");
        QTest::newRow("start of a line after text")
            << QStringLiteral("one\ntwo\n") << 4 << 4
            << QStringLiteral("one\n\n") + kExample + QStringLiteral("\n\ntwo\n");
        QTest::newRow("next to blank lines") << QStringLiteral("one\n\n\n\ntwo\n") << 5 << 5
                                             << QStringLiteral("one\n\n") + kExample + QStringLiteral("\n\ntwo\n");
        QTest::newRow("selection across two lines")
            << QStringLiteral("one two\nthree four\n") << 4 << 13
            << QStringLiteral("one \n\n") + kExample + QStringLiteral("\n\n four\n");
    }

    void theTableLandsAsItsOwnBlock() {
        QFETCH(QString, document);
        QFETCH(int, anchor);
        QFETCH(int, position);
        QFETCH(QString, expected);
        test::Fixture f;
        QVERIFY(openText(f, document));
        QTextCursor c(f.editor()->document());
        c.setPosition(anchor);
        c.setPosition(position, QTextCursor::KeepAnchor);
        f.editor()->setTextCursor(c);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), expected);
        // The caret stands after the last pasted character: the end of the table's last row.
        const qsizetype tableEnd = expected.indexOf(kExample) + kExample.size();
        QCOMPARE(f.editor()->textCursor().position(), int(tableEnd));
        QVERIFY(!f.editor()->textCursor().hasSelection());
    }

    void theCaretEndsAfterPastedTextAfterTheTable() {
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("Para one.\n\nPara two.\n")));
        f.placeCaret(1);
        test::putSampleOnClipboard(QStringLiteral("outlook-t2-text-around"));
        pressPaste(f.editor());
        f.settle();
        const qsizetype end = text(f).indexOf(QStringLiteral("Friday.")) + 7;
        QCOMPARE(f.editor()->textCursor().position(), int(end));
    }

    // Scenario: next to an existing table.
    void onTheBlankLineBelowATableOneBlankLineSeparatesTheTables() {
        const QString table = QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n");
        test::Fixture f;
        QVERIFY(openText(f, table + QStringLiteral("\nafter\n")));
        QVERIFY(f.editor()->tableViewAt(0) != nullptr);
        f.placeCaret(3);  // the blank line directly below the table
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), table + QStringLiteral("\n") + kExample + QStringLiteral("\n\nafter\n"));
        QVERIFY(f.editor()->tableViewAt(0) != nullptr);
        QVERIFY(f.editor()->tableViewAt(4) != nullptr);
        QVERIFY(f.editor()->tableViewAt(0) != f.editor()->tableViewAt(4));
    }

    void onATablesMarkdownLinesThePlainTextIsPasted() {
        const QString markdown = QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n\nafter\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        view->setFocus();
        view->setCurrentIndex(view->model()->index(1, 1));
        test::keyClick(view, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);  // source view
        f.settle();
        QVERIFY(f.editor()->isTableSourceShown(0));
        const int at = endOfLine(f, 0);
        QTextCursor c(f.editor()->document());
        c.setPosition(at);
        f.editor()->setTextCursor(c);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), inserted(markdown, at, test::s031Text(QStringLiteral("outlook-t1-example"))));
    }

    // Scenario: what stays plain text.
    void whatStaysPlainText_data() {
        QTest::addColumn<QString>("html");
        QTest::addColumn<QString>("plain");
        const auto sample = [](const char* name) {
            return std::pair{test::s031Html(QString::fromLatin1(name)), test::s031Text(QString::fromLatin1(name))};
        };
        const auto [oneRowHtml, oneRowText] = sample("firefox-t10-one-row");
        const auto [oneColumnHtml, oneColumnText] = sample("firefox-t11-one-column");
        const auto [nestedHtml, nestedText] = sample("firefox-t12-nested");
        QTest::newRow("one row") << oneRowHtml << oneRowText;
        QTest::newRow("one column") << oneColumnHtml << oneColumnText;
        QTest::newRow("a table in a cell") << nestedHtml << nestedText;
        QTest::newRow("HTML without a table that counts, Excel-like text")
            << QStringLiteral("<p>a</p>") << QStringLiteral("a\tb\nc\td\n");
        QTest::newRow("plain text, rows of different widths")
            << QString() << test::s031Text(QStringLiteral("outlook-t1-example"));
        QTest::newRow("plain text, tab-indented lines") << QString() << QStringLiteral("\ta\tb\n\tc\td\n");
    }

    void whatStaysPlainText() {
        QFETCH(QString, html);
        QFETCH(QString, plain);
        const QString markdown = QStringLiteral("Para one.\n\nPara two.\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        f.placeCaret(1);
        test::putOnClipboard(html, plain);
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), inserted(markdown, 10, plain));
    }

    void inACodeBlockOrFrontMatterThePlainTextIsPasted_data() {
        QTest::addColumn<QString>("markdown");
        QTest::addColumn<int>("line");
        QTest::addColumn<int>("column");
        QTest::newRow("fenced code block") << QStringLiteral("```\ncode\n```\n\ntext\n") << 1 << 4;
        QTest::newRow("front matter") << QStringLiteral("---\ntitle: x\n---\n\ntext\n") << 1 << 8;
    }

    void inACodeBlockOrFrontMatterThePlainTextIsPasted() {
        QFETCH(QString, markdown);
        QFETCH(int, line);
        QFETCH(int, column);
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        f.placeCaret(line, column);
        f.settle();
        const int at = f.editor()->textCursor().position();
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), inserted(markdown, at, test::s031Text(QStringLiteral("outlook-t1-example"))));
    }

    void inACellOfAShownTableThePlainTextIsPasted() {
        const QString markdown = QStringLiteral("| a | b |\n| --- | --- |\n| 1 | 2 |\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        view->setCurrentIndex(view->model()->index(1, 0));
        view->edit(view->model()->index(1, 0));
        QCoreApplication::processEvents();
        auto* cell = qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
        QVERIFY(cell != nullptr);
        test::keyClick(cell, Qt::Key_A, Qt::ControlModifier);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        pressPaste(cell);
        test::keyClick(cell, Qt::Key_Backtab);  // commits, as in S018
        f.settle();
        const std::u16string plain = test::s031Text(QStringLiteral("outlook-t1-example")).toStdU16String();
        const std::u16string row = core::replaceTableCell(u"| 1 | 2 |", 0, core::tableCellFromText(plain));
        QCOMPARE(text(f),
                 QStringLiteral("| a | b |\n| --- | --- |\n") + QString::fromStdU16String(row) + QStringLiteral("\n"));
    }

    void aDropIsNotConverted() {
        const QString markdown = QStringLiteral("Para one.\n\nPara two.\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        QMimeData data;
        data.setHtml(test::s031Html(QStringLiteral("outlook-t1-example")));
        data.setText(test::s031Text(QStringLiteral("outlook-t1-example")));
        QTextCursor c(f.editor()->document()->findBlockByNumber(1));
        const QPoint at = f.editor()->cursorRect(c).center();
        QWidget* viewport = f.editor()->viewport();
        QDragEnterEvent enter = dragEvent<QDragEnterEvent>(QPointF(at), &data);
        QCoreApplication::sendEvent(viewport, &enter);
        QDragMoveEvent move = dragEvent<QDragMoveEvent>(QPointF(at), &data);
        QCoreApplication::sendEvent(viewport, &move);
        QDropEvent drop(QPointF(at), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(viewport, &drop);
        f.settle();
        QCOMPARE(text(f), inserted(markdown, 10, test::s031Text(QStringLiteral("outlook-t1-example"))));
    }

    // Scenario: paste as plain text.
    void pasteAsPlainTextNeverConverts() {
        const QString markdown = QStringLiteral("Para one.\n\nPara two.\n");
        const QString plain = test::s031Text(QStringLiteral("outlook-t1-example"));
        {
            test::Fixture f;
            QVERIFY(openText(f, markdown));
            f.placeCaret(1);
            test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
            pressPastePlain(f.editor());
            f.settle();
            QCOMPARE(text(f), inserted(markdown, 10, plain));
        }
        {
            // Excel's plain text alone, from the Edit menu.
            test::Fixture f;
            QVERIFY(openText(f, markdown));
            f.placeCaret(1);
            test::putOnClipboard(QString(), test::s031Text(QStringLiteral("excel-t6-example")));
            QAction* action = menuAction(f, QStringLiteral("Paste as Plain Text"));
            QVERIFY(action != nullptr);
            QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
            action->trigger();
            f.settle();
            QCOMPARE(text(f), inserted(markdown, 10, test::s031Text(QStringLiteral("excel-t6-example"))));
        }
    }

    void pasteAsPlainTextIsInTheEditorsContextMenu() {
        test::Fixture f;
        QVERIFY(openText(f, QStringLiteral("Para one.\n\nPara two.\n")));
        f.placeCaret(1);
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        QWidget* viewport = f.editor()->viewport();
        const QPoint at = f.editor()->cursorRect().center();
        auto entries = contextMenuOf(viewport, at);
        QCOMPARE(entryEnabled(entries, QStringLiteral("Paste as Plain Text")), std::optional<bool>(true));

        // Triggered from the context menu, the plain text is pasted.
        QTimer::singleShot(0, [] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                for (QAction* entry : menu->actions()) {
                    if (entry->text().remove(u'&').section(u'\t', 0, 0) == QStringLiteral("Paste as Plain Text")) {
                        menu->close();
                        entry->trigger();
                        return;
                    }
                }
                menu->close();
            }
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, viewport->mapToGlobal(at));
        QCoreApplication::sendEvent(viewport, &event);
        f.settle();
        QCOMPARE(text(f), inserted(QStringLiteral("Para one.\n\nPara two.\n"), 10,
                                   test::s031Text(QStringLiteral("outlook-t1-example"))));
    }

    void pasteAsPlainTextIsDisabledWithoutText() {
        test::Fixture f;
        QAction* action = menuAction(f, QStringLiteral("Paste as Plain Text"));
        QVERIFY(action != nullptr);
        QApplication::clipboard()->clear();
        QCoreApplication::processEvents();
        QVERIFY(!action->isEnabled());
        auto entries = contextMenuOf(f.editor()->viewport(), f.editor()->cursorRect().center());
        QCOMPARE(entryEnabled(entries, QStringLiteral("Paste as Plain Text")), std::optional<bool>(false));
        test::putOnClipboard(test::s031Html(QStringLiteral("firefox-t8-web")), QString());  // HTML only
        QCoreApplication::processEvents();
        QVERIFY(!action->isEnabled());
        QApplication::clipboard()->setText(QStringLiteral("text"));
        QCoreApplication::processEvents();
        QVERIFY(action->isEnabled());
    }

    // Scenario: one undo step.
    void oneUndoStep() {
        const QString markdown = QStringLiteral("Para one.\n\nPara two.\n");
        test::Fixture f;
        QVERIFY(openText(f, markdown));
        f.placeCaret(1);
        test::putSampleOnClipboard(QStringLiteral("outlook-t2-text-around"));
        pressPaste(f.editor());
        f.settle();
        const QString pasted = text(f);
        QVERIFY(pasted.contains(kOrder));
        pressStandardKey(f.editor(), QKeySequence::Undo);
        f.settle();
        QCOMPARE(text(f), markdown);
        QCOMPARE(f.editor()->textCursor().position(), 10);
        QVERIFY(!f.editor()->textCursor().hasSelection());
        pressStandardKey(f.editor(), QKeySequence::Redo);
        f.settle();
        QCOMPARE(text(f), pasted);
    }

    void undoAfterReplacingASelection() {
        // Review O8: undo leaves the caret where undoing any paste over this selection leaves it;
        // the replaced selection is not selected again.
        const QString markdown = QStringLiteral("one two\nthree four\n");
        const auto pasteAndUndo = [&](bool table) {
            test::Fixture f;
            if (!openText(f, markdown)) {
                return -1;
            }
            QTextCursor c(f.editor()->document());
            c.setPosition(4);
            c.setPosition(13, QTextCursor::KeepAnchor);
            f.editor()->setTextCursor(c);
            if (table) {
                test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
            } else {
                QApplication::clipboard()->setText(QStringLiteral("plain"));
            }
            pressPaste(f.editor());
            f.settle();
            if (table && !text(f).contains(kExample)) {
                return -2;
            }
            pressStandardKey(f.editor(), QKeySequence::Undo);
            f.settle();
            if (text(f) != markdown || f.editor()->textCursor().hasSelection()) {
                return -3;
            }
            return f.editor()->textCursor().position();
        };
        const int afterPlain = pasteAndUndo(false);
        QVERIFY(afterPlain >= 0);
        QCOMPARE(pasteAndUndo(true), afterPlain);
    }

    // Scenario: paste time grows linearly with the cells (timing in hitokoto-bench; here the rules
    // and the wait cursor).
    void largeTablesAreConvertedByTheSameRules_data() {
        QTest::addColumn<QString>("sample");
        QTest::addColumn<int>("rows");
        QTest::addColumn<int>("columns");
        for (const char* sample : {"outlook-t1-example", "excel-t6-example"}) {
            for (const auto& [rows, columns] : {std::pair{50, 10}, std::pair{500, 10}, std::pair{50, 100}}) {
                QTest::addRow("%s %dx%d", sample, rows, columns) << QString::fromLatin1(sample) << rows << columns;
            }
        }
    }

    void largeTablesAreConvertedByTheSameRules() {
        QFETCH(QString, sample);
        QFETCH(int, rows);
        QFETCH(int, columns);
        const QString html = test::s031GeneratedHtml(test::s031Html(sample), rows, columns);
        QVERIFY(!html.isEmpty());
        test::Fixture f;
        test::putOnClipboard(html, test::s031GeneratedText(rows, columns));
        const CursorDuringChanges cursor(f.editor()->document());
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), test::s031GeneratedMarkdown(rows, columns));
        QVERIFY(cursor.waitCursorThroughout());
        QVERIFY(QGuiApplication::overrideCursor() == nullptr);
        QTableView* view = f.editor()->tableViewAt(0);
        QVERIFY(view != nullptr);
        QCOMPARE(view->model()->rowCount(), rows);
        QCOMPARE(view->model()->columnCount(), columns);
    }

    void theWaitCursorShowsWhileASmallPasteConverts() {
        test::Fixture f;
        test::putSampleOnClipboard(QStringLiteral("outlook-t1-example"));
        const CursorDuringChanges cursor(f.editor()->document());
        pressPaste(f.editor());
        f.settle();
        QCOMPARE(text(f), kExample);
        QVERIFY(cursor.waitCursorThroughout());
        QVERIFY(QGuiApplication::overrideCursor() == nullptr);
    }
};

QTEST_MAIN(tst_s031_paste_tables)
#include "tst_s031_paste_tables.moc"
