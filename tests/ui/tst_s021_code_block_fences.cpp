// SPDX-License-Identifier: GPL-3.0-or-later
// S021: code block fences inside the panel, revealed from anywhere in the block, language badge.
#include "ui_test_support.hpp"

#include <QScrollBar>

#include <tuple>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

QColor pixel(ui::EditorWidget* editor, qreal x, qreal y) {
    return editor->viewport()->grab().toImage().pixelColor(static_cast<int>(x), static_cast<int>(y));
}

/// A block's rectangle in viewport coordinates.
QRectF viewportRect(ui::EditorWidget* editor, const QTextBlock& block) {
    return editor->blockLayout()->blockBoundingRect(block).translated(0, -editor->verticalScrollBar()->value());
}

/// "before", a code block of `lines` code lines, "after".
QByteArray largeBlock(int lines) {
    QByteArray out = "before\n```python\n";
    for (int i = 0; i < lines; ++i) {
        out += "code line " + QByteArray::number(i) + "\n";
    }
    out += "```\nafter\n";
    return out;
}
}  // namespace

class tst_s021_code_block_fences : public QObject {
    Q_OBJECT
private slots:
    void backticksShownWhenCursorInBlockAndBlockDoesNotExtendOrShrink() {
        constexpr auto markdown =
            "before\n"
            "```python\n"
            "def check_indentation(level: int) -> bool:\n"
            "    return level == 2\n"
            "```\n"
            "after\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), markdown)));
        f.settle();

        // 1. Caret outside the block: backticks are hidden (transparent), block has fixed height
        f.placeCaret(0);
        f.settle();
        const auto opening = f.editor()->document()->findBlockByNumber(1);
        const auto line1 = f.editor()->document()->findBlockByNumber(2);
        const auto line2 = f.editor()->document()->findBlockByNumber(3);
        const auto closing = f.editor()->document()->findBlockByNumber(4);

        QCOMPARE(test::formatAt(opening, 0).foreground().color(), Qt::transparent);
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), Qt::transparent);

        const QRect openingRectBefore = f.editor()->cursorRect(QTextCursor(opening));
        const QRect line1RectBefore = f.editor()->cursorRect(QTextCursor(line1));
        const QRect line2RectBefore = f.editor()->cursorRect(QTextCursor(line2));
        const QRect closingRectBefore = f.editor()->cursorRect(QTextCursor(closing));

        // 2. Caret on line 3 (return level == 2) inside the block:
        // Backticks MUST be shown on both opening and closing lines
        f.placeCaret(3, 10);
        f.settle();

        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        QCOMPARE(test::formatAt(opening, 0).foreground().color(), qcolor(theme.marker));
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), qcolor(theme.marker));

        // The block must NOT extend or shrink (positions of all lines remain identical)
        QCOMPARE(f.editor()->cursorRect(QTextCursor(opening)), openingRectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line1)), line1RectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line2)), line2RectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(closing)), closingRectBefore);

        // 3. Move caret to line 2 (def check_indentation...): backticks remain shown
        f.placeCaret(2, 5);
        f.settle();
        QCOMPARE(test::formatAt(opening, 0).foreground().color(), qcolor(theme.marker));
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), qcolor(theme.marker));
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line1)), line1RectBefore);

        // 4. Move caret back outside (line 5): backticks hidden again without geometry change
        f.placeCaret(5);
        f.settle();
        QCOMPARE(test::formatAt(opening, 0).foreground().color(), Qt::transparent);
        QCOMPARE(test::formatAt(closing, 0).foreground().color(), Qt::transparent);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(opening)), openingRectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line1)), line1RectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(line2)), line2RectBefore);
        QCOMPARE(f.editor()->cursorRect(QTextCursor(closing)), closingRectBefore);
    }

    // Review O2: a caret step inside a block must not depend on the size of the block.
    void aCaretStepInALargeBlockRendersFewLines() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("large.md"), largeBlock(20000))));
        f.settle();
        f.placeCaret(10000);
        f.settle();
        auto& styler = f.editor()->styler();
        const quint64 renders = styler.renderCount();
        const quint64 styled = styler.styledCount();
        test::keyClick(f.editor(), Qt::Key_Down);
        test::keyClick(f.editor(), Qt::Key_Down);
        test::keyClick(f.editor(), Qt::Key_Up);
        QCOMPARE(f.editor()->textCursor().blockNumber(), 10001);
        QVERIFY2(styler.renderCount() - renders < 100, qPrintable(QString::number(styler.renderCount() - renders)));
        f.settle();
        QVERIFY2(styler.styledCount() - styled < 100, qPrintable(QString::number(styler.styledCount() - styled)));
    }

    void enteringALargeBlockRendersAndRestylesFewLines() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("large.md"), largeBlock(20000))));
        f.settle();
        auto& styler = f.editor()->styler();
        // From below onto the closing fence, and from above onto the opening fence.
        for (const auto& [from, key, to] : {std::tuple{20003, Qt::Key_Up, 20002}, std::tuple{0, Qt::Key_Down, 1}}) {
            f.placeCaret(from);
            f.settle();
            const quint64 renders = styler.renderCount();
            const quint64 styled = styler.styledCount();
            test::keyClick(f.editor(), key);
            QCOMPARE(f.editor()->textCursor().blockNumber(), to);
            QVERIFY2(styler.renderCount() - renders < 100, qPrintable(QString::number(styler.renderCount() - renders)));
            f.settle();
            QVERIFY2(styler.styledCount() - styled < 100, qPrintable(QString::number(styler.styledCount() - styled)));
            // Both fences are revealed although they are 20 000 lines apart.
            const auto doc = f.editor()->document();
            QVERIFY(styler.isRevealed(1));
            QVERIFY(styler.isRevealed(20002));
            QCOMPARE(test::formatAt(doc->findBlockByNumber(20002), 0).foreground().color(),
                     qcolor(core::builtinTheme(core::ColorScheme::Light).marker));
        }
    }

    // A block whose states are not known yet (just opened) is searched for its closing fence in
    // idle slices; the fence is revealed once found.
    void aClosingFenceFarAwayIsRevealedOnceFound() {
        test::Fixture f;
        QSignalSpy loaded(f.session(), &ui::DocumentSession::loadFinished);
        f.window->openFile(f.writeFile(QStringLiteral("large.md"), largeBlock(200000)));
        QVERIFY(QTest::qWaitFor([&] { return !loaded.isEmpty(); }, 60000));
        f.placeCaret(1);
        f.settle();
        auto& styler = f.editor()->styler();
        QVERIFY(styler.isRevealed(200002));
        QVERIFY(!styler.isRevealed(200003));
        f.placeCaret(200003);
        f.settle();
        QVERIFY(!styler.isRevealed(1));
        QVERIFY(!styler.isRevealed(200002));
    }

    // Typing in a block keeps both fences revealed; typing a closing fence moves the reveal.
    void editsInsideTheBlockKeepTheFencesRevealed() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), largeBlock(3))));
        f.settle();
        auto& styler = f.editor()->styler();
        f.placeCaret(3, 4);
        test::keyClicks(f.editor(), QStringLiteral("abc"));
        test::keyClick(f.editor(), Qt::Key_Return);
        test::keyClicks(f.editor(), QStringLiteral("more"));
        test::keyClick(f.editor(), Qt::Key_End);
        f.settle();
        QVERIFY(styler.isRevealed(1));
        QVERIFY(styler.isRevealed(6));
        QVERIFY(!styler.isRevealed(7));
        // A new closing fence above the old one: the block ends there now.
        test::keyClick(f.editor(), Qt::Key_Return);
        test::keyClicks(f.editor(), QStringLiteral("```"));
        f.settle();
        QCOMPARE(f.editor()->textCursor().blockNumber(), 5);
        QVERIFY(styler.isRevealed(1));
        QVERIFY(styler.isRevealed(5));
        QVERIFY2(!styler.isRevealed(7), "the old closing fence opens a new block now");
        f.placeCaret(0);
        f.settle();
        QVERIFY(!styler.isRevealed(1));
        QVERIFY(!styler.isRevealed(5));
    }

    // Review O3/O4: the margin is part of the block geometry (ADR 0003), set by a token.
    void theMarginIsPartOfTheLayout() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.codeBlock.margin = 6;
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\ncode\n```\nafter\n")));
        f.settle();
        const auto* layout = f.editor()->blockLayout();
        const auto doc = f.editor()->document();
        const auto height = [&](int n) { return layout->blockBoundingRect(doc->findBlockByNumber(n)).height(); };
        const auto line = [&](int n) { return doc->findBlockByNumber(n).layout()->lineAt(0); };
        QCOMPARE(line(1).y(), 6.0);
        QCOMPARE(height(1), line(1).height() + 6);
        QCOMPARE(line(2).y(), 0.0);
        QCOMPARE(height(2), line(2).height());
        QCOMPARE(line(3).y(), 0.0);
        QCOMPARE(height(3), line(3).height() + 6);
        QCOMPARE(height(0), line(0).height());
    }

    // Review O3: painting changes no line position.
    void paintingMovesNoLine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\ncode\n```\nafter\n")));
        f.settle();
        const double margin = f.editor()->styler().formats().codeBlock().margin;
        f.editor()->viewport()->repaint();
        const auto doc = f.editor()->document();
        QCOMPARE(doc->findBlockByNumber(1).layout()->lineAt(0).y(), margin);
        QCOMPARE(doc->findBlockByNumber(2).layout()->lineAt(0).y(), 0.0);
        QCOMPARE(doc->findBlockByNumber(3).layout()->lineAt(0).y(), 0.0);
    }

    void thePanelOverlapsNoNeighbour() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"), "before\n```\ncode\n```\nafter\n")));
        f.settle();
        f.placeCaret(4);
        f.settle();
        f.editor()->viewport()->repaint();
        const auto doc = f.editor()->document();
        const QColor panel = qcolor(f.editor()->styler().formats().codeBlock().background);
        const qreal x = f.editor()->viewport()->width() / 2.0;
        const QRectF before = viewportRect(f.editor(), doc->findBlockByNumber(0));
        const QRectF after = viewportRect(f.editor(), doc->findBlockByNumber(4));
        const QRectF code = viewportRect(f.editor(), doc->findBlockByNumber(2));
        QCOMPARE(pixel(f.editor(), x, code.center().y()), panel);
        QVERIFY(pixel(f.editor(), x, before.bottom() - 1) != panel);
        QVERIFY(pixel(f.editor(), x, after.top() + 1) != panel);
    }

    // Review O6/O7: the badge is the language from core, the first word of the info string.
    void theBadgeShowsTheLanguage() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("code.md"),
                                   "```python title=\"x\"\ncode\n```\n~~~ a`b c\ncode\n~~~\n```\ncode\n```\n"
                                   "```c\\+\\+\ncode\n```\nafter\n")));
        f.settle();
        f.placeCaret(12);
        f.settle();
        const auto doc = f.editor()->document();
        const auto badge = [&](int n) { return f.editor()->codeBlockBadge(doc->findBlockByNumber(n)); };
        QCOMPARE(badge(0), QStringLiteral("Python"));
        QCOMPARE(badge(3), QStringLiteral("A`b"));
        QCOMPARE(badge(6), QString());
        QCOMPARE(badge(9), QStringLiteral("C++"));
        QCOMPARE(badge(2), QString());
        f.placeCaret(1);
        f.settle();
        QVERIFY2(badge(0).isEmpty(), "no badge while the fence is revealed");
    }

    // Review O4: the badge font comes from its tokens.
    void theBadgeFontComesFromItsTokens() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.codeBlock.badgeFontFamily = "Badge Sans";
        theme.codeBlock.badgeFontScale = 0.5;
        f.window->applyTheme(theme);
        const QFont font = f.editor()->styler().formats().badgeFont();
        QCOMPARE(font.families().value(0), QStringLiteral("Badge Sans"));
        QCOMPARE(font.pointSizeF(), theme.editorFontSize * 0.5);
        f.window->applyTheme(core::builtinTheme(core::ColorScheme::Light));
        QCOMPARE(f.editor()->styler().formats().badgeFont(), f.editor()->styler().formats().baseFont());
    }
};

QTEST_MAIN(tst_s021_code_block_fences)
#include "tst_s021_code_block_fences.moc"
