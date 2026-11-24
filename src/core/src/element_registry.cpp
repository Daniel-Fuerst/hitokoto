// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include "block_rules.hpp"
#include "inline_steps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace hitokoto::core {
namespace {

thread_local std::uint64_t inlineStepCount = 0;

/// The ranges of a line that cut runs apart (hidden ranges under styled runs, inline runs under a
/// block's runs), prepared so that cutting one run costs what the run meets, not a pass over all
/// ranges: with a few thousand runs and as many ranges, as in a line of `a*a*a*`, comparing each
/// run with each range made rendering the line quadratic.
///
/// The ranges are joined where they overlap; ranges that only touch stay apart, and an empty range
/// stays a place where a run is split. What `pieces` and `meets` answer is what a pass over all
/// ranges in the order of their starts answered.
class Cuts {
public:
    /// `ranges` in the order of their starts; `spanOf` gives the span of one.
    template <typename Ranges, typename SpanOf>
    Cuts(const Ranges& ranges, SpanOf spanOf, detail::InlineSteps& steps) : steps_(steps) {
        // Most lines have a handful of ranges: those need no allocation.
        if (ranges.size() > few_.size()) {
            many_.resize(ranges.size());
            joined_ = many_.data();
        }
        steps_.add(ranges.size());
        for (const auto& range : ranges) {
            const Span span = spanOf(range);
            if (count_ > 0 && span.start < joined_[count_ - 1].end()) {
                Span& last = joined_[count_ - 1];
                last.length = std::max(last.end(), span.end()) - last.start;
            } else {
                joined_[count_++] = span;
            }
        }
    }
    Cuts(const Cuts&) = delete;
    Cuts& operator=(const Cuts&) = delete;

    /// Whether a range and `run` share a character, or an empty range lies inside `run`, or `run` is
    /// empty and lies inside a range.
    [[nodiscard]] bool meets(Span run) const {
        const Span* first = firstEndingAfter(run.start);
        return first != end() && first->start < run.end();
    }

    /// Calls `piece(start, length)` for each part of `run` that no range covers, left to right.
    template <typename Piece>
    void pieces(Span run, Piece piece) const {
        std::uint32_t start = run.start;
        for (const Span* cut = firstEndingAfter(run.start); cut != end() && cut->start < run.end(); ++cut) {
            steps_.add();
            if (cut->end() <= start) {
                continue;
            }
            if (cut->start > start) {
                piece(start, cut->start - start);
            }
            start = std::max(start, cut->end());
        }
        if (start < run.end()) {
            piece(start, run.end() - start);
        }
    }

private:
    [[nodiscard]] const Span* end() const noexcept { return joined_ + count_; }

    /// The first range that ends after `position`; the joined ranges end in the order they start.
    [[nodiscard]] const Span* firstEndingAfter(std::uint32_t position) const {
        steps_.add();
        return std::partition_point(static_cast<const Span*>(joined_), end(),
                                    [position](const Span& cut) { return cut.end() <= position; });
    }

    std::array<Span, 16> few_{};
    std::vector<Span> many_;
    Span* joined_ = few_.data();
    std::size_t count_ = 0;
    detail::InlineSteps& steps_;
};

/// Runs of inline elements (every run appended after the block element ran, from index
/// `firstInlineRun` on) take precedence over the block element's runs on the same characters.
/// The registry tells them apart by position, so a new inline element needs no new role here.
void splitAroundInlineRuns(LineRender& out, std::size_t firstInlineRun) {
    if (firstInlineRun >= out.runs.size()) {
        return;  // no inline runs
    }
    detail::InlineSteps steps;
    std::vector<StyleRun> inlineRuns(out.runs.begin() + static_cast<std::ptrdiff_t>(firstInlineRun), out.runs.end());
    std::sort(inlineRuns.begin(), inlineRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    steps.add(out.runs.size());
    std::vector<StyleRun> splitRuns;
    if (firstInlineRun > 0) {
        const Cuts cuts(inlineRuns, [](const StyleRun& run) { return run.span; }, steps);
        for (std::size_t i = 0; i < firstInlineRun; ++i) {
            const auto& run = out.runs[i];
            cuts.pieces(run.span, [&](std::uint32_t start, std::uint32_t length) {
                splitRuns.push_back({{start, length}, run.role, run.level});
            });
        }
    }
    splitRuns.insert(splitRuns.end(), inlineRuns.begin(), inlineRuns.end());
    std::sort(splitRuns.begin(), splitRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    out.runs = std::move(splitRuns);
}

/// Runs of markers and inline code inside link text take precedence over LinkText runs.
void carveLinkTextAroundOtherRuns(LineRender& out) {
    if (out.runs.size() <= 1) {
        return;
    }
    const bool hasLinkText =
        std::any_of(out.runs.begin(), out.runs.end(), [](const StyleRun& run) { return run.role == Role::LinkText; });
    if (!hasLinkText) {
        return;
    }
    detail::InlineSteps steps;
    steps.add(2 * out.runs.size());
    std::vector<StyleRun> otherRuns;
    std::vector<StyleRun> linkTextRuns;
    for (const auto& run : out.runs) {
        if (run.role == Role::LinkText) {
            linkTextRuns.push_back(run);
        } else {
            otherRuns.push_back(run);
        }
    }
    std::sort(otherRuns.begin(), otherRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    const Cuts cuts(otherRuns, [](const StyleRun& run) { return run.span; }, steps);
    std::vector<StyleRun> splitRuns;
    for (const auto& run : linkTextRuns) {
        cuts.pieces(run.span, [&](std::uint32_t start, std::uint32_t length) {
            splitRuns.push_back({{start, length}, run.role, run.level});
        });
    }
    splitRuns.insert(splitRuns.end(), otherRuns.begin(), otherRuns.end());
    std::sort(splitRuns.begin(), splitRuns.end(),
              [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
    out.runs = std::move(splitRuns);
}

/// LineRender contract: styled runs and hidden ranges never overlap; hidden ranges win.
void carveRunsAroundHidden(LineRender& out) {
    if (out.hidden.empty()) {
        return;
    }
    detail::InlineSteps steps;
    steps.add(out.hidden.size() + out.runs.size());
    if (out.hidden.size() > 1) {
        const auto byStart = [](const HiddenRange& left, const HiddenRange& right) {
            return left.span.start < right.span.start;
        };
        if (!std::is_sorted(out.hidden.begin(), out.hidden.end(), byStart)) {
            std::sort(out.hidden.begin(), out.hidden.end(), byStart);
        }
    }
    if (out.runs.empty()) {
        return;
    }
    const Cuts cuts(out.hidden, [](const HiddenRange& hidden) { return hidden.span; }, steps);
    const bool overlap =
        std::any_of(out.runs.begin(), out.runs.end(), [&](const StyleRun& run) { return cuts.meets(run.span); });
    if (!overlap) {
        return;  // the common case: nothing to carve, nothing to allocate
    }
    std::vector<StyleRun> visibleRuns;
    for (const auto& run : out.runs) {
        cuts.pieces(run.span, [&](std::uint32_t start, std::uint32_t length) {
            visibleRuns.push_back({{start, length}, run.role, run.level});
        });
    }
    out.runs = std::move(visibleRuns);
}

void carveInlineStylesAroundHidden(LineRender& out) {
    if (out.hidden.empty() || out.inlineStyles.empty()) {
        return;
    }
    detail::InlineSteps steps;
    steps.add(out.hidden.size() + out.inlineStyles.size());
    const auto byStart = [](const HiddenRange& left, const HiddenRange& right) {
        return left.span.start < right.span.start;
    };
    if (!std::is_sorted(out.hidden.begin(), out.hidden.end(), byStart)) {
        std::sort(out.hidden.begin(), out.hidden.end(), byStart);
    }
    const Cuts cuts(out.hidden, [](const HiddenRange& hidden) { return hidden.span; }, steps);
    const bool overlap = std::any_of(out.inlineStyles.begin(), out.inlineStyles.end(),
                                     [&](const InlineStyleRun& run) { return cuts.meets(run.span); });
    if (!overlap) {
        return;
    }
    std::vector<InlineStyleRun> visibleStyles;
    for (const auto& run : out.inlineStyles) {
        cuts.pieces(run.span, [&](std::uint32_t start, std::uint32_t length) {
            visibleStyles.push_back({{start, length}, run.style});
        });
    }
    out.inlineStyles = std::move(visibleStyles);
}

/// Columns of indentation past a paragraph's content column from which a line is indented code,
/// which cannot interrupt a paragraph (CommonMark 4.4).
constexpr std::uint32_t kIndentedCode = 4;

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

void detail::addInlineSteps(std::uint64_t steps) noexcept {
    inlineStepCount += steps;
}

std::uint64_t inlineSteps() noexcept {
    return inlineStepCount;
}

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
    const std::uint32_t indent = blocks::indentWidth(line);
    const auto continueParagraph = [&] {
        c.kind = Kind::Text;
        c.continues = true;
        c.paragraphAfter = above;
        return c;
    };
    const bool blank = blocks::firstNonBlank(line) == line.size();
    if (!blank && state.underlinable() && indent >= state.paragraph - 1U + kIndentedCode) {
        return continueParagraph();  // nothing starts this far past the content (CommonMark 4.4)
    }
    // A line less indented than the paragraph's content column can only continue it lazily.
    const bool lazy = above == BlockState::kLazyParagraph || (above != 0 && indent + 1U < above);
    if (blocks::isThematicBreak(line)) {
        c.kind = Kind::Break;  // takes precedence over a list item (4.1, example 60)
        return c;
    }
    for (const auto& element : elements_) {
        if (auto match = element->parse(line)) {
            if (above != 0 && !element->startsAfterParagraph(line, *match, lazy)) {
                return continueParagraph();  // e.g. an empty list item (5.2)
            }
            c.kind = Kind::Element;
            c.element = element.get();
            c.match = *match;
            const ParagraphRole role = element->paragraphRole(line, *match);
            c.paragraphAfter = role.kind == ParagraphRole::Kind::Opens      ? paragraphAt(role.column)
                               : role.kind == ParagraphRole::Kind::LazyText ? BlockState::kLazyParagraph
                                                                            : std::uint16_t{0};
            c.textStart = match->marker.end();
            return c;
        }
    }
    if (blank) {
        return c;
    }
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
        if (entry.always) {
            hasAlwaysInline_ = true;
        }
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
    if (line.empty() && !hasAlwaysInline_) {
        return;
    }
    // One table lookup per character tells which inline elements this line can trigger;
    // most lines trigger none.
    // S007: a line above either bound shows as text; the same pass over the line counts the
    // characters that can start inline syntax.
    if (line.size() > inlineBounds_.lineLength) {
        return;
    }
    std::uint64_t triggered = 0;
    bool nonAscii = false;
    std::size_t syntaxCharacters = 0;
    for (const char16_t c : line) {
        if (c < asciiTriggers_.size()) {
            const std::uint64_t elements = asciiTriggers_[c];
            triggered |= elements;
            syntaxCharacters += elements != 0 ? 1U : 0U;
        } else {
            nonAscii = true;
        }
    }
    if (nonAscii && hasOtherTriggers_) {
        for (const auto& entry : inlineElements_) {
            for (const char16_t trigger : entry.otherTriggers) {
                syntaxCharacters += static_cast<std::size_t>(std::count(line.begin(), line.end(), trigger));
            }
        }
    }
    if (syntaxCharacters > inlineBounds_.syntaxCharacters) {
        return;
    }
    if (triggered == 0 && !hasAlwaysInline_ && (!nonAscii || !hasOtherTriggers_)) {
        return;
    }
    detail::InlineSteps steps;
    InlineScan scan(line.size());
    for (std::size_t i = 0; i < inlineElements_.size(); ++i) {
        const auto& entry = inlineElements_[i];
        const bool run = entry.always || ((triggered >> i) & 1U) != 0 ||
                         (nonAscii && hasOtherTriggers_ && !entry.otherTriggers.empty() &&
                          line.find_first_of(entry.otherTriggers) != std::u16string_view::npos);
        if (run) {
            steps.add(scan.update(out));
            entry.element->render(line, context, scan, out);
        }
    }
}

std::size_t InlineScan::update(const LineRender& out) {
    std::size_t marked = 0;
    const auto mark = [this, &marked](Span span, std::uint8_t flag) {
        const std::size_t end = std::min<std::size_t>(span.end(), flags_.size());
        for (std::size_t i = span.start; i < end; ++i) {
            flags_[i] |= flag;
        }
        marked += end > span.start ? end - span.start : 0;
    };
    for (; opaqueSeen_ < out.opaqueSpans.size(); ++opaqueSeen_) {
        mark(out.opaqueSpans[opaqueSeen_], kOpaque);
    }
    for (; hiddenSeen_ < out.hidden.size(); ++hiddenSeen_) {
        mark(out.hidden[hiddenSeen_].span, kHidden);
    }
    return marked;
}

LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context) const {
    ParagraphLink link;
    return render(line, context, link);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): above the threshold when the check came in
LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context, ParagraphLink& link) const {
    using Kind = Classified::Kind;
    LineRender out;
    const Classified c = classify(line, context.blockState, context.nextLine);
    link = {c.kind == Kind::Text && c.continues, c.underline};
    out.blockState = c.stateAfter();
    switch (c.kind) {
        case Kind::Block:
            if (c.block) {
                multiLineElements_[c.block->element - 1U]->render(line, c.block->parsed, context, out);
            }
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
            if (out.blockState.inParagraph()) {
                const bool nextContinues =
                    !context.nextLine.empty() &&
                    paragraphLink(context.nextLine, out.blockState, context.nextNextLine).continues;
                if (c.continues && !context.paragraphRevealed && out.blockState.underlinable()) {
                    out.wrapIndent = static_cast<std::uint32_t>(out.blockState.paragraph - 1U);
                }
                if ((c.continues || nextContinues) && !context.paragraphRevealed && c.kind == Kind::Text) {
                    const auto firstNb = static_cast<std::uint32_t>(blocks::firstNonBlank(line));
                    if (firstNb > 0) {
                        out.hidden.push_back({{0, firstNb}, HideMode::Collapse});
                    }
                }
                if (nextContinues) {
                    const auto contentFirst =
                        std::max<std::uint32_t>(c.textStart, static_cast<std::uint32_t>(blocks::firstNonBlank(line)));
                    const auto inSpan = [](const std::vector<Span>& spans, std::uint32_t pos) {
                        return std::any_of(spans.begin(), spans.end(),
                                           [pos](const Span& s) { return pos >= s.start && pos < s.end(); });
                    };
                    const auto inHidden = [](const std::vector<HiddenRange>& ranges, std::uint32_t pos) {
                        return std::any_of(ranges.begin(), ranges.end(), [pos](const HiddenRange& h) {
                            return pos >= h.span.start && pos < h.span.end();
                        });
                    };
                    bool backslashHardBreak = false;
                    if (length > contentFirst && line[length - 1] == u'\\') {
                        const std::uint32_t last = length - 1;
                        if (!inSpan(out.opaqueSpans, last) && !inHidden(out.hidden, last)) {
                            // The backslashes at the end of the line, back to the last character
                            // that an opaque span holds: one pass over the backslashes and one over
                            // the spans, not the spans once for every backslash.
                            std::uint32_t runStart = length;
                            while (runStart > contentFirst && line[runStart - 1] == u'\\') {
                                --runStart;
                            }
                            std::uint32_t counted = runStart;
                            for (const Span& span : out.opaqueSpans) {
                                if (span.length > 0 && span.start < length && span.end() > runStart) {
                                    counted = std::max(counted, std::min(span.end(), length));
                                }
                            }
                            detail::addInlineSteps(std::uint64_t{length} - runStart + out.opaqueSpans.size());
                            const std::uint32_t bsRun = length - counted;
                            backslashHardBreak = (bsRun % 2U) == 1U;
                        }
                    }
                    if (backslashHardBreak) {
                        const Span bsSpan{length - 1, 1};
                        out.runs.push_back({bsSpan, Role::EscapeMarker, 0});
                        if (!context.paragraphRevealed) {
                            out.hardBreak = true;
                            out.hidden.push_back({bsSpan, HideMode::Collapse});
                        }
                    } else if (!context.paragraphRevealed) {
                        std::uint32_t spaceStart = length;
                        while (spaceStart > contentFirst && line[spaceStart - 1] == u' ') {
                            --spaceStart;
                        }
                        const std::uint32_t trailingSpaces = length - spaceStart;
                        if (trailingSpaces >= 2) {
                            out.hardBreak = true;
                            out.hidden.push_back({{spaceStart, trailingSpaces}, HideMode::Collapse});
                        } else {
                            out.flowsToNext = true;
                            std::uint32_t blankStart = spaceStart;
                            while (blankStart > contentFirst &&
                                   (line[blankStart - 1] == u' ' || line[blankStart - 1] == u'\t')) {
                                --blankStart;
                            }
                            if (blankStart < length) {
                                out.hidden.push_back({{blankStart, length - blankStart}, HideMode::Collapse});
                            }
                        }
                    }
                }
            }
            splitAroundInlineRuns(out, firstInlineRun);
            carveLinkTextAroundOtherRuns(out);
            break;
        }
    }
    carveRunsAroundHidden(out);
    carveInlineStylesAroundHidden(out);
    return out;
}

LineRender ElementRegistry::renderInline(std::u16string_view text) const {
    LineRender out;
    runInlineElements(text, {}, out);
    splitAroundInlineRuns(out, 0);
    carveLinkTextAroundOtherRuns(out);
    carveRunsAroundHidden(out);
    carveInlineStylesAroundHidden(out);
    return out;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): above the threshold when the check came in
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
    const auto nextNextOf = [&](std::size_t i) {
        return i + 2 < count ? std::u16string_view{lines[i + 2]} : std::u16string_view{};
    };
    std::vector<bool> shown(count, false);
    std::vector<bool> paraShown(count, false);
    std::size_t paragraphStart = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!links[i].continues && links[i].underline == 0) {
            paragraphStart = i;
        }
        shown[i] = revealed.contains(static_cast<std::int64_t>(i));
        if (shown[i]) {
            for (std::size_t k = paragraphStart; k <= i; ++k) {
                paraShown[k] = true;
            }
        } else if (paraShown[paragraphStart]) {
            paraShown[i] = true;
        }
        if (links[i].underline != 0 && paraShown[paragraphStart]) {
            // The underline is revealed while the caret is on any line of its heading.
            shown[i] = true;
        }
    }
    std::vector<LineRender> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(render(lines[i], {shown[i], before[i], nextOf(i), levels[i], paraShown[i], nextNextOf(i)}));
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
    registry.addMultiLine(makeDisplayMathElement());
    registry.add(makeHeadingElement());
    registry.add(makeBulletElement());
    registry.addInline(makeLinkElement());
    registry.addInline(makeInlineCodeElement());
    registry.addInline(makeInlineMathElement());
    registry.addInline(makeEmphasisElement());
    registry.addInline(makeEscapeElement());
    return registry;
}

}  // namespace hitokoto::core
