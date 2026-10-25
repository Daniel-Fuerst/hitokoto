// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

#include <algorithm>

namespace hitokoto::core {

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

LineRender ElementRegistry::render(std::u16string_view line, const LineContext& context) const {
    LineRender out;
    for (const auto& element : elements_) {
        if (auto match = element->parse(line)) {
            element->render(line, *match, context, out);
            break;
        }
    }
    for (const auto& element : inlineElements_) {
        element->render(line, context, out);
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
    registry.addInline(makeEmphasisElement());
    return registry;
}

}  // namespace hitokoto::core
