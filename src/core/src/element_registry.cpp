// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include <algorithm>

namespace hitokoto::core {
namespace {

struct FenceRun {
    char16_t marker = 0;
    std::uint32_t length = 0;
};

FenceRun fenceRun(std::u16string_view line) {
    std::size_t start = 0;
    while (start < line.size() && line[start] == u' ' && start < 4) {
        ++start;
    }
    if (start > 3 || start == line.size() || (line[start] != u'`' && line[start] != u'~')) {
        return {};
    }
    const char16_t marker = line[start];
    std::size_t end = start;
    while (end < line.size() && line[end] == marker) {
        ++end;
    }
    const auto length = end - start;
    if (length < 3) {
        return {};
    }
    return {marker, static_cast<std::uint32_t>(length)};
}

bool hasOnlyWhitespace(std::u16string_view text) {
    return std::all_of(text.begin(), text.end(), [](char16_t c) { return c == u' ' || c == u'\t'; });
}

bool validOpeningFence(std::u16string_view line, FenceRun run) {
    std::size_t start = 0;
    while (start < line.size() && line[start] == u' ') {
        ++start;
    }
    const auto info = line.substr(start + run.length);
    return run.marker != u'`' || info.find(u'`') == std::u16string_view::npos;
}

bool isClosingFence(std::u16string_view line, FenceState state) {
    const auto run = fenceRun(line);
    if (run.marker != state.marker || run.length < state.length) {
        return false;
    }
    std::size_t start = 0;
    while (start < line.size() && line[start] == u' ') {
        ++start;
    }
    return hasOnlyWhitespace(line.substr(start + run.length));
}

void addCodeLine(std::u16string_view line, const LineContext& context, LineRender& out) {
    out.codeBlock = true;
    out.runs.push_back({{0, static_cast<std::uint32_t>(line.size())}, Role::CodeBlock, 0});
    if (out.fenceLine && !context.revealed && !line.empty()) {
        out.hidden.push_back({{0, static_cast<std::uint32_t>(line.size())}, HideMode::Collapse});
    }
}

}  // namespace

void ElementRegistry::add(std::unique_ptr<LineElement> element) {
    if (element) {
        elements_.push_back(std::move(element));
    }
}

void ElementRegistry::addInline(std::unique_ptr<InlineElement> element) {
    if (element) {
        inlineElements_.push_back(std::move(element));
    }
}

FenceState ElementRegistry::fenceStateAfter(std::u16string_view line, FenceState state) const noexcept {
    if (state.active()) {
        return isClosingFence(line, state) ? FenceState{} : state;
    }
    const auto opening = fenceRun(line);
    return opening.marker != 0 && validOpeningFence(line, opening) ? FenceState{opening.marker, opening.length}
                                                                   : FenceState{};
}

LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context) const {
    LineRender out;
    out.fenceState = context.fenceState;
    if (context.fenceState.active()) {
        out.codeBlock = true;
        if (isClosingFence(line, context.fenceState)) {
            out.fenceLine = true;
            out.fenceState = {};
            addCodeLine(line, context, out);
        } else {
            addCodeLine(line, context, out);
        }
        return out;
    }
    const auto opening = fenceRun(line);
    if (opening.marker != 0 && validOpeningFence(line, opening)) {
        out.fenceLine = true;
        out.fenceState = {opening.marker, opening.length};
        addCodeLine(line, context, out);
        return out;
    }
    for (const auto& element : elements_) {
        if (auto match = element->parse(line)) {
            element->render(line, *match, context, out);
            break;
        }
    }
    for (const auto& element : inlineElements_) {
        element->render(line, context, out);
    }

    std::vector<StyleRun> codeRuns;
    for (const auto& run : out.runs) {
        if (run.role == Role::InlineCode) {
            codeRuns.push_back(run);
        }
    }
    if (!codeRuns.empty()) {
        std::vector<StyleRun> splitRuns;
        for (const auto& run : out.runs) {
            if (run.role == Role::InlineCode) {
                continue;
            }
            std::uint32_t start = run.span.start;
            for (const auto& code : codeRuns) {
                if (code.span.end() <= start || code.span.start >= run.span.end()) {
                    continue;
                }
                if (code.span.start > start) {
                    splitRuns.push_back({{start, code.span.start - start}, run.role, run.level});
                }
                start = std::max(start, code.span.end());
            }
            if (start < run.span.end()) {
                splitRuns.push_back({{start, run.span.end() - start}, run.role, run.level});
            }
        }
        splitRuns.insert(splitRuns.end(), codeRuns.begin(), codeRuns.end());
        std::sort(splitRuns.begin(), splitRuns.end(),
                  [](const StyleRun& left, const StyleRun& right) { return left.span.start < right.span.start; });
        out.runs = std::move(splitRuns);
    }

    std::sort(out.hidden.begin(), out.hidden.end(),
              [](const HiddenRange& left, const HiddenRange& right) { return left.span.start < right.span.start; });
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
    return out;
}

ElementRegistry ElementRegistry::withBuiltins() {
    ElementRegistry registry;
    registry.add(makeHeadingElement());
    registry.add(makeBulletElement());
    registry.addInline(makeInlineCodeElement());
    registry.addInline(makeEmphasisElement());
    return registry;
}

}  // namespace hitokoto::core
