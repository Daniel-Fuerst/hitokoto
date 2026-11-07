// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. The find bar below the editor searches the concealed view while
// you type, steps with Enter/F3, wraps, counts, highlights, shows tables and front matter as their
// source while they hold the current match (review O8), moves the caret only on Escape (decision
// 9) and never changes the document (ADR 0002).
#include "find_bar.hpp"
#include "find_controller.hpp"
#include "hitokoto/core/find.hpp"
#include "ui_test_support.hpp"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QAction>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QRandomGenerator>
#include <QTableView>
#include <QToolButton>
#include <QUrl>

using namespace hitokoto;

namespace {

struct UrlCollector : QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void handleUrl(const QUrl& url) { urls << url; }
};

ui::FindController* controller(const test::Fixture& f) {
    return f.window->findController();
}
ui::FindBar* bar(const test::Fixture& f) {
    return f.window->findBar();
}

/// Writes `text` to a file, opens it and lets the styler finish.
bool load(test::Fixture& f, const QString& text) {
    const QString path = f.writeFile(QStringLiteral("doc.md"), text.toUtf8());
    if (!f.open(path)) {
        return false;
    }
    f.settle();
    return true;
}

/// Ctrl+F (Cmd+F on macOS) from the widget that has the focus.
void pressFind(const test::Fixture& f) {
    QWidget* target = QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : f.editor();
    const QKeySequence find(QKeySequence::Find);
    const QKeyCombination key = find[0];
    test::keyClick(target, key.key(), key.keyboardModifiers());
}

/// Find next (F3 on Windows and Linux, Cmd+G on macOS) from the widget that has the focus.
void pressFindNext(const test::Fixture& f, bool previous = false) {
    QWidget* target = QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : f.editor();
    const QKeySequence seq(previous ? QKeySequence::FindPrevious : QKeySequence::FindNext);
    const QKeyCombination key = seq[0];
    test::keyClick(target, key.key(), key.keyboardModifiers());
}

/// Opens the bar and types `text` into the search field.
void search(const test::Fixture& f, const QString& text) {
    pressFind(f);
    QLineEdit* field = bar(f)->searchField();
    field->selectAll();
    test::keyClick(field, Qt::Key_Backspace);
    test::typeText(field, text);
}

bool waitComplete(const test::Fixture& f, int timeoutMs = 10000) {
    return QTest::qWaitFor([&] { return controller(f)->isComplete(); }, timeoutMs);
}

int positionOf(const test::Fixture& f, core::DocPos p) {
    const QTextBlock b = f.editor()->document()->findBlockByNumber(static_cast<int>(p.line));
    return b.position() + static_cast<int>(p.column);
}

/// The source text a match covers.
QString matched(const test::Fixture& f, const core::FindMatch& m) {
    QTextCursor c(f.editor()->document());
    c.setPosition(positionOf(f, m.start));
    c.setPosition(positionOf(f, m.end), QTextCursor::KeepAnchor);
    return c.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
}

/// The viewport rectangle of a match on one line.
QRect matchRect(const test::Fixture& f, const core::FindMatch& m) {
    QTextCursor a(f.editor()->document());
    a.setPosition(positionOf(f, m.start));
    QTextCursor b(f.editor()->document());
    b.setPosition(positionOf(f, m.end));
    const QRect ra = f.editor()->cursorRect(a);
    const QRect rb = f.editor()->cursorRect(b);
    return QRect(QPoint(ra.left(), ra.top()), QPoint(rb.left(), ra.bottom()));
}

QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

/// How many pixels of `rect` in `image` have exactly `color`.
int pixelsOf(const QImage& image, const QRect& rect, const QColor& color) {
    int n = 0;
    const QRect r = rect.intersected(image.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (image.pixelColor(x, y).rgb() == color.rgb()) {
                ++n;
            }
        }
    }
    return n;
}

QImage grabViewport(const test::Fixture& f) {
    QCoreApplication::processEvents();
    return f.editor()->viewport()->grab().toImage();
}

core::FindStyle lightFindStyle() {
    return core::findStyle(core::builtinTheme(core::ColorScheme::Light));
}

std::vector<std::u16string> documentLines(const test::Fixture& f) {
    std::vector<std::u16string> lines;
    for (QTextBlock b = f.editor()->document()->begin(); b.isValid(); b = b.next()) {
        lines.push_back(b.text().toStdU16String());
    }
    return lines;
}

QAction* findMenuAction(const test::Fixture& f) {
    for (QAction* menu : f.window->menuBar()->actions()) {
        if (menu->menu() == nullptr || !menu->text().contains(QStringLiteral("Edit"))) {
            continue;
        }
        for (QAction* a : menu->menu()->actions()) {
            if (a->shortcut() == QKeySequence(QKeySequence::Find)) {
                return a;
            }
        }
    }
    return nullptr;
}

const QString kTable = QStringLiteral(
    "Oben steht rot\n"
    "\n"
    "| Farbe | Text |\n"
    "| --- | --- |\n"
    "| **rot** und grün | blau |\n"
    "\n"
    "Unten steht rot\n");

const QString kFrontMatter = QStringLiteral(
    "---\n"
    "title: Hallo Welt\n"
    "date: 2026-11-07\n"
    "---\n"
    "Text mit Hallo\n");

}  // namespace

class TestS028FindInFile : public QObject {
    Q_OBJECT
private slots:
    // --- Decision 7: opening ------------------------------------------------------------------

    void ctrlFOpensTheBarBelowTheEditorWithTheFocusInTheField() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins zwei drei\n")));
        QVERIFY(!bar(f)->isVisible());
        f.editor()->setFocus();
        pressFind(f);
        QTRY_VERIFY(bar(f)->isVisible());
        QVERIFY(controller(f)->isOpen());
        QCOMPARE(QApplication::focusWidget(), bar(f)->searchField());
        QVERIFY(bar(f)->geometry().top() >= f.editor()->geometry().bottom());
        for (QWidget* w : std::initializer_list<QWidget*>{bar(f)->previousButton(), bar(f)->nextButton(),
                                                          bar(f)->matchCaseToggle(), bar(f)->highlightAllToggle(),
                                                          bar(f)->countLabel(), bar(f)->closeButton()}) {
            QVERIFY(w->isVisible());
        }
    }

    void ctrlFSelectsThePreviousTextAndSearchesAtOnce() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins zwei eins\n")));
        search(f, QStringLiteral("eins"));
        QVERIFY(waitComplete(f));
        test::keyClick(bar(f)->searchField(), Qt::Key_Escape);
        QTRY_VERIFY(!bar(f)->isVisible());
        f.placeCaret(0, 0);
        pressFind(f);
        QTRY_VERIFY(bar(f)->isVisible());
        QCOMPARE(bar(f)->searchField()->text(), QStringLiteral("eins"));
        QCOMPARE(bar(f)->searchField()->selectedText(), QStringLiteral("eins"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 2);
        QVERIFY(controller(f)->currentMatch().has_value());
    }

    void ctrlFBringsTheFocusBackFromTheEditorAndATableCell() {
        test::Fixture f;
        QVERIFY(load(f, kTable));
        search(f, QStringLiteral("Unten"));
        f.editor()->setFocus();
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(f.editor()));
        pressFind(f);
        QCOMPARE(QApplication::focusWidget(), bar(f)->searchField());
        QCOMPARE(bar(f)->searchField()->selectedText(), QStringLiteral("Unten"));
        QTableView* view = f.editor()->tableViewAt(2);
        QVERIFY(view != nullptr);
        view->setFocus();
        view->setCurrentIndex(view->model()->index(1, 1));
        QTRY_VERIFY(QApplication::focusWidget() != bar(f)->searchField());
        pressFind(f);
        QTRY_COMPARE(QApplication::focusWidget(), bar(f)->searchField());
    }

    void editFindOpensTheBar() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins\n")));
        QAction* action = findMenuAction(f);
        QVERIFY(action != nullptr);
        action->trigger();
        QTRY_VERIFY(bar(f)->isVisible());
        QCOMPARE(QApplication::focusWidget(), bar(f)->searchField());
    }

    void findNextWithTheBarClosedOpensIt() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins\n")));
        f.editor()->setFocus();
        pressFindNext(f);
        QTRY_VERIFY(bar(f)->isVisible());
        QCOMPARE(QApplication::focusWidget(), bar(f)->searchField());
    }

    // --- Decisions 1, 8, 9: typing, stepping, the caret ---------------------------------------

    void typingSearchesAndHighlightsWithoutMovingTheCaret() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Anfang\n\nDas ist **wichtig:** denken\n")));
        f.placeCaret(0, 2);
        const int caretBefore = f.editor()->textCursor().position();
        search(f, QStringLiteral("wichtig: denken"));
        QVERIFY(waitComplete(f));
        const auto current = controller(f)->currentMatch();
        QVERIFY(current.has_value());
        QCOMPARE(matched(f, *current), QStringLiteral("wichtig:** denken"));
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 1"));
        QCOMPARE(bar(f)->countLabel()->text(), QStringLiteral("1 of 1"));
        QCOMPARE(f.editor()->textCursor().position(), caretBefore);
        QVERIFY(!f.editor()->textCursor().hasSelection());
        QVERIFY(!f.editor()->styler().isRevealed(2));  // the line stays concealed
        QVERIFY(test::isCollapsed(f.editor()->document()->findBlockByNumber(2), 8));
        const QImage image = grabViewport(f);
        QVERIFY(pixelsOf(image, matchRect(f, *current), qcolor(lightFindStyle().currentBackground)) > 20);
    }

    void nothingFoundMarksTheField() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins zwei\n")));
        search(f, QStringLiteral("drei"));
        QVERIFY(waitComplete(f));
        QVERIFY(!controller(f)->currentMatch().has_value());
        QCOMPARE(controller(f)->countText(), QStringLiteral("No matches"));
        QVERIFY(bar(f)->noMatch());
        const QImage field = bar(f)->searchField()->grab().toImage();
        QVERIFY(pixelsOf(field, field.rect(), qcolor(lightFindStyle().noMatchBackground)) > field.width());
        bar(f)->searchField()->selectAll();
        test::keyClick(bar(f)->searchField(), Qt::Key_Backspace);  // no search text: nothing to mark
        QTRY_VERIFY(!bar(f)->noMatch());
    }

    void enterAndFindNextStepAndWrap() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("a x\nb x\nc x\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("x"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 3"));
        QLineEdit* field = bar(f)->searchField();
        test::keyClick(field, Qt::Key_Return);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 3"));
        QCOMPARE(controller(f)->currentMatch()->start.line, std::size_t{1});
        pressFindNext(f);
        QCOMPARE(controller(f)->countText(), QStringLiteral("3 of 3"));
        test::keyClick(field, Qt::Key_Return);  // wraps
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 3"));
        test::keyClick(field, Qt::Key_Return, Qt::ShiftModifier);  // wraps back
        QCOMPARE(controller(f)->countText(), QStringLiteral("3 of 3"));
        pressFindNext(f, true);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 3"));
        QTest::mouseClick(bar(f)->nextButton(), Qt::LeftButton);
        QCOMPARE(controller(f)->countText(), QStringLiteral("3 of 3"));
        QTest::mouseClick(bar(f)->previousButton(), Qt::LeftButton);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 3"));
    }

    void findNextWorksFromTheEditor() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("a x\nb x\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("x"));
        QVERIFY(waitComplete(f));
        f.editor()->setFocus();
        const int caret = f.editor()->textCursor().position();
        pressFindNext(f);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 2"));
        QCOMPARE(f.editor()->textCursor().position(), caret);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(f.editor()));
    }

    void theSearchStartsAtTheCaret() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("x eins\nx zwei\nx drei\n")));
        f.placeCaret(1, 1);
        search(f, QStringLiteral("x"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->currentMatch()->start, (core::DocPos{2, 0}));
        QCOMPARE(controller(f)->countText(), QStringLiteral("3 of 3"));
    }

    void typingKeepsTheCurrentMatchWhileItStillMatches() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("wort\nwolke\nwort\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("w"));
        QVERIFY(waitComplete(f));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QCOMPARE(controller(f)->currentMatch()->start.line, std::size_t{1});
        test::keyClicks(bar(f)->searchField(), QStringLiteral("o"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->currentMatch()->start.line, std::size_t{1});  // "wo" still matches "wolke"
        test::keyClicks(bar(f)->searchField(), QStringLiteral("r"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->currentMatch()->start.line, std::size_t{2});  // the next one after it
    }

    // --- Decision 2: link destinations ---------------------------------------------------------

    void aDestinationMatchHighlightsTheLinkTextAndEscapeSelectsIt() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Anfang\n\nAbsatz [Doku](https://example.com/handbuch) hier\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("handbuch"));
        QVERIFY(waitComplete(f));
        const auto m = controller(f)->currentMatch();
        QVERIFY(m.has_value());
        QVERIFY(m->inDestination);
        QCOMPARE(matched(f, *m), QStringLiteral("Doku"));
        const QImage image = grabViewport(f);
        QVERIFY(pixelsOf(image, matchRect(f, *m), qcolor(lightFindStyle().currentBackground)) > 20);
        test::keyClick(bar(f)->searchField(), Qt::Key_Escape);
        QCOMPARE(f.editor()->textCursor().selectedText(), QStringLiteral("Doku"));
    }

    // --- Decision 6: match case ----------------------------------------------------------------

    void matchCaseToggleShortcutAndSession() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Äpfel und äpfel\n")));
        search(f, QStringLiteral("äpfel"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 2);
        QVERIFY(!controller(f)->matchCase());
#ifdef Q_OS_MACOS
        const QKeySequence expected(Qt::CTRL | Qt::ALT | Qt::Key_C);
#else
        const QKeySequence expected(Qt::ALT | Qt::Key_C);
#endif
        QCOMPARE(bar(f)->matchCaseAction()->shortcut(), expected);
        const QKeyCombination key = expected[0];
        test::keyClick(bar(f)->searchField(), key.key(), key.keyboardModifiers());
        QVERIFY(controller(f)->matchCase());
        QVERIFY(bar(f)->matchCaseToggle()->isChecked());
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 1);
        QCOMPARE(bar(f)->searchField()->text(), QStringLiteral("äpfel"));  // the shortcut typed nothing
        test::keyClick(bar(f)->searchField(), Qt::Key_Escape);
        pressFind(f);
        QVERIFY(controller(f)->matchCase());  // kept for the session
        QTest::mouseClick(bar(f)->matchCaseToggle(), Qt::LeftButton);
        QVERIFY(!controller(f)->matchCase());
    }

    // --- Decision 10: highlight all ------------------------------------------------------------

    void highlightAllPaintsEveryVisibleMatch() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("x eins\nx zwei\nx drei\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("x"));
        QVERIFY(waitComplete(f));
        QVERIFY(!controller(f)->highlightAll());
        QCOMPARE(controller(f)->visibleMatches().size(), std::size_t{1});
#ifdef Q_OS_MACOS
        const QKeySequence expected(Qt::CTRL | Qt::ALT | Qt::Key_A);
#else
        const QKeySequence expected(Qt::ALT | Qt::Key_A);
#endif
        QCOMPARE(bar(f)->highlightAllAction()->shortcut(), expected);
        const QKeyCombination key = expected[0];
        test::keyClick(bar(f)->searchField(), key.key(), key.keyboardModifiers());
        QVERIFY(controller(f)->highlightAll());
        QTRY_COMPARE(controller(f)->visibleMatches().size(), std::size_t{3});
        const QImage image = grabViewport(f);
        const auto matches = controller(f)->visibleMatches();
        const core::FindStyle style = lightFindStyle();
        QVERIFY(pixelsOf(image, matchRect(f, matches[0]), qcolor(style.currentBackground)) > 10);
        QVERIFY(pixelsOf(image, matchRect(f, matches[1]), qcolor(style.matchBackground)) > 10);
        QVERIFY(pixelsOf(image, matchRect(f, matches[2]), qcolor(style.matchBackground)) > 10);
        QTest::mouseClick(bar(f)->highlightAllToggle(), Qt::LeftButton);
        QVERIFY(!controller(f)->highlightAll());
        QCOMPARE(controller(f)->visibleMatches().size(), std::size_t{1});
    }

    void highlightsNeverShowConcealedMarkers() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Anfang\n\n**ab** [c](u) `d` cd\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("ab c d"));
        QVERIFY(waitComplete(f));
        QVERIFY(controller(f)->currentMatch().has_value());
        const QTextBlock block = f.editor()->document()->findBlockByNumber(2);
        const auto formats = controller(f)->highlightFormats(2);
        QVERIFY(!formats.isEmpty());
        bool background = false;
        for (const auto& r : formats) {
            background = background || r.format.background().color() == qcolor(lightFindStyle().currentBackground);
            if (!r.format.hasProperty(QTextFormat::ForegroundBrush)) {
                continue;
            }
            for (int col = r.start; col < r.start + r.length; ++col) {
                QVERIFY2(!test::isCollapsed(block, col), qPrintable(QStringLiteral("column %1").arg(col)));
            }
        }
        QVERIFY(background);
    }

    // --- Decision 3 and review O8: tables ------------------------------------------------------

    void aMatchInATableShowsItsSourceWhileCurrent() {
        test::Fixture f;
        QVERIFY(load(f, kTable));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("rot"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 3"));
        QVERIFY(!f.editor()->isTableSourceShown(2));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 3"));
        QTRY_VERIFY(f.editor()->isTableSourceShown(2));
        QCOMPARE(matched(f, *controller(f)->currentMatch()), QStringLiteral("rot"));
        QCOMPARE(controller(f)->currentMatch()->start, (core::DocPos{4, 4}));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QCOMPARE(controller(f)->countText(), QStringLiteral("3 of 3"));
        QTRY_VERIFY(!f.editor()->isTableSourceShown(2));
        QVERIFY(f.editor()->tableViewAt(2) != nullptr);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 0);  // the caret never moved
    }

    void escapeIntoATableKeepsTheSourceUntilTheCaretLeaves() {
        test::Fixture f;
        QVERIFY(load(f, kTable));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("rot und"));
        QVERIFY(waitComplete(f));
        QTRY_VERIFY(f.editor()->isTableSourceShown(2));
        test::keyClick(bar(f)->searchField(), Qt::Key_Escape);
        QTRY_VERIFY(!bar(f)->isVisible());
        QCOMPARE(f.editor()->textCursor().selectedText(), QStringLiteral("rot** und"));
        QVERIFY(f.editor()->isTableSourceShown(2));
        f.placeCaret(6, 0);
        QTRY_VERIFY(!f.editor()->isTableSourceShown(2));
        QVERIFY(f.editor()->tableViewAt(2) != nullptr);
    }

    void highlightAllMarksCellsInTheTableView() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Oben blau\n\n| Farbe | Text |\n| --- | --- |\n| rot | blau |\n\nUnten\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("blau"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 2"));  // current outside the table
        QTest::mouseClick(bar(f)->highlightAllToggle(), Qt::LeftButton);
        QVERIFY(controller(f)->highlightAll());
        QTableView* view = f.editor()->tableViewAt(2);
        QVERIFY(view != nullptr);  // highlight all never switches a table to its source
        QModelIndex blau;
        for (int r = 0; r < view->model()->rowCount() && !blau.isValid(); ++r) {
            for (int c = 0; c < view->model()->columnCount(); ++c) {
                if (view->model()->index(r, c).data().toString() == QStringLiteral("blau")) {
                    blau = view->model()->index(r, c);
                }
            }
        }
        QVERIFY(blau.isValid());
        QCoreApplication::processEvents();
        const QImage image = view->viewport()->grab().toImage();
        const QRect cell = view->visualRect(blau);
        const QRect other = view->visualRect(view->model()->index(blau.row(), 0));
        QVERIFY(pixelsOf(image, cell, qcolor(lightFindStyle().matchBackground)) > cell.width());
        QCOMPARE(pixelsOf(image, other, qcolor(lightFindStyle().matchBackground)), 0);
    }

    // --- Decision 3 and review O8: front matter ------------------------------------------------

    void aMatchInFrontMatterShowsItsYamlWhileCurrent() {
        test::Fixture f;
        QVERIFY(load(f, kFrontMatter));
        QTRY_VERIFY(f.editor()->propertiesView() != nullptr);
        f.placeCaret(4, 0);
        search(f, QStringLiteral("Hallo"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 2"));  // after the caret: the body's
        QVERIFY(f.editor()->propertiesView() != nullptr);
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QCOMPARE(controller(f)->countText(), QStringLiteral("1 of 2"));
        QTRY_VERIFY(f.editor()->propertiesView() == nullptr);  // the YAML shows
        QCOMPARE(controller(f)->currentMatch()->start, (core::DocPos{1, 7}));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QTRY_VERIFY(f.editor()->propertiesView() != nullptr);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 4);
    }

    void yamlSyntaxIsNotFoundEvenWithTheCaretInTheBlock() {
        test::Fixture f;
        QVERIFY(load(f, kFrontMatter));
        f.placeCaret(1, 0);  // the YAML shows
        search(f, QStringLiteral("title: Hallo"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 0);
        bar(f)->searchField()->selectAll();
        test::keyClicks(bar(f)->searchField(), QStringLiteral("title Hallo"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 1);
    }

    // --- Escape and clicks -----------------------------------------------------------------------

    void escapeClosesSelectsAndRemovesTheHighlights() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Anfang\n\nDas ist **wichtig:** denken\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("wichtig: denken"));
        QVERIFY(waitComplete(f));
        test::keyClick(bar(f)->searchField(), Qt::Key_Escape);
        QTRY_VERIFY(!bar(f)->isVisible());
        QVERIFY(!controller(f)->isOpen());
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(f.editor()));
        QCOMPARE(f.editor()->textCursor().selectedText(), QStringLiteral("wichtig:** denken"));
        QTRY_VERIFY(f.editor()->styler().isRevealed(2));
        QVERIFY(controller(f)->visibleMatches().empty());
        QVERIFY(controller(f)->highlightFormats(2).isEmpty());
    }

    void escapeWithoutAMatchKeepsTheCaret() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins zwei\n")));
        f.placeCaret(0, 3);
        search(f, QStringLiteral("drei"));
        QVERIFY(waitComplete(f));
        QTest::mouseClick(bar(f)->closeButton(), Qt::LeftButton);
        QTRY_VERIFY(!bar(f)->isVisible());
        QCOMPARE(f.editor()->textCursor().position(), 3);
        QVERIFY(!f.editor()->textCursor().hasSelection());
    }

    void escapeInTheEditorDoesNotCloseTheBar() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("eins zwei\n")));
        search(f, QStringLiteral("zwei"));
        QVERIFY(waitComplete(f));
        f.editor()->setFocus();
        test::keyClick(f.editor(), Qt::Key_Escape);
        QVERIFY(bar(f)->isVisible());
        QVERIFY(controller(f)->isOpen());
    }

    void aClickOnALinkOpensItOnTheFirstClick() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Anfang\n\n[Doku](https://example.com/handbuch) hier\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("hier"));
        QVERIFY(waitComplete(f));
        UrlCollector collector;
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &collector, "handleUrl");
        QTextCursor c(f.editor()->document()->findBlockByNumber(2));
        c.setPosition(c.block().position() + 2);
        const QPoint at = f.editor()->cursorRect(c).center();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QCOMPARE(collector.urls.size(), 1);
        QVERIFY(bar(f)->isVisible());
    }

    void aClickInTheTextSetsTheCaretAndTheBarStays() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("a x\nb x\nc x\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("x"));
        QVERIFY(waitComplete(f));
        QTextCursor c(f.editor()->document()->findBlockByNumber(2));
        const QPoint at = f.editor()->cursorRect(c).center();
        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 2);
        QVERIFY(bar(f)->isVisible());
        pressFindNext(f);
        QCOMPARE(controller(f)->countText(), QStringLiteral("2 of 3"));  // from the current match
    }

    // --- Edits, the document, large files ------------------------------------------------------

    void editsUpdateTheMatchesLikeAFreshSearch() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("wort eins\n\n| a | b |\n| - | - |\n| wort | x |\n\nwort zwei\n")));
        search(f, QStringLiteral("wort"));
        QVERIFY(waitComplete(f));
        QCOMPARE(controller(f)->matchCount(), 3);
        QRandomGenerator rng(2811);
        const QStringList pieces{QStringLiteral("wort"), QStringLiteral("```"), QStringLiteral("| - |"),
                                 QStringLiteral("\n"),   QStringLiteral("**"),  QStringLiteral("---"),
                                 QStringLiteral("x"),    QStringLiteral(" ")};
        for (int round = 0; round < 40; ++round) {
            QTextCursor c(f.editor()->document());
            c.setPosition(static_cast<int>(rng.bounded(f.editor()->document()->characterCount())));
            f.editor()->setTextCursor(c);
            const QString piece = pieces[static_cast<int>(rng.bounded(pieces.size()))];
            if (rng.bounded(3) == 0) {
                test::keyClick(f.editor(), Qt::Key_Backspace);
            } else if (piece == QStringLiteral("\n")) {
                test::keyClick(f.editor(), Qt::Key_Return);
            } else {
                test::typeText(f.editor(), piece);
            }
            QVERIFY(waitComplete(f));
            const auto expected = core::findInDocument(test::registry(), documentLines(f), u"wort");
            QCOMPARE(controller(f)->matchCount(), static_cast<int>(expected.size()));
            if (const auto m = controller(f)->currentMatch()) {
                QVERIFY(std::any_of(expected.begin(), expected.end(),
                                    [&](const core::FindMatch& e) { return e.start == m->start && e.end == m->end; }));
            }
        }
    }

    void theSearchNeverChangesTheDocument() {
        test::Fixture f;
        const QString text = kFrontMatter + QStringLiteral("\n") + kTable;
        QVERIFY(load(f, text));
        QTextDocument* doc = f.editor()->document();
        const QString before = doc->toRawText();
        const int undo = doc->availableUndoSteps();
        QVERIFY(!doc->isModified());
        search(f, QStringLiteral("rot"));
        QVERIFY(waitComplete(f));
        for (int i = 0; i < 6; ++i) {
            test::keyClick(bar(f)->searchField(), Qt::Key_Return);
            QCoreApplication::processEvents();
        }
        QTest::mouseClick(bar(f)->highlightAllToggle(), Qt::LeftButton);
        search(f, QStringLiteral("Hallo"));
        QVERIFY(waitComplete(f));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QTest::mouseClick(bar(f)->closeButton(), Qt::LeftButton);
        QCOMPARE(doc->toRawText(), before);
        QCOMPARE(doc->availableUndoSteps(), undo);
        QVERIFY(!doc->isModified());
        const QString path = f.dir.filePath(QStringLiteral("doc.md"));
        QVERIFY(f.saveAndWait(path));
        QCOMPARE(test::Fixture::readFile(path), text.toUtf8());
    }

    void theCountShowsMatchesSoFarUntilTheSearchIsComplete() {
        test::Fixture f;
        QString text;
        for (int i = 0; i < 40000; ++i) {
            text += QStringLiteral("Zeile %1 mit **wort** und [link](https://x/wort) und Text dazu\n").arg(i);
        }
        QVERIFY(load(f, text));
        f.placeCaret(20000, 0);
        pressFind(f);
        QElapsedTimer timer;
        timer.start();
        test::keyClicks(bar(f)->searchField(), QStringLiteral("w"));
        const qint64 keyMs = timer.elapsed();
        QVERIFY2(keyMs < 200, qPrintable(QStringLiteral("key took %1 ms").arg(keyMs)));
        QVERIFY(!controller(f)->isComplete());
        QTRY_VERIFY(controller(f)->currentMatch().has_value());
        if (!controller(f)->isComplete()) {
            QVERIFY(controller(f)->countText().endsWith(QStringLiteral("+ matches")));
            QCOMPARE(controller(f)->currentIndex(), 0);
        }
        QVERIFY(waitComplete(f, 60000));
        // Per line: "wort" in the text and in the link's destination.
        QCOMPARE(controller(f)->matchCount(), 40000 * 2);
        QCOMPARE(controller(f)->currentMatch()->start.line, std::size_t{20000});
        QCOMPARE(controller(f)->countText(), QStringLiteral("%1 of %2").arg(20000 * 2 + 1).arg(40000 * 2));
    }

    // --- Accessibility, theming, case folding ------------------------------------------------

    void accessibleNamesAndAnnouncements() {
        test::Fixture f;
        QVERIFY(load(f, QStringLiteral("Die **Agenten** arbeiten heute\nund Agenten ruhen\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("Agenten"));
        QVERIFY(waitComplete(f));
        QCOMPARE(bar(f)->searchField()->accessibleName(), QStringLiteral("Find"));
        QCOMPARE(bar(f)->previousButton()->accessibleName(), QStringLiteral("Previous match"));
        QCOMPARE(bar(f)->nextButton()->accessibleName(), QStringLiteral("Next match"));
        QCOMPARE(bar(f)->matchCaseToggle()->accessibleName(), QStringLiteral("Match case"));
        QCOMPARE(bar(f)->highlightAllToggle()->accessibleName(), QStringLiteral("Highlight all"));
        QCOMPARE(bar(f)->closeButton()->accessibleName(), QStringLiteral("Close find bar"));
        QVERIFY(bar(f)->matchCaseToggle()->isCheckable());
        QVERIFY(bar(f)->highlightAllToggle()->isCheckable());
        QCOMPARE(controller(f)->lastAnnouncement(), QStringLiteral("1 of 2: Die Agenten arbeiten heute"));
        test::keyClick(bar(f)->searchField(), Qt::Key_Return);
        QCOMPARE(controller(f)->lastAnnouncement(), QStringLiteral("2 of 2: und Agenten ruhen"));
    }

    void findTokensStyleTheBarAndTheHighlights() {
        test::Fixture f;
        const auto parsed = core::parseTheme(
            "[find]\nbackground = \"#102030\"\nborder = \"#405060\"\nborder-width = 2\n"
            "current-background = \"#00ff00\"\nfield-background = \"#ffeedd\"\n",
            core::builtinTheme(core::ColorScheme::Light));
        QVERIFY(parsed.theme.has_value());
        f.window->applyTheme(*parsed.theme);
        QVERIFY(load(f, QStringLiteral("Anfang\nwort\n")));
        f.placeCaret(0, 0);
        search(f, QStringLiteral("wort"));
        QVERIFY(waitComplete(f));
        QCOMPARE(bar(f)->findStyle(), core::findStyle(*parsed.theme));
        const QImage barImage = bar(f)->grab().toImage();
        QCOMPARE(barImage.pixelColor(barImage.width() / 2, 0).rgb(), QColor(0x40, 0x50, 0x60).rgb());
        QCOMPARE(barImage.pixelColor(1, barImage.height() / 2).rgb(), QColor(0x10, 0x20, 0x30).rgb());
        const QImage image = grabViewport(f);
        QVERIFY(pixelsOf(image, matchRect(f, *controller(f)->currentMatch()), QColor(0, 255, 0)) > 20);
    }

    void caseFoldingAgreesWithQtForEveryCharacterQtKnows() {
        // ADR 0008: the core's table against QChar::toCaseFolded; code points that Qt's Unicode
        // version does not know are listed, not compared.
        int compared = 0;
        int unknown = 0;
        QStringList differences;
        for (char32_t c = 0; c <= 0x10FFFF; ++c) {
            if (c >= 0xD800 && c < 0xE000) {
                continue;
            }
            const QChar::UnicodeVersion v = QChar::unicodeVersion(c);
            if (v == QChar::Unicode_Unassigned || v > QChar::currentUnicodeVersion()) {
                if (core::foldCaseSimple(c) != c) {
                    ++unknown;
                }
                continue;
            }
            ++compared;
            if (QChar::toCaseFolded(c) != core::foldCaseSimple(c)) {
                differences << QStringLiteral("U+%1").arg(static_cast<uint>(c), 4, 16, QLatin1Char('0'));
            }
        }
        qInfo("compared %d code points; %d mappings for characters unknown to Qt", compared, unknown);
        QVERIFY2(differences.isEmpty(), qPrintable(differences.join(QLatin1Char(' '))));
    }
};

QTEST_MAIN(TestS028FindInFile)
#include "tst_s028_find_in_file.moc"
