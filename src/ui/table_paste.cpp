// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_paste.hpp"

#include "editor_widget.hpp"
#include "qt_bridge.hpp"

#include <QGuiApplication>
#include <QMimeData>
#include <QScopeGuard>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace hitokoto::ui {

namespace {

std::uint64_t imports = 0;

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
    ++imports;
    NoResourceDocument document;
    document.setHtml(html);
    std::vector<core::PastedPart> parts;
    readFrame(document.rootFrame(), parts);
    return parts;
}

std::uint64_t pastedHtmlImports() noexcept {
    return imports;
}

namespace {

/// The cells of an HTML paste with exactly one table of two cells or more and no text beside it.
PastedCells htmlTableCells(const QString& pasted) {
    const std::u16string html = core::htmlWithoutFormatting(view(pasted));
    if (core::scanHtmlTables(html) != core::HtmlTables::Flat) {
        return {};
    }
    // The wait cursor while Qt imports the HTML (S031, ADR 0011).
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const auto restore = qScopeGuard([] { QGuiApplication::restoreOverrideCursor(); });
    const core::PastedTable* only = nullptr;
    bool text = false;
    const std::vector<core::PastedPart> parts = readPastedHtml(toQString(html));
    for (const core::PastedPart& part : parts) {
        if (part.table) {
            if (only != nullptr || part.table->containsTable || !core::countsAsCellTable(*part.table)) {
                return {};
            }
            only = &*part.table;
        } else if (std::any_of(part.lines.begin(), part.lines.end(), [](const std::u16string& line) {
                       return line.find_first_not_of(u" \t\u00A0") != std::u16string::npos;
                   })) {
            text = true;
        }
    }
    if (only == nullptr) {
        return {};
    }
    if (text) {
        return {std::nullopt, true};
    }
    return {core::pastedTableCells(*only), false};
}

}  // namespace

PastedCells pastedCells(const QMimeData* source, const core::ElementRegistry& registry) {
    if (source == nullptr) {
        return {};
    }
    const QString markdown = QStringLiteral("text/markdown");
    if (source->hasFormat(EditorWidget::ownCopyFormat())) {
        if (!source->hasFormat(markdown)) {
            return {};
        }
        const QString text = QString::fromUtf8(source->data(markdown));
        return {core::markdownTableCells(registry, view(text)), false};
    }
    if (source->hasHtml()) {
        return htmlTableCells(source->html());
    }
    if (source->hasText()) {
        if (const std::optional<core::PastedTable> table = core::parseTabSeparatedCells(view(source->text()))) {
            return {core::pastedTableCells(*table), false};
        }
    }
    return {};
}

}  // namespace hitokoto::ui
