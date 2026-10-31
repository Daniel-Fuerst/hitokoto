// SPDX-License-Identifier: GPL-3.0-or-later
// S023: a thematic break is a horizontal line while the caret is elsewhere and its characters while
// the caret is on it; text underlined with === or --- is a heading; front matter stays text; the
// lazy rendering equals the whole-document reference of the core (ADR 0006).
#include "ui_test_support.hpp"

#include <QRandomGenerator>
#include <QScrollBar>

#include <cmath>
#include <functional>
#include <tuple>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

/// A block's rectangle in viewport coordinates (logical pixels).
QRectF viewportRect(ui::EditorWidget* editor, const QTextBlock& block) {
    return editor->blockLayout()->blockBoundingRect(block).translated(0, -editor->verticalScrollBar()->value());
}

/// The viewport as painted, in device pixels.
QImage grabViewport(ui::EditorWidget* editor) {
    editor->viewport()->repaint();
    return editor->viewport()->grab().toImage();
}

/// Device-pixel rows of the block whose pixel at logical `x` has exactly `colour`.
QList<int> rowsWithColour(ui::EditorWidget* editor, const QTextBlock& block, qreal x, const QColor& colour) {
    const QImage image = grabViewport(editor);
    const qreal dpr = image.devicePixelRatio();
    const QRectF rect = viewportRect(editor, block);
    const int column = static_cast<int>(std::floor(x * dpr));
    QList<int> rows;
    for (int y = static_cast<int>(std::floor(rect.top() * dpr)); y < static_cast<int>(std::ceil(rect.bottom() * dpr));
         ++y) {
        if (y >= 0 && y < image.height() && column >= 0 && column < image.width() &&
            image.pixelColor(column, y) == colour) {
            rows << y;
        }
    }
    return rows;
}

/// Left and right end of the text column in viewport coordinates: where the first line of the
/// document starts and how wide the layout makes it, cut to the visible part of the viewport.
std::pair<qreal, qreal> textColumn(ui::EditorWidget* editor) {
    const QTextLine line = editor->document()->firstBlock().layout()->lineAt(0);
    const qreal left = line.x();  // lines start at the document margin (ADR 0003)
    const qreal right = std::min(left + line.width(), static_cast<qreal>(editor->viewport()->width()));
    return {left, right};
}

/// Expected thickness of the line in device pixels: max(1, round(border-width x device pixel ratio)).
int expectedThickness(ui::EditorWidget* editor, double borderWidth) {
    const qreal dpr = grabViewport(editor).devicePixelRatio();
    return std::max(1, static_cast<int>(std::lround(borderWidth * dpr)));
}

std::vector<std::u16string> linesOf(const QTextDocument* doc) {
    std::vector<std::u16string> lines;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        lines.push_back(b.text().toStdU16String());
    }
    return lines;
}

/// Every line as the styler renders it and as it is formatted equals the whole-document reference
/// (ADR 0006); empty if so, else the first difference.
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
            return QStringLiteral("line %1 \"%2\" differs (heading %3/%4, break %5/%6, underline %7/%8)")
                .arg(b.blockNumber())
                .arg(b.text())
                .arg(got.headingLevel)
                .arg(want.headingLevel)
                .arg(got.thematicBreak)
                .arg(want.thematicBreak)
                .arg(got.setextUnderline)
                .arg(want.setextUnderline);
        }
    }
    return {};
}

QColor lineColour() {
    // thematic-break.border falls back to colors.border.
    return qcolor(core::builtinTheme(core::ColorScheme::Light).border);
}

constexpr auto kBreaks =
    "before\n"
    "\n"
    "---\n"
    "\n"
    "* * *\n"
    "\n"
    "___\n"
    "\n"
    "after\n";
}  // namespace

class tst_s023_thematic_breaks : public QObject {
    Q_OBJECT
private slots:
    void aConcealedBreakIsALineAcrossTheText() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("breaks.md"), kBreaks)));
        f.placeCaret(0);
        f.settle();
        const auto [left, right] = textColumn(f.editor());
        const auto* doc = f.editor()->document();
        const int thickness = expectedThickness(f.editor(), 1);
        for (const int n : {2, 4, 6}) {
            const QTextBlock block = doc->findBlockByNumber(n);
            QVERIFY2(f.editor()->styler().renderFor(block).thematicBreak, qPrintable(QString::number(n)));
            QCOMPARE(test::formatAt(block, 0).foreground().color(), QColor(Qt::transparent));
            QVERIFY2(!test::isCollapsed(block, 0), "the characters keep their space");
            // Crisp rows of full colour, the same at both ends and in the middle of the column.
            const QList<int> mid = rowsWithColour(f.editor(), block, (left + right) / 2, lineColour());
            QCOMPARE(mid.size(), thickness);
            QCOMPARE(rowsWithColour(f.editor(), block, left + 1, lineColour()), mid);
            QCOMPARE(rowsWithColour(f.editor(), block, right - 2, lineColour()), mid);
            const qreal dpr = grabViewport(f.editor()).devicePixelRatio();
            const qreal centre = (mid.front() + mid.back() + 1) / 2.0 / dpr;
            QVERIFY2(qAbs(centre - viewportRect(f.editor(), block).center().y()) <= 1.0, "centred in the block");
            QVERIFY2(rowsWithColour(f.editor(), block, left - 3, lineColour()).isEmpty(), "nothing left of the column");
        }
        QVERIFY(rowsWithColour(f.editor(), doc->findBlockByNumber(1), (left + right) / 2, lineColour()).isEmpty());
    }

    // Also run by tst_s023_thematic_breaks_scaled with QT_SCALE_FACTOR=1.5.
    void aConcealedBreakIsCrispAtTheDeviceScale() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.thematicBreak.border = core::Color{200, 30, 40, 255};
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("breaks.md"), kBreaks)));
        f.placeCaret(0);
        f.settle();
        if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
            QCOMPARE(grabViewport(f.editor()).devicePixelRatio(), qEnvironmentVariable("QT_SCALE_FACTOR").toDouble());
        }
        const auto [left, right] = textColumn(f.editor());
        for (const double width : {1.0, 2.0, 3.0}) {
            theme.thematicBreak.borderWidth = width;
            f.window->applyTheme(theme);
            f.settle();
            const QList<int> rows = rowsWithColour(f.editor(), f.editor()->document()->findBlockByNumber(2),
                                                   (left + right) / 2, QColor(200, 30, 40));
            QCOMPARE(rows.size(), expectedThickness(f.editor(), width));
            QCOMPARE(rows.back() - rows.front() + 1, static_cast<int>(rows.size()));
        }
    }

    // A break that wraps (narrow window) keeps its height and gets one line, centred in the block.
    void aWrappedBreakKeepsItsHeightAndGetsOneLine() {
        test::Fixture f;
        const QByteArray dashes(400, '-');
        QVERIFY(f.open(f.writeFile(QStringLiteral("wide.md"), "before\n\n" + dashes + "\n\nafter\n")));
        f.placeCaret(0);
        f.settle();
        const QTextBlock block = f.editor()->document()->findBlockByNumber(2);
        QVERIFY2(block.layout()->lineCount() > 1, "the break wraps");
        const auto [left, right] = textColumn(f.editor());
        const QList<int> rows = rowsWithColour(f.editor(), block, (left + right) / 2, lineColour());
        QCOMPARE(rows.size(), expectedThickness(f.editor(), 1));
        const qreal dpr = grabViewport(f.editor()).devicePixelRatio();
        QVERIFY(qAbs((rows.front() + 0.5) / dpr - viewportRect(f.editor(), block).center().y()) <= 1.0);
        const QRectF concealed = f.editor()->blockLayout()->blockBoundingRect(block);
        f.placeCaret(2);
        f.settle();
        QCOMPARE(f.editor()->blockLayout()->blockBoundingRect(block), concealed);
    }

    void theCaretRevealsTheCharactersWithoutALine() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("breaks.md"), kBreaks)));
        f.placeCaret(0);
        f.settle();
        const auto* doc = f.editor()->document();
        std::vector<QRectF> concealed;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            concealed.push_back(f.editor()->blockLayout()->blockBoundingRect(b));
        }
        const auto theme = core::builtinTheme(core::ColorScheme::Light);
        const auto [left, right] = textColumn(f.editor());
        for (const int n : {2, 4, 6}) {
            f.placeCaret(n, 1);
            f.settle();
            const QTextBlock block = doc->findBlockByNumber(n);
            const auto render = f.editor()->styler().renderFor(block);
            QVERIFY(render.thematicBreak);
            QVERIFY(render.hidden.empty());
            for (int column = 0; column < block.length() - 1; ++column) {
                QCOMPARE(test::formatAt(block, column).foreground().color(), qcolor(theme.marker));
            }
            QVERIFY2(rowsWithColour(f.editor(), block, (left + right) / 2, lineColour()).isEmpty(),
                     "no line while revealed");
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                QCOMPARE(f.editor()->blockLayout()->blockBoundingRect(b),
                         concealed.at(static_cast<std::size_t>(b.blockNumber())));
            }
        }
    }

    void textUnderlinedIsAHeading() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("setext.md"), "Foo\n===\n\nbar\nbaz\n---\n\nend\n")));
        f.placeCaret(7);
        f.settle();
        const auto* doc = f.editor()->document();
        const auto& formats = f.editor()->styler().formats();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        const auto [left, right] = textColumn(f.editor());
        QCOMPARE(test::formatAt(block(0), 0).fontPointSize(), formats.heading(1).fontPointSize());
        QCOMPARE(test::formatAt(block(3), 0).fontPointSize(), formats.heading(2).fontPointSize());
        QCOMPARE(test::formatAt(block(4), 0).fontPointSize(), formats.heading(2).fontPointSize());
        for (const int underline : {1, 5}) {
            QCOMPARE(test::formatAt(block(underline), 0).foreground().color(), QColor(Qt::transparent));
            QVERIFY2(!test::isCollapsed(block(underline), 0), "the underline keeps its line");
            QVERIFY2(rowsWithColour(f.editor(), block(underline), (left + right) / 2, lineColour()).isEmpty(),
                     "an underline draws no line");
        }
        std::vector<QRectF> concealed;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            concealed.push_back(f.editor()->blockLayout()->blockBoundingRect(b));
        }

        // The caret on a line of a heading reveals its underline, and only that one.
        const QColor marker = qcolor(core::builtinTheme(core::ColorScheme::Light).marker);
        for (const auto& [caret, revealed, other] : {std::tuple{0, 1, 5}, std::tuple{3, 5, 1}, std::tuple{4, 5, 1},
                                                     std::tuple{5, 5, 1}, std::tuple{1, 1, 5}}) {
            f.placeCaret(caret);
            f.settle();
            QCOMPARE(test::formatAt(block(revealed), 0).foreground().color(), marker);
            QCOMPARE(test::formatAt(block(revealed), 0).fontPointSize(), formats.heading(0).fontPointSize());
            QCOMPARE(test::formatAt(block(other), 0).foreground().color(), QColor(Qt::transparent));
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                QCOMPARE(f.editor()->blockLayout()->blockBoundingRect(b),
                         concealed.at(static_cast<std::size_t>(b.blockNumber())));
            }
        }
    }

    // The block states of the lines above and below decide; editing restyles both directions.
    void typingAboveABreakMakesAHeadingAndBack() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("edit.md"), "intro\n\n---\n\nafter\n")));
        f.placeCaret(1);
        f.settle();
        auto& styler = f.editor()->styler();
        const auto* doc = f.editor()->document();
        const auto block = [&](int n) { return doc->findBlockByNumber(n); };
        QVERIFY(styler.renderFor(block(2)).thematicBreak);

        test::keyClicks(f.editor(), QStringLiteral("Foo"));
        f.settle();
        QCOMPARE(styler.renderFor(block(2)).setextUnderline, std::uint8_t{2});
        QVERIFY(!styler.renderFor(block(2)).thematicBreak);
        QCOMPARE(test::formatAt(block(1), 0).fontPointSize(), styler.formats().heading(2).fontPointSize());
        // Revealed from the heading line.
        QCOMPARE(test::formatAt(block(2), 0).foreground().color(),
                 qcolor(core::builtinTheme(core::ColorScheme::Light).marker));

        for (int i = 0; i < 3; ++i) {
            test::keyClick(f.editor(), Qt::Key_Backspace);
        }
        f.settle();
        QVERIFY(styler.renderFor(block(2)).thematicBreak);
        QCOMPARE(test::formatAt(block(2), 0).foreground().color(), QColor(Qt::transparent));
    }

    void typingAnUnderlineMakesTheLineAboveAHeading() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("edit.md"), "one\ntwo\n\nafter\n")));
        f.placeCaret(2);
        f.settle();
        auto& styler = f.editor()->styler();
        const auto* doc = f.editor()->document();
        QCOMPARE(styler.renderFor(doc->findBlockByNumber(0)).headingLevel, std::uint8_t{0});
        test::keyClicks(f.editor(), QStringLiteral("==="));
        f.settle();
        for (const int n : {0, 1}) {
            QCOMPARE(styler.renderFor(doc->findBlockByNumber(n)).headingLevel, std::uint8_t{1});
            QCOMPARE(test::formatAt(doc->findBlockByNumber(n), 0).fontPointSize(),
                     styler.formats().heading(1).fontPointSize());
        }
    }

    void frontMatterStaysText() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("note.md"), "---\ntitle: x\n---\n\n***\n")));
        f.placeCaret(3);
        f.settle();
        const auto* doc = f.editor()->document();
        const auto [left, right] = textColumn(f.editor());
        for (const int n : {0, 1, 2}) {
            const QTextBlock block = doc->findBlockByNumber(n);
            QVERIFY2(f.editor()->styler().renderFor(block).empty(), qPrintable(QString::number(n)));
            QVERIFY(test::formatAt(block, 0).foreground().color() != QColor(Qt::transparent));
            QVERIFY(rowsWithColour(f.editor(), block, (left + right) / 2, lineColour()).isEmpty());
        }
        QCOMPARE(rowsWithColour(f.editor(), doc->findBlockByNumber(4), (left + right) / 2, lineColour()).size(),
                 expectedThickness(f.editor(), 1));
    }

    // ADR 0006: the lazy rendering gives every line the LineRender of the whole-document reference,
    // with the caret anywhere and after edits above and below.
    void lazyRenderingEqualsTheReference() {
        constexpr auto document =
            "---\n"
            "title: x\n"
            "---\n"
            "# Heading\n"
            "---\n"
            "Text with *emphasis*\n"
            "***\n"
            "- item\n"
            "  continued\n"
            "---\n"
            "- item\n"
            "  ---\n"
            "One\n"
            "two\n"
            "---\n"
            "\n"
            "1. ordered\n"
            "---\n"
            "> quote\n"
            "---\n"
            "| a | b |\n"
            "| - | - |\n"
            "| 1 | 2 |\n"
            "---\n"
            "Title\n"
            "=\n"
            "last\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("mixed.md"), document)));
        f.settle();
        auto* editor = f.editor();
        auto& styler = editor->styler();
        const auto compareAll = [&](const QString& when) {
            const auto* doc = editor->document();
            const int caret = editor->textCursor().blockNumber();
            const auto lines = linesOf(doc);
            const auto reference = test::registry().renderDocument(lines, core::revealedLines(caret, caret));
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                auto got = styler.renderFor(b);
                auto want = reference.at(static_cast<std::size_t>(b.blockNumber()));
                got.blockState = {};
                want.blockState = {};
                QVERIFY2(got == want, qPrintable(QStringLiteral("%1: line %2 \"%3\" differs from the reference")
                                                     .arg(when)
                                                     .arg(b.blockNumber())
                                                     .arg(b.text())));
            }
        };
        for (const int caret : {0, 4, 9, 12, 13, 14, 24, 26}) {
            f.placeCaret(caret);
            f.settle();
            compareAll(QStringLiteral("caret on %1").arg(caret));
            if (QTest::currentTestFailed()) {
                return;
            }
        }
        // Edits that change the meaning of the lines below and above.
        f.placeCaret(15);  // the blank line after the setext heading "One / two"
        test::keyClicks(f.editor(), QStringLiteral("x"));
        f.settle();
        compareAll(QStringLiteral("after typing on line 15"));
        f.placeCaret(12);
        test::keyClick(f.editor(), Qt::Key_Home);
        test::keyClicks(f.editor(), QStringLiteral("- "));
        f.settle();
        compareAll(QStringLiteral("after typing a bullet on line 12"));
        f.placeCaret(1);
        test::keyClick(f.editor(), Qt::Key_End);
        test::keyClick(f.editor(), Qt::Key_Down);
        test::keyClick(f.editor(), Qt::Key_End);
        test::keyClicks(f.editor(), QStringLiteral("x"));  // "---x": the front matter is no longer closed
        f.settle();
        compareAll(QStringLiteral("after opening the front matter"));
    }

    // Review O2 (code): typing or removing an underline under a long paragraph passes the new level
    // up a bounded number of lines on the key; the rest is restyled in idle slices.
    void anUnderlineUnderALongParagraphCostsBoundedWorkOnTheKey() {
        constexpr int kLines = 20000;
        QByteArray document = "intro\n\n";
        for (int i = 0; i < kLines; ++i) {
            document += "hard-wrapped text of a long paragraph, line " + QByteArray::number(i) + "\n";
        }
        document += "\nafter\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), document)));
        f.placeCaret(kLines + 2);  // the blank line below the paragraph
        f.settle();
        auto& styler = f.editor()->styler();
        const auto* doc = f.editor()->document();
        for (const auto& [key, level] : {std::pair{Qt::Key_Minus, 2}, std::pair{Qt::Key_Backspace, 0}}) {
            const quint64 before = styler.levelSteps();
            test::keyClick(f.editor(), key);
            QVERIFY2(styler.levelSteps() - before <= 4096,
                     qPrintable(QStringLiteral("%1 lines on the key").arg(styler.levelSteps() - before)));
            f.settle();
            for (const int n : {2, kLines / 2, kLines + 1}) {
                const QTextBlock block = doc->findBlockByNumber(n);
                QCOMPARE(styler.renderFor(block).headingLevel, std::uint8_t(level));
                QCOMPARE(test::formatAt(block, 0).fontPointSize(), styler.formats().heading(level).fontPointSize());
            }
        }
    }

    // Review O4 (code): inside a setext heading too long to walk on one key, a caret step up keeps
    // its underline revealed at once.
    void stepsUpInALongHeadingKeepItsUnderlineRevealed() {
        constexpr int kLines = 60000;
        QByteArray document;
        for (int i = 0; i < kLines; ++i) {
            document += "a long setext heading, line " + QByteArray::number(i) + "\n";
        }
        document += "---\n\nafter\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("heading.md"), document)));
        f.placeCaret(kLines / 2);
        f.settle();
        auto& styler = f.editor()->styler();
        QVERIFY(styler.isRevealed(kLines));
        for (int step = 0; step < 3; ++step) {
            test::keyClick(f.editor(), Qt::Key_Up);
            QVERIFY2(styler.isRevealed(kLines), qPrintable(QStringLiteral("step %1").arg(step)));
        }
    }

    // Review O7 (code): random edits of small documents, with the caret or a selection anywhere; after
    // each, every line equals the reference and is formatted for it.
    void randomEditsEqualTheReference() {
        const QStringList pieces = {
            "Foo",   "bar baz", "",      "---",       "***",       "___",     "===", "-",     "- foo", "* foo",
            "  ---", "  ===",   "  bar", "    code",  "# H",       "1. x",    "> q", "<div>", "```",   "~~~",
            "- - -", "| a |",   "| - |", "| 1 | 2 |", "*em* text", "-   foo", "x\\", "*"};
        QRandomGenerator rng(20261030);
        for (int round = 0; round < 12; ++round) {
            QString document = rng.bounded(4) == 0 ? QStringLiteral("---\ntitle: x\n---\n") : QString();
            for (int i = 2 + static_cast<int>(rng.bounded(12)); i > 0; --i) {
                document += pieces.at(rng.bounded(pieces.size())) + QLatin1Char('\n');
            }
            test::Fixture f;
            QVERIFY(f.open(f.writeFile(QStringLiteral("random.md"), document.toUtf8())));
            f.settle();
            auto* doc = f.editor()->document();
            for (int step = 0; step < 20; ++step) {
                QTextCursor c(doc->findBlockByNumber(static_cast<int>(rng.bounded(doc->blockCount()))));
                switch (rng.bounded(5)) {
                    case 0:
                        c.insertText(pieces.at(rng.bounded(pieces.size())));
                        break;
                    case 1:
                        c.insertText(QStringLiteral("\n"));
                        break;
                    case 2:
                        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                        c.removeSelectedText();
                        break;
                    case 3:
                        c.deletePreviousChar();
                        break;
                    default:
                        c.movePosition(QTextCursor::EndOfBlock);
                        c.insertText(rng.bounded(2) == 0 ? QStringLiteral("-") : QStringLiteral("="));
                        break;
                }
                const int caret = static_cast<int>(rng.bounded(doc->blockCount()));
                f.placeCaret(caret == 0 ? doc->blockCount() - 1 : 0);  // the reveal follows a moved caret
                f.placeCaret(caret);
                if (rng.bounded(3) == 0) {
                    QTextCursor selection = f.editor()->textCursor();
                    selection.setPosition(
                        doc->findBlockByNumber(static_cast<int>(rng.bounded(doc->blockCount()))).position(),
                        QTextCursor::KeepAnchor);
                    f.editor()->setTextCursor(selection);
                }
                f.settle();
                const QString difference = differenceFromTheReference(f.editor());
                QVERIFY2(difference.isEmpty(), qPrintable(QStringLiteral("round %1, step %2: %3\n%4")
                                                              .arg(round)
                                                              .arg(step)
                                                              .arg(difference, doc->toPlainText())));
            }
        }
    }

    // Review O7 (code): paragraphs and a heading longer than the walks on a key run their idle
    // slices; front matter, typing "- " before a lazily continued paragraph, breaking and restoring
    // an underline and typing inside the heading leave every line equal to the reference.
    void longParagraphsEqualTheReference() {
        constexpr int kLines = 12000;
        QByteArray document = "---\ntitle: x\n---\n- an item\n";
        for (int i = 0; i < kLines; ++i) {
            document += "lazily continued text of the item, line " + QByteArray::number(i) + "\n";
        }
        document += "\n";
        const int heading = kLines + 5;
        for (int i = 0; i < kLines; ++i) {
            document += "a long setext heading, line " + QByteArray::number(i) + "\n";
        }
        const int underline = heading + kLines;
        document += "---\n\nafter\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("long.md"), document)));
        f.placeCaret(heading + kLines / 2);
        f.settle();
        QCOMPARE(differenceFromTheReference(f.editor()), QString());
        auto* editor = f.editor();
        const auto step = [&](const QString& what, const std::function<void()>& edit) {
            edit();
            f.settle();
            const QString difference = differenceFromTheReference(editor);
            QVERIFY2(difference.isEmpty(), qPrintable(what + QStringLiteral(": ") + difference));
        };
        step(QStringLiteral("\"- \" before the item's second line"), [&] {
            f.placeCaret(4);
            test::keyClicks(editor, QStringLiteral("- "));
        });
        step(QStringLiteral("removed again"), [&] {
            test::keyClick(editor, Qt::Key_Backspace);
            test::keyClick(editor, Qt::Key_Backspace);
        });
        step(QStringLiteral("the underline broken"), [&] {
            f.placeCaret(underline);
            test::keyClick(editor, Qt::Key_End);
            test::keyClicks(editor, QStringLiteral("x"));
        });
        step(QStringLiteral("the underline restored"), [&] { test::keyClick(editor, Qt::Key_Backspace); });
        step(QStringLiteral("typing inside the heading"), [&] {
            f.placeCaret(heading + kLines / 3);
            test::keyClicks(editor, QStringLiteral("abc"));
        });
        step(QStringLiteral("a blank line splitting the heading"), [&] { test::keyClick(editor, Qt::Key_Return); });
        step(QStringLiteral("joined again"), [&] { test::keyClick(editor, Qt::Key_Backspace); });
    }

    // Theming baseline (ADR 0004): colour and width of the line, marker colour.
    void theLookComesFromItsTokens() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.thematicBreak.border = core::Color{200, 30, 40, 255};
        theme.thematicBreak.borderWidth = 4;
        theme.thematicBreak.marker = core::Color{20, 140, 60, 255};
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("breaks.md"), kBreaks)));
        f.placeCaret(0);
        f.settle();
        const auto* doc = f.editor()->document();
        const auto [left, right] = textColumn(f.editor());
        const QTextBlock block = doc->findBlockByNumber(2);
        const QList<int> rows = rowsWithColour(f.editor(), block, (left + right) / 2, QColor(200, 30, 40));
        QCOMPARE(rows.size(), expectedThickness(f.editor(), 4));
        QCOMPARE(rows.back() - rows.front() + 1, static_cast<int>(rows.size()));

        f.placeCaret(2);
        f.settle();
        QCOMPARE(test::formatAt(block, 0).foreground().color(), QColor(20, 140, 60));
        f.window->applyTheme(core::builtinTheme(core::ColorScheme::Light));
    }

    // The margin is part of the block geometry (ADR 0003), concealed and revealed alike, and follows
    // a theme change while the document is open.
    void theMarginIsPartOfTheLayout() {
        test::Fixture f;
        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.thematicBreak.margin = 6;
        f.window->applyTheme(theme);
        QVERIFY(f.open(f.writeFile(QStringLiteral("margin.md"), "before\n***\nafter\n")));
        f.placeCaret(0);
        f.settle();
        const auto* layout = f.editor()->blockLayout();
        const auto* doc = f.editor()->document();
        const auto rect = [&](int n) { return layout->blockBoundingRect(doc->findBlockByNumber(n)); };
        const auto line = [&](int n) { return doc->findBlockByNumber(n).layout()->lineAt(0); };
        QCOMPARE(line(1).y(), 6.0);
        QCOMPARE(rect(1).height(), line(1).height() + 12);
        QCOMPARE(line(0).y(), 0.0);
        QCOMPARE(rect(0).height(), line(0).height());
        QCOMPARE(rect(2).height(), line(2).height());
        const QRectF concealed = rect(1);
        f.placeCaret(1);
        f.settle();
        QCOMPARE(rect(1), concealed);
        QCOMPARE(line(1).y(), 6.0);

        theme.thematicBreak.margin = 2;
        f.window->applyTheme(theme);
        f.settle();
        QCOMPARE(line(1).y(), 2.0);
        QCOMPARE(rect(1).height(), line(1).height() + 4);
        f.window->applyTheme(core::builtinTheme(core::ColorScheme::Light));
    }
};

QTEST_MAIN(tst_s023_thematic_breaks)
#include "tst_s023_thematic_breaks.moc"
