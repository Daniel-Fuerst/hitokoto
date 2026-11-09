// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_paste.hpp"

#include "qt_bridge.hpp"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace hitokoto::ui {

namespace {

/// Qt's HTML import, with every resource refused: a stylesheet <link> or an image is never read
/// from disk or anywhere else (ADR 0011: nothing is loaded or followed).
class NoResourceDocument : public QTextDocument {
protected:
    QVariant loadResource(int /*type*/, const QUrl& /*name*/) override { return {}; }
};

/// Appends the text of every block in `frame` to `lines`; tables inside it set `nested`.
void appendFrameText(QTextFrame* frame, std::vector<std::u16string>& lines, bool& nested) {
    for (auto it = frame->begin(); !it.atEnd(); ++it) {
        if (QTextFrame* child = it.currentFrame()) {
            if (qobject_cast<QTextTable*>(child) != nullptr) {
                nested = true;
            }
            appendFrameText(child, lines, nested);
        } else {
            lines.emplace_back(view(it.currentBlock().text()));
        }
    }
}

core::PastedTable readTable(QTextTable* table) {
    core::PastedTable out;
    for (int r = 0; r < table->rows(); ++r) {
        for (int c = 0; c < table->columns(); ++c) {
            const QTextTableCell cell = table->cellAt(r, c);
            if (!cell.isValid() || cell.row() != r || cell.column() != c) {
                continue;  // covered by a span
            }
            core::PastedCell pasted;
            pasted.row = static_cast<std::uint32_t>(r);
            pasted.column = static_cast<std::uint32_t>(c);
            pasted.rowSpan = static_cast<std::uint32_t>(std::max(1, cell.rowSpan()));
            pasted.columnSpan = static_cast<std::uint32_t>(std::max(1, cell.columnSpan()));
            for (auto it = cell.begin(); !it.atEnd(); ++it) {
                if (QTextFrame* child = it.currentFrame()) {
                    if (qobject_cast<QTextTable*>(child) != nullptr) {
                        out.containsTable = true;
                    }
                    appendFrameText(child, pasted.lines, out.containsTable);
                } else {
                    pasted.lines.emplace_back(view(it.currentBlock().text()));
                }
            }
            out.cells.push_back(std::move(pasted));
        }
    }
    return out;
}

void readFrame(QTextFrame* frame, std::vector<core::PastedPart>& parts) {
    for (auto it = frame->begin(); !it.atEnd(); ++it) {
        if (QTextFrame* child = it.currentFrame()) {
            if (auto* table = qobject_cast<QTextTable*>(child)) {
                core::PastedPart part;
                part.table = readTable(table);
                parts.push_back(std::move(part));
            } else {
                readFrame(child, parts);
            }
            continue;
        }
        if (parts.empty() || parts.back().table) {
            parts.emplace_back();
        }
        parts.back().lines.emplace_back(view(it.currentBlock().text()));
    }
}

}  // namespace

std::vector<core::PastedPart> readPastedHtml(const QString& html) {
    NoResourceDocument document;
    document.setHtml(html);
    std::vector<core::PastedPart> parts;
    readFrame(document.rootFrame(), parts);
    return parts;
}

}  // namespace hitokoto::ui
