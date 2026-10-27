// SPDX-License-Identifier: GPL-3.0-or-later
// S018 / ADR 0005: a table's Markdown lines as a Qt table model. Reads cells through the core and
// writes a changed cell back into its line; the lines stay the only copy (ADR 0002).
#pragma once

#include <QAbstractTableModel>

namespace hitokoto::ui {

class TableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Role {
        /// true for a row whose line holds text the table does not show (cells beyond the header's
        /// column count); the source view shows it.
        HiddenTextRole = Qt::UserRole + 1,
    };
    using QAbstractTableModel::QAbstractTableModel;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
};

}  // namespace hitokoto::ui
