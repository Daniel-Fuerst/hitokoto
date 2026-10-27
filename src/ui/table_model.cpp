// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_model.hpp"

#include "editor_formats.hpp"
#include "qt_bridge.hpp"

#include <QColor>
#include <QFont>
#include <QTextCursor>
#include <QTextDocument>

namespace hitokoto::ui {

TableModel::TableModel(QTextDocument* document, const EditorFormats* formats, QObject* parent)
    : QAbstractTableModel(parent), document_(document), formats_(formats) {}

void TableModel::setFormats(const EditorFormats* formats) {
    formats_ = formats;
    if (!rows_.empty()) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
}

void TableModel::setLines(const QTextBlock& header, const QTextBlock& last) {
    std::vector<QTextBlock> rows;
    std::vector<core::TableAlign> aligns;
    if (header.isValid()) {
        rows.push_back(header);
        const QTextBlock delimiter = header.next();
        if (delimiter.isValid()) {
            if (auto parsed = core::tableDelimiterAligns(view(delimiter.text()))) {
                aligns = std::move(*parsed);
            }
            if (delimiter != last) {
                for (QTextBlock b = delimiter.next(); b.isValid(); b = b.next()) {
                    rows.push_back(b);
                    if (b == last) {
                        break;
                    }
                }
            }
        }
    }
    const int columns = static_cast<int>(aligns.size());
    const bool sameShape = rows.size() == rows_.size() && columns == columns_;
    if (sameShape) {
        rows_ = std::move(rows);
        aligns_ = std::move(aligns);
        last_ = last;
        if (!rows_.empty() && columns_ > 0) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        }
        return;
    }
    beginResetModel();
    rows_ = std::move(rows);
    aligns_ = std::move(aligns);
    columns_ = columns;
    last_ = last;
    endResetModel();
}

QTextBlock TableModel::lineOfRow(int row) const {
    return row >= 0 && static_cast<std::size_t>(row) < rows_.size() ? rows_[static_cast<std::size_t>(row)]
                                                                    : QTextBlock{};
}

int TableModel::rowOfBlock(int blockNumber) const {
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        if (rows_[r].blockNumber() == blockNumber) {
            return static_cast<int>(r);
        }
    }
    return !rows_.empty() && rows_.front().blockNumber() + 1 == blockNumber ? 0 : -1;  // delimiter
}

int TableModel::columnAt(int row, int positionInBlock) const {
    const QTextBlock line = lineOfRow(row);
    if (!line.isValid()) {
        return 0;
    }
    const QString text = line.text();
    const auto cells = core::tableRowCells(view(text));
    for (std::size_t c = 0; c < cells.size() && static_cast<int>(c) < columns_; ++c) {
        if (positionInBlock <= static_cast<int>(cells[c].raw.end())) {
            return static_cast<int>(c);
        }
    }
    return std::max(0, std::min(static_cast<int>(cells.size()), columns_) - 1);
}

int TableModel::positionOfCell(int row, int column) const {
    const QTextBlock line = lineOfRow(row);
    if (!line.isValid()) {
        return 0;
    }
    const QString text = line.text();
    const auto cells = core::tableRowCells(view(text));
    if (column >= 0 && static_cast<std::size_t>(column) < cells.size()) {
        return static_cast<int>(cells[static_cast<std::size_t>(column)].content.start);
    }
    return static_cast<int>(text.size());
}

bool TableModel::isRowEmpty(int row) const {
    for (int c = 0; c < columns_; ++c) {
        if (!cellMarkdown(row, c).empty()) {
            return false;
        }
    }
    return true;
}

std::u16string TableModel::cellMarkdown(int row, int column) const {
    const QTextBlock line = lineOfRow(row);
    if (!line.isValid()) {
        return {};
    }
    const QString text = line.text();
    const std::u16string_view chars = view(text);
    const auto cells = core::tableRowCells(chars);
    if (column < 0 || static_cast<std::size_t>(column) >= cells.size()) {
        return {};
    }
    const core::Span content = cells[static_cast<std::size_t>(column)].content;
    return std::u16string(chars.substr(content.start, content.length));
}

int TableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int TableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : columns_;
}

QVariant TableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount() || index.column() >= columnCount()) {
        return {};
    }
    const int row = index.row();
    const bool header = row == 0;
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return toQString(core::tableCellToText(cellMarkdown(row, index.column())));
        case MarkdownRole:
            return toQString(cellMarkdown(row, index.column()));
        case Qt::TextAlignmentRole: {
            const auto align = aligns_[static_cast<std::size_t>(index.column())];
            const Qt::Alignment horizontal = align == core::TableAlign::Center  ? Qt::AlignHCenter
                                             : align == core::TableAlign::Right ? Qt::AlignRight
                                                                                : Qt::AlignLeft;
            return QVariant::fromValue(static_cast<int>(horizontal | Qt::AlignTop));
        }
        case HiddenTextRole: {
            const QString text = lineOfRow(row).text();
            return static_cast<int>(core::tableRowCells(view(text)).size()) > columns_;
        }
        default:
            break;
    }
    if (formats_ == nullptr) {
        return {};
    }
    const core::TableStyle& style = formats_->table();
    switch (role) {
        case Qt::BackgroundRole:
            return toQColor(header ? style.headerBackground : row % 2 == 1 ? style.background : style.stripeBackground);
        case Qt::ForegroundRole:
            return toQColor(header ? style.headerText : style.text);
        case Qt::FontRole:
            return formats_->tableFont(header);
        default:
            return {};
    }
}

bool TableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= rowCount() || index.column() >= columnCount()) {
        return false;
    }
    QTextBlock line = lineOfRow(index.row());
    const QString before = line.text();
    const std::u16string markdown = core::tableCellFromText(view(value.toString()));
    const QString after =
        toQString(core::replaceTableCell(view(before), static_cast<std::size_t>(index.column()), markdown));
    if (after == before) {
        return true;  // nothing changed: the file stays as it is
    }
    QTextCursor cursor(line);
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(after);
    cursor.endEditBlock();
    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), columnCount() - 1));
    return true;
}

Qt::ItemFlags TableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

}  // namespace hitokoto::ui
