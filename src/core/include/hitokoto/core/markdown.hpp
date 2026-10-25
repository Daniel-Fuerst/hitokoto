// SPDX-License-Identifier: GPL-3.0-or-later
// Markdown element engine: parse rules and render rules per element, registered with a
// registry. The UI asks the registry how to show one line; it never knows the elements.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
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

/// State carried from one line to the next while a multi-line element (fenced code block, ...)
/// is open. Opaque outside the registry: compare it, store it per line, pass it back in.
struct BlockState {
    std::uint8_t element = 0;  ///< 0 = no block open, else which multi-line element owns it
    std::uint32_t value = 0;   ///< that element's own state (never 0 while open)
    [[nodiscard]] bool active() const noexcept { return element != 0; }
    friend bool operator==(const BlockState&, const BlockState&) = default;
};

/// How one line is shown. All spans lie within the line.
struct LineRender {
    std::uint8_t headingLevel = 0;  ///< 1..6 if the whole line is a heading, else 0
    bool codeBlock = false;         ///< line belongs to a code block (the UI paints a panel)
    bool fenceLine = false;         ///< opening or closing line of a code block (no panel)
    BlockState blockState;          ///< state to pass when rendering the next line
    std::vector<StyleRun> runs;
    std::vector<InlineStyleRun> inlineStyles;
    std::vector<HiddenRange> hidden;
    std::vector<Decoration> decorations;
    [[nodiscard]] bool empty() const noexcept {
        return headingLevel == 0 && !codeBlock && !fenceLine && runs.empty() && inlineStyles.empty() &&
               hidden.empty() && decorations.empty();
    }
    friend bool operator==(const LineRender&, const LineRender&) = default;
};

/// Context for rendering one line.
struct LineContext {
    bool revealed = false;  ///< caret or selection is on this line: show raw syntax
    BlockState blockState;  ///< state entering this line (`LineRender::blockState` of the line above)

    LineContext(bool showSource = false, BlockState state = {}) : revealed(showSource), blockState(state) {}
};

/// Result of a line element's parse rule.
struct LineMatch {
    Span marker;                   ///< the syntax marker incl. leading indentation and trailing blanks
    std::uint8_t level = 0;        ///< heading level, or list nesting depth
    std::uint32_t markerChar = 0;  ///< position of the significant marker character
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
};

/// An inline Markdown element that decorates text spans without owning or changing the source.
class InlineElement {
public:
    InlineElement() = default;
    InlineElement(const InlineElement&) = delete;
    InlineElement& operator=(const InlineElement&) = delete;
    virtual ~InlineElement() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
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
    /// Render rule: how to show a line the parse rule accepted.
    virtual void render(std::u16string_view line, const BlockLine& parsed, const LineContext& context,
                        LineRender& out) const = 0;
};

class ElementRegistry {
public:
    /// Registers an element. Earlier registrations win when several elements match a line.
    void add(std::unique_ptr<LineElement> element);
    /// Registers an inline element. All registered inline elements render after the block element.
    void addInline(std::unique_ptr<InlineElement> element);
    /// Registers a multi-line element. Multi-line elements are tried before line elements; earlier
    /// registrations win. At most 255 (throws std::length_error beyond).
    void addMultiLine(std::unique_ptr<MultiLineElement> element);

    /// How to show `line`. Never throws for any input.
    [[nodiscard]] LineRender render(std::u16string_view line, const LineContext& context) const;
    /// `render(line, {any, state}).blockState`, without rendering (the UI walks lines with it).
    [[nodiscard]] BlockState blockStateAfter(std::u16string_view line, BlockState state = {}) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return elements_.size(); }

    /// Registry with all built-in elements of this version.
    static ElementRegistry withBuiltins();

private:
    struct BlockMatch {
        std::uint8_t element = 0;  ///< index + 1 in multiLineElements_
        BlockLine parsed;
        [[nodiscard]] BlockState stateAfter() const noexcept;
    };
    [[nodiscard]] std::optional<BlockMatch> matchBlock(std::u16string_view line, BlockState state) const noexcept;

    std::vector<std::unique_ptr<LineElement>> elements_;
    std::vector<std::unique_ptr<InlineElement>> inlineElements_;
    std::vector<std::unique_ptr<MultiLineElement>> multiLineElements_;
};

/// Built-in elements (each in its own source file).
std::unique_ptr<LineElement> makeHeadingElement();
std::unique_ptr<LineElement> makeBulletElement();
std::unique_ptr<InlineElement> makeInlineCodeElement();
std::unique_ptr<InlineElement> makeEmphasisElement();
std::unique_ptr<MultiLineElement> makeFencedCodeElement();

}  // namespace hitokoto::core
