// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/markdown.hpp"

namespace hitokoto::core {

void ElementRegistry::add(std::unique_ptr<LineElement> element) {
    if (element) {
        elements_.push_back(std::move(element));
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
    return out;
}

ElementRegistry ElementRegistry::withBuiltins() {
    ElementRegistry registry;
    registry.add(makeHeadingElement());
    registry.add(makeBulletElement());
    return registry;
}

}  // namespace hitokoto::core
