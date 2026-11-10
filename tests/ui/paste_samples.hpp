// SPDX-License-Identifier: GPL-3.0-or-later
// S031: the clipboard samples of tests/data/s031/samples (as resources under :/s031/) and large
// tables generated from their markup, shared by tst_s031_paste_tables and hitokoto-bench.
#pragma once

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QMimeData>
#include <QString>
#include <QStringList>

namespace hitokoto::test {

/// A sample's file as UTF-8 text; empty if it is missing.
inline QString s031Sample(const QString& file) {
    QFile f(QStringLiteral(":/s031/") + file);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

/// The HTML of sample `name` as Qt hands it to the editor on Windows.
inline QString s031Html(const QString& name) {
    return s031Sample(name + QStringLiteral(".html"));
}

/// The plain text of sample `name` (LF line breaks).
inline QString s031Text(const QString& name) {
    return s031Sample(name + QStringLiteral(".txt"));
}

/// The lines of the generated cell at (row, column), like the samples' cells (S031 review O10): in
/// turn one word, two lines, a word with `&`, and two lines with `:` and `_`.
inline QStringList s031CellLines(int row, int column, int columns) {
    const QString name = QStringLiteral("r%1c%2").arg(row).arg(column);
    switch ((row * columns + column) % 4) {
        case 1:
            return {name, QStringLiteral("second line")};
        case 2:
            return {name + QStringLiteral(" R&D")};
        case 3:
            return {name + QStringLiteral(" 09:30"), QStringLiteral("a_b")};
        default:
            return {name};
    }
}

/// A table of `rows` x `columns` cells in the markup of `html`, a sample whose first cell reads
/// "Product" (outlook-t1-example, excel-t6-example): its head, the opening tag of its first row and
/// the markup of its first cell, repeated; each cell holds s031CellLines(row, column), its lines
/// as the sample separates them (Outlook: one paragraph each; Excel: `<br>` as Excel writes it).
inline QString s031GeneratedHtml(const QString& html, int rows, int columns) {
    const qsizetype firstRow = html.indexOf(QStringLiteral("<tr"));
    const qsizetype rowTagEnd = html.indexOf(u'>', firstRow) + 1;
    const qsizetype cellStart = html.indexOf(QStringLiteral("<td"), rowTagEnd);
    const qsizetype cellEnd = html.indexOf(QStringLiteral("</td>"), cellStart) + 5;
    const qsizetype lastRowEnd = html.lastIndexOf(QStringLiteral("</tr>")) + 5;
    if (firstRow < 0 || cellStart < 0 || cellEnd < 5 || lastRowEnd < 5) {
        return {};
    }
    const QString head = html.left(firstRow);
    const QString rowTag = html.mid(firstRow, rowTagEnd - firstRow);
    const QString cell = html.mid(cellStart, cellEnd - cellStart);
    const qsizetype text = cell.indexOf(QStringLiteral("Product"));
    if (text < 0) {
        return {};
    }
    const QString beforeText = cell.left(text);
    const QString afterText = cell.mid(text + 7);
    const QString tail = html.mid(lastRowEnd);
    const qsizetype paragraph = beforeText.indexOf(QStringLiteral("<p"));
    const QString lineBreak =
        paragraph >= 0 ? afterText.left(afterText.indexOf(QStringLiteral("</td>"))) + beforeText.mid(paragraph)
                       : QStringLiteral("<br style='mso-data-placement:same-cell'>");

    QString out = head;
    out.reserve(head.size() + tail.size() + rows * (rowTag.size() + 8 + columns * (2 * cell.size() + 16)));
    for (int r = 0; r < rows; ++r) {
        out += rowTag;
        for (int c = 0; c < columns; ++c) {
            QStringList lines = s031CellLines(r, c, columns);
            for (QString& line : lines) {
                line = line.toHtmlEscaped();
            }
            out += beforeText + lines.join(lineBreak) + afterText;
        }
        out += QStringLiteral("</tr>\n");
    }
    out += tail;
    return out;
}

/// The plain text that goes with a generated table, as Excel writes it: cells separated by tabs,
/// rows by line breaks, a cell with a line break in quotes.
inline QString s031GeneratedText(int rows, int columns) {
    QString out;
    for (int r = 0; r < rows; ++r) {
        QStringList cells;
        for (int c = 0; c < columns; ++c) {
            const QStringList lines = s031CellLines(r, c, columns);
            cells << (lines.size() > 1 ? u'"' + lines.join(u'\n') + u'"' : lines.front());
        }
        out += cells.join(u'\t') + u'\n';
    }
    return out;
}

/// The Markdown S031 makes of a generated table (checked with cmark-gfm: `R&D` stays as it is,
/// `_` is escaped, the lines of a cell are joined with `<br>`).
inline QString s031GeneratedMarkdown(int rows, int columns) {
    QString out;
    for (int r = 0; r < rows; ++r) {
        QStringList cells;
        for (int c = 0; c < columns; ++c) {
            cells << s031CellLines(r, c, columns).join(QStringLiteral("<br>")).replace(u'_', QStringLiteral("\\_"));
        }
        out += QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
        if (r == 0) {
            QStringList delimiter;
            for (int c = 0; c < columns; ++c) {
                delimiter << QStringLiteral("---");
            }
            out += QStringLiteral("\n| ") + delimiter.join(QStringLiteral(" | ")) + QStringLiteral(" |");
        }
        if (r + 1 < rows) {
            out += u'\n';
        }
    }
    return out;
}

/// Puts `html` (if not empty) and `text` (if not empty) on the clipboard.
inline void putOnClipboard(const QString& html, const QString& text) {
    auto* data = new QMimeData;
    if (!html.isEmpty()) {
        data->setHtml(html);
    }
    if (!text.isEmpty()) {
        data->setText(text);
    }
    QApplication::clipboard()->setMimeData(data);
}

/// Puts sample `name` on the clipboard, HTML and plain text.
inline void putSampleOnClipboard(const QString& name) {
    putOnClipboard(s031Html(name), s031Text(name));
}

}  // namespace hitokoto::test
