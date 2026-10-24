// SPDX-License-Identifier: GPL-3.0-or-later
// Any line renders to well-formed spans, revealed or not.
#include "hitokoto/core/markdown.hpp"

#include <cstdlib>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const auto registry = hitokoto::core::ElementRegistry::withBuiltins();
    std::u16string line(size / 2, u' ');
    for (std::size_t i = 0; i < line.size(); ++i) {
        line[i] = static_cast<char16_t>(data[2 * i] | (data[2 * i + 1] << 8));
    }
    for (bool revealed : {false, true}) {
        const auto r = registry.render(line, {revealed});
        std::vector<int> cover(line.size(), 0);
        for (const auto& run : r.runs) {
            if (run.span.end() > line.size())
                std::abort();
            for (auto k = run.span.start; k < run.span.end(); ++k)
                ++cover[k];
        }
        for (const auto& h : r.hidden) {
            if (revealed || h.span.end() > line.size())
                std::abort();
            for (auto k = h.span.start; k < h.span.end(); ++k)
                ++cover[k];
        }
        for (const auto& d : r.decorations) {
            if (d.column >= line.size())
                std::abort();
        }
        for (int c : cover) {
            if (c > 1)
                std::abort();
        }
    }
    return 0;
}
