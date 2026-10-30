// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include "block_rules.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace hitokoto::core {
namespace {

/// Runs of inline elements (every run appended after the block element ran, from index
/// `firstInlineRun` on) take precedence over the block element's runs on the same characters.
/// The registry tells them apart by position, so a new inline element needs no new role here.
void splitAroundInlineRuns(LineRender& out, std::size_t firstInlineRun) {
    if (firstInlineRun >= out.runs.size()) {
        return;  // no inline runs
    }
    std::vector<StyleRun> inlineRuns(out.runs.begin() + static_cast<std::ptrdiff_t>(firstInlineRun), out.runs.end());
    std::sort(inlineRuns.begin(), inlineRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    std::vector<StyleRun> splitRuns;
    for (std::size_t i = 0; i < firstInlineRun; ++i) {
        const auto& run = out.runs[i];
        std::uint32_t start = run.span.start;
        for (const auto& inlineRun : inlineRuns) {
            if (inlineRun.span.end() <= start || inlineRun.span.start >= run.span.end()) {
                continue;
            }
            if (inlineRun.span.start > start) {
                splitRuns.push_back({{start, inlineRun.span.start - start}, run.role, run.level});
            }
            start = std::max(start, inlineRun.span.end());
        }
        if (start < run.span.end()) {
            splitRuns.push_back({{start, run.span.end() - start}, run.role, run.level});
        }
    }
    splitRuns.insert(splitRuns.end(), inlineRuns.begin(), inlineRuns.end());
    std::sort(splitRuns.begin(), splitRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    out.runs = std::move(splitRuns);
}

/// LineRender contract: styled runs and hidden ranges never overlap; hidden ranges win.
void carveRunsAroundHidden(LineRender& out) {
    if (out.hidden.empty()) {
        return;
    }
    const auto byStart = [](const HiddenRange& left, const HiddenRange& right) {
        return left.span.start < right.span.start;
    };
    if (!std::is_sorted(out.hidden.begin(), out.hidden.end(), byStart)) {
        std::sort(out.hidden.begin(), out.hidden.end(), byStart);
    }
    const bool overlap = std::any_of(out.runs.begin(), out.runs.end(), [&](const StyleRun& run) {
        return std::any_of(out.hidden.begin(), out.hidden.end(), [&](const HiddenRange& hidden) {
            return hidden.span.start < run.span.end() && run.span.start < hidden.span.end();
        });
    });
    if (!overlap) {
        return;  // the common case: nothing to carve, nothing to allocate
    }
    std::vector<StyleRun> visibleRuns;
    for (const auto& run : out.runs) {
        std::uint32_t start = run.span.start;
        for (const auto& hidden : out.hidden) {
            if (hidden.span.end() <= start || hidden.span.start >= run.span.end()) {
                continue;
            }
            if (hidden.span.start > start) {
                visibleRuns.push_back({{start, hidden.span.start - start}, run.role, run.level});
            }
            start = std::max(start, hidden.span.end());
        }
        if (start < run.span.end()) {
            visibleRuns.push_back({{start, run.span.end() - start}, run.role, run.level});
        }
    }
    out.runs = std::move(visibleRuns);
}

void carveInlineStylesAroundHidden(LineRender& out) {
    if (out.hidden.empty() || out.inlineStyles.empty()) {
        return;
    }
    const auto byStart = [](const HiddenRange& left, const HiddenRange& right) {
        return left.span.start < right.span.start;
    };
    if (!std::is_sorted(out.hidden.begin(), out.hidden.end(), byStart)) {
        std::sort(out.hidden.begin(), out.hidden.end(), byStart);
    }
    const bool overlap = std::any_of(out.inlineStyles.begin(), out.inlineStyles.end(), [&](const InlineStyleRun& run) {
        return std::any_of(out.hidden.begin(), out.hidden.end(), [&](const HiddenRange& hidden) {
            return hidden.span.start < run.span.end() && run.span.start < hidden.span.end();
        });
    });
    if (!overlap) {
        return;
    }
    std::vector<InlineStyleRun> visibleStyles;
    for (const auto& run : out.inlineStyles) {
        std::uint32_t start = run.span.start;
        for (const auto& hidden : out.hidden) {
            if (hidden.span.end() <= start || hidden.span.start >= run.span.end()) {
                continue;
            }
            if (hidden.span.start > start) {
                visibleStyles.push_back({{start, hidden.span.start - start}, run.style});
            }
            start = std::max(start, hidden.span.end());
        }
        if (start < run.span.end()) {
            visibleStyles.push_back({{start, run.span.end() - start}, run.style});
        }
    }
    out.inlineStyles = std::move(visibleStyles);
}

/// The paragraph value of `BlockState` for text at content column `column`.
std::uint16_t paragraphAt(std::uint32_t column) noexcept {
    constexpr std::uint32_t kMaxColumn = BlockState::kLazyParagraph - 2U;
    return static_cast<std::uint16_t>(std::min(column, kMaxColumn) + 1U);
}

/// A thematic break or setext underline: concealed, the line keeps its space and the UI draws
/// (or, for an underline, leaves) the space; revealed, the line as written in the marker colour.
void renderMarkerLine(std::u16string_view line, bool revealed, LineRender& out) {
    const Span whole{0, static_cast<std::uint32_t>(line.size())};
    if (revealed) {
        out.runs.push_back({whole, Role::Marker, 0});
    } else {
        out.hidden.push_back({whole, HideMode::KeepSpace});
    }
}

}  // namespace

/// What a line is, decided from the line, the state entering it and the next line (ADR 0006).
struct ElementRegistry::Classified {
    enum class Kind : std::uint8_t {
        Block,      ///< owned by a multi-line element
        Underline,  ///< setext underline of the paragraph above
        Break,      ///< thematic break
        Element,    ///< matched by a line element
        Other,      ///< blank, or a block hitokoto does not render yet
        Text,       ///< paragraph text no element claims
    };
    Kind kind = Kind::Other;
    std::optional<BlockMatch> block;
    std::uint8_t underline = 0;
    const LineElement* element = nullptr;
    LineMatch match;
    bool continues = false;            ///< paragraph text continuing the paragraph above
    std::uint32_t textStart = 0;       ///< where the line's paragraph text starts
    std::uint16_t paragraphAfter = 0;  ///< `BlockState::paragraph` after the line

    [[nodiscard]] BlockState stateAfter() const noexcept {
        if (block) {
            return block->stateAfter();
        }
        return BlockState{0, 0, paragraphAfter};
    }
};

ElementRegistry::Classified ElementRegistry::classify(std::u16string_view line, BlockState state,
                                                      std::u16string_view nextLine) const noexcept {
    using Kind = Classified::Kind;
    Classified c;
    if (auto block = matchBlock(line, state, nextLine)) {
        c.kind = Kind::Block;
        c.block = block;
        return c;
    }
    // The paragraph above, if the line follows paragraph text (a block that was open ended above).
    const std::uint16_t above = state.active() ? 0 : state.paragraph;
    if (state.underlinable() && !state.active()) {
        if (const std::uint8_t level = blocks::setextUnderline(line, state.paragraph - 1U); level != 0) {
            c.kind = Kind::Underline;  // takes precedence over a break and an empty list item (4.3)
            c.underline = level;
            return c;
        }
    }
    if (blocks::isThematicBreak(line)) {
        c.kind = Kind::Break;  // takes precedence over a list item (4.1, example 60)
        return c;
    }
    for (const auto& element : elements_) {
        if (auto match = element->parse(line)) {
            c.kind = Kind::Element;
            c.element = element.get();
            c.match = *match;
            const auto column = element->paragraphColumn(line, *match);
            c.paragraphAfter = column ? paragraphAt(*column) : 0;
            c.textStart = match->marker.end();
            return c;
        }
    }
    if (blocks::firstNonBlank(line) == line.size()) {
        return c;  // blank
    }
    // A line less indented than the paragraph's content column can only continue it lazily.
    const bool lazy = above == BlockState::kLazyParagraph || (above != 0 && blocks::indentWidth(line) + 1U < above);
    const blocks::After after = above == 0 ? blocks::After::Block
                                : lazy     ? blocks::After::Lazy
                                           : blocks::After::Paragraph;
    switch (blocks::otherBlockStart(line, after)) {
        case blocks::OtherBlock::Opaque:
            return c;
        case blocks::OtherBlock::LazyText:
            c.paragraphAfter = BlockState::kLazyParagraph;
            return c;
        case blocks::OtherBlock::None:
            break;
    }
    c.kind = Kind::Text;
    c.continues = above != 0;
    c.paragraphAfter = above != 0 ? above : paragraphAt(0);
    return c;
}

void ElementRegistry::add(std::unique_ptr<LineElement> element) {
    if (element) {
        elements_.push_back(std::move(element));
    }
}

void ElementRegistry::addInline(std::unique_ptr<InlineElement> element) {
    if (element) {
        InlineEntry entry;
        const std::size_t index = inlineElements_.size();
        const auto triggers = element->triggerCharacters();
        entry.always = triggers.empty() || index >= 64;
        for (const char16_t c : triggers) {
            if (entry.always) {
                break;
            }
            if (c < asciiTriggers_.size()) {
                asciiTriggers_[c] |= std::uint64_t{1} << index;
            } else {
                entry.otherTriggers.push_back(c);
                hasOtherTriggers_ = true;
            }
        }
        entry.element = std::move(element);
        inlineElements_.push_back(std::move(entry));
    }
}

void ElementRegistry::addMultiLine(std::unique_ptr<MultiLineElement> element) {
    if (!element) {
        return;
    }
    if (multiLineElements_.size() >= 255) {
        throw std::length_error("too many multi-line elements");
    }
    multiLineElements_.push_back(std::move(element));
}

std::uint8_t ElementRegistry::blockElement(std::string_view id) const noexcept {
    for (std::size_t i = 0; i < multiLineElements_.size(); ++i) {
        if (multiLineElements_[i]->id() == id) {
            return static_cast<std::uint8_t>(i + 1U);
        }
    }
    return 0;
}

std::optional<ElementRegistry::BlockMatch> ElementRegistry::matchBlock(std::u16string_view line, BlockState state,
                                                                       std::u16string_view nextLine) const noexcept {
    if (state.active() && state.element <= multiLineElements_.size()) {
        if (auto parsed = multiLineElements_[state.element - 1U]->parseWithNextLine(line, state.value, nextLine)) {
            return BlockMatch{state.element, *parsed};
        }
    }
    for (std::size_t i = 0; i < multiLineElements_.size(); ++i) {
        if (auto parsed = multiLineElements_[i]->parseWithNextLine(line, 0, nextLine)) {
            return BlockMatch{static_cast<std::uint8_t>(i + 1U), *parsed};
        }
    }
    return std::nullopt;
}

BlockState ElementRegistry::BlockMatch::stateAfter() const noexcept {
    return parsed.stateAfter == 0 ? BlockState{} : BlockState{element, parsed.stateAfter};
}

BlockState ElementRegistry::blockStateAfter(std::u16string_view line, BlockState state,
                                            std::u16string_view nextLine) const noexcept {
    return classify(line, state, nextLine).stateAfter();
}

ParagraphLink ElementRegistry::paragraphLink(std::u16string_view line, BlockState state,
                                             std::u16string_view nextLine) const noexcept {
    const Classified c = classify(line, state, nextLine);
    return {c.kind == Classified::Kind::Text && c.continues, c.underline};
}

BlockState ElementRegistry::documentStart() const noexcept {
    for (std::size_t i = 0; i < multiLineElements_.size(); ++i) {
        if (const std::uint32_t start = multiLineElements_[i]->documentStartState(); start != 0) {
            return {static_cast<std::uint8_t>(i + 1U), start};
        }
    }
    return {};
}

void ElementRegistry::runInlineElements(std::u16string_view line, const LineContext& context, LineRender& out) const {
    // One table lookup per character tells which inline elements this line can trigger;
    // most lines trigger none.
    std::uint64_t triggered = 0;
    bool nonAscii = false;
    for (const char16_t c : line) {
        if (c < asciiTriggers_.size()) {
            triggered |= asciiTriggers_[c];
        } else {
            nonAscii = true;
        }
    }
    for (std::size_t i = 0; i < inlineElements_.size(); ++i) {
        const auto& entry = inlineElements_[i];
        const bool run = entry.always || ((triggered >> i) & 1U) != 0 ||
                         (nonAscii && hasOtherTriggers_ && !entry.otherTriggers.empty() &&
                          line.find_first_of(entry.otherTriggers) != std::u16string_view::npos);
        if (run) {
            entry.element->render(line, context, out);
        }
    }
}

LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context) const {
    ParagraphLink link;
    return render(line, context, link);
}

LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context, ParagraphLink& link) const {
    using Kind = Classified::Kind;
    LineRender out;
    const Classified c = classify(line, context.blockState, context.nextLine);
    link = {c.kind == Kind::Text && c.continues, c.underline};
    out.blockState = c.stateAfter();
    switch (c.kind) {
        case Kind::Block:
            multiLineElements_[c.block->element - 1U]->render(line, c.block->parsed, context, out);
            break;
        case Kind::Underline:
            out.setextUnderline = c.underline;
            renderMarkerLine(line, context.revealed, out);
            break;
        case Kind::Break:
            out.thematicBreak = true;
            renderMarkerLine(line, context.revealed, out);
            break;
        case Kind::Element:
        case Kind::Other:
        case Kind::Text: {
            if (c.element != nullptr) {
                c.element->render(line, c.match, context, out);
            }
            // Paragraph text of a setext heading (4.3): every line of the paragraph, from where its
            // text starts (after a list item's marker).
            const auto length = static_cast<std::uint32_t>(line.size());
            if (context.setextLevel != 0 && out.blockState.underlinable() && c.kind != Kind::Other) {
                out.headingLevel = context.setextLevel;
                if (length > c.textStart) {
                    out.runs.push_back({{c.textStart, length - c.textStart}, Role::HeadingText, context.setextLevel});
                }
            }
            const std::size_t firstInlineRun = out.runs.size();
            runInlineElements(line, context, out);
            splitAroundInlineRuns(out, firstInlineRun);
            break;
        }
    }
    carveRunsAroundHidden(out);
    carveInlineStylesAroundHidden(out);
    return out;
}

std::vector<LineRender> ElementRegistry::renderDocument(std::span<const std::u16string> lines,
                                                        LineRange revealed) const {
    // ADR 0006: the reference for the lazy rendering in the UI. States from above, front matter
    // only if closed, setext levels from below, an underline revealed from any line of its heading.
    const std::size_t count = lines.size();
    const auto nextOf = [&](std::size_t i) {
        return i + 1 < count ? std::u16string_view{lines[i + 1]} : std::u16string_view{};
    };
    BlockState start = documentStart();
    if (start.active()) {
        BlockState state = start;
        bool closed = false;
        for (std::size_t i = 0; i < count && state.element == start.element; ++i) {
            state = blockStateAfter(lines[i], state, nextOf(i));
            closed = i > 0 && state.element != start.element;
            if (i == 0 && state.element != start.element) {
                break;  // the first line opens no block
            }
        }
        start = closed ? start : BlockState{};
    }
    std::vector<BlockState> before(count);
    std::vector<ParagraphLink> links(count);
    BlockState state = start;
    for (std::size_t i = 0; i < count; ++i) {
        before[i] = state;
        links[i] = paragraphLink(lines[i], state, nextOf(i));
        state = blockStateAfter(lines[i], state, nextOf(i));
    }
    std::vector<std::uint8_t> levels(count, 0);
    for (std::size_t i = count; i-- > 0;) {
        const bool paragraph = i + 1 < count && before[i + 1].underlinable();
        if (paragraph) {
            levels[i] = links[i + 1].underline != 0 ? links[i + 1].underline
                        : links[i + 1].continues    ? levels[i + 1]
                                                    : 0;
        }
    }
    std::vector<bool> shown(count, false);
    std::size_t paragraphStart = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!links[i].continues && links[i].underline == 0) {
            paragraphStart = i;
        }
        shown[i] = revealed.contains(static_cast<std::int64_t>(i));
        if (links[i].underline != 0) {
            // The underline is revealed while the caret is on any line of its heading.
            for (std::size_t k = paragraphStart; k < i && !shown[i]; ++k) {
                shown[i] = revealed.contains(static_cast<std::int64_t>(k));
            }
        }
    }
    std::vector<LineRender> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(render(lines[i], {shown[i], before[i], nextOf(i), levels[i]}));
    }
    // S021: a range in a code block reveals the opening fence of the block that holds its first
    // line and the closing fence of the block that holds its last line. Every line of a block
    // leaves the same state; an opening fence leaves the block open, a closing one closes it.
    const auto revealFence = [&](std::int64_t line, bool opening) {
        if (line < 0 || static_cast<std::size_t>(line) >= count || !out[static_cast<std::size_t>(line)].codeBlock) {
            return;
        }
        const auto at = static_cast<std::size_t>(line);
        const bool opens = out[at].fenceLine && out[at].blockState.active();
        const bool closes = out[at].fenceLine && !out[at].blockState.active();
        const BlockState inside = opens ? out[at].blockState : before[at];
        std::size_t fence = at;
        if (opening && !opens) {
            while (fence > 0 && out[fence - 1].blockState == inside) {
                --fence;
            }
        } else if (!opening && !closes) {
            while (fence < count && out[fence].blockState == inside) {
                ++fence;
            }
            if (fence >= count) {
                return;  // the block runs to the end of the document
            }
        }
        if (!shown[fence]) {
            out[fence] = render(lines[fence], {true, before[fence], nextOf(fence), levels[fence]});
        }
    };
    if (!revealed.empty()) {
        revealFence(revealed.first, true);
        revealFence(revealed.last, false);
    }
    return out;
}

ElementRegistry ElementRegistry::withBuiltins() {
    ElementRegistry registry;
    registry.addMultiLine(makeFencedCodeElement());
    registry.addMultiLine(makeTableElement());
    registry.addMultiLine(makeFrontMatterElement());
    registry.add(makeHeadingElement());
    registry.add(makeBulletElement());
    registry.addInline(makeInlineCodeElement());
    registry.addInline(makeEmphasisElement());
    registry.addInline(makeEscapeElement());
    return registry;
}

}  // namespace hitokoto::core
