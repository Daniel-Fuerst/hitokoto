// SPDX-License-Identifier: GPL-3.0-or-later
// S018 / ADR 0005: a table's Markdown lines as a Qt table model. Reads cells through the core and
// writes a changed cell back into its line; the lines stay the only copy (ADR 0002).
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <QAbstractTableModel>
#include <QTextBlock>

#include <vector>

class QTextDocument;

namespace hitokoto::ui {

class EditorFormats;

class TableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Role {
        /// true for a row whose line holds text the table does not show (cells beyond the header's
        /// column count); the source view shows it.
        HiddenTextRole = Qt::UserRole + 1,
        /// The cell's Markdown as written in the line.
        MarkdownRole,
    };

    TableModel(QTextDocument* document, const EditorFormats* formats, QObject* parent = nullptr);

    /// Reads the table whose header is `header` and whose last line is `last`. Keeps the view's
    /// current cell when the shape did not change.
    void setLines(const QTextBlock& header, const QTextBlock& last);
    void setFormats(const EditorFormats* formats);

    [[nodiscard]] QTextBlock headerLine() const { return rows_.empty() ? QTextBlock{} : rows_.front(); }
    [[nodiscard]] QTextBlock lastLine() const { return last_; }
    [[nodiscard]] QTextBlock lineOfRow(int row) const;
    /// The row shown for document block `blockNumber` (the delimiter line counts as the header).
    [[nodiscard]] int rowOfBlock(int blockNumber) const;
    /// The cell of `row` that holds position `positionInBlock` of its line.
    [[nodiscard]] int columnAt(int row, int positionInBlock) const;
    /// Where the content of a cell starts in its line (for the source view).
    [[nodiscard]] int positionOfCell(int row, int column) const;
    [[nodiscard]] bool isRowEmpty(int row) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    [[nodiscard]] std::u16string cellMarkdown(int row, int column) const;

    QTextDocument* document_;
    const EditorFormats* formats_;
    std::vector<QTextBlock> rows_;  ///< header and body lines; the delimiter line is not a row
    QTextBlock last_;
    std::vector<core::TableAlign> aligns_;
    int columns_ = 0;
};

}  // namespace hitokoto::ui
