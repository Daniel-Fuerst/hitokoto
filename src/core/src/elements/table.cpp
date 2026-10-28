// SPDX-License-Identifier: GPL-3.0-or-later
// S018: GitHub Flavored Markdown pipe tables (GFM 0.29 section 4.10, ADR 0005), following
// cmark-gfm where the spec leaves room. The UI shows table lines in a table view; the lines carry
// no formats of their own.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <array>

namespace hitokoto::core {
namespace {

// State while a table is open (non-zero):
//   bit 31: set after the header row (the next line must be the delimiter row)
//   bits 0..7: column count (1..kMaxColumns)
constexpr std::uint32_t kDelimiterNext = 1U << 31U;
constexpr std::uint32_t kColumnMask = 0xFFU;
constexpr std::size_t kMaxColumns = 255;

bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

/// cmark-gfm: a pipe right after a backslash does not separate cells.
bool escapedPipe(std::u16string_view line, std::size_t pos) noexcept {
    return pos > 0 && line[pos - 1] == u'\\';
}

std::size_t firstNonBlank(std::u16string_view line) noexcept {
    std::size_t i = 0;
    while (i < line.size() && isBlank(line[i])) {
        ++i;
    }
    return i;
}

/// Index after the last non-blank character.
std::size_t trimmedEnd(std::u16string_view line) noexcept {
    std::size_t end = line.size();
    while (end > 0 && isBlank(line[end - 1])) {
        --end;
    }
    return end;
}

std::size_t leadingSpaces(std::u16string_view line) noexcept {
    std::size_t width = 0;
    for (const char16_t c : line) {
        if (c == u' ') {
            ++width;
        } else if (c == u'\t') {
            width += 4 - (width % 4);
        } else {
            break;
        }
    }
    return width;
}

bool isThematicBreak(std::u16string_view line, std::size_t start) noexcept {
    const char16_t c = line[start];
    if (c != u'-' && c != u'*' && c != u'_') {
        return false;
    }
    std::size_t count = 0;
    for (std::size_t i = start; i < line.size(); ++i) {
        if (line[i] == c) {
            ++count;
        } else if (!isBlank(line[i])) {
            return false;
        }
    }
    return count >= 3;
}

bool isOrderedListMarker(std::u16string_view line, std::size_t start) noexcept {
    std::size_t i = start;
    while (i < line.size() && i - start < 9 && line[i] >= u'0' && line[i] <= u'9') {
        ++i;
    }
    if (i == start || i >= line.size() || (line[i] != u'.' && line[i] != u')')) {
        return false;
    }
    return i + 1 == line.size() || isBlank(line[i + 1]);
}

/// True if `line` starts a block that ends a table (or is blank): indented code, fence, ATX
/// heading, thematic break, list item, block quote or HTML block.
bool startsOtherBlock(std::u16string_view line) noexcept {
    const std::size_t start = firstNonBlank(line);
    if (start >= line.size()) {
        return true;  // blank
    }
    if (leadingSpaces(line) >= 4) {
        return true;
    }
    const char16_t c = line[start];
    if (c == u'|') {
        return false;
    }
    if (c == u'`' || c == u'~') {
        std::size_t run = 0;
        while (start + run < line.size() && line[start + run] == c) {
            ++run;
        }
        if (run >= 3) {
            return true;
        }
    }
    if (c == u'#') {
        std::size_t hashes = 0;
        while (start + hashes < line.size() && hashes < 7 && line[start + hashes] == u'#') {
            ++hashes;
        }
        if (hashes <= 6 && (start + hashes == line.size() || isBlank(line[start + hashes]))) {
            return true;
        }
    }
    if (isThematicBreak(line, start)) {
        return true;
    }
    if ((c == u'-' || c == u'*' || c == u'+') && (start + 1 == line.size() || isBlank(line[start + 1]))) {
        return true;
    }
    if (isOrderedListMarker(line, start)) {
        return true;
    }
    if (c == u'>') {
        return true;
    }
    if (c == u'<' && start + 1 < line.size()) {
        const char16_t n = line[start + 1];
        if ((n >= u'a' && n <= u'z') || (n >= u'A' && n <= u'Z') || n == u'/' || n == u'!' || n == u'?') {
            return true;
        }
    }
    return false;
}

/// The cell-separating pipes of a row, without allocating.
struct RowPipes {
    std::size_t first = 0;  ///< first non-blank character
    std::size_t end = 0;    ///< after the last non-blank character
    bool leadingPipe = false;
    bool trailingPipe = false;
    std::size_t inner = 0;  ///< number of pipes between cells (capped)
    std::array<std::size_t, kMaxColumns> positions{};
    std::size_t pipes = 0;  ///< all separating pipes, also beyond the cap
};

RowPipes scanRow(std::u16string_view line) noexcept {
    RowPipes r;
    r.first = firstNonBlank(line);
    r.end = std::max(trimmedEnd(line), r.first);
    for (std::size_t i = r.first; i < r.end; ++i) {
        if (line[i] != u'|' || escapedPipe(line, i)) {
            continue;
        }
        ++r.pipes;
        if (i == r.first) {
            r.leadingPipe = true;
        } else if (i + 1 == r.end) {
            r.trailingPipe = true;
        } else if (r.inner < kMaxColumns - 1) {
            r.positions[r.inner++] = i;
        }
    }
    return r;
}

std::size_t cellCount(const RowPipes& r) noexcept {
    if (r.first >= r.end || (r.leadingPipe && r.end - r.first == 1)) {
        return 0;  // blank, or a bare "|"
    }
    return r.inner + 1;
}

Span cellRawSpan(const RowPipes& r, std::size_t index) noexcept {
    const std::size_t start = index == 0 ? (r.leadingPipe ? r.first + 1 : r.first) : r.positions[index - 1] + 1;
    const std::size_t end = index < r.inner ? r.positions[index] : (r.trailingPipe ? r.end - 1 : r.end);
    return {static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(std::max(end, start) - start)};
}

Span cellContentSpan(std::u16string_view line, Span raw) noexcept {
    std::uint32_t start = raw.start;
    std::uint32_t end = raw.end();
    while (start < end && isBlank(line[start])) {
        ++start;
    }
    while (end > start && isBlank(line[end - 1])) {
        --end;
    }
    if (start < end) {
        return {start, end - start};
    }
    // Empty cell: the empty span sits after the leading space when `|  |` has at least two.
    const std::uint32_t pos = raw.length >= 2 ? raw.start + 1 : raw.start;
    return {pos, 0};
}

/// Number of columns of a delimiter row, or 0 if `line` is not one.
std::size_t delimiterColumns(std::u16string_view line, std::array<TableAlign, kMaxColumns>* aligns) noexcept {
    const RowPipes r = scanRow(line);
    if (r.pipes == 0) {
        return 0;
    }
    const std::size_t columns = cellCount(r);
    if (columns == 0 || columns > kMaxColumns) {
        return 0;
    }
    for (std::size_t c = 0; c < columns; ++c) {
        const Span content = cellContentSpan(line, cellRawSpan(r, c));
        if (content.length == 0) {
            return 0;
        }
        const std::u16string_view token = line.substr(content.start, content.length);
        const bool left = token.front() == u':';
        const bool right = token.size() > 1 && token.back() == u':';
        const std::size_t from = left ? 1 : 0;
        const std::size_t to = right ? token.size() - 1 : token.size();
        if (from >= to) {
            return 0;
        }
        for (std::size_t i = from; i < to; ++i) {
            if (token[i] != u'-') {
                return 0;
            }
        }
        if (aligns != nullptr) {
            (*aligns)[c] = left && right ? TableAlign::Center : right ? TableAlign::Right : TableAlign::Left;
        }
    }
    return columns;
}

class TableElement final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "table"; }

    [[nodiscard]] std::optional<BlockLine> parse(std::u16string_view line,
                                                 std::uint32_t state) const noexcept override {
        return parseWithNextLine(line, state, {});
    }

    [[nodiscard]] std::optional<BlockLine> parseWithNextLine(std::u16string_view line, std::uint32_t state,
                                                             std::u16string_view nextLine) const noexcept override {
        if (state == 0) {
            // A header row: pipes, not another block, and a delimiter row with as many cells below.
            if (nextLine.empty() || line.find(u'|') == std::u16string_view::npos ||
                nextLine.find(u'-') == std::u16string_view::npos || startsOtherBlock(line)) {
                return std::nullopt;
            }
            const RowPipes header = scanRow(line);
            const std::size_t columns = delimiterColumns(nextLine, nullptr);
            if (header.pipes == 0 || columns == 0 || cellCount(header) != columns) {
                return std::nullopt;
            }
            return BlockLine{static_cast<std::uint32_t>(columns) | kDelimiterNext, false};
        }
        if ((state & kDelimiterNext) != 0) {
            if (delimiterColumns(line, nullptr) != (state & kColumnMask)) {
                return std::nullopt;
            }
            return BlockLine{state & ~kDelimiterNext, true};
        }
        // A body row: anything up to a blank line or the start of another block, also without
        // pipes; a bare "|" ends the table (cmark-gfm).
        if (startsOtherBlock(line) || cellCount(scanRow(line)) == 0) {
            return std::nullopt;
        }
        return BlockLine{state, false};
    }

    void render(std::u16string_view line, const BlockLine& parsed, const LineContext& /*context*/,
                LineRender& out) const override {
        out.table = true;
        if (parsed.delimiter) {
            out.tableDelimiter = true;
            return;
        }
        out.tableHeader = (parsed.stateAfter & kDelimiterNext) != 0;
        const RowPipes r = scanRow(line);
        const std::size_t columns = std::min<std::size_t>(cellCount(r), parsed.stateAfter & kColumnMask);
        out.cells.reserve(columns);
        for (std::size_t c = 0; c < columns; ++c) {
            const Span raw = cellRawSpan(r, c);
            out.cells.push_back({cellContentSpan(line, raw), raw, TableAlign::Left});
        }
    }
};

}  // namespace

std::unique_ptr<MultiLineElement> makeTableElement() {
    return std::make_unique<TableElement>();
}

std::vector<TableCell> tableRowCells(std::u16string_view line) {
    const RowPipes r = scanRow(line);
    const std::size_t count = cellCount(r);
    std::vector<TableCell> cells;
    cells.reserve(count);
    for (std::size_t c = 0; c < count; ++c) {
        const Span raw = cellRawSpan(r, c);
        cells.push_back({cellContentSpan(line, raw), raw, TableAlign::Left});
    }
    return cells;
}

std::optional<std::vector<TableAlign>> tableDelimiterAligns(std::u16string_view line) {
    std::array<TableAlign, kMaxColumns> aligns{};
    const std::size_t columns = delimiterColumns(line, &aligns);
    if (columns == 0) {
        return std::nullopt;
    }
    return std::vector<TableAlign>(aligns.begin(), aligns.begin() + static_cast<std::ptrdiff_t>(columns));
}

std::u16string tableCellToText(std::u16string_view markdown) {
    std::u16string out;
    out.reserve(markdown.size());
    for (std::size_t i = 0; i < markdown.size(); ++i) {
        const char16_t c = markdown[i];
        if (c == u'\\' && i + 1 < markdown.size() && markdown[i + 1] == u'|') {
            out += u'|';
            ++i;
            continue;
        }
        if (c == u'<' && i + 3 < markdown.size() && (markdown[i + 1] == u'b' || markdown[i + 1] == u'B') &&
            (markdown[i + 2] == u'r' || markdown[i + 2] == u'R')) {
            // <br>, <br/>, <br />
            std::size_t j = i + 3;
            while (j < markdown.size() && markdown[j] == u' ') {
                ++j;
            }
            if (j < markdown.size() && markdown[j] == u'/') {
                ++j;
            }
            if (j < markdown.size() && markdown[j] == u'>') {
                out += u'\n';
                i = j;
                continue;
            }
        }
        out += c;
    }
    return out;
}

std::u16string tableCellFromText(std::u16string_view text) {
    std::u16string out;
    out.reserve(text.size() + 8);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char16_t c = text[i];
        if (c == u'\r') {
            if (i + 1 < text.size() && text[i + 1] == u'\n') {
                ++i;
            }
            out += u"<br>";
        } else if (c == u'\n' || c == u' ' || c == u' ') {
            out += u"<br>";
        } else if (c == u'|') {
            out += u"\\|";
        } else {
            out += c;
        }
    }
    return out;
}

std::u16string replaceTableCell(std::u16string_view line, std::size_t column, std::u16string_view markdown) {
    const std::vector<TableCell> cells = tableRowCells(line);
    if (column < cells.size()) {
        const Span content = cells[column].content;
        std::u16string out(line.substr(0, content.start));
        out += markdown;
        out += line.substr(content.end());
        return out;
    }
    std::u16string out(line.substr(0, trimmedEnd(line)));
    if (out.empty() || out.back() != u'|' || escapedPipe(out, out.size() - 1)) {
        out += out.empty() ? u"|" : u" |";
    }
    for (std::size_t c = cells.size(); c < column; ++c) {
        out += u"  |";
    }
    out += u' ';
    out += markdown;
    out += u" |";
    return out;
}

std::u16string makeEmptyTableRow(std::size_t columns) {
    const std::size_t cols = std::max<std::size_t>(1, columns);
    std::u16string row = u"|";
    row.reserve(1 + cols * 3);
    for (std::size_t i = 0; i < cols; ++i) {
        row += u"  |";
    }
    return row;
}

std::u16string appendTableColumn(std::u16string_view line, bool delimiterRow) {
    std::u16string out(line.substr(0, trimmedEnd(line)));
    if (out.empty() || out.back() != u'|') {
        out += u" |";
    }
    out += delimiterRow ? u" --- |" : u"  |";
    return out;
}

std::u16string removeTableColumn(std::u16string_view line, std::size_t column) {
    const RowPipes r = scanRow(line);
    const std::size_t cols = cellCount(r);
    if (cols <= 1 || column >= cols) {
        return std::u16string(line);
    }
    std::u16string out(line.substr(0, r.first));
    out += u'|';
    for (std::size_t c = 0; c < cols; ++c) {
        if (c == column) {
            continue;
        }
        const Span raw = cellRawSpan(r, c);
        if (raw.length == 0) {
            out += u"  |";
        } else {
            out.append(line.substr(raw.start, raw.length));
            out += u'|';
        }
    }
    return out;
}

std::u16string padTableRowCells(std::u16string_view line, std::size_t columns) {
    const std::size_t existing = cellCount(scanRow(line));
    std::u16string out(line.substr(0, trimmedEnd(line)));
    if (out.empty()) {
        return makeEmptyTableRow(columns);
    }
    if (out.back() != u'|') {
        out += u" |";
    }
    for (std::size_t c = existing; c < columns; ++c) {
        out += u"  |";
    }
    return out;
}

std::size_t separateTextAfterTables(DecodedText& decoded, const ElementRegistry& registry) {
    const std::uint8_t table = registry.blockElement("table");
    const std::u16string_view text = decoded.text;
    if (table == 0 || text.find(u'|') == std::u16string_view::npos) {
        return 0;
    }
    std::u16string out;
    std::vector<LineEnding> perBreak;
    std::size_t added = 0;
    std::size_t line = 0;  // index of the current line = breaks before it
    BlockState state;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = std::min(text.find(u'\n', start), text.size());
        const std::u16string_view current = text.substr(start, end - start);
        const std::size_t nextStart = end < text.size() ? end + 1 : text.size();
        const std::u16string_view next =
            end < text.size() ? text.substr(nextStart, std::min(text.find(u'\n', nextStart), text.size()) - nextStart)
                              : std::u16string_view{};
        BlockState after = registry.blockStateAfter(current, state, next);
        // A body row keeps the table's state unchanged; one without a pipe is text to Obsidian.
        if (state.element == table && after == state && current.find(u'|') == std::u16string_view::npos) {
            if (added == 0) {
                out.assign(text.substr(0, start));
                if (!decoded.perBreak.empty()) {
                    perBreak.assign(decoded.perBreak.begin(),
                                    decoded.perBreak.begin() + static_cast<std::ptrdiff_t>(line));
                }
            }
            out.push_back(u'\n');
            if (!decoded.perBreak.empty()) {
                perBreak.push_back(decoded.uniformEnding);
            }
            ++added;
            after = registry.blockStateAfter(current, registry.blockStateAfter({}, state, current), next);
        }
        if (added > 0) {
            out.append(text.substr(start, nextStart - start));
            if (end < text.size() && line < decoded.perBreak.size()) {
                perBreak.push_back(decoded.perBreak[line]);
            }
        }
        if (end >= text.size()) {
            break;
        }
        state = after;
        start = nextStart;
        ++line;
    }
    if (added > 0) {
        decoded.text = std::move(out);
        if (!decoded.perBreak.empty()) {
            decoded.perBreak = std::move(perBreak);
        }
    }
    return added;
}

}  // namespace hitokoto::core
