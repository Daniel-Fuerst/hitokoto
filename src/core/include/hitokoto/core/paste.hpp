// SPDX-License-Identifier: GPL-3.0-or-later
// S031 / ADR 0011: a table pasted from Outlook, Excel, Word or a web page becomes a GFM pipe table.
// The UI reads the clipboard (Qt's HTML import) into the model below; the core decides whether the
// paste converts, writes the Markdown and places it in the document.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// One cell of a pasted table: where it starts, what it spans, and its lines as the source gives
/// them (each line break and each paragraph boundary starts a new line; nothing trimmed yet).
struct PastedCell {
    std::uint32_t row = 0;
    std::uint32_t column = 0;
    std::uint32_t rowSpan = 1;
    std::uint32_t columnSpan = 1;
    std::vector<std::u16string> lines;
    friend bool operator==(const PastedCell&, const PastedCell&) = default;
};

/// A table of a paste. Cells may come in any order; positions no cell covers are empty.
struct PastedTable {
    std::vector<PastedCell> cells;
    bool containsTable = false;  ///< a cell holds a table of its own (S031, "Nested tables")
    friend bool operator==(const PastedTable&, const PastedTable&) = default;
};

/// A part of an HTML paste in source order: text outside tables (its lines as the source gives
/// them, one per paragraph and per line break) or a table.
struct PastedPart {
    std::vector<std::u16string> lines;  ///< text; empty for a table
    std::optional<PastedTable> table;
    friend bool operator==(const PastedPart&, const PastedPart&) = default;
};

/// A block to place: text (kept as plain text) or a converted table, lines joined by '\n'.
struct PasteBlock {
    bool table = false;
    std::u16string text;
    friend bool operator==(const PasteBlock&, const PasteBlock&) = default;
};

/// What the HTML of a paste holds, found by one linear scan before Qt's import (S031 review O1,
/// O9): no table start tag, tables only side by side, or a table inside a table. Tags are matched
/// without case; comments are skipped. `OverBudget`: tables side by side, but Qt's import would
/// take longer than the paste's cells allow (kPasteHtmlBounds).
enum class HtmlTables : std::uint8_t { None, Flat, Nested, OverBudget };

/// What Qt's HTML import costs per unit of what `scanHtml` counts, in picoseconds.
struct PasteHtmlCosts {
    std::uint64_t cellTag = 0;              ///< a td or th start tag
    std::uint64_t rowTag = 0;               ///< a tr start tag
    std::uint64_t tableTag = 0;             ///< a table start tag
    std::uint64_t blockTag = 0;             ///< a start tag of another block, of img, br, hr, meta
    std::uint64_t otherTag = 0;             ///< a start tag of an inline or unknown element
    std::uint64_t endTag = 0;               ///< an end tag, a comment, a declaration
    std::uint64_t attributeChar = 0;        ///< a character of an attribute
    std::uint64_t depthStep = 0;            ///< a start tag, per element it is inside
    std::uint64_t endDepthStep = 0;         ///< an end tag, per element it is inside
    std::uint64_t positionWithoutCell = 0;  ///< a position of a table's grid that no cell starts on
    std::uint64_t mergeStep = 0;            ///< per position a span covers, times its table's positions
    std::uint64_t imageStep = 0;            ///< an image, per character before it in its block
    std::uint64_t listBlockStep = 0;        ///< a block that joins a list, per block that joined before
};

/// How long Qt's import of a paste's HTML may take, and what it costs. S031 grants a paste
/// 50 ms up to 400 cells and 125 µs per cell above; half of it is for Qt's import, the other half
/// for the table (`floorPs`, `perCellPs`). A paste whose import would take longer stays plain
/// text, as a nested table does; Qt's import does not run.
struct PasteHtmlBounds {
    PasteHtmlCosts costs;
    std::uint64_t floorPs = 0;    ///< what any paste may take
    std::uint64_t perCellPs = 0;  ///< what a paste may take per cell (td or th start tag)
    std::size_t maxDepth = 0;     ///< elements open around a start tag, at most
};

/// The costs: Qt's import alone (`ui::readPastedHtml`) of pastes that each repeat one thing, on
/// the development VM (2 vCPU, release, offscreen, 2026-11-20), the larger of Qt 6.8.3 and 6.12.0,
/// rounded up; per unit they are close to the slowest run of each, and above the typical one.
/// - Tags: `<b>x</b>` 1.7 to 3.5 µs, `<span>` 1.8, `<a href>` 2.8 to 4.4 with its attribute,
///   `<p>`, `<div>`, `<li>`, `<h1>`, `<pre>` 4 to 5.5, a cell 10 to 13 µs, a table of one cell 17 µs.
/// - Positions no cell starts on (a span, a row with fewer cells): 4 to 8 µs each (2,000 rows
///   below a cell spanning 20,000 columns are 40 million of them). Merging a span takes longer
///   the larger the table: 400,000 covered positions in a table of 420,000 took 11 s.
/// - Nesting: Qt walks up from every element to the root for its style; 4,000 elements nested
///   in each other took 170 ms, 8,000 took 760 ms (a sum of depths of 32 million).
/// - Images: Qt steps over the block for each image it inserts; 10,000 images in one cell took
///   5 s, in 10,000 cells 0.15 s.
/// - Not estimated, because they leave before the import (htmlWithoutFormatting): style sheets
///   and the attributes that only format. A sheet the scan meets is beyond the estimate.
/// - Not counted: text, entities and comments. They cost Qt 5 to 18 ns a character, less than a
///   plain paste of the same text (75 ns a character).
/// What the cells allow: the samples' markup stays well below it (40 x 10 to 500 x 100 cells with
/// Outlook's markup 55 % of it in the estimate, with Excel's 26 %, both without what only formats;
/// Outlook's import itself took up to 62 % of its estimate). Pastes at the bounds (one kind each)
/// took 10 to 40 ms until the table showed (a table of 2,011 cells 80 ms), except a cell of 4,089
/// lines (90 ms, the table's layout of its lines, as on main); just above the bounds the paste
/// stays plain text in 0.3 to 1 ms.
/// `maxDepth` bounds the scan's own work for an end tag; the samples nest at most 10 deep.
inline constexpr PasteHtmlBounds kPasteHtmlBounds{
    {
        .cellTag = 12'000'000,
        .rowTag = 6'000'000,
        .tableTag = 10'000'000,
        .blockTag = 5'000'000,
        .otherTag = 2'500'000,
        .endTag = 1'000'000,
        .attributeChar = 80'000,
        .depthStep = 25'000,
        .endDepthStep = 5'000,
        .positionWithoutCell = 7'000'000,
        .mergeStep = 70,
        .imageStep = 130'000,
        .listBlockStep = 100'000,
    },
    25'000'000'000,
    62'500'000,
    256,
};

/// What one scan of a paste's HTML found.
struct HtmlScan {
    HtmlTables tables = HtmlTables::None;
    std::uint64_t cells = 0;      ///< td and th start tags, as Qt's parser reads them
    std::uint64_t importPs = 0;   ///< Qt's import, estimated from what the scan counted
    std::uint64_t allowedPs = 0;  ///< what the cells allow (kPasteHtmlBounds)
    friend bool operator==(const HtmlScan&, const HtmlScan&) = default;
};

/// Reads `html` once, as Qt's HTML parser reads it, for its tables and the cost of importing it.
/// `html` is what Qt imports: the paste without what only formats (`htmlWithoutFormatting`); the
/// text of a style element, a style sheet to Qt, is beyond the estimate. Linear in the input, for
/// any input.
[[nodiscard]] HtmlScan scanHtml(std::u16string_view html) noexcept;

/// `scanHtml(html).tables`.
[[nodiscard]] HtmlTables scanHtmlTables(std::u16string_view html) noexcept;

/// The HTML of a paste as Qt imports it (S031, rework of 2026-11-20), read as Qt's HTML parser
/// reads it (scanHtml): without what only formats. Every attribute leaves its tag except `colspan`
/// and `rowspan` of td and th and `name` and `content` of meta (Qt's mark of its own HTML, which
/// changes how it reads spaces), and a style element loses its text, the style sheet Qt would read.
/// A pasted table keeps only text, lines and spans; Qt's import grows in what formats faster than
/// in the cells (a rule `b ~ span` before 8,785 spans took 30 s, 10,000 tables whose cells differ
/// in width 7 to 8 s, 64,000 fonts whose size and colour add up to the same number 46 to 58 s).
/// Text, comments, end tags and the kept attributes stay byte for byte; an attribute leaves with
/// the blanks before it, up to one where a word follows. From an entity in a tag or attribute name
/// on, which Qt would decode, the rest stays as it is (the scan keeps such a paste plain text).
/// Style attributes have left before the import since 2026-11-10 (Outlook's inline CSS was half of
/// it, Armin). Qt's import ignores `display: none` (6.8.3 and 6.12.0), so no text appears or
/// disappears; what a sheet says about spaces (`white-space`) no longer applies. Linear in the
/// input.
[[nodiscard]] std::u16string htmlWithoutFormatting(std::u16string_view html);

/// Whether a paste at `line` lands in a block quote or a list item, so that it stays plain text
/// (S031 review O4): `line` starts one, continues the paragraph of one (also lazily), or is indented
/// under a list item after blank lines. `above(i)` is the i-th line above `line` (0 = the line
/// directly above), nothing at the start of the document. Conservative: a line that only looks
/// like a container's (in a code block above, say) keeps the paste plain text too.
[[nodiscard]] bool pasteLineInContainer(std::u16string_view line,
                                        const std::function<std::optional<std::u16string>(std::size_t)>& above);

/// Whether a table counts as a table (S031): at least two rows and two columns.
[[nodiscard]] bool countsAsTable(const PastedTable& table);

/// One line of copied cell text escaped for a cell (S031, "Escaping"): `|` as `\|`; `\`, `` ` ``,
/// `*`, `_`, `~`, `[`, `]`, `<` with a backslash; `&` before `#` or before letters and digits
/// and `;`. No trimming.
[[nodiscard]] std::u16string escapeTableCellText(std::u16string_view text);

/// The Markdown of a table (S031): header row, delimiter row, body rows, joined by '\n' without a
/// final line break. Lines in a cell are trimmed and joined with `<br>`, merged cells keep their
/// text in the top-left cell, every row is as wide as the widest.
[[nodiscard]] std::u16string pastedTableToMarkdown(const PastedTable& table);

/// The blocks of an HTML paste: tables that count as Markdown, the text around them and tables
/// that do not count as plain text (S031, "Text around tables"). Nothing if the paste stays plain
/// text: no table counts, or a table holds a table.
[[nodiscard]] std::optional<std::vector<PasteBlock>> pastedTablesToMarkdown(std::span<const PastedPart> parts);

/// Excel's plain text as a table (S031, "What counts as a table, plain text"), or nothing if the
/// text is no such table.
[[nodiscard]] std::optional<PastedTable> parseExcelPlainText(std::u16string_view text);

/// Where a paste lands: the caret's line split at the caret (after the selection is removed) and
/// the lines around it.
struct PastePlace {
    std::u16string_view head;                      ///< the caret's line before the caret
    std::u16string_view tail;                      ///< the caret's line after the caret
    std::optional<std::u16string_view> lineAbove;  ///< nothing at the start of the document
    std::optional<std::u16string_view> lineBelow;  ///< nothing at the end of the document
};

/// What to insert at the caret, lines joined by '\n', and the caret's offset in it afterwards.
struct PasteInsertion {
    std::u16string text;
    std::size_t caret = 0;
    friend bool operator==(const PasteInsertion&, const PasteInsertion&) = default;
};

/// Places the blocks at `place` (S031, "Where it lands"): each table its own block, exactly one
/// blank line between a table and text or another table, none at the start or end of the document
/// or next to a blank line; text continues the line as a plain paste would. The caret ends after
/// the last pasted character.
[[nodiscard]] PasteInsertion placePastedBlocks(std::span<const PasteBlock> blocks, const PastePlace& place);

}  // namespace hitokoto::core
