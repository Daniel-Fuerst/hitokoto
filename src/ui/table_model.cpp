// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_model.hpp"

namespace hitokoto::ui {

// Interface only (S018 revision, test first).

int TableModel::rowCount(const QModelIndex& /*parent*/) const {
    return 0;
}

int TableModel::columnCount(const QModelIndex& /*parent*/) const {
    return 0;
}

QVariant TableModel::data(const QModelIndex& /*index*/, int /*role*/) const {
    return {};
}

}  // namespace hitokoto::ui
