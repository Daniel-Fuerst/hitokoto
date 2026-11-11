// SPDX-License-Identifier: GPL-3.0-or-later
// S033: consecutive lines of a paragraph flow as one wrapped paragraph while the caret is outside
// the paragraph; CommonMark 6.7 hard line breaks (2+ trailing spaces or trailing '\') start a new
// visual line inside the flowing paragraph; placing the caret or selection on any line of the
// paragraph reveals all source lines of that paragraph.
#include "find_bar.hpp"
#include "find_controller.hpp"
#include "ui_test_support.hpp"

#include <QElapsedTimer>
#include <QLineEdit>
#include <QScrollBar>

#include <vector>

using namespace hitokoto;

namespace {

QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

std::vector<std::u16string> linesOf(const QTextDocument* doc) {
    std::vector<std::u16string> lines;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        lines.push_back(b.text().toStdU16String());
    }
    return lines;
}

QString differenceFromTheReference(ui::EditorWidget* editor) {
    const auto* doc = editor->document();
    const QTextCursor cursor = editor->textCursor();
    const int anchor = doc->findBlock(cursor.anchor()).blockNumber();
    const auto reference =
        test::registry().renderDocument(linesOf(doc), core::revealedLines(anchor, cursor.blockNumber()));
    auto& styler = editor->styler();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        auto got = styler.renderFor(b);
        auto want = reference.at(static_cast<std::size_t>(b.blockNumber()));
        if (!got.table && b.layout()->formats() != styler.formats().ranges(got)) {
            return QStringLiteral("line %1 \"%2\" is formatted for another render").arg(b.blockNumber()).arg(b.text());
        }
        got.blockState = {};
        want.blockState = {};
        if (!(got == want)) {
            return QStringLiteral("line %1 \"%2\" differs (flows %3/%4, hardBreak %5/%6, wrapIndent %7/%8)")
                .arg(b.blockNumber())
                .arg(b.text())
                .arg(got.flowsToNext)
                .arg(want.flowsToNext)
                .arg(got.hardBreak)
                .arg(want.hardBreak)
                .arg(got.wrapIndent)
                .arg(want.wrapIndent);
        }
    }
    return {};
}

/// Counts the resize events a widget receives itself (not those of its viewport).
class ResizeCounter : public QObject {
public:
    explicit ResizeCounter(QWidget* widget) : widget_(widget) { widget_->installEventFilter(this); }
    ~ResizeCounter() override { widget_->removeEventFilter(this); }
    ResizeCounter(const ResizeCounter&) = delete;
    ResizeCounter& operator=(const ResizeCounter&) = delete;
    ResizeCounter(ResizeCounter&&) = delete;
    ResizeCounter& operator=(ResizeCounter&&) = delete;

    int count = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == widget_ && event->type() == QEvent::Resize) {
            ++count;
        }
        return false;
    }

private:
    QWidget* widget_;
};

}  // namespace

class tst_s033_soft_line_breaks : public QObject {
    Q_OBJECT
private slots:
    void consecutiveParagraphLinesFlowAsOneWrappedBlock() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("flow.md"), "Alpha\nBeta\nGamma\n\nOutside\n")));
        f.placeCaret(4);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto rect = [&](int n) { return editor->blockLayout()->blockBoundingRect(block(n)); };

        QVERIFY(editor->styler().renderFor(block(0)).flowsToNext);
        QVERIFY(editor->styler().renderFor(block(1)).flowsToNext);
        QVERIFY(!editor->styler().renderFor(block(2)).flowsToNext);

        // Wide window: all three source lines sit on the same visual line with a space gap.
        QCOMPARE(rect(0).top(), rect(1).top());
        QCOMPARE(rect(1).top(), rect(2).top());
        const QTextLine l0 = block(0).layout()->lineAt(0);
        const QTextLine l1 = block(1).layout()->lineAt(0);
        const QTextLine l2 = block(2).layout()->lineAt(0);
        QVERIFY2(l1.x() > l0.x() + l0.naturalTextWidth(), "Beta sits to the right of Alpha with a space gap");
        QVERIFY2(l2.x() > l1.x() + l1.naturalTextWidth(), "Gamma sits to the right of Beta with a space gap");
        QCOMPARE(differenceFromTheReference(editor), QString());
    }

    void caretOrSelectionOnAnyLineRevealsTheWholeParagraph() {
        test::Fixture f;
        constexpr auto kDoc =
            "First *em* line\n"
            "  Second **bold** line\n"
            "  Third `code` line\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("reveal.md"), kDoc)));
        f.placeCaret(4);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto rect = [&](int n) { return editor->blockLayout()->blockBoundingRect(block(n)); };

        // While caret is outside: lines 0, 1, 2 flow together and leading spaces on 1, 2 are collapsed.
        QCOMPARE(rect(0).top(), rect(1).top());
        QVERIFY(test::isCollapsed(block(1), 0));
        QVERIFY(test::isCollapsed(block(1), 1));
        QVERIFY(test::isCollapsed(block(2), 0));
        QVERIFY(test::isCollapsed(block(2), 1));
        QVERIFY(test::isCollapsed(block(0), 6));  // '*' on line 0
        QVERIFY(test::isCollapsed(block(1), 9));  // '**' on line 1
        QVERIFY(test::isCollapsed(block(2), 8));  // '`' on line 2

        // Place caret on line 1: all 3 lines reveal on separate visual lines; only line 1 reveals inline markers.
        f.placeCaret(1, 3);
        f.settle();
        QVERIFY2(rect(0).top() < rect(1).top(), "line 0 and line 1 are on separate visual lines");
        QVERIFY2(rect(1).top() < rect(2).top(), "line 1 and line 2 are on separate visual lines");
        QVERIFY(!test::isCollapsed(block(1), 0));
        QVERIFY(!test::isCollapsed(block(2), 0));
        QVERIFY(test::isCollapsed(block(0), 6));   // '*' on line 0 stays concealed
        QVERIFY(!test::isCollapsed(block(1), 9));  // '**' on line 1 is revealed
        QVERIFY(test::isCollapsed(block(2), 8));   // '`' on line 2 stays concealed
        QCOMPARE(differenceFromTheReference(editor), QString());

        // Move caret back outside: paragraph flows again.
        f.placeCaret(4);
        f.settle();
        QCOMPARE(rect(0).top(), rect(1).top());
        QVERIFY(test::isCollapsed(block(1), 0));
        QCOMPARE(differenceFromTheReference(editor), QString());
    }

    void hardLineBreaksWithBackslashAndTwoSpaces() {
        test::Fixture f;
        constexpr auto kDoc =
            "Line one\\\n"
            "Line two  \n"
            "Line three\\\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("hard.md"), kDoc)));
        f.placeCaret(4);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto rect = [&](int n) { return editor->blockLayout()->blockBoundingRect(block(n)); };

        // While flowing: line 0 and line 1 have hard breaks; line 2 is the last line so trailing '\' is literal.
        QVERIFY(editor->styler().renderFor(block(0)).hardBreak);
        QVERIFY(!editor->styler().renderFor(block(0)).flowsToNext);
        QVERIFY(editor->styler().renderFor(block(1)).hardBreak);
        QVERIFY(!editor->styler().renderFor(block(1)).flowsToNext);
        QVERIFY(!editor->styler().renderFor(block(2)).hardBreak);
        QVERIFY(!editor->styler().renderFor(block(2)).flowsToNext);

        QVERIFY(test::isCollapsed(block(0), 8));    // trailing '\' on line 0 concealed
        QVERIFY(test::isCollapsed(block(1), 8));    // trailing ' ' on line 1 concealed
        QVERIFY(test::isCollapsed(block(1), 9));    // trailing ' ' on line 1 concealed
        QVERIFY(!test::isCollapsed(block(2), 10));  // trailing '\' on last line is literal

        // Each hard-broken line starts on a new visual line at the left margin.
        QVERIFY(rect(0).top() < rect(1).top());
        QVERIFY(rect(1).top() < rect(2).top());
        QCOMPARE(block(0).layout()->lineAt(0).x(), block(1).layout()->lineAt(0).x());
        QCOMPARE(block(1).layout()->lineAt(0).x(), block(2).layout()->lineAt(0).x());

        // Reveal the paragraph by placing the caret on line 2 (not even line 0 or 1!).
        f.placeCaret(2, 0);
        f.settle();
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        const auto escapeStyle = core::escapeStyle(theme);
        QVERIFY(!test::isCollapsed(block(0), 8));
        QCOMPARE(test::formatAt(block(0), 8).foreground().color(), qcolor(escapeStyle.marker));
        QVERIFY(!test::isCollapsed(block(1), 8));
        QVERIFY(!test::isCollapsed(block(1), 9));
        QCOMPARE(differenceFromTheReference(editor), QString());
    }

    void bulletListContinuationLinesFlowAndIndent() {
        test::Fixture f;
        constexpr auto kDoc =
            "- first part\n"
            "  second part\\\n"
            "  third part after hard break\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("bullet.md"), kDoc)));
        f.placeCaret(4);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto rect = [&](int n) { return editor->blockLayout()->blockBoundingRect(block(n)); };

        // Line 0 and line 1 flow onto the same visual line; line 2 starts on a new line at the hanging indent.
        QCOMPARE(rect(0).top(), rect(1).top());
        QVERIFY(rect(1).top() < rect(2).top());
        QVERIFY(test::isCollapsed(block(1), 0));
        QVERIFY(test::isCollapsed(block(1), 1));
        QVERIFY(test::isCollapsed(block(2), 0));
        QVERIFY(test::isCollapsed(block(2), 1));

        const qreal itemTextX = block(0).layout()->lineAt(0).cursorToX(2);
        QCOMPARE(block(2).layout()->lineAt(0).x(), itemTextX);
        QCOMPARE(differenceFromTheReference(editor), QString());
    }

    void orderedListContinuationLinesFlowAndReveal() {
        test::Fixture f;
        f.window->resize(900, 700);
        constexpr auto kDoc =
            "1. Implement `LineElement` for line-local block constructs, `MultiLineElement` for blocks that\n"
            "   span lines (fenced code) or `InlineElement` for inline constructs in\n"
            "   `src/core/src/elements/<name>.cpp`, with a factory function. Inline code runs take precedence\n"
            "   over other inline styles. A multi-line element keeps a non-zero number of its own as state;\n"
            "   the registry wraps it in `BlockState`, which the UI caches per block without knowing the\n"
            "   element.\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("ordered.md"), kDoc)));
        f.placeCaret(7);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        for (int i = 0; i <= 5; ++i) {
            editor->blockLayout()->ensureBlockLayout(block(i));
        }
        // Concealed: continuation lines 1..5 conceal their 3 leading spaces and flow with the line above.
        // (On line 2, column 3 is the opening backtick of `src/...`, which is also concealed.)
        for (int i = 1; i <= 5; ++i) {
            const int firstVisibleCol = (i == 2) ? 4 : 3;
            QVERIFY2(test::isCollapsed(block(i), 0), qPrintable(QStringLiteral("line %1 col 0").arg(i)));
            QVERIFY2(test::isCollapsed(block(i), 2), qPrintable(QStringLiteral("line %1 col 2").arg(i)));
            QVERIFY2(!test::isCollapsed(block(i), firstVisibleCol),
                     qPrintable(QStringLiteral("line %1 col %2").arg(i).arg(firstVisibleCol)));
        }
        // Line 1 ("   span lines...") shares the last visual line of line 0 ("... that").
        QVERIFY(editor->blockLayout()->sharesLineWithPrevious(block(1)));
        QCOMPARE(differenceFromTheReference(editor), QString());

        // Caret on any line of the ordered item reveals all 6 lines onto their own visual lines.
        f.placeCaret(2);
        f.settle();
        for (int i = 1; i <= 5; ++i) {
            QVERIFY(!editor->blockLayout()->sharesLineWithPrevious(block(i)));
            QVERIFY(!test::isCollapsed(block(i), 0));
        }
        QCOMPARE(differenceFromTheReference(editor), QString());
    }

    void multiLineSetextHeadingFlowsAndReveals() {
        test::Fixture f;
        constexpr auto kDoc =
            "Heading one\n"
            "heading two\n"
            "===\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("setext.md"), kDoc)));
        f.placeCaret(4);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto rect = [&](int n) { return editor->blockLayout()->blockBoundingRect(block(n)); };

        // Concealed: lines 0 and 1 flow on one visual line at heading 1 size; underline is hidden.
        QCOMPARE(rect(0).top(), rect(1).top());
        QCOMPARE(test::formatAt(block(0), 0).fontPointSize(), editor->styler().formats().heading(1).fontPointSize());
        QCOMPARE(test::formatAt(block(1), 0).fontPointSize(), editor->styler().formats().heading(1).fontPointSize());
        QCOMPARE(test::formatAt(block(2), 0).foreground().color(), QColor(Qt::transparent));

        // Caret on any of 0, 1, or 2 (the underline) reveals all lines of the heading and the underline.
        for (const int caret : {0, 1, 2}) {
            f.placeCaret(caret);
            f.settle();
            QVERIFY2(rect(0).top() < rect(1).top(), qPrintable(QStringLiteral("caret on %1").arg(caret)));
            QCOMPARE(test::formatAt(block(2), 0).foreground().color(),
                     qcolor(core::builtinTheme(core::ColorScheme::Light).marker));
            QCOMPARE(differenceFromTheReference(editor), QString());
        }
    }

    void clickingContinuationLinePlacesCaretAndRevealsParagraph() {
        test::Fixture f;
        constexpr auto kDoc =
            "First line\n"
            "  Second line\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("click.md"), kDoc)));
        f.placeCaret(3);
        f.settle();

        auto* editor = f.editor();
        const auto* doc = editor->document();
        const QTextBlock b0 = doc->findBlockByNumber(0);
        const QTextBlock b1 = doc->findBlockByNumber(1);
        editor->blockLayout()->ensureBlockLayout(b0);
        editor->blockLayout()->ensureBlockLayout(b1);

        const QRectF r0 = editor->blockLayout()->blockBoundingRect(b0);
        const QRectF r1 = editor->blockLayout()->blockBoundingRect(b1);
        QCOMPARE(r0.top(), r1.top());
        const QTextLine line0 = b1.layout()->lineAt(0);
        QVERIFY(line0.x() > b0.layout()->lineAt(0).x());
        // Click in the middle of "Second" on block 1 while it is flowing on block 0's visual line.
        const qreal clickX = line0.cursorToX(4);
        const qreal clickY = r1.top() + line0.y() + line0.height() / 2.0;
        const QPoint viewportPt(static_cast<int>(clickX), static_cast<int>(clickY));

        QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPt);
        f.settle();

        const QTextCursor c = editor->textCursor();
        QCOMPARE(c.blockNumber(), 1);
        QVERIFY2(c.positionInBlock() >= 2 && c.positionInBlock() <= 8,
                 qPrintable(QStringLiteral("column %1").arg(c.positionInBlock())));
        QVERIFY(editor->blockLayout()->blockBoundingRect(b0).top() <
                editor->blockLayout()->blockBoundingRect(b1).top());
    }

    void findMatchesAcrossSoftLineBreakAndEscapeSelects() {
        test::Fixture f;
        constexpr auto kDoc =
            "Hello\n"
            "  world today\n"
            "\n"
            "Outside\n";
        QVERIFY(f.open(f.writeFile(QStringLiteral("find.md"), kDoc)));
        f.placeCaret(3);
        f.settle();

        auto* editor = f.editor();
        QCOMPARE(editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(0)).top(),
                 editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(1)).top());
        ui::FindController* find = f.window->findController();
        ui::FindBar* bar = f.window->findBar();
        find->open();
        test::keyClicks(bar->searchField(), QStringLiteral("hello world"));
        QVERIFY(QTest::qWaitFor([&] { return find->isComplete(); }, 30000));
        QCOMPARE(find->matchCount(), 1);

        test::keyClick(bar->searchField(), Qt::Key_Escape);
        f.settle();
        const QTextCursor c = editor->textCursor();
        QVERIFY(c.hasSelection());
        QCOMPARE(editor->document()->findBlock(c.selectionStart()).blockNumber(), 0);
        QCOMPARE(editor->document()->findBlock(c.selectionEnd()).blockNumber(), 1);
        // The paragraph is now revealed because the selection spans lines 0..1.
        QVERIFY(editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(0)).top() <
                editor->blockLayout()->blockBoundingRect(editor->document()->findBlockByNumber(1)).top());
    }

    void savePreservesExactBytesAfterWalkingThroughFlowingParagraphs() {
        test::Fixture f;
        const QByteArray original =
            "Paragraph line one\n"
            "  line two with trailing space \n"
            "  line three with hard break\\\n"
            "  line four with two spaces  \n"
            "  line five.\n"
            "\n"
            "- bullet one\n"
            "  bullet two\n";
        const QString path = f.writeFile(QStringLiteral("roundtrip.md"), original);
        QVERIFY(f.open(path));
        f.placeCaret(0);
        f.settle();
        for (int i = 0; i < 8; ++i) {
            test::keyClick(f.editor(), Qt::Key_Down);
            f.settle();
            QCOMPARE(differenceFromTheReference(f.editor()), QString());
        }
        const QString saved = f.dir.filePath(QStringLiteral("saved.md"));
        QVERIFY(f.saveAndWait(saved));
        QCOMPARE(test::Fixture::readFile(saved), original);
    }

    // A new width re-estimates every block (S019 finding 2). In a flowing paragraph the line at the
    // top of the view can start inside a block that shares its first line with the blocks before
    // it; the reading position is that line, not the first block of the shared line.
    void resizingKeepsTheTopLineInsideAFlowingParagraph() {
        QByteArray markdown = "# Notes\n\n";
        for (int p = 0; p < 40; ++p) {
            for (int i = 0; i < 24; ++i) {
                const QByteArray words = i % 5 == 0 ? QByteArray(120, 'a').replace("aaaa", "aa a") : QByteArray("aa a");
                markdown += "word " + QByteArray::number(p * 100 + i) + ' ' + words + '\n';
            }
            markdown += '\n';
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("resize_flow.md"), markdown)));
        f.placeCaret(0);
        f.settle();

        auto* editor = f.editor();
        auto* bar = editor->verticalScrollBar();
        const auto topBlock = [&] { return editor->cursorForPosition(QPoint(40, 2)).blockNumber(); };
        // Page down until the top line belongs to a block well behind the first block of its shared
        // lines (firstBlockOfLineAt), so that blocks between the two lie above the reading position.
        bool inside = false;
        for (int page = 0; page < 40 && !inside; ++page) {
            bar->triggerAction(QAbstractSlider::SliderPageStepAdd);
            f.settle();
            const QTextBlock top = editor->document()->findBlockByNumber(topBlock());
            inside = editor->blockLayout()->sharesLineWithPrevious(top) &&
                     top.blockNumber() - editor->blockLayout()->firstBlockOfLineAt(bar->value()).blockNumber() >= 6;
        }
        if (!inside) {
            // Which page starts where depends on the font; tst_s019 resizingKeepsTheReadingPosition
            // covers the same case at the end of a document.
            QSKIP("no page starts well inside a shared line with this font");
        }
        const int before = topBlock();

        f.window->resize(f.window->width() - 40, f.window->height());
        f.settle();
        QCOMPARE(topBlock(), before);
        f.window->resize(f.window->width() + 40, f.window->height());
        f.settle();
        QCOMPARE(topBlock(), before);
    }

    // The new margins of a resize re-lay out the viewport, and Qt passes that resize on to the
    // editor's resizeEvent while the first one still runs (S033 review O3). The nested call must
    // not restore a reading position of its own, taken from the width in between: one resize of
    // the editor restores the reading position once.
    void aResizeRestoresTheReadingPositionOnce() {
        QByteArray markdown;
        for (int i = 0; i < 1500; ++i) {
            markdown += "line " + QByteArray::number(i) + ' ' +
                        QByteArray(i % 3 == 0 ? 180 : 10, 'a').replace("aaaa", "aa a") + '\n';
        }
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("resize_once.md"), markdown)));
        f.settle();
        auto* editor = f.editor();
        auto* bar = editor->verticalScrollBar();
        for (int page = 0; page < 30; ++page) {
            bar->triggerAction(QAbstractSlider::SliderPageStepAdd);
            f.settle();
        }
        QVERIFY(bar->value() > 0);  // at the very top there is no reading position to restore

        // The window is wider than the built-in theme's editor-max-width, so the side margins
        // change with the width.
        QVERIFY(editor->width() > 760 + 40);
        ResizeCounter editorResizes(editor);
        const quint64 before = editor->anchorRestores();
        f.window->resize(f.window->width() - 40, f.window->height());
        f.settle();
        QVERIFY(editorResizes.count > 0);
        QCOMPARE(editor->anchorRestores() - before, static_cast<quint64>(editorResizes.count));
    }

    void longFlowingParagraphStaysWithinLatencyBudget() {
        constexpr int kLines = 20000;
        QByteArray document = "Outside top\n\n";
        for (int i = 0; i < kLines; ++i) {
            document += "hard-wrapped text of a long paragraph, line " + QByteArray::number(i) + "\n";
        }
        document += "\nOutside bottom\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long_para.md"), document)));
        f.placeCaret(0);
        f.settle();

        auto* editor = f.editor();
        auto& styler = editor->styler();
        QVERIFY(styler.renderFor(editor->document()->findBlockByNumber(2)).flowsToNext);

        // Enter the 20,000-line paragraph from above: synchronous work is bounded.
        QElapsedTimer timer;
        timer.start();
        f.placeCaret(2);
        const qint64 enterMs = timer.elapsed();
        QVERIFY2(enterMs < 50, qPrintable(QStringLiteral("enter paragraph took %1 ms").arg(enterMs)));
        f.settle();

        // Stepping inside the revealed 20,000-line paragraph restyles only the old and new caret lines.
        const quint64 styledBefore = styler.styledCount();
        timer.restart();
        test::keyClick(editor, Qt::Key_Down);
        const qint64 stepMs = timer.elapsed();
        const quint64 styledOnStep = styler.styledCount() - styledBefore;
        QVERIFY2(stepMs < 50, qPrintable(QStringLiteral("step inside paragraph took %1 ms").arg(stepMs)));
        QVERIFY2(styledOnStep <= 4, qPrintable(QStringLiteral("styled %1 blocks on step").arg(styledOnStep)));
    }
};

QTEST_MAIN(tst_s033_soft_line_breaks)
#include "tst_s033_soft_line_breaks.moc"
