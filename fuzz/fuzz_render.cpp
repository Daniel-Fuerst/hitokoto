// SPDX-License-Identifier: GPL-3.0-or-later
// Any text renders to well-formed spans, line by line, revealed or not, with the fence state carried
// from line to line as the editor does. Spans stay inside the line, hidden and styled ranges never
// overlap, a revealed line hides nothing, and the registry's fence-state shortcut agrees with render().
#include "hitokoto/core/markdown.hpp"

#include <cstdlib>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const auto registry = hitokoto::core::ElementRegistry::withBuiltins();
    std::u16string text(size / 2, u' ');
    for (std::size_t i = 0; i < text.size(); ++i) {
        text[i] = static_cast<char16_t>(data[2 * i] | (data[2 * i + 1] << 8));
    }
    hitokoto::core::FenceState state;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find(u'\n', start);
        const std::u16string_view line =
            std::u16string_view(text).substr(start, nl == std::u16string::npos ? std::u16string::npos : nl - start);
        for (bool revealed : {false, true}) {
            const auto r = registry.render(line, {revealed, state});
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
            for (const auto& s : r.inlineStyles) {
                if (s.span.end() > line.size())
                    std::abort();
            }
            for (const auto& d : r.decorations) {
                if (d.column >= line.size())
                    std::abort();
            }
            for (int c : cover) {
                if (c > 1)
                    std::abort();
            }
            if (r.fenceState != registry.fenceStateAfter(line, state))
                std::abort();
        }
        state = registry.fenceStateAfter(line, state);
        if (nl == std::u16string::npos)
            break;
        start = nl + 1;
    }
    return 0;
}
