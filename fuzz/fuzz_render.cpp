// SPDX-License-Identifier: GPL-3.0-or-later
// Any text renders to well-formed spans, line by line, revealed or not, as a setext heading line or
// not, with the block state carried from line to line as the editor does. Spans stay inside the
// line, hidden and styled ranges never overlap, a revealed non-table line hides nothing, and the
// registry's block-state shortcut agrees with render(). The whole-document reference (S023, ADR
// 0006) renders the same text with the same guarantees and the same block states. The YAML of
// closed front matter, and the whole input as lines, go to the properties parser (S024, ADR 0007):
// it returns, and every line and column it reports lies in its input.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/properties.hpp"

#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

/// What the parser reports lies in `lines` (S024).
void checkProperties(std::span<const std::u16string> lines) {
    const auto p = hitokoto::core::parseProperties(lines);
    if (p.error) {
        if (!p.rows.empty() || (p.error->line >= lines.size() && !lines.empty()))
            std::abort();
        return;
    }
    for (const auto& row : p.rows) {
        if (row.keyLine >= lines.size() || row.valueLine >= lines.size() ||
            row.valueColumn > lines[row.valueLine].size())
            std::abort();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const auto registry = hitokoto::core::ElementRegistry::withBuiltins();
    std::u16string text(size / 2, u' ');
    for (std::size_t i = 0; i < text.size(); ++i) {
        text[i] = static_cast<char16_t>(data[2 * i] | (data[2 * i + 1] << 8));
    }
    hitokoto::core::BlockState state;
    std::vector<std::u16string> lines;
    std::vector<hitokoto::core::BlockState> states;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find(u'\n', start);
        const std::u16string_view line =
            std::u16string_view(text).substr(start, nl == std::u16string::npos ? std::u16string::npos : nl - start);
        std::u16string_view nextLine{};
        if (nl != std::u16string::npos) {
            const auto nextNl = text.find(u'\n', nl + 1);
            nextLine = std::u16string_view(text).substr(
                nl + 1, nextNl == std::u16string::npos ? std::u16string::npos : nextNl - (nl + 1));
        }
        lines.emplace_back(line);
        states.push_back(state);
        for (const auto [revealed, setext] : {std::pair{false, 0}, std::pair{true, 0}, std::pair{false, 2}}) {
            const auto r = registry.render(line, {revealed, state, nextLine, static_cast<std::uint8_t>(setext)});
            std::vector<int> cover(line.size(), 0);
            for (const auto& run : r.runs) {
                if (run.span.end() > line.size())
                    std::abort();
                for (auto k = run.span.start; k < run.span.end(); ++k)
                    ++cover[k];
            }
            for (const auto& h : r.hidden) {
                if ((!r.table && revealed) || h.span.end() > line.size())
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
            if (r.blockState != registry.blockStateAfter(line, state, nextLine))
                std::abort();
        }
        state = registry.blockStateAfter(line, state, nextLine);
        if (nl == std::u16string::npos)
            break;
        start = nl + 1;
    }
    // The reference walks the same states (front matter aside, which only it confirms).
    const auto document = registry.renderDocument(lines, {0, 0});
    if (document.size() != lines.size())
        std::abort();
    const bool frontMatter = !lines.empty() && lines.front().starts_with(u"---");
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& r = document[i];
        for (const auto& run : r.runs) {
            if (run.span.end() > lines[i].size())
                std::abort();
        }
        for (const auto& h : r.hidden) {
            if (h.span.end() > lines[i].size())
                std::abort();
        }
        if (!frontMatter && i + 1 < lines.size() && r.blockState != states[i + 1])
            std::abort();
    }
    // S024: the YAML of closed front matter (lines 1 to before the closing one), and every line.
    std::size_t close = 0;
    while (close < document.size() && document[close].frontMatter)
        ++close;
    if (close >= 2)
        checkProperties(std::span<const std::u16string>(lines).subspan(1, close - 2));
    if (close == 1 || (close > 0 && !document[0].frontMatter))
        std::abort();
    checkProperties(lines);
    return 0;
}
