// SPDX-License-Identifier: GPL-3.0-or-later
// Any text renders to well-formed spans, line by line, revealed or not, as a setext heading line or
// not, with the block state carried from line to line as the editor does. Spans stay inside the
// line, hidden and styled ranges never overlap, a revealed non-table line hides nothing, and the
// registry's block-state shortcut agrees with render(). The whole-document reference (S023, ADR
// 0006) renders the same text with the same guarantees and the same block states. The YAML of
// closed front matter, and the whole input as lines, go to the properties parser (S024, ADR 0007):
// it returns, and every line and column it reports lies in its input. What it reads, and the whole
// input, written back as a key, a value or a list item (S025) is one line that can be saved and
// reads back as it was. A line above the bounds of
// S007 shows no inline formatting; it is rendered once more by a registry without the bounds, with
// the same guarantees, so that the inline elements also see long lines (they render a line in
// linear time: a run with a large -max_len and a -timeout shows an input that does not).
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/properties.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <algorithm>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

/// `text` written by the writers of S025 is one line that can be saved, and reads back as it was.
void checkWritten(std::u16string_view text) {
    using namespace hitokoto::core;
    const auto readBack = [](std::u16string line) {
        const std::vector<std::u16string> lines{std::move(line)};
        return parseProperties(lines);
    };
    const std::u16string scalar = serializePropertyScalar(text);
    for (const char16_t c : scalar) {
        if (c == u'\n' || c == u'\r' || c == 0x85 || c == 0x2028 || c == 0x2029 || c == 0xFDD0 || c == 0xFDD1)
            std::abort();
    }
    if (findLoneSurrogate(scalar) != std::u16string_view::npos)
        std::abort();
    const auto value = readBack(u"k: " + scalar);
    if (value.error || value.rows.size() != 1 || value.rows.front().text != text)
        std::abort();
    const auto item = readBack(u"k: [" + scalar + u"]");
    if (item.error || item.rows.size() != 1 ||
        item.rows.front().items != std::vector<std::u16string>{std::u16string(text)})
        std::abort();
    if (!text.empty()) {
        const auto key = readBack(scalar + u": 1");
        if (key.error || key.rows.size() != 1 || key.rows.front().key != text)
            std::abort();
    }
    for (const auto type : {PropertyType::Number, PropertyType::Date, PropertyType::DateTime}) {
        const std::u16string written = serializePropertyValue(type, text);
        const auto typed = readBack(u"k: " + written);
        if (typed.error || typed.rows.size() != 1 || typed.rows.front().text != text ||
            (written != scalar && typed.rows.front().type != type))
            std::abort();
    }
}

/// What the parser reports lies in `lines` (S024).
void checkProperties(std::span<const std::u16string> lines) {
    const auto p = hitokoto::core::parseProperties(lines);
    if (p.error) {
        if (!p.rows.empty() || (p.error->line >= lines.size() && !lines.empty()))
            std::abort();
        return;
    }
    for (const auto& row : p.rows) {
        checkWritten(row.key);
        if (row.type == hitokoto::core::PropertyType::Text)
            checkWritten(row.text);
        for (const auto& item : row.items)
            checkWritten(item);
        if (row.keyLine >= lines.size() || row.valueLine >= lines.size() ||
            row.valueColumn > lines[row.valueLine].size())
            std::abort();
        // Code review O5: the tooltip's JSON has at most 20 lines and one more for "…".
        if (row.type == hitokoto::core::PropertyType::Unsupported) {
            const auto tip = hitokoto::core::prettyJson(row.text, 20);
            if (std::count(tip.begin(), tip.end(), u'\n') > 20)
                std::abort();
        }
    }
}

/// Spans stay inside the line, and no character is covered by two runs or hidden ranges.
void checkSpans(const hitokoto::core::LineRender& r, std::size_t length, bool mayHide) {
    std::vector<int> cover(length, 0);
    for (const auto& run : r.runs) {
        if (run.span.end() > length)
            std::abort();
        for (auto k = run.span.start; k < run.span.end(); ++k)
            ++cover[k];
    }
    for (const auto& h : r.hidden) {
        if (!mayHide || h.span.end() > length)
            std::abort();
        for (auto k = h.span.start; k < h.span.end(); ++k)
            ++cover[k];
    }
    for (const auto& s : r.inlineStyles) {
        if (s.span.end() > length)
            std::abort();
    }
    for (const auto& d : r.decorations) {
        if (d.column >= length)
            std::abort();
    }
    for (int c : cover) {
        if (c > 1)
            std::abort();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const auto registry = hitokoto::core::ElementRegistry::withBuiltins();
    static const auto unbounded = [] {
        auto r = hitokoto::core::ElementRegistry::withBuiltins();
        r.setInlineBounds({static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)});
        return r;
    }();
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
            const hitokoto::core::LineContext context{revealed, state, nextLine, static_cast<std::uint8_t>(setext)};
            const auto r = registry.render(line, context);
            checkSpans(r, line.size(), r.table || !revealed);
            if (r.blockState != registry.blockStateAfter(line, state, nextLine))
                std::abort();
            const auto bounds = registry.inlineBounds();
            const bool aboveBounds =
                line.size() > bounds.lineLength ||
                static_cast<std::size_t>(std::count_if(line.begin(), line.end(), [](char16_t c) {
                    return c == u'*' || c == u'_' || c == u'~' || c == u'`' || c == u'\\' || c == u'[';
                })) > bounds.syntaxCharacters;
            if (aboveBounds) {
                const auto u = unbounded.render(line, context);
                checkSpans(u, line.size(), u.table || !revealed);
                if (u.blockState != r.blockState)
                    std::abort();
            }
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
    checkWritten(text);
    return 0;
}
