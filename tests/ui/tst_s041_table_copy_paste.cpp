// SPDX-License-Identifier: GPL-3.0-or-later
// S041: copying and pasting table cells as Markdown. Keys only through test::keyClick (CLAUDE.md).
#include "editor_widget.hpp"
#include "table_view.hpp"
#include "ui_test_support.hpp"

#include <QAbstractScrollArea>
#include <QAction>
#include <QClipboard>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTimer>

#include <memory>

using namespace hitokoto;

namespace {

// The table of the story's scenarios: cells with emphasis, an escaped pipe and a line break,
// columns aligned left, centre, right and not at all.
constexpr auto kTable =
    "above\n"
    "\n"
    "| L | C | R | N |\n"
    "| :--- | :---: | ---: | --- |\n"
    "| **bold** | a \\| b | x<br>y | 1 |\n"
    "| p | q | r | s |\n"
    "| t | u | v | w |\n"
    "\n"
    "below";
constexpr int kHeaderBlock = 2;
constexpr int kBelowBlock = 8;

// What copying the cells of rows 1 to 2, columns 0 to 2 gives (decision 2).
constexpr auto kCopiedBlock =
    "| **bold** | a \\| b | x<br>y |\n"
    "| :--- | :---: | ---: |\n"
    "| p | q | r |";

const QString kMarkdown = QStringLiteral("text/markdown");

QPlainTextEdit* focusedCell() {
    return qobject_cast<QPlainTextEdit*>(QApplication::focusWidget());
}

void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    test::keyClick(QApplication::focusWidget(), key, modifiers);
    QCoreApplication::processEvents();
}

/// Presses the first key combination of a standard key sequence in the focused widget.
void pressStandardKey(QKeySequence::StandardKey key) {
    const QKeyCombination combination = QKeySequence(key)[0];
    press(combination.key(), combination.keyboardModifiers());
}

/// Clicks the cell, then puts the caret at `position` of its text (-1: at its end).
QPlainTextEdit* caretIn(QTableView* view, int row, int column, int position) {
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {},
                      view->visualRect(view->model()->index(row, column)).center());
    QCoreApplication::processEvents();
    QPlainTextEdit* cell = focusedCell();
    if (cell != nullptr) {
        QTextCursor cursor = cell->textCursor();
        cursor.setPosition(position < 0 ? static_cast<int>(cell->toPlainText().size()) : position);
        cell->setTextCursor(cursor);
    }
    return cell;
}

/// Selects the cells of rows `top`..`bottom`, columns `left`..`right` with Shift and the arrows
/// (S040, decision 8): from the end of the top left cell's text.
bool selectCells(QTableView* view, int top, int left, int bottom, int right) {
    if (caretIn(view, top, left, -1) == nullptr) {
        return false;
    }
    press(Qt::Key_Right, Qt::ShiftModifier);  // the cell itself
    for (int c = left; c < right; ++c) {
        press(Qt::Key_Right, Qt::ShiftModifier);
    }
    for (int r = top; r < bottom; ++r) {
        press(Qt::Key_Down, Qt::ShiftModifier);
    }
    auto* table = qobject_cast<ui::TableView*>(view);
    return table != nullptr && table->cellSelection() == core::TableCellRange{top, left, bottom, right};
}

QAction* action(QTableView* view, const char* name) {
    return view->findChild<QAction*>(QString::fromLatin1(name));
}

/// The entries of a cell's context menu, without accelerators and shortcuts, with `disabled`
/// marking those that cannot be chosen.
QStringList contextMenuOf(QWidget* widget) {
    auto texts = std::make_shared<QStringList>();
    QTimer::singleShot(0, [texts] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            for (const QAction* entry : menu->actions()) {
                if (!entry->isSeparator() && entry->isVisible()) {
                    QString text = entry->text().remove(u'&').section(u'\t', 0, 0);
                    *texts << (entry->isEnabled() ? text : text + QStringLiteral(" (disabled)"));
                }
            }
            menu->close();
        }
    });
    QWidget* target = widget;
    if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
        target = area->viewport();
    }
    const QPoint point = target->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, target->mapToGlobal(point));
    QCoreApplication::sendEvent(target, &event);
    QCoreApplication::processEvents();
    widget->window()->activateWindow();
    (void)QTest::qWaitForWindowActive(widget->window());
    return *texts;
}

/// The clipboard's data; empty data while it holds none, so that a check fails instead of crashing.
const QMimeData* clipboard() {
    static const QMimeData empty;
    const QMimeData* data = QApplication::clipboard()->mimeData();
    return data != nullptr ? data : &empty;
}

/// The clipboard's plain text with the platform's line breaks as '\n' (decision 3: Qt writes
/// CRLF on Windows).
QString clipboardText() {
    QString text = clipboard()->text();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return text;
}

QByteArray clipboardMarkdown() {
    return clipboard()->data(kMarkdown);
}

/// Clipboard data as hitokoto's own copy carries it: the marker and Markdown (decision 5).
QMimeData* ownCopy(const QString& markdown, const QString& plain) {
    auto* data = new QMimeData;
    data->setText(plain);
    data->setData(QStringLiteral("text/markdown"), markdown.toUtf8());
    data->setData(ui::EditorWidget::ownCopyFormat(), QByteArrayLiteral("1"));
    return data;
}

/// The document's text with `from` replaced by `to` once.
QString replaced(const QString& from, const QString& to) {
    QString text = QString::fromUtf8(kTable);
    const qsizetype at = text.indexOf(from);
    return at < 0 ? QString() : text.replace(at, from.size(), to);
}

}  // namespace

class tst_s041_table_copy_paste : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<test::Fixture> f_;

    QString text() const { return f_->editor()->toPlainText(); }
    QTableView* view() const { return f_->editor()->tableViewAt(kHeaderBlock); }

    /// Ctrl+Z in the focused widget; then the file as it was.
    bool undoneOnce() {
        press(Qt::Key_Z, Qt::ControlModifier);
        f_->settle();
        return text() == QString::fromUtf8(kTable);
    }

private slots:
    void init() {
        f_ = std::make_unique<test::Fixture>();
        QVERIFY(f_->open(f_->writeFile(QStringLiteral("table.md"), kTable)));
        f_->settle();
        QVERIFY(view() != nullptr);
        QApplication::clipboard()->clear();
    }

    void cleanup() { f_.reset(); }

    // Scenario: copying cells gives a Markdown table.
    void copyingCellsGivesAMarkdownTable() {
        QVERIFY(selectCells(view(), 1, 0, 2, 2));
        pressStandardKey(QKeySequence::Copy);
        QCOMPARE(clipboardText(), QString::fromUtf8(kCopiedBlock));
        QCOMPARE(clipboardMarkdown(), QByteArray(kCopiedBlock));
        QVERIFY(clipboard()->hasFormat(ui::EditorWidget::ownCopyFormat()));
        QVERIFY(!clipboard()->hasHtml());
        // Copying changes nothing and keeps the selection.
        QCOMPARE(text(), QString::fromUtf8(kTable));
        QCOMPARE(qobject_cast<ui::TableView*>(view())->cellSelection(), (core::TableCellRange{1, 0, 2, 2}));
    }

    void copyingTheWholeTableGivesItsLines() {
        QVERIFY(caretIn(view(), 1, 0, 0) != nullptr);
        qobject_cast<ui::TableView*>(view())->selectTable();
        pressStandardKey(QKeySequence::Copy);
        const QString lines = QString::fromUtf8(kTable).section(QStringLiteral("\n\n"), 1, 1);
        QCOMPARE(clipboardText(), lines);
        QCOMPARE(clipboardMarkdown(), lines.toUtf8());
    }

    void copyingOneCellGivesItsText() {
        QVERIFY(selectCells(view(), 1, 2, 1, 2));
        pressStandardKey(QKeySequence::Copy);
        QCOMPARE(clipboardText(), QStringLiteral("x\ny"));
        QCOMPARE(clipboardMarkdown(), QByteArray("x\ny"));
    }

    void theMarkdownHasLfLineEndings() {
        QVERIFY(selectCells(view(), 1, 0, 3, 3));
        pressStandardKey(QKeySequence::Copy);
        QVERIFY(!clipboardMarkdown().isEmpty());
        QVERIFY(!clipboardMarkdown().contains('\r'));
        QCOMPARE(clipboardMarkdown().count('\n'), 3);
    }

    // Scenario: copying as tab-separated text.
    void copyingAsTabSeparatedText() {
        QVERIFY(selectCells(view(), 1, 0, 2, 3));
        QAction* tsv = action(view(), "tableCopyTabSeparated");
        QVERIFY(tsv != nullptr);
        QVERIFY(tsv->isEnabled());
        // Directly below Copy in the cell's context menu.
        const QStringList entries = contextMenuOf(focusedCell());
        const qsizetype copy = entries.indexOf(QStringLiteral("Copy"));
        QVERIFY2(copy >= 0, qPrintable(entries.join(u'|')));
        QCOMPARE(entries.value(copy + 1), QStringLiteral("Copy as Tab-Separated Text"));
        tsv->trigger();
        QCOMPARE(clipboard()->formats(), QStringList{QStringLiteral("text/plain")});
        QCOMPARE(clipboardText(), QStringLiteral("bold\ta | b\t\"x\ny\"\t1\np\tq\tr\ts\n"));
        QCOMPARE(text(), QString::fromUtf8(kTable));
    }

    void tabSeparatedTextNeedsCells() {
        QVERIFY(caretIn(view(), 1, 0, 0) != nullptr);
        QAction* tsv = action(view(), "tableCopyTabSeparated");
        QVERIFY(tsv != nullptr);
        QVERIFY(!tsv->isEnabled());
        const QStringList entries = contextMenuOf(focusedCell());
        QVERIFY2(entries.contains(QStringLiteral("Copy as Tab-Separated Text (disabled)")),
                 qPrintable(entries.join(u'|')));
    }

    // Scenario: hitokoto's own copy has no HTML (Armin's sample, Text1 to Text2 selected).
    void theOwnCopyHasNoHtml() {
        const QString sample = QStringLiteral(
            "Text1\n\n| Linke Spalte | Rechte Spalte |\n| --- | --- |\n| Wert 1 | Wert 2 |\n\n\nText2\n");
        QVERIFY(f_->open(f_->writeFile(QStringLiteral("sample.md"), sample.toUtf8())));
        f_->settle();
        f_->editor()->setFocus();
        f_->editor()->selectAll();
        pressStandardKey(QKeySequence::Copy);
        QCOMPARE(clipboard()->formats(),
                 (QStringList{QStringLiteral("text/plain"), kMarkdown, ui::EditorWidget::ownCopyFormat()}));
        QVERIFY(!clipboard()->hasHtml());
        QCOMPARE(clipboardText(), sample);
        QCOMPARE(clipboardMarkdown(), sample.toUtf8());
    }

    // Scenario: paste prefers hitokoto's own Markdown.
    void pastePrefersTheOwnMarkdown() {
        QVERIFY(selectCells(view(), 1, 0, 2, 2));
        pressStandardKey(QKeySequence::Copy);
        f_->editor()->setFocus();
        f_->placeCaret(kBelowBlock, 5);
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable) + QStringLiteral("\n\n") + QString::fromUtf8(kCopiedBlock));
        QVERIFY(f_->editor()->tableViewAt(kBelowBlock + 2) != nullptr);
        f_->editor()->setFocus();
        QVERIFY(undoneOnce());
    }

    void otherOwnMarkdownIsInsertedAsText() {
        QApplication::clipboard()->setMimeData(ownCopy(QStringLiteral("**md** | x"), QStringLiteral("plain")));
        f_->editor()->setFocus();
        f_->placeCaret(kBelowBlock, 5);
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable) + QStringLiteral("**md** | x"));
    }

    void otherProgramsMarkdownIsNotPreferred() {
        auto* data = new QMimeData;
        data->setText(QStringLiteral("plain"));
        data->setData(kMarkdown, QByteArrayLiteral("| x | y |\n| - | - |\n| 1 | 2 |"));
        QApplication::clipboard()->setMimeData(data);
        f_->editor()->setFocus();
        f_->placeCaret(kBelowBlock, 5);
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable) + QStringLiteral("plain"));
    }

    // Scenario: cut and paste act on cells.
    void cutCopiesAndEmptiesTheCells() {
        QVERIFY(selectCells(view(), 1, 0, 1, 1));
        pressStandardKey(QKeySequence::Cut);
        f_->settle();
        QCOMPARE(clipboardText(), QStringLiteral("| **bold** | a \\| b |\n| :--- | :---: |"));
        QCOMPARE(text(), replaced(QStringLiteral("| **bold** | a \\| b | x<br>y | 1 |"),
                                  QStringLiteral("|  |  | x<br>y | 1 |")));
        QVERIFY(undoneOnce());
    }

    void cuttingTheWholeTableRemovesIt() {
        QVERIFY(caretIn(view(), 1, 0, 0) != nullptr);
        qobject_cast<ui::TableView*>(view())->selectTable();
        pressStandardKey(QKeySequence::Cut);
        f_->settle();
        QCOMPARE(clipboardText(), QString::fromUtf8(kTable).section(QStringLiteral("\n\n"), 1, 1));
        QCOMPARE(text(), QStringLiteral("above\n\nbelow"));
    }

    void pastingATableAddsARowAndKeepsTheOtherCells() {
        QApplication::clipboard()->setMimeData(
            ownCopy(QString::fromUtf8(kCopiedBlock), QString::fromUtf8(kCopiedBlock)));
        QVERIFY(selectCells(view(), 3, 1, 3, 1));
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), replaced(QStringLiteral("| t | u | v | w |"),
                                  QStringLiteral("| t | **bold** | a \\| b | x<br>y |\n|  | p | q | r |")));
        QVERIFY(undoneOnce());
    }

    void pastingTooManyColumnsChangesNothing() {
        QStringList header;
        QStringList delimiter;
        for (int i = 0; i < 253; ++i) {
            header << QStringLiteral("c%1").arg(i);
            delimiter << QStringLiteral("---");
        }
        const QString wide = QStringLiteral("| %1 |\n| %2 |")
                                 .arg(header.join(QStringLiteral(" | ")), delimiter.join(QStringLiteral(" | ")));
        QApplication::clipboard()->setMimeData(ownCopy(wide, wide));
        QVERIFY(selectCells(view(), 1, 3, 1, 3));  // 3 + 253 = 256 columns
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), QString::fromUtf8(kTable));
        QVERIFY(!f_->editor()->document()->isModified());
    }

    void textFillsEverySelectedCell() {
        auto* data = new QMimeData;
        data->setText(QStringLiteral("z|z"));
        QApplication::clipboard()->setMimeData(data);
        QVERIFY(selectCells(view(), 2, 0, 2, 2));
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(),
                 replaced(QStringLiteral("| p | q | r | s |"), QStringLiteral("| z\\|z | z\\|z | z\\|z | s |")));
        QVERIFY(undoneOnce());
    }

    void aTableFillsFromTheCaretsCell() {
        QApplication::clipboard()->setMimeData(
            ownCopy(QString::fromUtf8(kCopiedBlock), QString::fromUtf8(kCopiedBlock)));
        QVERIFY(caretIn(view(), 3, 0, 1) != nullptr);
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), replaced(QStringLiteral("| t | u | v | w |"),
                                  QStringLiteral("| **bold** | a \\| b | x<br>y | w |\n| p | q | r |  |")));
        QVERIFY(undoneOnce());
    }

    void textGoesToTheCaretAsBefore() {
        auto* data = new QMimeData;
        data->setText(QStringLiteral("z"));
        QApplication::clipboard()->setMimeData(data);
        QVERIFY(caretIn(view(), 3, 0, 1) != nullptr);
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), replaced(QStringLiteral("| t | u |"), QStringLiteral("| tz | u |")));
    }

    void spreadsheetTextAndHtmlFillCells() {
        // Excel's plain text (S031): two rows of two cells.
        auto* tsv = new QMimeData;
        tsv->setText(QStringLiteral("1\t2\r\n3\t4\r\n"));
        QApplication::clipboard()->setMimeData(tsv);
        QVERIFY(selectCells(view(), 1, 0, 1, 0));
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), replaced(QStringLiteral("| **bold** | a \\| b | x<br>y | 1 |\n| p | q |"),
                                  QStringLiteral("| 1 | 2 | x<br>y | 1 |\n| 3 | 4 |")));
        QVERIFY(undoneOnce());

        // An HTML table: its cells as S031 writes them.
        auto* html = new QMimeData;
        html->setHtml(QStringLiteral("<table><tr><td>h|1</td><td>*x*</td></tr><tr><td>a</td><td>b</td></tr></table>"));
        html->setText(QStringLiteral("h|1\t*x*\na\tb"));
        QApplication::clipboard()->setMimeData(html);
        QVERIFY(selectCells(view(), 2, 2, 2, 2));
        pressStandardKey(QKeySequence::Paste);
        f_->settle();
        QCOMPARE(text(), replaced(QStringLiteral("| p | q | r | s |\n| t | u | v | w |"),
                                  QStringLiteral("| p | q | h\\|1 | \\*x\\* |\n| t | u | a | b |")));
        QVERIFY(undoneOnce());
    }
};

QTEST_MAIN(tst_s041_table_copy_paste)
#include "tst_s041_table_copy_paste.moc"
