// SPDX-License-Identifier: GPL-3.0-or-later
// S024: front matter shown as properties while the caret is elsewhere, as YAML text while the caret
// and the anchor of the selection are in it; invalid YAML stays text with a badge.
#include "ui_test_support.hpp"

#include <QAccessible>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QScrollBar>
#include <QTableView>
#include <QTime>

using namespace hitokoto;

namespace {

/// Armin's example (2026-10-30): a review record, front matter on lines 0 to 26.
constexpr auto kRecord =
    "---\n"
    "mode: code\n"
    "spec: docs/stories/022-inline-links.md\n"
    "branch: feature/links_references  # Daniel's branch, pull request #21\n"
    "commit_range: d5e959f..60263e3  # third follow-up: 3ecbe77, 6357d45, 60263e3\n"
    "reviewed_on: 2026-10-30\n"
    "environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations "
    "checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark)\n"
    "objections:\n"
    "  - id: O19\n"
    "    category: failure\n"
    "    severity: major\n"
    "    title: On a revealed line link text overlaps emphasis and escape markers; fuzz_render aborts (since "
    "bb344f0)\n"
    "    disposition: pending\n"
    "    disposition_rationale: null\n"
    "  - id: O20\n"
    "    category: failure\n"
    "    severity: major\n"
    "    title: A code span no longer lets a link form across it; the new test pins this against CommonMark\n"
    "    disposition: pending\n"
    "    disposition_rationale: null\n"
    "  - id: O21\n"
    "    category: operational\n"
    "    severity: minor\n"
    "    title: Fix committed before its tests\n"
    "    disposition: pending\n"
    "    disposition_rationale: null\n"
    "---\n"
    "\n"
    "# Review S022, mode code: third follow-up\n"
    "\n"
    "Check of Daniel's three commits after d5e959f, on 2026-10-30:\n";
constexpr int kRecordClose = 26;
constexpr int kRecordBody = 30;

QAccessibleInterface* accessible(QWidget* view) {
    return QAccessible::queryAccessibleInterface(view);
}

/// Names of the block's accessible children: the title, then one per row.
QStringList names(QWidget* view) {
    QStringList out;
    QAccessibleInterface* a = accessible(view);
    for (int i = 0; a != nullptr && i < a->childCount(); ++i) {
        out << a->child(i)->text(QAccessible::Name);
    }
    return out;
}

/// Descriptions (the types) of the rows.
QStringList types(QWidget* view) {
    QStringList out;
    QAccessibleInterface* a = accessible(view);
    for (int i = 1; a != nullptr && i < a->childCount(); ++i) {
        out << a->child(i)->text(QAccessible::Description);
    }
    return out;
}

/// The row's rectangle in the block's coordinates (row 0 = the first property).
QRect rowRect(QWidget* view, int row) {
    QAccessibleInterface* a = accessible(view);
    if (a == nullptr || row + 1 >= a->childCount()) {
        return {};
    }
    const QRect global = a->child(row + 1)->rect();
    return {view->mapFromGlobal(global.topLeft()), global.size()};
}

/// A click at `point` of the block, delivered to the widget there.
void clickBlock(QWidget* view, QPoint point, Qt::KeyboardModifiers modifiers = {}) {
    QWidget* target = view->childAt(point);
    if (target == nullptr) {
        target = view;
    }
    QTest::mouseClick(target, Qt::LeftButton, modifiers, target->mapFrom(view, point));
}

/// A point on the value of a row: right of the key column.
QPoint onValue(QWidget* view, int row) {
    const auto style = core::propertiesStyle(core::builtinTheme(core::ColorScheme::Light));
    const QRect r = rowRect(view, row);
    return {r.left() + static_cast<int>(style.keyWidth) + 8, r.center().y()};
}

qreal heightOf(ui::EditorWidget* editor, int blockNumber) {
    return editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(blockNumber)).height();
}

qreal topOf(ui::EditorWidget* editor, int blockNumber) {
    return editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(blockNumber)).top();
}

/// The front matter on lines 0 to `close` shows as properties: the first line holds the block and
/// its margins, the other lines take no space.
void expectConcealed(ui::EditorWidget* editor, int close, const char* where) {
    QWidget* view = editor->propertiesView();
    QVERIFY2(view != nullptr, where);
    const double margin = core::propertiesStyle(core::builtinTheme(core::ColorScheme::Light)).margin;
    QVERIFY2(qAbs(heightOf(editor, 0) - (view->height() + 2 * margin)) < 1, where);
    for (int n = 1; n <= close; ++n) {
        QVERIFY2(heightOf(editor, n) == 0, qPrintable(QStringLiteral("%1: line %2").arg(where).arg(n)));
    }
}

/// The front matter on lines 0 to `close` shows its YAML: no block, every line has its height.
void expectRevealed(ui::EditorWidget* editor, int close, const char* where) {
    QVERIFY2(editor->propertiesView() == nullptr, where);
    for (int n = 0; n <= close; ++n) {
        QVERIFY2(heightOf(editor, n) > 0, qPrintable(QStringLiteral("%1: line %2").arg(where).arg(n)));
    }
}

QString shortDate(int y, int m, int d) {
    return QLocale::system().toString(QDate(y, m, d), QLocale::ShortFormat);
}

}  // namespace

class tst_s024_front_matter : public QObject {
    Q_OBJECT
private slots:
    void aReviewRecordShowsAsProperties() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("record.md"), kRecord)));
        f.placeCaret(kRecordBody);
        f.settle();
        auto* editor = f.editor();
        expectConcealed(editor, kRecordClose, "record");
        if (QTest::currentTestFailed()) {
            return;
        }
        QWidget* view = editor->propertiesView();
        const QStringList n = names(view);
        QCOMPARE(n.size(), 8);
        QCOMPARE(n.at(0), QStringLiteral("Properties"));
        QCOMPARE(n.at(1), QStringLiteral("mode: code"));
        QCOMPARE(n.at(3), QStringLiteral("branch: feature/links_references"));
        QCOMPARE(n.at(4), QStringLiteral("commit_range: d5e959f..60263e3"));
        QCOMPARE(n.at(5), QStringLiteral("reviewed_on: ") + shortDate(2026, 10, 30));
        QVERIFY(n.at(7).startsWith(QStringLiteral(R"(objections: [{"id":"O19","category":"failure")")));
        QVERIFY2(
            n.at(7).endsWith(QStringLiteral(
                R"("title":"Fix committed before its tests","disposition":"pending","disposition_rationale":null}])")),
            "the accessible name holds the full JSON");
        QCOMPARE(types(view), (QStringList{QStringLiteral("text"), QStringLiteral("text"), QStringLiteral("text"),
                                           QStringLiteral("text"), QStringLiteral("date"), QStringLiteral("text"),
                                           QStringLiteral("unsupported")}));
        // The JSON shows at most three lines: the block is far shorter than its 1,000 characters.
        const auto style = core::propertiesStyle(core::builtinTheme(core::ColorScheme::Light));
        const qreal line = QFontMetricsF(view->font()).lineSpacing();
        QVERIFY2(view->height() <= 16 * line + 2 * style.padding + 8 * style.rowSpacing,
                 qPrintable(QStringLiteral("%1 px").arg(view->height())));
        QCOMPARE(editor->styler().renderFor(editor->document()->findBlockByNumber(28)).headingLevel, std::uint8_t{1});
        QCOMPARE(editor->frontMatterBadge(), QString());
    }

    void eachTypeHasItsLook() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("types.md"),
                                   "---\n"
                                   "title: A note\n"
                                   "count: 42\n"
                                   "done: true\n"
                                   "draft: false\n"
                                   "day: 2026-10-30\n"
                                   "start: 2026-10-30T16:28\n"
                                   "list:\n"
                                   "  - a\n"
                                   "  - b\n"
                                   "flow: [x, y]\n"
                                   "tags: one\n"
                                   "empty:\n"
                                   "author:\n"
                                   "  name: Armin\n"
                                   "---\n"
                                   "body\n")));
        f.placeCaret(16);
        f.settle();
        QWidget* view = f.editor()->propertiesView();
        QVERIFY(view != nullptr);
        const QString start =
            QLocale::system().toString(QDateTime(QDate(2026, 10, 30), QTime(16, 28)), QLocale::ShortFormat);
        QCOMPARE(
            names(view),
            (QStringList{QStringLiteral("Properties"), QStringLiteral("title: A note"), QStringLiteral("count: 42"),
                         QStringLiteral("done: checked"), QStringLiteral("draft: not checked"),
                         QStringLiteral("day: ") + shortDate(2026, 10, 30), QStringLiteral("start: ") + start,
                         QStringLiteral("list: a, b"), QStringLiteral("flow: x, y"), QStringLiteral("tags: one"),
                         QStringLiteral("empty:"), QStringLiteral(R"(author: {"name":"Armin"})")}));
        QCOMPARE(types(view),
                 (QStringList{QStringLiteral("text"), QStringLiteral("number"), QStringLiteral("checkbox"),
                              QStringLiteral("checkbox"), QStringLiteral("date"), QStringLiteral("date and time"),
                              QStringLiteral("list"), QStringLiteral("list"), QStringLiteral("tags"),
                              QStringLiteral("text"), QStringLiteral("unsupported")}));
    }

    void theCaretInTheBlockShowsTheYaml() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("note.md"), "---\ntitle: A note\ncount: 1\n---\n# Heading\nbody\n")));
        f.placeCaret(5);
        f.settle();
        auto* editor = f.editor();
        expectConcealed(editor, 3, "caret on the body");
        for (const int line : {0, 1, 2, 3}) {
            f.placeCaret(line);
            f.settle();
            expectRevealed(editor, 3, "caret in the block");
            const QTextBlock block = editor->document()->findBlockByNumber(line);
            QVERIFY(test::formatAt(block, 0).foreground().color() != QColor(Qt::transparent));
        }
        // Edited as text; leaving the block shows the edited value.
        f.placeCaret(1);
        test::keyClick(editor, Qt::Key_End);
        test::keyClicks(editor, QStringLiteral(" edited"));
        test::keyClick(editor, Qt::Key_Down);
        test::keyClick(editor, Qt::Key_Down);
        test::keyClick(editor, Qt::Key_Down);
        f.settle();
        QCOMPARE(editor->textCursor().blockNumber(), 4);
        expectConcealed(editor, 3, "after leaving");
        if (QTest::currentTestFailed()) {
            return;
        }
        QCOMPARE(names(editor->propertiesView()).value(1), QStringLiteral("title: A note edited"));
        // Up from the first line below the block: the caret on the closing line, the YAML shown.
        test::keyClick(editor, Qt::Key_Up);
        f.settle();
        QCOMPARE(editor->textCursor().blockNumber(), 3);
        expectRevealed(editor, 3, "up into the block");
    }

    void aClickOnARowPutsTheCaretAtItsValue() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("record.md"), kRecord)));
        f.placeCaret(kRecordBody);
        f.settle();
        auto* editor = f.editor();
        QWidget* view = editor->propertiesView();
        QVERIFY(view != nullptr);
        clickBlock(view, onValue(view, 2));  // branch
        f.settle();
        QCOMPARE(editor->textCursor().blockNumber(), 3);
        QCOMPARE(editor->textCursor().positionInBlock(), 8);
        expectRevealed(editor, kRecordClose, "after the click");

        f.placeCaret(kRecordBody);
        f.settle();
        view = editor->propertiesView();
        QVERIFY(view != nullptr);
        clickBlock(view, onValue(view, 6));  // objections: the value starts on the line below the key
        f.settle();
        QCOMPARE(editor->textCursor().blockNumber(), 8);
        QCOMPARE(editor->textCursor().positionInBlock(), 2);

        f.placeCaret(kRecordBody);
        f.settle();
        view = editor->propertiesView();
        QVERIFY(view != nullptr);
        const QRect firstRow = rowRect(view, 0);
        clickBlock(view, QPoint(firstRow.center().x(), firstRow.top() - 2));  // the title
        f.settle();
        QCOMPARE(editor->textCursor().blockNumber(), 1);
        QCOMPARE(editor->textCursor().positionInBlock(), 0);
    }

    // Review O6: a selection with one end outside the block leaves the properties block; a mouse
    // button held keeps the form; the YAML shows when caret and anchor are both inside.
    void aSelectionAcrossTheBlockKeepsTheProperties() {
        QByteArray document = "---\ntags: [a, b, c]\n---\n";
        for (int i = 0; i < 40; ++i) {
            document += "body line " + QByteArray::number(i) + "\n";
        }
        test::Fixture f;
        core::Theme theme = core::builtinTheme(core::ColorScheme::Light);
        theme.properties.padding = 30;  // the block clearly taller than its three YAML lines
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("select.md"), document)));
        f.placeCaret(10);
        f.settle();
        auto* editor = f.editor();
        QWidget* view = editor->propertiesView();
        QVERIFY(view != nullptr);
        const qreal yamlHeight = 3 * QFontMetricsF(editor->document()->defaultFont()).lineSpacing();
        QVERIFY2(heightOf(editor, 0) > yamlHeight, "the block is taller than its YAML");
        const qreal bodyTop = topOf(editor, 3);

        clickBlock(view, onValue(view, 0), Qt::ShiftModifier);  // Shift+click into the block from below
        f.settle();
        QVERIFY2(editor->textCursor().hasSelection(), "Shift+click selects");
        QVERIFY2(editor->propertiesView() != nullptr, "Shift+click from below");
        QCOMPARE(topOf(editor, 3), bodyTop);

        f.placeCaret(10);
        test::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
        f.settle();
        QVERIFY2(editor->propertiesView() != nullptr, "select all");
        QCOMPARE(topOf(editor, 3), bodyTop);

        // A drag from the body up across the block, the button held all the way.
        f.placeCaret(10);
        f.settle();
        QWidget* viewport = editor->viewport();
        const QPoint body = editor->cursorRect(QTextCursor(editor->document()->findBlockByNumber(8))).center();
        QTest::mousePress(viewport, Qt::LeftButton, {}, body);
        for (int y = body.y(); y > 0; y -= 4) {
            QTest::mouseMove(viewport, QPoint(body.x(), y));
            QVERIFY2(editor->propertiesView() != nullptr, qPrintable(QStringLiteral("drag at y %1").arg(y)));
            QCOMPARE(topOf(editor, 3), bodyTop);
        }
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, QPoint(body.x(), 1));
        f.settle();
        QVERIFY2(editor->propertiesView() != nullptr, "the anchor stays outside");

        // Pressed on a row, the block keeps its form until the button is released.
        f.placeCaret(10);
        f.settle();
        view = editor->propertiesView();
        QVERIFY(view != nullptr);
        const QPoint value = onValue(view, 0);
        QWidget* target = view->childAt(value) != nullptr ? view->childAt(value) : view;
        QTest::mousePress(target, Qt::LeftButton, {}, target->mapFrom(view, value));
        QCoreApplication::processEvents();
        QVERIFY2(editor->propertiesView() != nullptr, "held");
        QTest::mouseRelease(target, Qt::LeftButton, {}, target->mapFrom(view, value));
        f.settle();
        expectRevealed(editor, 2, "released inside");
    }

    void invalidYamlStaysTextWithABadge() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("dup.md"), "---\ntitle: x\ntitle: y\n---\nbody\n")));
        f.placeCaret(4);
        f.settle();
        auto* editor = f.editor();
        expectRevealed(editor, 3, "invalid");
        QCOMPARE(editor->frontMatterBadge(), QStringLiteral("Invalid YAML (line 3): duplicate key"));
        f.placeCaret(1);
        f.settle();
        QCOMPARE(editor->frontMatterBadge(), QString());

        test::Fixture g;
        QVERIFY(g.open(g.writeFile(QStringLiteral("anchor.md"), "---\na: &x 1\nb: *x\n---\nbody\n")));
        g.placeCaret(4);
        g.settle();
        QCOMPARE(g.editor()->frontMatterBadge(), QStringLiteral("Unsupported YAML (line 2): anchor or alias"));
        QVERIFY(g.editor()->propertiesView() == nullptr);
    }

    void specialCases() {
        {
            test::Fixture f;  // empty front matter: the title alone
            QVERIFY(f.open(f.writeFile(QStringLiteral("empty.md"), "---\n---\nbody\n")));
            f.placeCaret(2);
            f.settle();
            QVERIFY(f.editor()->propertiesView() != nullptr);
            QCOMPARE(names(f.editor()->propertiesView()), QStringList{QStringLiteral("Properties")});
        }
        {
            test::Fixture f;  // byte order mark and CRLF
            const QByteArray bytes = "\xEF\xBB\xBF---\r\ntitle: x\r\n---\r\nbody\r\n";
            const QString path = f.writeFile(QStringLiteral("bom.md"), bytes);
            QVERIFY(f.open(path));
            f.placeCaret(3);
            f.settle();
            QVERIFY(f.editor()->propertiesView() != nullptr);
            QVERIFY(f.saveAndWait(path));
            QCOMPARE(test::Fixture::readFile(path), bytes);
        }
        for (const QByteArray& noFrontMatter :
             {QByteArray("\n---\ntitle: x\n---\nbody\n"), QByteArray("---\ntitle: x\n")}) {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("none.md"), noFrontMatter)));
            f.placeCaret(f.editor()->document()->blockCount() - 1);
            f.settle();
            QVERIFY2(f.editor()->propertiesView() == nullptr, noFrontMatter.constData());
            QCOMPARE(f.editor()->frontMatterBadge(), QString());
        }
        {
            test::Fixture f;  // `...` does not close; the block reaches the next `---` and is invalid
            QVERIFY(f.open(f.writeFile(QStringLiteral("dots.md"), "---\na: 1\n...\nbody\n---\nend\n")));
            f.placeCaret(5);
            f.settle();
            QVERIFY(f.editor()->propertiesView() == nullptr);
            QCOMPARE(f.editor()->frontMatterBadge(), QStringLiteral("Invalid YAML (line 3): document end"));
        }
        {
            test::Fixture f;  // blank lines and comments show nothing
            QVERIFY(f.open(f.writeFile(QStringLiteral("comments.md"), "---\n# c\na: 1\n\n# d\nb: 2\n---\nbody\n")));
            f.placeCaret(7);
            f.settle();
            QVERIFY(f.editor()->propertiesView() != nullptr);
            QCOMPARE(names(f.editor()->propertiesView()),
                     (QStringList{QStringLiteral("Properties"), QStringLiteral("a: 1"), QStringLiteral("b: 2")}));
        }
    }

    // Review O4: the table embedder must not undo the properties block, nor the block the table.
    void frontMatterAboveATableStaysConcealed() {
        QByteArray document = "---\na: 1\nb: 2\n---\n| x | y |\n| - | - |\n| 1 | 2 |\n\n";
        for (int i = 0; i < 80; ++i) {
            document += "after " + QByteArray::number(i) + "\n";
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("table.md"), document)));
        f.placeCaret(20);
        f.settle();
        auto* editor = f.editor();
        expectConcealed(editor, 3, "before");
        QVERIFY(editor->tableViewAt(4) != nullptr);
        // Change the table's lines and scroll, so that the tables refresh.
        QTextCursor c(editor->document()->findBlockByNumber(6));
        c.movePosition(QTextCursor::EndOfBlock);
        c.insertText(QStringLiteral(" 3 |"));
        f.settle();
        editor->verticalScrollBar()->setValue(editor->verticalScrollBar()->maximum());
        f.settle();
        editor->verticalScrollBar()->setValue(0);
        f.settle();
        expectConcealed(editor, 3, "after the table refreshed");
        QVERIFY(editor->tableViewAt(4) != nullptr);
    }

    void longFrontMatterIsReadAndBuilt() {
        for (const bool nested : {false, true}) {
            QByteArray document = "---\n";
            if (nested) {
                document += "deep:\n";
                for (int i = 0; i < 5000; ++i) {
                    document += "  - k" + QByteArray::number(i) + ": " + QByteArray::number(i) + "\n";
                }
            } else {
                for (int i = 0; i < 5000; ++i) {
                    document += "key" + QByteArray::number(i) + ": value " + QByteArray::number(i) + "\n";
                }
            }
            const int close = nested ? 5002 : 5001;
            document += "---\nbody\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), document)));
            f.placeCaret(close + 1);
            f.settle();
            auto* editor = f.editor();
            expectConcealed(editor, close, nested ? "nested, opened" : "keys, opened");
            if (QTest::currentTestFailed()) {
                return;
            }
            QCOMPARE(names(editor->propertiesView()).size(), nested ? 2 : 5001);
            f.placeCaret(close);
            f.settle();
            test::keyClick(editor, Qt::Key_Down);  // leaving: the YAML until the block is built
            QVERIFY2(editor->propertiesView() != nullptr || heightOf(editor, close) > 0, "no state in between");
            f.settle();
            expectConcealed(editor, close, nested ? "nested, left" : "keys, left");
        }
    }

    // Armin, 2026-10-31: after opening, the caret stands on the first line below closed front
    // matter, so the first screen shows the properties block; Ctrl+Home still shows the YAML.
    void openingPutsTheCaretBelowTheFrontMatter() {
        {
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("note.md"), "---\ntitle: x\n---\n# Heading\nbody\n")));
            f.settle();
            QCOMPARE(f.editor()->textCursor().position(), f.editor()->document()->findBlockByNumber(3).position());
            expectConcealed(f.editor(), 2, "opened");
            test::keyClick(f.editor(), Qt::Key_Home, Qt::ControlModifier);
            f.settle();
            expectRevealed(f.editor(), 2, "Ctrl+Home");
        }
        {
            QByteArray document = "---\n";  // closed in a later slice of the file, confirmed in idle slices
            for (int i = 0; i < 5000; ++i) {
                document += "key" + QByteArray::number(i) + ": value " + QByteArray::number(i) + "\n";
            }
            document += "---\nbody\n";
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), document)));
            f.settle();
            QCOMPARE(f.editor()->textCursor().blockNumber(), 5002);
            expectConcealed(f.editor(), 5001, "long, opened");
        }
        for (const QByteArray& start : {QByteArray("# No front matter\n---\ntext\n"), QByteArray("---\ntitle: x\n---"),
                                        QByteArray("---\ntitle: x\ntitle: y\n---\nbody\n")}) {
            test::Fixture f;  // no front matter, nothing below it, or invalid YAML: the caret at the start
            QVERIFY(f.open(f.writeFile(QStringLiteral("start.md"), start)));
            f.settle();
            QVERIFY2(f.editor()->textCursor().position() == 0, start.constData());
        }
        {
            test::Fixture f;  // the toggle off: as in S023
            ui::Options options = f.window->options();
            options.frontMatterAsProperties = false;
            f.window->setOptions(options);
            QVERIFY(f.open(f.writeFile(QStringLiteral("off.md"), "---\ntitle: x\n---\nbody\n")));
            f.settle();
            QCOMPARE(f.editor()->textCursor().position(), 0);
        }
    }

    void theToggleGivesTheS023Behaviour() {
        test::Fixture f;
        ui::Options options = f.window->options();
        options.frontMatterAsProperties = false;
        f.window->setOptions(options);
        const QString path = f.writeFile(QStringLiteral("note.md"), "---\ntitle: x\n---\nbody\n");
        QVERIFY(f.open(path));
        f.placeCaret(3);
        f.settle();
        expectRevealed(f.editor(), 2, "toggle off");
        options.frontMatterAsProperties = true;
        f.window->setOptions(options);
        QVERIFY(f.open(path));
        f.placeCaret(3);
        f.settle();
        expectConcealed(f.editor(), 2, "toggle on");
    }

    void theFileStaysByteForByte() {
        for (const QByteArray& bytes : {QByteArray("\xEF\xBB\xBF---\r\nmode: code\r\nlist: [a, b]\r\n---\r\nbody\r\n"),
                                        QByteArray("---\ntitle: x\ntitle: y\n---\nbody\n"), QByteArray(kRecord)}) {
            test::Fixture f;
            const QString path = f.writeFile(QStringLiteral("bytes.md"), bytes);
            QVERIFY(f.open(path));
            auto* editor = f.editor();
            f.placeCaret(editor->document()->blockCount() - 1);
            f.settle();
            for (const auto key : {Qt::Key_Home, Qt::Key_Up, Qt::Key_Up, Qt::Key_Up, Qt::Key_Down, Qt::Key_PageUp,
                                   Qt::Key_End, Qt::Key_PageDown}) {
                test::keyClick(editor, key);
                f.settle();
            }
            f.placeCaret(editor->document()->blockCount() - 1);
            f.settle();
            if (QWidget* view = editor->propertiesView()) {
                clickBlock(view, onValue(view, 0));
                f.settle();
            }
            QVERIFY(f.saveAndWait(path));
            QCOMPARE(test::Fixture::readFile(path), bytes);
        }
        // The valid ones show as properties at all.
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("shown.md"), kRecord)));
        f.placeCaret(kRecordBody);
        f.settle();
        QVERIFY(f.editor()->propertiesView() != nullptr);
    }

    void theBlockFollowsItsTokens() {
        test::Fixture f;
        core::Theme theme = core::builtinTheme(core::ColorScheme::Light);
        theme.properties.background = core::Color{0x10, 0x20, 0x30, 255};
        theme.properties.radius = 0;
        theme.properties.borderWidth = 0;
        theme.properties.padding = 12;
        theme.properties.margin = 10;
        theme.properties.fontFamily = "Props Sans";
        theme.properties.fontScale = 0.5;
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("note.md"), "---\ntitle: x\n---\nbody\n")));
        f.placeCaret(3);
        f.settle();
        auto* editor = f.editor();
        QWidget* view = editor->propertiesView();
        QVERIFY(view != nullptr);
        QCOMPARE(view->grab().toImage().pixelColor(1, 1), QColor(0x10, 0x20, 0x30));
        QVERIFY(qAbs(heightOf(editor, 0) - (view->height() + 20)) < 1);
        QCOMPARE(view->font().families().value(0), QStringLiteral("Props Sans"));
        QCOMPARE(view->font().pointSizeF(), editor->styler().formats().baseFont().pointSizeF() * 0.5);
    }
};

QTEST_MAIN(tst_s024_front_matter)
#include "tst_s024_front_matter.moc"
