// SPDX-License-Identifier: GPL-3.0-or-later
// S031 / ADR 0011: the rules for pasting tables. The UI hands over what Qt's HTML import or the
// clipboard's plain text holds; everything decided here is plain text in, plain text out.
#include "hitokoto/core/paste.hpp"

#include "hitokoto/core/find.hpp"

#include <algorithm>
#include <utility>

namespace hitokoto::core {

namespace {

constexpr char16_t kNoBreakSpace = u' ';
constexpr char16_t kObjectReplacement = u'￼';  // an image without text in Qt's import

bool isLineBreak(char16_t c) noexcept {
    return c == u'\n' || c == u'\r' || c == u' ' || c == u' ';
}

bool isWhitespace(char16_t c) noexcept {
    return isUnicodeWhitespace(c);
}

bool isBlank(std::u16string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](char16_t c) { return c == u' ' || c == u'\t'; });
}

/// `lines` split further at every line break inside them (U+2028 from `<br>`, CR LF, CR, LF).
std::vector<std::u16string> splitLines(const std::vector<std::u16string>& lines) {
    std::vector<std::u16string> out;
    out.reserve(lines.size());
    for (const auto& line : lines) {
        std::u16string current;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char16_t c = line[i];
            if (isLineBreak(c)) {
                out.push_back(std::move(current));
                current.clear();
                if (c == u'\r' && i + 1 < line.size() && line[i + 1] == u'\n') {
                    ++i;
                }
            } else if (c != kObjectReplacement) {
                current += c;
            }
        }
        out.push_back(std::move(current));
    }
    return out;
}

std::u16string trimmed(std::u16string_view s) {
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && isWhitespace(s[start])) {
        ++start;
    }
    while (end > start && isWhitespace(s[end - 1])) {
        --end;
    }
    return std::u16string(s.substr(start, end - start));
}

/// Each line trimmed, empty lines at the start and end dropped (cells and text parts alike).
std::vector<std::u16string> cleanLines(const std::vector<std::u16string>& lines) {
    std::vector<std::u16string> out;
    for (const auto& line : splitLines(lines)) {
        out.push_back(trimmed(line));
    }
    const auto first = std::find_if(out.begin(), out.end(), [](const auto& l) { return !l.empty(); });
    out.erase(out.begin(), first);
    while (!out.empty() && out.back().empty()) {
        out.pop_back();
    }
    return out;
}

bool isAsciiAlphanumeric(char16_t c) noexcept {
    return (c >= u'0' && c <= u'9') || (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z');
}

/// Whether the `&` at `at` could start a character reference: `#` follows, or letters and digits
/// and then `;` (review O9: also names that are no reference).
bool referenceCouldStart(std::u16string_view s, std::size_t at) noexcept {
    if (at + 1 >= s.size()) {
        return false;
    }
    if (s[at + 1] == u'#') {
        return true;
    }
    std::size_t i = at + 1;
    while (i < s.size() && isAsciiAlphanumeric(s[i])) {
        ++i;
    }
    return i > at + 1 && i < s.size() && s[i] == u';';
}

struct Size {
    std::size_t rows = 0;
    std::size_t columns = 0;
};

/// Rows: the last row a cell starts in; columns: the last column a cell covers (a column span
/// makes columns, a row span reaching past the table does not make rows, as in HTML).
Size sizeOf(const PastedTable& table) {
    Size s;
    for (const auto& c : table.cells) {
        s.rows = std::max<std::size_t>(s.rows, std::size_t{c.row} + 1);
        s.columns = std::max<std::size_t>(s.columns, std::size_t{c.column} + std::max<std::uint32_t>(c.columnSpan, 1));
    }
    return s;
}

/// The cells on a grid: each position holds the cell that starts there, or nothing (empty or
/// covered by a span). Where cells overlap, the first in row-major order wins.
std::vector<std::vector<const PastedCell*>> gridOf(const PastedTable& table, Size size) {
    std::vector<std::vector<const PastedCell*>> grid(size.rows, std::vector<const PastedCell*>(size.columns, nullptr));
    std::vector<std::vector<bool>> taken(size.rows, std::vector<bool>(size.columns, false));
    std::vector<const PastedCell*> order;
    order.reserve(table.cells.size());
    for (const auto& c : table.cells) {
        order.push_back(&c);
    }
    std::stable_sort(order.begin(), order.end(), [](const PastedCell* a, const PastedCell* b) {
        return std::pair{a->row, a->column} < std::pair{b->row, b->column};
    });
    for (const PastedCell* c : order) {
        if (taken[c->row][c->column]) {
            continue;
        }
        grid[c->row][c->column] = c;
        const std::size_t rowEnd =
            std::min<std::size_t>(size.rows, std::size_t{c->row} + std::max<std::uint32_t>(c->rowSpan, 1));
        const std::size_t columnEnd =
            std::min<std::size_t>(size.columns, std::size_t{c->column} + std::max<std::uint32_t>(c->columnSpan, 1));
        for (std::size_t r = c->row; r < rowEnd; ++r) {
            for (std::size_t k = c->column; k < columnEnd; ++k) {
                taken[r][k] = true;
            }
        }
    }
    return grid;
}

/// A cell's Markdown: its lines cleaned, a no-break space inside a line a space, each line
/// escaped, joined with `<br>`.
std::u16string cellMarkdown(const PastedCell* cell) {
    if (cell == nullptr) {
        return {};
    }
    std::u16string out;
    bool first = true;
    for (auto line : cleanLines(cell->lines)) {
        std::replace(line.begin(), line.end(), kNoBreakSpace, u' ');
        if (!first) {
            out += u"<br>";
        }
        out += escapeTableCellText(line);
        first = false;
    }
    return out;
}

void appendRow(std::u16string& out, const std::vector<std::u16string>& cells) {
    out += u'|';
    for (const auto& c : cells) {
        out += u' ';
        out += c;
        out += u" |";
    }
}

/// The lines of a table that does not count, row by row and cell by cell (S031).
std::vector<std::u16string> tableAsLines(const PastedTable& table) {
    const Size size = sizeOf(table);
    std::vector<std::u16string> lines;
    for (const auto& row : gridOf(table, size)) {
        for (const PastedCell* c : row) {
            if (c != nullptr) {
                const auto cell = splitLines(c->lines);
                lines.insert(lines.end(), cell.begin(), cell.end());
            }
        }
    }
    return lines;
}

/// Lines of text joined by '\n' after cleaning; empty if nothing is left.
std::u16string textBlock(const std::vector<std::u16string>& lines) {
    std::u16string out;
    bool first = true;
    for (const auto& line : cleanLines(lines)) {
        if (!first) {
            out += u'\n';
        }
        out += line;
        first = false;
    }
    return out;
}

}  // namespace

HtmlTables scanHtmlTables(std::u16string_view html) noexcept {
    // `<table` or `</table` (`closing`) at `at`, the name ending in a blank, `>` or `/`.
    const auto tagAt = [html](std::size_t at, bool closing) {
        constexpr std::u16string_view kName = u"table";
        std::size_t i = at + (closing ? 2 : 1);
        if (i + kName.size() > html.size()) {
            return false;
        }
        for (const char16_t c : kName) {
            const char16_t h = html[i++];
            if (h != c && h != c - (u'a' - u'A')) {
                return false;
            }
        }
        if (i == html.size()) {
            return true;
        }
        const char16_t next = html[i];
        return next == u' ' || next == u'\t' || next == u'\n' || next == u'\r' || next == u'\f' || next == u'>' ||
               next == u'/';
    };
    bool seen = false;
    int depth = 0;
    for (std::size_t at = html.find(u'<'); at != std::u16string_view::npos; at = html.find(u'<', at + 1)) {
        if (html.substr(at, 4) == u"<!--") {
            const std::size_t end = html.find(u"-->", at + 4);
            if (end == std::u16string_view::npos) {
                break;
            }
            at = end + 2;
            continue;
        }
        if (at + 1 < html.size() && html[at + 1] == u'/') {
            if (depth > 0 && tagAt(at, true)) {
                --depth;
            }
        } else if (tagAt(at, false)) {
            seen = true;
            if (++depth > 1) {
                return HtmlTables::Nested;
            }
        }
    }
    return seen ? HtmlTables::Flat : HtmlTables::None;
}

std::u16string htmlWithoutStyles(std::u16string_view html) {
    const auto isSpace = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f'; };
    const auto isLetter = [](char16_t c) { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'); };
    const auto lower = [](char16_t c) { return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + (u'a' - u'A')) : c; };
    std::u16string out;
    out.reserve(html.size());
    std::size_t done = 0;  // html[0, done) is in `out`
    std::size_t at = html.find(u'<');
    while (at != std::u16string_view::npos) {
        if (html.substr(at, 4) == u"<!--") {
            const std::size_t end = html.find(u"-->", at + 4);
            if (end == std::u16string_view::npos) {
                break;
            }
            at = html.find(u'<', end + 3);
            continue;
        }
        if (at + 1 >= html.size() || !isLetter(html[at + 1])) {
            at = html.find(u'<', at + 1);
            continue;
        }
        // A start tag: its attributes up to the `>` outside quotes.
        std::size_t i = at + 1;
        while (i < html.size() && !isSpace(html[i]) && html[i] != u'>' && html[i] != u'/') {
            ++i;
        }
        bool closed = false;
        while (i < html.size()) {
            const std::size_t blank = i;
            while (i < html.size() && (isSpace(html[i]) || html[i] == u'/')) {
                ++i;
            }
            if (i >= html.size()) {
                break;
            }
            if (html[i] == u'>') {
                closed = true;
                ++i;
                break;
            }
            const std::size_t name = i;
            while (i < html.size() && !isSpace(html[i]) && html[i] != u'=' && html[i] != u'>' && html[i] != u'/') {
                ++i;
            }
            const bool style = i - name == 5 && lower(html[name]) == u's' && lower(html[name + 1]) == u't' &&
                               lower(html[name + 2]) == u'y' && lower(html[name + 3]) == u'l' &&
                               lower(html[name + 4]) == u'e';
            std::size_t j = i;
            while (j < html.size() && isSpace(html[j])) {
                ++j;
            }
            if (j < html.size() && html[j] == u'=') {
                ++j;
                while (j < html.size() && isSpace(html[j])) {
                    ++j;
                }
                if (j < html.size() && (html[j] == u'"' || html[j] == u'\'')) {
                    const std::size_t close = html.find(html[j], j + 1);
                    if (close == std::u16string_view::npos) {
                        i = html.size();  // an unclosed quote: the tag stays as it is
                        break;
                    }
                    j = close + 1;
                } else {
                    while (j < html.size() && !isSpace(html[j]) && html[j] != u'>') {
                        ++j;
                    }
                }
                i = j;
            }
            if (style && blank < name) {
                out.append(html.substr(done, blank - done));  // drop the blank before it and the attribute
                done = i;
            }
        }
        if (!closed) {
            break;
        }
        at = html.find(u'<', i);
    }
    out.append(html.substr(done));
    return out;
}

namespace {

/// Whether `line` starts a block quote or a list item (CommonMark 5.1, 5.2), at any indentation.
bool startsContainer(std::u16string_view line) noexcept {
    std::size_t i = 0;
    while (i < line.size() && (line[i] == u' ' || line[i] == u'\t')) {
        ++i;
    }
    if (i == line.size()) {
        return false;
    }
    const auto blankOrEnd = [line](std::size_t at) {
        return at == line.size() || line[at] == u' ' || line[at] == u'\t';
    };
    const char16_t c = line[i];
    if (c == u'>') {
        return true;
    }
    if (c == u'-' || c == u'+' || c == u'*') {
        // A thematic break (`- - -`, `* * *`) is no item.
        const std::u16string_view rest = line.substr(i);
        std::size_t marks = 0;
        const bool onlyMarks = std::all_of(rest.begin(), rest.end(), [&](char16_t r) {
            marks += r == c ? 1 : 0;
            return r == c || r == u' ' || r == u'\t';
        });
        if ((c == u'-' || c == u'*') && onlyMarks && marks >= 3) {
            return false;
        }
        return blankOrEnd(i + 1);
    }
    std::size_t digits = 0;
    while (i + digits < line.size() && line[i + digits] >= u'0' && line[i + digits] <= u'9') {
        ++digits;
    }
    return digits >= 1 && digits <= 9 && i + digits < line.size() &&
           (line[i + digits] == u'.' || line[i + digits] == u')') && blankOrEnd(i + digits + 1);
}

bool indented(std::u16string_view line) noexcept {
    return !line.empty() && (line.front() == u' ' || line.front() == u'\t');
}

}  // namespace

bool pasteLineInContainer(std::u16string_view line,
                          const std::function<std::optional<std::u16string>(std::size_t)>& above) {
    if (startsContainer(line)) {
        return true;
    }
    if (isBlank(line)) {
        return false;
    }
    // Up through the paragraph the line belongs to; an indented line also through blank lines and
    // indented lines, to the item it may be a further paragraph of.
    const bool lineIndented = indented(line);
    bool afterBlank = false;
    for (std::size_t i = 0;; ++i) {
        const std::optional<std::u16string> l = above(i);
        if (!l) {
            return false;
        }
        if (isBlank(*l)) {
            if (!lineIndented) {
                return false;
            }
            afterBlank = true;
            continue;
        }
        if (startsContainer(*l)) {
            return true;
        }
        if (afterBlank && !indented(*l)) {
            return false;
        }
    }
}

bool countsAsTable(const PastedTable& table) {
    const Size s = sizeOf(table);
    return s.rows >= 2 && s.columns >= 2;
}

std::u16string escapeTableCellText(std::u16string_view text) {
    std::u16string out;
    out.reserve(text.size() + text.size() / 8 + 4);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char16_t c = text[i];
        switch (c) {
            case u'|':
            case u'\\':
            case u'`':
            case u'*':
            case u'_':
            case u'~':
            case u'[':
            case u']':
            case u'<':
                out += u'\\';
                break;
            case u'&':
                if (referenceCouldStart(text, i)) {
                    out += u'\\';
                }
                break;
            default:
                break;
        }
        out += c;
    }
    return out;
}

std::u16string pastedTableToMarkdown(const PastedTable& table) {
    const Size size = sizeOf(table);
    if (size.rows == 0 || size.columns == 0) {
        return {};
    }
    const auto grid = gridOf(table, size);
    std::u16string out;
    std::vector<std::u16string> cells(size.columns);
    for (std::size_t r = 0; r < size.rows; ++r) {
        for (std::size_t c = 0; c < size.columns; ++c) {
            cells[c] = cellMarkdown(grid[r][c]);
        }
        if (r > 0) {
            out += u'\n';
        }
        appendRow(out, cells);
        if (r == 0) {
            out += u'\n';
            std::fill(cells.begin(), cells.end(), std::u16string(u"---"));
            appendRow(out, cells);
        }
    }
    return out;
}

std::optional<std::vector<PasteBlock>> pastedTablesToMarkdown(std::span<const PastedPart> parts) {
    bool anyTable = false;
    for (const auto& part : parts) {
        if (part.table) {
            if (part.table->containsTable) {
                return std::nullopt;  // a table in a cell: the whole paste stays plain text
            }
            anyTable = anyTable || countsAsTable(*part.table);
        }
    }
    if (!anyTable) {
        return std::nullopt;
    }
    std::vector<PasteBlock> blocks;
    std::vector<std::u16string> text;  // text since the last table that counts
    const auto flushText = [&] {
        std::u16string block = textBlock(text);
        if (!block.empty()) {
            blocks.push_back({false, std::move(block)});
        }
        text.clear();
    };
    for (const auto& part : parts) {
        if (!part.table) {
            text.insert(text.end(), part.lines.begin(), part.lines.end());
        } else if (!countsAsTable(*part.table)) {
            const auto lines = tableAsLines(*part.table);
            text.insert(text.end(), lines.begin(), lines.end());
        } else {
            flushText();
            blocks.push_back({true, pastedTableToMarkdown(*part.table)});
        }
    }
    flushText();
    return blocks;
}

std::optional<PastedTable> parseExcelPlainText(std::u16string_view text) {
    struct Row {
        std::vector<std::u16string> fields;
        bool startsWithTab = false;
    };
    std::vector<Row> rows;
    const std::size_t n = text.size();
    std::size_t i = 0;
    const auto isFieldEnd = [&](std::size_t at) {
        return at >= n || text[at] == u'\t' || text[at] == u'\n' || text[at] == u'\r';
    };
    while (i < n) {
        Row row;
        row.startsWithTab = text[i] == u'\t';
        for (;;) {
            std::u16string field;
            bool quoted = false;
            if (i < n && text[i] == u'"') {
                // A quoted field: closes with a `"` before a tab, a line break or the end, and holds
                // a line break or a tab; `""` inside is one `"`.
                std::u16string content;
                std::size_t j = i + 1;
                bool closed = false;
                while (j < n) {
                    if (text[j] == u'"') {
                        if (j + 1 < n && text[j + 1] == u'"') {
                            content += u'"';
                            j += 2;
                            continue;
                        }
                        closed = isFieldEnd(j + 1);
                        break;
                    }
                    if (text[j] == u'\r' && j + 1 < n && text[j + 1] == u'\n') {
                        ++j;  // CR LF inside the field: one line break
                    }
                    content += text[j] == u'\r' ? u'\n' : text[j];
                    ++j;
                }
                if (closed && content.find_first_of(u"\n\t") != std::u16string::npos) {
                    field = std::move(content);
                    quoted = true;
                    i = j + 1;
                }
            }
            if (!quoted) {
                const std::size_t start = i;
                while (!isFieldEnd(i)) {
                    ++i;
                }
                field = std::u16string(text.substr(start, i - start));
            }
            row.fields.push_back(std::move(field));
            if (i < n && text[i] == u'\t') {
                ++i;
                continue;
            }
            if (i < n && text[i] == u'\r') {
                ++i;
                if (i < n && text[i] == u'\n') {
                    ++i;
                }
            } else if (i < n && text[i] == u'\n') {
                ++i;
            }
            break;
        }
        rows.push_back(std::move(row));
    }

    if (rows.size() < 2) {
        return std::nullopt;
    }
    const std::size_t columns = rows.front().fields.size();
    if (columns < 2) {
        return std::nullopt;
    }
    bool allIndented = true;
    for (const auto& row : rows) {
        if (row.fields.size() != columns) {
            return std::nullopt;
        }
        allIndented = allIndented && row.startsWithTab;
    }
    if (allIndented) {
        return std::nullopt;
    }
    PastedTable table;
    table.cells.reserve(rows.size() * columns);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        for (std::size_t c = 0; c < columns; ++c) {
            PastedCell cell;
            cell.row = static_cast<std::uint32_t>(r);
            cell.column = static_cast<std::uint32_t>(c);
            std::u16string_view field = rows[r].fields[c];
            std::size_t start = 0;
            for (;;) {
                const std::size_t nl = field.find(u'\n', start);
                cell.lines.emplace_back(
                    field.substr(start, nl == std::u16string_view::npos ? std::u16string_view::npos : nl - start));
                if (nl == std::u16string_view::npos) {
                    break;
                }
                start = nl + 1;
            }
            table.cells.push_back(std::move(cell));
        }
    }
    return table;
}

PasteInsertion placePastedBlocks(std::span<const PasteBlock> blocks, const PastePlace& place) {
    PasteInsertion out;
    if (blocks.empty()) {
        return out;
    }
    if (blocks.front().table) {
        if (!place.head.empty()) {
            out.text += isBlank(place.head) ? u"\n" : u"\n\n";  // split; the line before stays
        } else if (place.lineAbove && !isBlank(*place.lineAbove)) {
            out.text += u'\n';  // the caret's empty line becomes the blank line
        }
    }
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        if (b > 0) {
            out.text += blocks[b - 1].table || blocks[b].table ? u"\n\n" : u"\n";
        }
        out.text += blocks[b].text;
    }
    out.caret = out.text.size();
    if (blocks.back().table) {
        if (!place.tail.empty()) {
            out.text += isBlank(place.tail) ? u"\n" : u"\n\n";
        } else if (place.lineBelow && !isBlank(*place.lineBelow)) {
            out.text += u'\n';
        }
    }
    return out;
}

}  // namespace hitokoto::core
