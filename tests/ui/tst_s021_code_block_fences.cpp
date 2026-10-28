// SPDX-License-Identifier: GPL-3.0-or-later
// S021: code block fences inside the panel, revealed from anywhere in the block, language badge.
#include "ui_test_support.hpp"

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
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
};

QTEST_MAIN(tst_s021_code_block_fences)
#include "tst_s021_code_block_fences.moc"
