// SPDX-License-Identifier: GPL-3.0-or-later
// S009 (standing quality gate): the caret never stands in concealed text. Extended for code block
// fences (S021) and inline links (S022).
#include "ui_test_support.hpp"

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
    "Lines with links: a standard [link](https://example.com) and a [titled link](https://example.com \"Title\").\n"
    "Badge link: [![CI](https://x/badge.svg)](https://github.com/x) and [nested **bold** link](https://nested.org).\n"
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
};

QTEST_MAIN(tst_s009_concealment)
#include "tst_s009_concealment.moc"
