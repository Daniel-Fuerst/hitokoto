// SPDX-License-Identifier: GPL-3.0-or-later
// S009 (standing quality gate): the caret never stands in concealed text. Extended for code block
// fences (S021): a caret anywhere in a block reveals both fences, and no line of it moves.
// Extended for thematic breaks and setext headings (S023): breaks revealed under the caret, underlines
// while the caret is on their heading; no line moves. Extended for front matter shown as properties
// (S024): the YAML shows exactly while the caret is in it; only then the lines below move.
#include "ui_test_support.hpp"

#include <QAccessible>
#include <QMap>
#include <QRandomGenerator>

#include <array>

using namespace hitokoto;

namespace {

constexpr auto kDocument =
    "# Heading one\n"
    "Text with *emphasis*, **bold** and `code`.\n"
    "- a bullet\n"
    "  - a nested bullet\n"
    "```python title=\"x\"\n"
    "def f(x):\n"
    "\n"
    "    return x * 2\n"
    "```\n"
    "## Heading two\n"
    "~~~\n"
    "plain code\n"
    "~~~\n"
    "```c\n"
    "int main() {}\n"
    "```\n"
    "Last line with ~~strike~~.\n"
    "```\n"
    "unterminated block\n";

/// Fences of the code block holding `line` per the core, or {-1, -1}; close -1 if unterminated.
std::pair<int, int> fencesOf(const QTextDocument* doc, int line) {
    const auto& registry = test::registry();
    core::BlockState state;
    int open = -1;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const std::u16string text = b.text().toStdU16String();
        const auto render = registry.render(text, {false, state});
        if (render.codeBlock && render.fenceLine && !state.active()) {
            open = b.blockNumber();
        }
        const bool closes = render.codeBlock && render.fenceLine && state.active();
        if (b.blockNumber() >= line && open >= 0 && render.codeBlock) {
            if (closes || !b.next().isValid()) {
                return {open, closes ? b.blockNumber() : -1};
            }
        } else if (b.blockNumber() >= line) {
            return {-1, -1};
        }
        if (closes) {
            open = -1;
        }
        state = render.blockState;
    }
    return {-1, -1};
}

}  // namespace

class tst_s009_concealment : public QObject {
    Q_OBJECT
private slots:
    void caretNeverInConcealedTextWhileWalking() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("gate.md"), kDocument)));
        f.settle();
        auto* editor = f.editor();
        auto& styler = editor->styler();
        const auto* doc = editor->document();
        std::vector<QRectF> concealedGeometry;
        f.placeCaret(doc->blockCount() - 1);
        f.placeCaret(9);  // a heading between the blocks: every block concealed
        f.settle();
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            concealedGeometry.push_back(editor->blockLayout()->blockBoundingRect(b));
        }

        constexpr std::array kKeys = {Qt::Key_Up,   Qt::Key_Down, Qt::Key_Left,   Qt::Key_Right,
                                      Qt::Key_Home, Qt::Key_End,  Qt::Key_PageUp, Qt::Key_PageDown};
        QRandomGenerator rng(20261028);
        for (int step = 0; step < 600; ++step) {
            const auto key = kKeys.at(rng.bounded(static_cast<quint32>(kKeys.size())));
            test::keyClick(editor, key);
            f.settle();
            const QTextCursor c = editor->textCursor();
            const QTextBlock block = c.block();
            const auto column = static_cast<std::uint32_t>(c.positionInBlock());
            for (const auto& h : styler.renderFor(block).hidden) {
                QVERIFY2(column < h.span.start || column >= h.span.end() || h.span.length == 0,
                         qPrintable(QStringLiteral("step %1: caret in hidden text, line %2 column %3")
                                        .arg(step)
                                        .arg(block.blockNumber())
                                        .arg(column)));
            }
            // S021: a caret in a code block reveals both fences; no line of the block moves.
            const auto [open, close] = fencesOf(doc, block.blockNumber());
            if (open >= 0) {
                for (const int fence : {open, close}) {
                    if (fence >= 0) {
                        QVERIFY2(styler.renderFor(doc->findBlockByNumber(fence)).hidden.empty(),
                                 qPrintable(QStringLiteral("step %1: fence %2 concealed with the caret on line %3")
                                                .arg(step)
                                                .arg(fence)
                                                .arg(block.blockNumber())));
                    }
                }
                const int last = close >= 0 ? close : doc->blockCount() - 1;
                for (int n = open; n <= last; ++n) {
                    QCOMPARE(editor->blockLayout()->blockBoundingRect(doc->findBlockByNumber(n)),
                             concealedGeometry.at(static_cast<std::size_t>(n)));
                }
            }
        }
    }

    // S023: the break under the caret is revealed, every other break concealed; an underline is
    // revealed exactly while the caret is on a line of its heading; no line moves. (Front matter
    // walks in frontMatterWhileWalking since S024: entering it changes its height.)
    void thematicBreaksAndSetextHeadingsWhileWalking() {
        constexpr auto document =
            "Intro\n"               // 0 plain
            "\n"                    // 1
            "text\n"                // 2
            "# Heading\n"           // 3
            "---\n"                 // 4 break
            "Text above a break\n"  // 5
            "***\n"                 // 6 break
            "- item\n"              // 7
            "---\n"                 // 8 break
            "Setext line one\n"     // 9 heading 9-10
            "line two\n"            // 10
            "---\n"                 // 11 underline
            "\n"                    // 12
            "   _ _ _   \n"         // 13 break
            "* * *\n"               // 14 break
            "Title\n"               // 15 heading
            "===\n"                 // 16 underline
            "last line\n";          // 17
        const QList<int> breaks = {4, 6, 8, 13, 14};
        const QMap<int, std::pair<int, int>> underlines = {{11, {9, 10}}, {16, {15, 15}}};
        const QList<int> plain = {0, 1, 2};
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("breaks.md"), document)));
        f.placeCaret(12);
        f.settle();
        auto* editor = f.editor();
        auto& styler = editor->styler();
        const auto* doc = editor->document();
        std::vector<QRectF> concealedGeometry;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            concealedGeometry.push_back(editor->blockLayout()->blockBoundingRect(b));
        }

        constexpr std::array kKeys = {Qt::Key_Up,   Qt::Key_Down, Qt::Key_Left,   Qt::Key_Right,
                                      Qt::Key_Home, Qt::Key_End,  Qt::Key_PageUp, Qt::Key_PageDown};
        QRandomGenerator rng(20261030);
        for (int step = 0; step < 300; ++step) {
            test::keyClick(editor, kKeys.at(rng.bounded(static_cast<quint32>(kKeys.size()))));
            f.settle();
            const int caretLine = editor->textCursor().blockNumber();
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                const int n = b.blockNumber();
                const auto render = styler.renderFor(b);
                const QString where = QStringLiteral("step %1, caret on %2, line %3").arg(step).arg(caretLine).arg(n);
                QVERIFY2(render.thematicBreak == breaks.contains(n), qPrintable(where));
                QVERIFY2((render.setextUnderline != 0) == underlines.contains(n), qPrintable(where));
                if (render.thematicBreak) {
                    QVERIFY2(render.hidden.empty() == (n == caretLine), qPrintable(where));
                }
                if (underlines.contains(n)) {
                    const auto [first, last] = underlines.value(n);
                    const bool onHeading = caretLine >= first && caretLine <= n;
                    QVERIFY2(render.hidden.empty() == onHeading, qPrintable(where + QStringLiteral(": underline")));
                }
                if (plain.contains(n)) {
                    QVERIFY2(render.empty(), qPrintable(where + QStringLiteral(": plain")));
                }
                QVERIFY2(
                    editor->blockLayout()->blockBoundingRect(b) == concealedGeometry.at(static_cast<std::size_t>(n)),
                    qPrintable(where + QStringLiteral(": moved")));
            }
        }
    }

    // S024: the YAML of the front matter shows exactly while the caret is on one of its lines, the
    // properties block exactly while it is not; the caret is never on a concealed line; no other line
    // changes height, and the lines below move only when the caret enters or leaves the block.
    void frontMatterWhileWalking() {
        constexpr auto document =
            "---\n"                    // 0 front matter
            "title: A note\n"          // 1
            "date: 2026-10-30\n"       // 2
            "tags: [a, b]\n"           // 3
            "nested:\n"                // 4
            "  key: value\n"           // 5
            "---\n"                    // 6
            "# Heading\n"              // 7
            "Text with *emphasis*.\n"  // 8
            "```c\n"                   // 9
            "int x;\n"                 // 10
            "```\n"                    // 11
            "last line\n";             // 12
        constexpr int kClose = 6;
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("front-matter.md"), document)));
        f.placeCaret(12);
        f.settle();
        auto* editor = f.editor();
        const auto* doc = editor->document();
        QWidget* view = editor->propertiesView();
        QVERIFY2(view != nullptr, "the properties block with the caret below it");
        // Accessibility: a title and one element per row.
        QAccessibleInterface* a = QAccessible::queryAccessibleInterface(view);
        QVERIFY(a != nullptr);
        QCOMPARE(a->childCount(), 5);
        QCOMPARE(a->child(0)->text(QAccessible::Name), QStringLiteral("Properties"));
        QCOMPARE(a->child(1)->text(QAccessible::Name), QStringLiteral("title: A note"));

        const auto rectOf = [&](int n) { return editor->blockLayout()->blockBoundingRect(doc->findBlockByNumber(n)); };
        std::vector<QRectF> concealed;
        concealed.reserve(static_cast<std::size_t>(doc->blockCount()));
        for (int n = 0; n < doc->blockCount(); ++n) {
            concealed.push_back(rectOf(n));
        }
        constexpr std::array kKeys = {Qt::Key_Up,   Qt::Key_Down, Qt::Key_Left,   Qt::Key_Right,
                                      Qt::Key_Home, Qt::Key_End,  Qt::Key_PageUp, Qt::Key_PageDown};
        QRandomGenerator rng(20261031);
        for (int step = 0; step < 300; ++step) {
            test::keyClick(editor, kKeys.at(rng.bounded(static_cast<quint32>(kKeys.size()))));
            f.settle();
            const int caretLine = editor->textCursor().blockNumber();
            const bool inBlock = caretLine <= kClose;
            const QString where = QStringLiteral("step %1, caret on %2").arg(step).arg(caretLine);
            QVERIFY2((editor->propertiesView() == nullptr) == inBlock, qPrintable(where));
            QVERIFY2(rectOf(caretLine).height() > 0, qPrintable(where + QStringLiteral(": caret on a concealed line")));
            for (int n = kClose + 1; n < doc->blockCount(); ++n) {
                const QRectF r = rectOf(n);
                const QRectF c = concealed.at(static_cast<std::size_t>(n));
                QVERIFY2(r.height() == c.height(),
                         qPrintable(where + QStringLiteral(": line %1 changed height").arg(n)));
                if (!inBlock) {
                    QVERIFY2(r.top() == c.top(), qPrintable(where + QStringLiteral(": line %1 moved").arg(n)));
                }
            }
        }
    }
};

QTEST_MAIN(tst_s009_concealment)
#include "tst_s009_concealment.moc"
