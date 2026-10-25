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
    Marker,       ///< visible syntax marker (cursor on the line)
    HeadingText,  ///< text of a heading; StyleRun::level holds 1..6
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

/// How one line is shown. All spans lie within the line.
struct LineRender {
    std::uint8_t headingLevel = 0;  ///< 1..6 if the whole line is a heading, else 0
    std::vector<StyleRun> runs;
    std::vector<InlineStyleRun> inlineStyles;
    std::vector<HiddenRange> hidden;
    std::vector<Decoration> decorations;
    [[nodiscard]] bool empty() const noexcept {
        return headingLevel == 0 && runs.empty() && inlineStyles.empty() && hidden.empty() && decorations.empty();
    }
    friend bool operator==(const LineRender&, const LineRender&) = default;
};

/// Context for rendering one line.
struct LineContext {
    bool revealed = false;  ///< caret or selection is on this line: show raw syntax
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

class ElementRegistry {
public:
    /// Registers an element. Earlier registrations win when several elements match a line.
    void add(std::unique_ptr<LineElement> element);
    /// Registers an inline element. All registered inline elements render after the block element.
    void addInline(std::unique_ptr<InlineElement> element);

    /// How to show `line`. Never throws for any input.
    [[nodiscard]] LineRender render(std::u16string_view line, const LineContext& context) const;

    [[nodiscard]] std::size_t size() const noexcept { return elements_.size(); }

    /// Registry with all built-in elements of this version.
    static ElementRegistry withBuiltins();

private:
    std::vector<std::unique_ptr<LineElement>> elements_;
    std::vector<std::unique_ptr<InlineElement>> inlineElements_;
};

/// Built-in elements (each in its own source file).
std::unique_ptr<LineElement> makeHeadingElement();
std::unique_ptr<LineElement> makeBulletElement();
std::unique_ptr<InlineElement> makeEmphasisElement();

}  // namespace hitokoto::core
