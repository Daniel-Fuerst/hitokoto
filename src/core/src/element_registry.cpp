// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

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

/// Runs of markers and inline code inside link text take precedence over LinkText runs.
void carveLinkTextAroundOtherRuns(LineRender& out) {
    const bool hasLinkText =
        std::any_of(out.runs.begin(), out.runs.end(), [](const StyleRun& run) { return run.role == Role::LinkText; });
    if (!hasLinkText) {
        return;
    }
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
    std::vector<StyleRun> splitRuns;
    for (const auto& run : linkTextRuns) {
        std::uint32_t start = run.span.start;
        for (const auto& other : otherRuns) {
            if (other.span.end() <= start || other.span.start >= run.span.end()) {
                continue;
            }
            if (other.span.start > start) {
                splitRuns.push_back({{start, other.span.start - start}, run.role, run.level});
            }
            start = std::max(start, other.span.end());
        }
        if (start < run.span.end()) {
            splitRuns.push_back({{start, run.span.end() - start}, run.role, run.level});
        }
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

}  // namespace

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
    const auto block = matchBlock(line, state, nextLine);
    return block ? block->stateAfter() : BlockState{};
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
    LineRender out;
    if (const auto block = matchBlock(line, context.blockState, context.nextLine)) {
        const auto& multi = multiLineElements_[block->element - 1U];
        multi->render(line, block->parsed, context, out);
        out.blockState = block->stateAfter();
    } else {
        for (const auto& element : elements_) {
            if (auto match = element->parse(line)) {
                element->render(line, *match, context, out);
                break;
            }
        }
        const std::size_t firstInlineRun = out.runs.size();
        runInlineElements(line, context, out);
        splitAroundInlineRuns(out, firstInlineRun);
        carveLinkTextAroundOtherRuns(out);
    }
    carveRunsAroundHidden(out);
    carveInlineStylesAroundHidden(out);
    return out;
}

ElementRegistry ElementRegistry::withBuiltins() {
    ElementRegistry registry;
    registry.addMultiLine(makeFencedCodeElement());
    registry.addMultiLine(makeTableElement());
    registry.add(makeHeadingElement());
    registry.add(makeBulletElement());
    registry.addInline(makeLinkElement());
    registry.addInline(makeInlineCodeElement());
    registry.addInline(makeEmphasisElement());
    registry.addInline(makeEscapeElement());
    return registry;
}

}  // namespace hitokoto::core
