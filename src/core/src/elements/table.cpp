// SPDX-License-Identifier: GPL-3.0-or-later
// S018: GitHub Flavored Markdown pipe tables (GFM 4.10, ADR 0005).
// Pipe delimiters, escaped-pipe backslashes and the delimiter row are always concealed.
#include "hitokoto/core/markdown.hpp"

#include <algorithm>
#include <array>

namespace hitokoto::core {
namespace {

// State layout (non-zero while a table is open):
//   bit 31: 1 after the header row (the next line must be the delimiter row), 0 on delimiter/body rows
//   bits 24..30: column count (1..64)
//   bits 0..23: TableAlign (2 bits per column) for the first 12 columns
constexpr std::uint32_t kDelimiterNext = 1U << 31U;
constexpr std::uint32_t kColumnShift = 24U;
constexpr std::uint32_t kColumnMask = 0x7FU;
constexpr std::size_t kMaxColumns = 64;
constexpr std::size_t kMaxPackedAlignColumns = 12;

bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

bool escaped(std::u16string_view line, std::size_t pos) noexcept {
    std::size_t slashes = 0;
    while (pos > slashes && line[pos - slashes - 1] == u'\\') {
        ++slashes;
    }
    return (slashes % 2) != 0;
}

bool isSingleBacktick(std::u16string_view line, std::size_t i) noexcept {
    return line[i] == u'`' && (i == 0 || line[i - 1] != u'`') && (i + 1 == line.size() || line[i + 1] != u'`');
}

/// Finds the closing single backtick on `line` after `opener`, or npos if unmatched.
std::size_t findClosingBacktick(std::u16string_view line, std::size_t opener) noexcept {
    for (std::size_t i = opener + 1; i < line.size(); ++i) {
        if (isSingleBacktick(line, i)) {
            return i;
        }
    }
    return std::u16string_view::npos;
}

/// True if `line` starts with a fenced-code opener, ATX heading or bullet list marker.
bool startsOtherBlockElement(std::u16string_view line, std::size_t firstNonBlank) noexcept {
    if (firstNonBlank >= line.size()) {
        return true;
    }
    const char16_t c = line[firstNonBlank];
    if (c == u'|') {
        return false;
    }
    if (c == u'`' || c == u'~') {
        std::size_t run = 0;
        while (firstNonBlank + run < line.size() && line[firstNonBlank + run] == c) {
            ++run;
        }
        if (run >= 3) {
            return true;
        }
    }
    if (c == u'#') {
        std::size_t hashes = 0;
        while (firstNonBlank + hashes < line.size() && hashes < 7 && line[firstNonBlank + hashes] == u'#') {
            ++hashes;
        }
        if (hashes >= 1 && hashes <= 6 &&
            (firstNonBlank + hashes == line.size() || isBlank(line[firstNonBlank + hashes]))) {
            return true;
        }
    }
    if ((c == u'-' || c == u'*' || c == u'+') && firstNonBlank + 1 < line.size() && isBlank(line[firstNonBlank + 1])) {
        return true;
    }
    if (c == u'>' && (firstNonBlank + 1 == line.size() || isBlank(line[firstNonBlank + 1]))) {
        return true;
    }
    return false;
}

struct RowPipes {
    std::size_t firstNonBlank = 0;
    std::size_t lastNonBlank = 0;  ///< inclusive index of last non-blank character
    bool leadingPipe = false;
    bool trailingPipe = false;
    std::size_t count = 0;  ///< number of internal cell-boundary pipes
    std::array<std::size_t, kMaxColumns> positions{};
    bool valid = false;
};

/// Scans `line` for unescaped pipes outside inline-code spans without heap allocation.
RowPipes scanRowPipes(std::u16string_view line) noexcept {
    RowPipes result;
    if (line.find(u'|') == std::u16string_view::npos) {
        return result;
    }
    std::size_t indent = 0;
    while (indent < line.size() && indent < 4 && line[indent] == u' ') {
        ++indent;
    }
    if (indent > 3 || indent == line.size() || line[indent] == u'\t') {
        return result;
    }
    std::size_t first = indent;
    while (first < line.size() && isBlank(line[first])) {
        ++first;
    }
    if (first == line.size() || startsOtherBlockElement(line, first)) {
        return result;
    }
    std::size_t last = line.size() - 1;
    while (last > first && isBlank(line[last])) {
        --last;
    }
    result.firstNonBlank = first;
    result.lastNonBlank = last;

    std::size_t totalPipes = 0;
    for (std::size_t i = first; i <= last; ++i) {
        if (isSingleBacktick(line, i) && !escaped(line, i)) {
            if (const std::size_t closer = findClosingBacktick(line, i);
                closer != std::u16string_view::npos && closer <= last) {
                i = closer;
                continue;
            }
        }
        if (line[i] == u'|' && !escaped(line, i)) {
            ++totalPipes;
            if (i == first) {
                result.leadingPipe = true;
            } else if (i == last) {
                result.trailingPipe = true;
            } else if (result.count < kMaxColumns - 1) {
                result.positions[result.count++] = i;
            } else {
                return result;
            }
        }
    }
    if (totalPipes == 0) {
        return result;
    }
    result.valid = true;
    return result;
}

std::size_t cellCount(const RowPipes& pipes) noexcept {
    return pipes.valid ? pipes.count + 1 : 0;
}

Span cellRawSpan(const RowPipes& pipes, std::size_t index) noexcept {
    const std::size_t rawStart = index == 0 ? (pipes.leadingPipe ? pipes.firstNonBlank + 1 : pipes.firstNonBlank)
                                            : pipes.positions[index - 1] + 1;
    const std::size_t rawEnd = index < pipes.count ? pipes.positions[index]
                                                   : (pipes.trailingPipe ? pipes.lastNonBlank : pipes.lastNonBlank + 1);
    return {static_cast<std::uint32_t>(rawStart), static_cast<std::uint32_t>(rawEnd - rawStart)};
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
    // Empty cell: place the 0-length span after the leading space when `|  |` has at least 2 spaces.
    const std::uint32_t pos = raw.length >= 2 ? raw.start + 1 : raw.start;
    return {pos, 0};
}

struct DelimiterInfo {
    std::size_t columns = 0;
    std::array<TableAlign, kMaxColumns> aligns{};
    bool valid = false;
};

DelimiterInfo parseDelimiterRow(std::u16string_view line) noexcept {
    DelimiterInfo info;
    const RowPipes pipes = scanRowPipes(line);
    if (!pipes.valid) {
        return info;
    }
    const std::size_t cols = cellCount(pipes);
    if (cols == 0 || cols > kMaxColumns) {
        return info;
    }
    for (std::size_t c = 0; c < cols; ++c) {
        const Span content = cellContentSpan(line, cellRawSpan(pipes, c));
        if (content.length == 0) {
            return info;
        }
        const std::u16string_view token = line.substr(content.start, content.length);
        const bool leftColon = token.front() == u':';
        const bool rightColon = token.size() > 1 && token.back() == u':';
        const std::size_t dashStart = leftColon ? 1 : 0;
        const std::size_t dashEnd = rightColon ? token.size() - 1 : token.size();
        if (dashStart >= dashEnd) {
            return info;
        }
        for (std::size_t i = dashStart; i < dashEnd; ++i) {
            if (token[i] != u'-') {
                return info;
            }
        }
        info.aligns[c] = (leftColon && rightColon) ? TableAlign::Center
                         : rightColon              ? TableAlign::Right
                                                   : TableAlign::Left;
    }
    info.columns = cols;
    info.valid = true;
    return info;
}

std::uint32_t encodeState(const DelimiterInfo& delim, bool delimiterNext) noexcept {
    std::uint32_t state = (static_cast<std::uint32_t>(delim.columns) & kColumnMask) << kColumnShift;
    const std::size_t packed = std::min(delim.columns, kMaxPackedAlignColumns);
    for (std::size_t c = 0; c < packed; ++c) {
        state |= (static_cast<std::uint32_t>(delim.aligns[c]) & 0x3U) << (c * 2U);
    }
    if (delimiterNext) {
        state |= kDelimiterNext;
    }
    return state;
}

std::size_t stateColumns(std::uint32_t state) noexcept {
    return static_cast<std::size_t>((state >> kColumnShift) & kColumnMask);
}

TableAlign stateAlign(std::uint32_t state, std::size_t column) noexcept {
    if (column >= kMaxPackedAlignColumns) {
        return TableAlign::Left;
    }
    const std::uint32_t bits = (state >> (column * 2U)) & 0x3U;
    return bits == 1 ? TableAlign::Center : bits == 2 ? TableAlign::Right : TableAlign::Left;
}

void hideEscapedPipeBackslashes(std::u16string_view line, Span content, std::vector<HiddenRange>& hidden) {
    const std::size_t end = content.end();
    for (std::size_t i = content.start; i < end; ++i) {
        if (isSingleBacktick(line, i) && !escaped(line, i)) {
            if (const std::size_t closer = findClosingBacktick(line, i);
                closer != std::u16string_view::npos && closer < end) {
                i = closer;
                continue;
            }
        }
        if (line[i] == u'|' && i > content.start && escaped(line, i)) {
            hidden.push_back({{static_cast<std::uint32_t>(i - 1), 1}, HideMode::Collapse});
        }
    }
}

class TableElement final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "table"; }
    [[nodiscard]] bool allowsInlineElements() const noexcept override { return true; }

    [[nodiscard]] std::optional<BlockLine> parse(std::u16string_view line,
                                                 std::uint32_t state) const noexcept override {
        return parseWithNextLine(line, state, {});
    }

    [[nodiscard]] std::optional<BlockLine> parseWithNextLine(std::u16string_view line, std::uint32_t state,
                                                             std::u16string_view nextLine) const noexcept override {
        if (state == 0 && nextLine.empty()) {
            return std::nullopt;
        }
        if (line.find(u'|') == std::u16string_view::npos) {
            return std::nullopt;
        }
        if (state == 0) {
            if (nextLine.find(u'-') == std::u16string_view::npos || nextLine.find(u'|') == std::u16string_view::npos) {
                return std::nullopt;
            }
            const DelimiterInfo delim = parseDelimiterRow(nextLine);
            if (!delim.valid) {
                return std::nullopt;
            }
            const RowPipes header = scanRowPipes(line);
            if (!header.valid || cellCount(header) != delim.columns) {
                return std::nullopt;
            }
            return BlockLine{encodeState(delim, true), false};
        }
        if ((state & kDelimiterNext) != 0) {
            const DelimiterInfo delim = parseDelimiterRow(line);
            if (!delim.valid || delim.columns != stateColumns(state)) {
                return std::nullopt;
            }
            return BlockLine{state & ~kDelimiterNext, true};
        }
        const RowPipes body = scanRowPipes(line);
        if (!body.valid) {
            return std::nullopt;
        }
        return BlockLine{state, false};
    }

    void render(std::u16string_view line, const BlockLine& parsed, const LineContext& /*context*/,
                LineRender& out) const override {
        out.table = true;
        if (parsed.delimiter) {
            out.tableDelimiter = true;
            if (!line.empty()) {
                out.hidden.push_back({{0, static_cast<std::uint32_t>(line.size())}, HideMode::Collapse});
            }
            return;
        }
        out.tableHeader = (parsed.stateAfter & kDelimiterNext) != 0;
        const std::size_t expectedCols = stateColumns(parsed.stateAfter);
        const RowPipes pipes = scanRowPipes(line);
        if (!pipes.valid) {
            return;
        }
        const std::size_t cols = std::min(cellCount(pipes), expectedCols > 0 ? expectedCols : kMaxColumns);
        out.cells.reserve(cols);
        for (std::size_t c = 0; c < cols; ++c) {
            const Span raw = cellRawSpan(pipes, c);
            const Span content = cellContentSpan(line, raw);
            out.cells.push_back({content, raw, stateAlign(parsed.stateAfter, c)});
        }

        // Conceal all pipe syntax and surrounding cell-padding whitespace (ADR 0005).
        std::uint32_t cursor = 0;
        const Role cellRole = out.tableHeader ? Role::TableHeader : Role::TableCell;
        for (const auto& cell : out.cells) {
            if (cell.content.start > cursor) {
                out.hidden.push_back({{cursor, cell.content.start - cursor}, HideMode::Collapse});
            }
            if (cell.content.length > 0) {
                out.runs.push_back({cell.content, cellRole, 0});
                hideEscapedPipeBackslashes(line, cell.content, out.hidden);
            }
            cursor = cell.content.end();
        }
        const auto lineEnd = static_cast<std::uint32_t>(line.size());
        if (cursor < lineEnd) {
            out.hidden.push_back({{cursor, lineEnd - cursor}, HideMode::Collapse});
        }
    }
};

}  // namespace

std::unique_ptr<MultiLineElement> makeTableElement() {
    return std::make_unique<TableElement>();
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
    std::size_t end = line.size();
    while (end > 0 && isBlank(line[end - 1])) {
        --end;
    }
    std::u16string out(line.substr(0, end));
    if (out.empty() || out.back() != u'|') {
        out += u" |";
    }
    out += delimiterRow ? u" --- |" : u"  |";
    return out;
}

std::u16string removeTableColumn(std::u16string_view line, std::size_t column) {
    const RowPipes pipes = scanRowPipes(line);
    const std::size_t cols = cellCount(pipes);
    if (!pipes.valid || cols <= 1 || column >= cols) {
        return std::u16string(line);
    }
    std::u16string out(line.substr(0, pipes.firstNonBlank));
    out += u'|';
    for (std::size_t c = 0; c < cols; ++c) {
        if (c == column) {
            continue;
        }
        const Span raw = cellRawSpan(pipes, c);
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
    const RowPipes pipes = scanRowPipes(line);
    const std::size_t existing = pipes.valid ? cellCount(pipes) : 0;
    std::size_t end = line.size();
    while (end > 0 && isBlank(line[end - 1])) {
        --end;
    }
    std::u16string out(line.substr(0, end));
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

}  // namespace hitokoto::core
