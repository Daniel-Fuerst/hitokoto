// SPDX-License-Identifier: GPL-3.0-or-later
// Dedicated painter for interactive Markdown table panels, stripes, grid lines, and buttons.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QRectF>

class QPainter;
class QTextDocument;

namespace hitokoto::ui {

class BlockStyler;
class TableController;
struct TableMetrics;

class TablePainter {
public:
    static void paintTableBackgrounds(QPainter& painter, const QTextDocument* doc, BlockStyler* styler,
                                      const TableController& controller, const TableMetrics& metrics);

    static void paintTableGridAndControls(QPainter& painter, const QTextDocument* doc, BlockStyler* styler,
                                          const TableController& controller, const TableMetrics& metrics);

    static void drawTableControlButton(QPainter& painter, const QRectF& rect, const core::TableStyle& style, bool plus);
};

}  // namespace hitokoto::ui
