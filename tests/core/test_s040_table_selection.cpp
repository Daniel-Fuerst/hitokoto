// SPDX-License-Identifier: GPL-3.0-or-later
// S040: the rules of selecting cells, rows and columns that need no Qt, and the lines that
// deleting a selection leaves.
#include "hitokoto/core/table_selection.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {

// The table of the story's scenarios: a header row and three body rows of four columns.
constexpr int kRows = 4;
constexpr int kColumns = 4;

TableCellSelection at(int row, int column) {
    return {kRows, kColumns, {row, column}};
}

}  // namespace

TEST(S040_TableSelection, ARectangleGrowsFromWhereItStarted) {
    // Scenario: a rectangle of cells, from row 1, column 1 to row 3, column 3.
    TableCellSelection s = at(1, 1);
    EXPECT_EQ(s.range(), (TableCellRange{1, 1, 1, 1}));
    s.extend(TableStep::Right);
    s.extend(TableStep::Right);
    s.extend(TableStep::Down);
    s.extend(TableStep::Down);
    EXPECT_EQ(s.range(), (TableCellRange{1, 1, 3, 3}));
    EXPECT_EQ(s.anchor(), (TableCellPos{1, 1}));
    // It shrinks towards the anchor and grows past it to the other side.
    s.extend(TableStep::Left);
    s.extend(TableStep::Left);
    s.extend(TableStep::Left);
    EXPECT_EQ(s.range(), (TableCellRange{1, 0, 3, 1}));
    EXPECT_TRUE(s.range().contains(2, 0));
    EXPECT_FALSE(s.range().contains(0, 0));
    EXPECT_FALSE(s.range().contains(2, 2));
}

TEST(S040_TableSelection, StepsStayInTheTable) {
    TableCellSelection s = at(0, 0);
    s.extend(TableStep::Up);
    s.extend(TableStep::Left);
    EXPECT_EQ(s.range(), (TableCellRange{0, 0, 0, 0}));
    s.extend(TableStep::TableEnd);
    EXPECT_EQ(s.range(), (TableCellRange{0, 0, 3, 3}));
    EXPECT_TRUE(s.coversWholeTable());
    s.extend(TableStep::Down);
    s.extend(TableStep::Right);
    EXPECT_EQ(s.range(), (TableCellRange{0, 0, 3, 3}));
}

TEST(S040_TableSelection, HomeAndEndGoToTheFirstOrLastColumn) {
    TableCellSelection s = at(2, 1);
    s.extend(TableStep::RowEnd);
    EXPECT_EQ(s.range(), (TableCellRange{2, 1, 2, 3}));
    s.extend(TableStep::RowStart);
    EXPECT_EQ(s.range(), (TableCellRange{2, 0, 2, 1}));
    s.extend(TableStep::TableStart);
    EXPECT_EQ(s.range(), (TableCellRange{0, 0, 2, 1}));
}

TEST(S040_TableSelection, ShiftSpaceSelectsTheRowsOfTheSelection) {
    TableCellSelection s = at(1, 2);
    s.selectRows();
    EXPECT_EQ(s.range(), (TableCellRange{1, 0, 1, 3}));
    EXPECT_TRUE(s.coversWholeRows());
    EXPECT_FALSE(s.coversWholeColumns());
    // Rows grow by Up and Down; Left and Right keep them whole.
    s.extend(TableStep::Down);
    EXPECT_EQ(s.range(), (TableCellRange{1, 0, 2, 3}));
    s.extend(TableStep::Left);
    s.extend(TableStep::RowStart);
    EXPECT_EQ(s.range(), (TableCellRange{1, 0, 2, 3}));
    EXPECT_EQ(s.kind(), TableCellSelection::Kind::Rows);
    // A rectangle: all rows it lies in.
    TableCellSelection r = at(1, 1);
    r.extend(TableStep::Down);
    r.extend(TableStep::Right);
    r.selectRows();
    EXPECT_EQ(r.range(), (TableCellRange{1, 0, 2, 3}));
}

TEST(S040_TableSelection, CtrlSpaceSelectsTheColumnsOfTheSelectionWithTheHeader) {
    TableCellSelection s = at(2, 1);
    s.selectColumns();
    EXPECT_EQ(s.range(), (TableCellRange{0, 1, 3, 1}));
    EXPECT_TRUE(s.coversWholeColumns());
    EXPECT_FALSE(s.coversWholeRows());
    s.extend(TableStep::Right);
    EXPECT_EQ(s.range(), (TableCellRange{0, 1, 3, 2}));
    s.extend(TableStep::Up);
    s.extend(TableStep::TableEnd);
    EXPECT_EQ(s.range(), (TableCellRange{0, 1, 3, 2}));
    EXPECT_EQ(s.kind(), TableCellSelection::Kind::Columns);
}

TEST(S040_TableSelection, WholeRowsAndColumnsOfANarrowTable) {
    // A table of one column: a cell is a whole row, a whole column only with every row.
    TableCellSelection s(3, 1, {1, 0});
    EXPECT_TRUE(s.coversWholeRows());
    EXPECT_FALSE(s.coversWholeColumns());
    const TableCellSelection all = TableCellSelection::wholeTable(3, 1);
    EXPECT_TRUE(all.coversWholeRows());
    EXPECT_TRUE(all.coversWholeColumns());
    EXPECT_TRUE(all.coversWholeTable());
    EXPECT_EQ(all.range(), (TableCellRange{0, 0, 2, 0}));
}

TEST(S040_TableSelection, DraggingExtendsToTheCellUnderThePointer) {
    TableCellSelection s = at(3, 3);
    s.extendTo({1, 1});
    EXPECT_EQ(s.range(), (TableCellRange{1, 1, 3, 3}));
    s.extendTo({9, -2});
    EXPECT_EQ(s.range(), (TableCellRange{3, 0, 3, 3}));
    EXPECT_EQ(TableCellRange::between({2, 3}, {0, 1}), (TableCellRange{0, 1, 2, 3}));
}

TEST(S040_TableSelection, AShiftStepSelectsTheCellAtTheEdgeOfItsText) {
    // Decision 8: Right at the end of the text, Left at its start; Down on the last line at the end,
    // Up on the first line at the start; elsewhere the step moves in the text.
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::Right, 3, 5, true, true));
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Right, 5, 5, true, true));
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::Left, 1, 5, true, true));
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Left, 0, 5, true, true));
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::Down, 3, 9, false, true));  // on the last line: to the end first
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::Down, 9, 9, false, false));
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Down, 9, 9, false, true));
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::Up, 2, 9, true, false));
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Up, 0, 9, true, false));
    // An empty cell: the first step selects it.
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Right, 0, 0, true, true));
    EXPECT_TRUE(shiftStepSelectsCell(TableStep::Up, 0, 0, true, true));
    // Home and End act on the text while text is selected.
    EXPECT_FALSE(shiftStepSelectsCell(TableStep::RowEnd, 5, 5, true, true));
}

namespace {

const std::vector<std::u16string> kLines = {
    u"| A | B | C |", u"| --- | :-: | --- |", u"| a1 | b1 | c1 |", u"|a2|b2|c2|", u"| a3 | b\\|3 | c3 |",
};

}  // namespace

TEST(S040_TableSelection, DeletingEmptiesTheSelectedCellsOnly) {
    const auto lines = emptyTableCells(kLines, {2, 1, 3, 2});
    const std::vector<std::u16string> expected = {
        u"| A | B | C |", u"| --- | :-: | --- |", u"| a1 | b1 | c1 |", u"|a2|||", u"| a3 |  |  |",
    };
    EXPECT_EQ(lines, expected);
    // The header's cells too; the delimiter row is never touched.
    const auto header = emptyTableCells(kLines, {0, 0, 0, 0});
    EXPECT_EQ(header[0], u"|  | B | C |");
    EXPECT_EQ(header[1], kLines[1]);
}

TEST(S040_TableSelection, DeletingRowsKeepsTheHeader) {
    const auto lines = removeTableRows(kLines, 1, 2);
    const std::vector<std::u16string> expected = {u"| A | B | C |", u"| --- | :-: | --- |", u"| a3 | b\\|3 | c3 |"};
    EXPECT_EQ(lines, expected);
    // With the header row in the selection (the whole table): every body row goes.
    const auto all = removeTableRows(kLines, 0, 3);
    EXPECT_EQ(all, (std::vector<std::u16string>{kLines[0], kLines[1]}));
    // Only the header row: nothing changes.
    EXPECT_EQ(removeTableRows(kLines, 0, 0), kLines);
}

TEST(S040_TableSelection, DeletingColumnsKeepsOneColumn) {
    const auto lines = removeTableColumns(kLines, 1, 1);
    const std::vector<std::u16string> expected = {u"| A | C |", u"| --- | --- |", u"| a1 | c1 |", u"|a2|c2|",
                                                  u"| a3 | c3 |"};
    EXPECT_EQ(lines, expected);
    const auto all = removeTableColumns(kLines, 0, 2);
    const std::vector<std::u16string> first = {u"| A |", u"| --- |", u"| a1 |", u"|a2|", u"| a3 |"};
    EXPECT_EQ(all, first);
}

// --- Theming baseline ---------------------------------------------------------------------------

TEST(S040_TableSelection, SelectionTokensFallBackAsDocumented) {
    for (const auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& theme = builtinTheme(scheme);
        const TableStyle s = tableStyle(theme);
        EXPECT_EQ(s.selectionBackground, theme.selection);
        EXPECT_EQ(s.selectionText, theme.selectionText);
        Color current = theme.selection;
        current.a = 60;  // as before S040
        EXPECT_EQ(s.currentBackground, current);
        EXPECT_EQ(s.selectPointerFill, (Color{0, 0, 0, 255}));
        EXPECT_EQ(s.selectPointerOutline, (Color{255, 255, 255, 255}));
    }
}

TEST(S040_TableSelection, SelectionTokensAreParsed) {
    const auto parsed = parseTheme(
        "[table]\nselection-background = \"#010101\"\nselection-text = \"#020202\"\n"
        "current-background = \"#03030380\"\nselect-pointer-fill = \"#040404\"\nselect-pointer-outline = \"#050505\"\n",
        builtinTheme(ColorScheme::Light));
    if (!parsed.theme) {  // explicit for bugprone-unchecked-optional-access
        FAIL() << "theme not parsed";
    }
    const TableStyle s = tableStyle(*parsed.theme);
    EXPECT_EQ(s.selectionBackground, (Color{1, 1, 1, 255}));
    EXPECT_EQ(s.selectionText, (Color{2, 2, 2, 255}));
    EXPECT_EQ(s.currentBackground, (Color{3, 3, 3, 0x80}));
    EXPECT_EQ(s.selectPointerFill, (Color{4, 4, 4, 255}));
    EXPECT_EQ(s.selectPointerOutline, (Color{5, 5, 5, 255}));
}
