// SPDX-License-Identifier: GPL-3.0-or-later
// Markdown element engine: parse rules and render rules per element, registered with a
// registry. The UI asks the registry how to show one line; it never knows the elements.
#pragma once

#include "hitokoto/core/reveal.hpp"

#include <array>
#include <cstddef>
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
    Math,              ///< LaTeX source of a formula (S044)
    MathMarker,        ///< a revealed math delimiter (`$`, `$$`; S044)
};

/// A LaTeX formula on a line (S044): where its source is, and whether it is display math (`$$`)
/// or inline math (`$`). The source excludes the delimiters.
struct MathSpan {
    Span source;
    bool display = false;
    friend bool operator==(const MathSpan&, const MathSpan&) = default;
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
    ZeroWidth,  ///< takes no space; used for concealed link syntax
    KeepSpace,  ///< invisible but keeps its width (a decoration is drawn there)
    Box,        ///< invisible, as wide as the checkbox the UI draws before it plus its gap (S052)
};

struct HiddenRange {
    Span span;
    HideMode mode = HideMode::Collapse;
    friend bool operator==(const HiddenRange&, const HiddenRange&) = default;
};

enum class DecorationKind : std::uint8_t {
    Bullet,    ///< the dot of a bullet item (S003)
    Checkbox,  ///< the box of a task item (S052), drawn with every reveal
};

/// Something the UI draws on top of the text, anchored at a character.
struct Decoration {
    DecorationKind kind = DecorationKind::Bullet;
    std::uint32_t column = 0;  ///< anchor character (UTF-16 index in the line)
    std::uint8_t depth = 0;    ///< nesting depth for lists (0 = top level)
    bool checked = false;      ///< Checkbox: the box is ticked
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
    /// The line has paragraph text (top-level, inside a list item, or lazy text after `1.` / `>`).
    [[nodiscard]] bool inParagraph() const noexcept { return paragraph != 0; }
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
    bool mathBlock = false;            ///< line of a `$$` display math block, delimiter lines included (S044)
    bool mathDelimiter = false;        ///< opening or closing `$$` line of a display math block (S044)
    bool flowsToNext = false;          ///< soft line break: the next line continues this paragraph (S033)
    bool hardBreak = false;            ///< hard line break (2+ spaces or `\`) before the next paragraph line (S033)
    BlockState blockState;             ///< state to pass when rendering the next line
    std::vector<TableCell> cells;      ///< cells of a table header or body row
    std::vector<StyleRun> runs;
    std::vector<InlineStyleRun> inlineStyles;
    std::vector<HiddenRange> hidden;
    std::vector<Decoration> decorations;
    std::vector<LinkTarget> links;
    std::vector<MathSpan> math;        ///< formulas on the line, in order (S044)
    std::vector<Span> opaqueSpans;     ///< spans that later inline elements must not parse (code spans, link markers)
    std::vector<Span> inlineContexts;  ///< isolated inline contexts (e.g. link text: delimiters never pair outside)
    std::u16string codeLanguage;       ///< opening fence: language of the info string (S021), else empty
    std::uint32_t wrapIndent = 0;      ///< column where continuation lines align (hanging indent, S003); 0 = none
    Span doneText;                     ///< text of a checked task item (S052): shown as done; empty = none
    [[nodiscard]] bool empty() const noexcept {
        return headingLevel == 0 && !codeBlock && !fenceLine && !table && !thematicBreak && setextUnderline == 0 &&
               !frontMatter && !mathBlock && !mathDelimiter && !flowsToNext && !hardBreak && runs.empty() &&
               inlineStyles.empty() && hidden.empty() && decorations.empty() && links.empty() && math.empty() &&
               opaqueSpans.empty() && inlineContexts.empty() && wrapIndent == 0 && doneText.length == 0;
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
    /// S033 (ADR 0006): true if the caret or selection is on any line of this line's paragraph (or
    /// its setext underline), so the paragraph's source lines and hard-break markers are revealed
    /// even when `revealed` is false for inline elements on this line.
    bool paragraphRevealed = false;
    /// S033: line after `nextLine` in the document (so `render` can tell whether `nextLine` opens a
    /// pipe table or continues this paragraph).
    std::u16string_view nextNextLine{};

    LineContext(bool showSource = false, BlockState state = {}, std::u16string_view next = {}, std::uint8_t setext = 0,
                bool showParagraph = false, std::u16string_view nextNext = {})
        : revealed(showSource),
          blockState(state),
          nextLine(next),
          setextLevel(setext),
          paragraphRevealed(showSource || showParagraph),
          nextNextLine(nextNext) {}
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

/// What the elements registered before an inline element have claimed on its line, one entry per
/// character: inside an opaque span (a code span, link syntax; `LineRender::opaqueSpans`) or
/// hidden (`LineRender::hidden`, also by a block element). The registry builds it once per line
/// and adds what each inline element claims before the next one runs, so no element walks the
/// spans of the others itself.
class InlineScan {
public:
    explicit InlineScan(std::size_t length) : flags_(length, 0) {}

    /// Character `i` lies in an opaque span.
    [[nodiscard]] bool isOpaque(std::size_t i) const noexcept { return (flags_[i] & kOpaque) != 0; }
    /// Character `i` lies in an opaque span or is hidden.
    [[nodiscard]] bool isTaken(std::size_t i) const noexcept { return flags_[i] != 0; }

    /// Takes in the opaque spans and hidden ranges `out` gained since the last call (the parts
    /// within the line); returns how many characters it marked.
    std::size_t update(const LineRender& out);

private:
    static constexpr std::uint8_t kOpaque = 1;
    static constexpr std::uint8_t kHidden = 2;
    std::vector<std::uint8_t> flags_;
    std::size_t opaqueSeen_ = 0;
    std::size_t hiddenSeen_ = 0;
};

/// An inline Markdown element that decorates text spans without owning or changing the source.
class InlineElement {
public:
    InlineElement() = default;
    InlineElement(const InlineElement&) = delete;
    InlineElement& operator=(const InlineElement&) = delete;
    virtual ~InlineElement() = default;

    /// Characters that can start this element. The registry scans each line once and skips the
    /// element on lines that contain none of them. Empty (the default) = run on every line.
    [[nodiscard]] virtual std::u16string_view triggerCharacters() const noexcept { return {}; }
    /// Adds this element's runs, hidden ranges and spans to `out`; `scan` tells what the elements
    /// before it claimed on the line.
    virtual void render(std::u16string_view line, const LineContext& context, const InlineScan& scan,
                        LineRender& out) const = 0;
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
    /// `nextLine` is the line below, for an element that opens only with it (a table's delimiter
    /// row); empty where the caller has none. Runs for every line above the visible ones, so it
    /// must be cheap and must not allocate.
    [[nodiscard]] virtual std::optional<BlockLine> parseWithNextLine(std::u16string_view line, std::uint32_t state,
                                                                     std::u16string_view nextLine) const noexcept = 0;
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

/// Above which sizes a line shows no inline formatting (S007). Inline elements run only on a line
/// within both bounds; a longer or denser line shows its characters as they are written. Block
/// elements still apply, and the text never changes (ADR 0002).
struct InlineBounds {
    /// Characters (UTF-16 code units) in the line.
    std::size_t lineLength = 0;
    /// Characters in the line that can start inline syntax: the trigger characters of the
    /// registered inline elements (for the built-in ones `*`, `_`, `~`, `` ` ``, `\` and `[`).
    std::size_t syntaxCharacters = 0;
    friend bool operator==(const InlineBounds&, const InlineBounds&) = default;
};

/// The bounds every registry starts with. They keep a key typed in a line, or beside it, within the
/// 50 ms that no key event may take (brief, section 4), also on a machine half as fast as the
/// development VM, as a CI runner can be. Measured with the release build on that VM, Qt 6.12.0
/// and 6.8.3 (`docs/performance.md`, 2026-11-16):
/// - 300 syntax characters. What a line costs to lay out and draw grows with its formatted pieces:
///   about 25 µs for each link `[a](b)`, the dearest element, about 12 µs for each `*` of `a*a*`.
///   A key in a line of 300 links takes 7 to 8 ms, the slowest of 30 keys 12 ms; with 500 links
///   13 ms and up to 26 ms, with 1,000 links 22 ms and up to 47 ms, with 2,000 links 50 ms and up
///   to 81 ms. Text does not come near 300 such characters in one line.
/// - 10,000 characters. The inline elements render a line in linear time, so the length adds
///   little; what grows is Qt's layout of the block. With 500 links and words up to 10,000
///   characters a key takes 12 to 13 ms, up to 20,000 characters 13 ms, up to 50,000 characters 18
///   to 20 ms. At both bounds a key takes at most 8 ms and the slowest one 18 ms. Before the
///   elements were linear, each of 500 `[](` read on to the end of the line, and the bound was
///   4,000 characters.
inline constexpr InlineBounds kInlineBounds{10000, 300};

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
    /// The bounds above which `render` and `renderInline` run no inline element on a line
    /// (`kInlineBounds` unless set). Tests and measurements set others.
    void setInlineBounds(InlineBounds bounds) noexcept { inlineBounds_ = bounds; }
    [[nodiscard]] InlineBounds inlineBounds() const noexcept { return inlineBounds_; }
    /// Only the inline elements on `text`, as inside a table cell (S028: cells hold inline content
    /// only, so `- x` stays text). Never throws for any input.
    [[nodiscard]] LineRender renderInline(std::u16string_view text) const;
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
    bool hasAlwaysInline_ = false;
    InlineBounds inlineBounds_ = kInlineBounds;
    std::vector<std::unique_ptr<MultiLineElement>> multiLineElements_;
};

/// The work the inline pass has done on the calling thread so far, in steps: one for each character
/// an inline element or the registry's merge of their results looks at, and one for each entry of
/// a list they walk. A test renders a line, takes the difference and compares it with the length
/// of the line: a line renders in linear time (S013, S015, S020, S022). The number means nothing else.
[[nodiscard]] std::uint64_t inlineSteps() noexcept;

/// Built-in elements (each in its own source file).
std::unique_ptr<LineElement> makeHeadingElement();
std::unique_ptr<LineElement> makeBulletElement();
/// S052: task list items (`- [ ]`, `- [x]`); registered before the bullet element.
std::unique_ptr<LineElement> makeTaskElement();
std::unique_ptr<InlineElement> makeInlineCodeElement();
std::unique_ptr<InlineElement> makeLinkElement();
std::unique_ptr<InlineElement> makeEmphasisElement();
std::unique_ptr<InlineElement> makeEscapeElement();
/// S044: inline math `$…$` and display math written on one line `$$…$$`.
std::unique_ptr<InlineElement> makeInlineMathElement();
/// S044: display math blocks between `$$` lines.
std::unique_ptr<MultiLineElement> makeDisplayMathElement();
std::unique_ptr<MultiLineElement> makeFencedCodeElement();
std::unique_ptr<MultiLineElement> makeTableElement();
std::unique_ptr<MultiLineElement> makeFrontMatterElement();

}  // namespace hitokoto::core
