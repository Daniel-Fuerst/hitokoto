// SPDX-License-Identifier: GPL-3.0-or-later
// Markdown element engine: parse rules and render rules per element, registered with a
// registry. The UI asks the registry how to show one line; it never knows the elements.
#pragma once

#include "hitokoto/core/reveal.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// Half-open range of UTF-16 code units within one line.
struct Span {
    std::uint32_t start = 0;
    std::uint32_t length = 0;
    [[nodiscard]] std::uint32_t end() const noexcept { return start + length; }
    friend bool operator==(const Span&, const Span&) = default;
};

/// Semantic role of a run of text; the UI maps roles to theme formats.
enum class Role : std::uint8_t {
    Marker,            ///< visible syntax marker (cursor on the line)
    HeadingText,       ///< text of a heading; StyleRun::level holds 1..6
    CodeBlock,         ///< text inside a fenced code block
    InlineCode,        ///< text inside a single-backtick code span; one delimiter character on each side
    InlineCodeMarker,  ///< a revealed inline-code delimiter (backtick)
    EmphasisMarker,    ///< a revealed emphasis delimiter (* _ ~~)
    EscapeMarker,      ///< a revealed backslash escape delimiter (\)
    LinkText,          ///< text of an inline link
    LinkMarker,        ///< a revealed link delimiter or target
};

/// An inline Markdown link with its text span, target URL and optional title.
struct LinkTarget {
    Span span;                   ///< span of the link text [start, length]
    Span full;                   ///< full span [start of '[', length to closing ')']
    std::u16string destination;  ///< URL / destination
    std::u16string title;        ///< title, if any
    friend bool operator==(const LinkTarget&, const LinkTarget&) = default;
};

enum class TableAlign : std::uint8_t {
    Left = 0,
    Center = 1,
    Right = 2,
};

struct TableCell {
    Span content;  ///< trimmed cell content span within the line (empty cell: start == end)
    Span raw;      ///< full cell span between pipe delimiters
    TableAlign align = TableAlign::Left;
    friend bool operator==(const TableCell&, const TableCell&) = default;
};

enum class InlineStyle : std::uint8_t {
    None = 0,
    Bold = 1,
    Italic = 2,
    BoldItalic = 3,
    Strikethrough = 4,
    BoldStrikethrough = 5,
    ItalicStrikethrough = 6,
    BoldItalicStrikethrough = 7,
};

[[nodiscard]] constexpr InlineStyle operator|(InlineStyle left, InlineStyle right) noexcept {
    return static_cast<InlineStyle>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr bool hasStyle(InlineStyle styles, InlineStyle style) noexcept {
    return (static_cast<std::uint8_t>(styles) & static_cast<std::uint8_t>(style)) != 0;
}

struct StyleRun {
    Span span;
    Role role = Role::Marker;
    std::uint8_t level = 0;
    friend bool operator==(const StyleRun&, const StyleRun&) = default;
};

struct InlineStyleRun {
    Span span;
    InlineStyle style = InlineStyle::None;
    friend bool operator==(const InlineStyleRun&, const InlineStyleRun&) = default;
};

enum class HideMode : std::uint8_t {
    Collapse,   ///< takes (almost) no space; following text moves left
    KeepSpace,  ///< invisible but keeps its width (a decoration is drawn there)
};

struct HiddenRange {
    Span span;
    HideMode mode = HideMode::Collapse;
    friend bool operator==(const HiddenRange&, const HiddenRange&) = default;
};

enum class DecorationKind : std::uint8_t { Bullet };

/// Something the UI draws on top of the text, anchored at a character.
struct Decoration {
    DecorationKind kind = DecorationKind::Bullet;
    std::uint32_t column = 0;  ///< anchor character (UTF-16 index in the line)
    std::uint8_t depth = 0;    ///< nesting depth for lists (0 = top level)
    friend bool operator==(const Decoration&, const Decoration&) = default;
};

/// State carried from one line to the next: the multi-line element (fenced code block, ...) that
/// is open, and the paragraph the line belongs to (S023, ADR 0006). Opaque outside the registry:
/// compare it, store it per line, pass it back in.
struct BlockState {
    /// `paragraph` of text that only continues lazily (an ordered list item or block quote, which
    /// hitokoto does not render yet): no setext underline can follow it.
    static constexpr std::uint16_t kLazyParagraph = 0xFFFF;

    std::uint8_t element = 0;  ///< 0 = no block open, else which multi-line element owns it
    std::uint32_t value = 0;   ///< that element's own state (never 0 while open)
    /// 0 = the line is no paragraph text; else 1 + the content column of its paragraph (0 for a
    /// paragraph at the top level, the content column of a list item), or kLazyParagraph.
    std::uint16_t paragraph = 0;
    /// A multi-line block is open (independent of `paragraph`).
    [[nodiscard]] bool active() const noexcept { return element != 0; }
    /// The line is paragraph text that a setext underline on the next line can underline.
    [[nodiscard]] bool underlinable() const noexcept { return paragraph != 0 && paragraph != kLazyParagraph; }
    friend bool operator==(const BlockState&, const BlockState&) = default;
};

/// How one line is shown. All spans lie within the line.
struct LineRender {
    std::uint8_t headingLevel = 0;     ///< 1..6 if the whole line is a heading, else 0
    bool codeBlock = false;            ///< line belongs to a code block (the UI paints a panel)
    bool fenceLine = false;            ///< opening or closing line of a code block (inside the panel, S021)
    bool table = false;                ///< line belongs to a Markdown table
    bool tableHeader = false;          ///< header row of a table
    bool tableDelimiter = false;       ///< delimiter row (| --- | --- |) of a table
    bool thematicBreak = false;        ///< thematic break (S023); concealed, the UI draws a horizontal line
    std::uint8_t setextUnderline = 0;  ///< 1 or 2: the line underlines a setext heading of that level (S023)
    bool frontMatter = false;          ///< line of closed front matter, delimiters included (S024)
    BlockState blockState;             ///< state to pass when rendering the next line
    std::vector<TableCell> cells;      ///< cells of a table header or body row
    std::vector<StyleRun> runs;
    std::vector<InlineStyleRun> inlineStyles;
    std::vector<HiddenRange> hidden;
    std::vector<Decoration> decorations;
    std::vector<LinkTarget> links;
    std::vector<Span> opaqueSpans;     ///< spans that later inline elements must not parse (code spans, link markers)
    std::vector<Span> inlineContexts;  ///< isolated inline contexts (e.g. link text: delimiters never pair outside)
    std::u16string codeLanguage;       ///< opening fence: language of the info string (S021), else empty
    [[nodiscard]] bool empty() const noexcept {
        return headingLevel == 0 && !codeBlock && !fenceLine && !table && !thematicBreak && setextUnderline == 0 &&
               !frontMatter && runs.empty() && inlineStyles.empty() && hidden.empty() && decorations.empty() &&
               links.empty() && opaqueSpans.empty() && inlineContexts.empty();
    }
    friend bool operator==(const LineRender&, const LineRender&) = default;
};

/// Context for rendering one line.
struct LineContext {
    bool revealed = false;           ///< caret or selection is on this line: show raw syntax
    BlockState blockState;           ///< state entering this line (`LineRender::blockState` of the line above)
    std::u16string_view nextLine{};  ///< following line in the document (for 1-line lookahead elements)
    /// S023 (ADR 0006): 1 or 2 if the paragraph this line belongs to ends in a setext underline of
    /// that level, found by walking the lines below (`ElementRegistry::paragraphLink`); else 0.
    /// Ignored on lines that are no paragraph text.
    std::uint8_t setextLevel = 0;

    LineContext(bool showSource = false, BlockState state = {}, std::u16string_view next = {}, std::uint8_t setext = 0)
        : revealed(showSource), blockState(state), nextLine(next), setextLevel(setext) {}
};

/// Result of a line element's parse rule.
struct LineMatch {
    Span marker;                   ///< the syntax marker incl. leading indentation and trailing blanks
    std::uint8_t level = 0;        ///< heading level, or list nesting depth
    std::uint32_t markerChar = 0;  ///< position of the significant marker character
};

/// How a line an element matched relates to paragraph text (S023, ADR 0006).
struct ParagraphRole {
    enum class Kind : std::uint8_t {
        Ends,      ///< the line ends paragraph text (a heading)
        Opens,     ///< its text is paragraph text at `column` (a bullet with text)
        LazyText,  ///< its text continues lazily into the lines below, which never underline it
    };
    Kind kind = Kind::Ends;
    std::uint32_t column = 0;  ///< Opens: the content column; the text starts at `LineMatch::marker.end()`
};

/// A block-level element that is decided by one line (heading, list item, ...).
/// Adding an element = implementing this interface and registering it; nothing else changes.
class LineElement {
public:
    LineElement() = default;
    LineElement(const LineElement&) = delete;
    LineElement& operator=(const LineElement&) = delete;
    virtual ~LineElement() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    /// Parse rule: does this line belong to the element?
    [[nodiscard]] virtual std::optional<LineMatch> parse(std::u16string_view line) const = 0;
    /// Render rule: how to show a matched line.
    virtual void render(std::u16string_view line, const LineMatch& match, const LineContext& context,
                        LineRender& out) const = 0;
    /// Paragraph role (S023, ADR 0006): whether a matched line ends paragraph text (the default, as
    /// a heading does), opens paragraph text at a content column (a bullet with text), or has text
    /// that lines below continue lazily but never underline. Lines no element matches are
    /// paragraph text.
    [[nodiscard]] virtual ParagraphRole paragraphRole(std::u16string_view line, const LineMatch& match) const noexcept {
        (void)line;
        (void)match;
        return {};
    }
    /// Whether a matched line starts the element directly after paragraph text (CommonMark 5.2:
    /// an empty list item does not; an ordered item only if numbered 1). `lazy`: the line is less
    /// indented than the paragraph's content, so outside its container. If not, the line continues
    /// the paragraph. Default: it does.
    [[nodiscard]] virtual bool startsAfterParagraph(std::u16string_view line, const LineMatch& match,
                                                    bool lazy) const noexcept {
        (void)line;
        (void)match;
        (void)lazy;
        return true;
    }
};

/// An inline Markdown element that decorates text spans without owning or changing the source.
class InlineElement {
public:
    InlineElement() = default;
    InlineElement(const InlineElement&) = delete;
    InlineElement& operator=(const InlineElement&) = delete;
    virtual ~InlineElement() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    /// Characters that can start this element. The registry scans each line once and skips the
    /// element on lines that contain none of them. Empty (the default) = run on every line.
    [[nodiscard]] virtual std::u16string_view triggerCharacters() const noexcept { return {}; }
    virtual void render(std::u16string_view line, const LineContext& context, LineRender& out) const = 0;
};

/// Result of a multi-line element's parse rule for one line.
struct BlockLine {
    std::uint32_t stateAfter = 0;  ///< element state for the next line; 0 = the block ends with this line
    bool delimiter = false;        ///< opening or closing line (e.g. a fence), for the render rule
};

/// A block-level element that spans lines (fenced code block, ...). While one is open it owns every
/// line until it ends: line and inline elements do not run there. The element sees only its own
/// state, a non-zero number it chooses; the registry carries it between lines.
/// Adding an element = implementing this interface and registering it; nothing else changes.
class MultiLineElement {
public:
    MultiLineElement() = default;
    MultiLineElement(const MultiLineElement&) = delete;
    MultiLineElement& operator=(const MultiLineElement&) = delete;
    virtual ~MultiLineElement() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    /// Parse rule. `state` is 0 outside a block: return a BlockLine if `line` opens one, else
    /// nullopt. Inside, `state` is the element's state from the previous line: return the line's
    /// BlockLine, or nullopt if the block ended before `line` (the line is then parsed afresh).
    /// Runs for every line above the visible ones, so it must be cheap and must not allocate.
    [[nodiscard]] virtual std::optional<BlockLine> parse(std::u16string_view line,
                                                         std::uint32_t state) const noexcept = 0;
    /// Parse rule with one-line lookahead (default: ignores `nextLine` and calls `parse`).
    [[nodiscard]] virtual std::optional<BlockLine> parseWithNextLine(std::u16string_view line, std::uint32_t state,
                                                                     std::u16string_view nextLine) const noexcept {
        (void)nextLine;
        return parse(line, state);
    }
    /// Render rule: how to show a line the parse rule accepted.
    virtual void render(std::u16string_view line, const BlockLine& parsed, const LineContext& context,
                        LineRender& out) const = 0;
    /// A non-zero state passed to `parse` for the first line of a document only (front matter,
    /// S023). A block opened from it counts only if it ends before the document does; the caller
    /// confirms that (`ElementRegistry::documentStart`). 0 = the element does not start documents.
    [[nodiscard]] virtual std::uint32_t documentStartState() const noexcept { return 0; }
};

/// How a line relates to the paragraph text of the line above (S023, ADR 0006).
struct ParagraphLink {
    bool continues = false;      ///< the line is paragraph text of the same paragraph
    std::uint8_t underline = 0;  ///< 1 or 2: the line is a setext underline of that paragraph
    friend bool operator==(const ParagraphLink&, const ParagraphLink&) = default;
};

class ElementRegistry {
public:
    /// Registers an element. Earlier registrations win when several elements match a line.
    void add(std::unique_ptr<LineElement> element);
    /// Registers an inline element. Inline elements render after the block element, in registration
    /// order, and each sees the runs the earlier ones produced (`LineRender::runs`). An element that
    /// must not style another element's text, as emphasis must not style inline code, therefore
    /// comes after it; `withBuiltins()` keeps that order.
    void addInline(std::unique_ptr<InlineElement> element);
    /// Registers a multi-line element. Multi-line elements are tried before line elements; earlier
    /// registrations win. At most 255 (throws std::length_error beyond).
    void addMultiLine(std::unique_ptr<MultiLineElement> element);

    /// How to show `line`. Never throws for any input.
    [[nodiscard]] LineRender render(std::u16string_view line, const LineContext& context) const;
    /// `render`, and `paragraphLink(line, context.blockState, context.nextLine)` from the same pass.
    [[nodiscard]] LineRender render(std::u16string_view line, const LineContext& context, ParagraphLink& link) const;
    /// `render(line, {any, state, nextLine}).blockState`, without rendering (the UI walks lines with it).
    [[nodiscard]] BlockState blockStateAfter(std::u16string_view line, BlockState state = {},
                                             std::u16string_view nextLine = {}) const noexcept;
    /// How `line`, entered with `state`, relates to the paragraph above it, without rendering. A
    /// paragraph ends in a setext heading of level n if, walking down from it, the lines continue
    /// it until one is an underline of level n (S023, ADR 0006).
    [[nodiscard]] ParagraphLink paragraphLink(std::u16string_view line, BlockState state,
                                              std::u16string_view nextLine = {}) const noexcept;
    /// The state entering the first line of a document whose first block (front matter, S023) is
    /// closed. A caller walks `blockStateAfter` from it: the block is closed if the state leaves its
    /// element before the document ends; otherwise, and if no element starts documents, the first
    /// line is entered with `BlockState{}`. Returns `BlockState{}` if no element starts documents.
    [[nodiscard]] BlockState documentStart() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return elements_.size(); }
    /// How every line of a whole document looks while the lines in `revealed` hold the caret or a
    /// selection (S023, ADR 0006). The reference for the lazy rendering in the UI, which must give
    /// the same `LineRender` for every line; only `LineRender::blockState` is internal.
    [[nodiscard]] std::vector<LineRender> renderDocument(std::span<const std::u16string> lines,
                                                         LineRange revealed = {}) const;
    /// `BlockState::element` of the multi-line element with this id, or 0 if none is registered.
    [[nodiscard]] std::uint8_t blockElement(std::string_view id) const noexcept;

    /// Registry with all built-in elements of this version.
    static ElementRegistry withBuiltins();

private:
    struct BlockMatch {
        std::uint8_t element = 0;  ///< index + 1 in multiLineElements_
        BlockLine parsed;
        [[nodiscard]] BlockState stateAfter() const noexcept;
    };
    [[nodiscard]] std::optional<BlockMatch> matchBlock(std::u16string_view line, BlockState state,
                                                       std::u16string_view nextLine) const noexcept;
    /// What a line is, decided without rendering (S023, ADR 0006).
    struct Classified;
    [[nodiscard]] Classified classify(std::u16string_view line, BlockState state,
                                      std::u16string_view nextLine) const noexcept;
    void runInlineElements(std::u16string_view line, const LineContext& context, LineRender& out) const;

    std::vector<std::unique_ptr<LineElement>> elements_;
    struct InlineEntry {
        std::unique_ptr<InlineElement> element;
        std::u16string otherTriggers;  ///< non-ASCII trigger characters
        bool always = false;           ///< no trigger characters declared, or more than 64 inline elements
    };
    std::vector<InlineEntry> inlineElements_;
    /// For each ASCII character, bit i is set if it triggers inline element i.
    std::array<std::uint64_t, 128> asciiTriggers_{};
    bool hasOtherTriggers_ = false;
    std::vector<std::unique_ptr<MultiLineElement>> multiLineElements_;
};

/// Built-in elements (each in its own source file).
std::unique_ptr<LineElement> makeHeadingElement();
std::unique_ptr<LineElement> makeBulletElement();
std::unique_ptr<InlineElement> makeInlineCodeElement();
std::unique_ptr<InlineElement> makeLinkElement();
std::unique_ptr<InlineElement> makeEmphasisElement();
std::unique_ptr<InlineElement> makeEscapeElement();
std::unique_ptr<MultiLineElement> makeFencedCodeElement();
std::unique_ptr<MultiLineElement> makeTableElement();
std::unique_ptr<MultiLineElement> makeFrontMatterElement();

/// ADR 0002, decision 6 (S018): adds a blank line before each line without `|` that directly
/// follows a table's rows, which GFM would make a row and Obsidian ends the table with. Added
/// breaks take the majority ending in `decoded.perBreak`. Returns how many lines were added.
std::size_t separateTextAfterTables(DecodedText& decoded, const ElementRegistry& registry);

/// Returns a blank GFM pipe-table row with `columns` empty cells, e.g. "|  |  |".
[[nodiscard]] std::u16string makeEmptyTableRow(std::size_t columns);
/// Appends one new column to a table line (`|  |` for header/body rows, `| --- |` for delimiter rows).
[[nodiscard]] std::u16string appendTableColumn(std::u16string_view line, bool delimiterRow);
/// Removes `column` (0-based) from a table header, delimiter or body row when the row has at least 2 columns.
[[nodiscard]] std::u16string removeTableColumn(std::u16string_view line, std::size_t column);
/// Ensures a table header or body row has at least `columns` cells by appending empty ` |` cells.
[[nodiscard]] std::u16string padTableRowCells(std::u16string_view line, std::size_t columns);
/// Every cell of a table row under GFM rules (also cells beyond the header's column count). A line
/// without a pipe is one cell.
[[nodiscard]] std::vector<TableCell> tableRowCells(std::u16string_view line);
/// The column alignments of a delimiter row, or nothing if `line` is not one.
[[nodiscard]] std::optional<std::vector<TableAlign>> tableDelimiterAligns(std::u16string_view line);
/// Cell Markdown as text to edit: `\|` becomes `|`, `<br>` (any spelling) a line break.
[[nodiscard]] std::u16string tableCellToText(std::u16string_view markdown);
/// Edited text as cell Markdown: `|` becomes `\|`, a line break `<br>`.
[[nodiscard]] std::u16string tableCellFromText(std::u16string_view text);
/// `line` with the content of cell `column` replaced by `markdown`; the rest stays byte for byte.
/// A missing cell is added after the existing ones.
[[nodiscard]] std::u16string replaceTableCell(std::u16string_view line, std::size_t column,
                                              std::u16string_view markdown);

}  // namespace hitokoto::core
