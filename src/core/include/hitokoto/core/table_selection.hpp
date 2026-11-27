// SPDX-License-Identifier: GPL-3.0-or-later
// S040: selecting cells, rows and columns of a table, and what deleting a selection does to the
// table's lines. The rules need no Qt; the table view holds the selection in its selection model
// (for screen readers) and asks these functions how it grows and what the lines become.
#pragma once

#include <string>
#include <vector>

namespace hitokoto::core {

/// A cell as the table view counts it: row 0 is the header row, column 0 the first column.
struct TableCellPos {
    int row = 0;
    int column = 0;
    friend bool operator==(const TableCellPos&, const TableCellPos&) = default;
};

/// A rectangle of cells, both corners included.
struct TableCellRange {
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
    friend bool operator==(const TableCellRange&, const TableCellRange&) = default;
    /// The rectangle spanned by two cells, in any order.
    [[nodiscard]] static TableCellRange between(TableCellPos a, TableCellPos b) noexcept;
    [[nodiscard]] bool contains(int row, int column) const noexcept;
};

/// A step of a key with Shift (decision 8): one cell, to the first or last column (Home, End), to
/// the first or last cell of the table (Ctrl+Home, Ctrl+End).
enum class TableStep { Left, Right, Up, Down, RowStart, RowEnd, TableStart, TableEnd };

/// Decision 1: a rectangle of cells that grows and shrinks from the cell where it started (the
/// anchor), as a text selection does; no cell is the active one. Whole rows and whole columns
/// stay whole while they grow (decision 4).
class TableCellSelection {
public:
    enum class Kind { Cells, Rows, Columns };

    /// One cell, in a table of `rows` rows (the header's included) and `columns` columns.
    TableCellSelection(int rows, int columns, TableCellPos anchor) noexcept;
    /// The whole table (decision 3).
    [[nodiscard]] static TableCellSelection wholeTable(int rows, int columns) noexcept;

    [[nodiscard]] TableCellRange range() const noexcept;
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] TableCellPos anchor() const noexcept { return anchor_; }
    [[nodiscard]] TableCellPos focus() const noexcept { return focus_; }
    /// Whether every cell of the rows it covers is selected (also a single whole row).
    [[nodiscard]] bool coversWholeRows() const noexcept;
    /// Whether every cell of the columns it covers is selected, the header's included.
    [[nodiscard]] bool coversWholeColumns() const noexcept;
    [[nodiscard]] bool coversWholeTable() const noexcept;

    /// Moves the end that is not the anchor by `step`, within the table. Rows grow by Up and Down
    /// only, columns by Left and Right only; the other steps leave them as they are.
    void extend(TableStep step) noexcept;
    /// Extends to `cell` (dragging, Shift with a click): the rectangle from the anchor to it.
    void extendTo(TableCellPos cell) noexcept;
    /// Shift+Space: the whole rows the selection lies in.
    void selectRows() noexcept;
    /// Ctrl+Space: the whole columns the selection lies in, the header cells included.
    void selectColumns() noexcept;

private:
    int rows_;
    int columns_;
    TableCellPos anchor_;
    TableCellPos focus_;
    Kind kind_ = Kind::Cells;
};

/// Decision 8: whether a step with Shift selects the cell as a cell instead of moving in its text.
/// `position` is the caret (the moving end of a text selection) in a text of `length`
/// characters; `onFirstLine` and `onLastLine` say whether it is on the first or last line of the
/// cell's wrapped text. Right and Down select the cell at the end of the text (Down on its last
/// line), Left and Up at its start (Up on its first line); the step before moves there as today.
[[nodiscard]] bool shiftStepSelectsCell(TableStep step, int position, int length, bool onFirstLine,
                                        bool onLastLine) noexcept;

/// Decision 10: the lines of a table (header, delimiter, body rows, in that order) with the cells
/// of `range` emptied; rows and columns stay, every other byte stays.
[[nodiscard]] std::vector<std::u16string> emptyTableCells(const std::vector<std::u16string>& lines,
                                                          TableCellRange range);
/// Decision 10: the lines without body rows `first` to `last` (counted as the table view counts
/// rows, so 1 is the first body row). The header row stays, also when `first` is 0.
[[nodiscard]] std::vector<std::u16string> removeTableRows(const std::vector<std::u16string>& lines, int first,
                                                          int last);
/// Decision 10: the lines without columns `first` to `last`. A table keeps one column: when every
/// column is named, all but the first are removed.
[[nodiscard]] std::vector<std::u16string> removeTableColumns(const std::vector<std::u16string>& lines, int first,
                                                             int last);

}  // namespace hitokoto::core
