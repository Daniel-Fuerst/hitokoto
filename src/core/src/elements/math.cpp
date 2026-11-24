// SPDX-License-Identifier: GPL-3.0-or-later
// S044: LaTeX math. Inline math `$…$` and display math on one line `$$…$$` (an inline element),
// and display math blocks between `$$` lines (a multi-line element). The rules are Pandoc's
// (`tex_math_dollars`), which Obsidian and GitHub follow closely: an opening `$` has no blank after
// it, a closing `$` no blank before it and no digit after it, and a backslash escapes a dollar.
// The source of a formula is opaque to the inline elements after this one; typesetting it is
// S045–S047's work, this element only says where formulas are.
#include "hitokoto/core/markdown.hpp"

#include "../inline_steps.hpp"

#include <algorithm>
#include <vector>

namespace hitokoto::core {
namespace {

constexpr std::uint32_t kNone = 0xFFFFFFFFU;

bool isBlank(char16_t c) noexcept {
    return c == u' ' || c == u'\t';
}

bool isWhitespace(char16_t c) noexcept {
    return isBlank(c) || c == u'\n' || c == u'\r' || c == u'\f' || c == u'\v' || c == u' ';
}

bool isDigit(char16_t c) noexcept {
    return c >= u'0' && c <= u'9';
}

/// Odd number of backslashes directly before `position`. A run of backslashes stands before one
/// character only, so all calls on one line look at each backslash at most once per dollar run.
bool escaped(std::u16string_view line, std::size_t position, detail::InlineSteps& steps) noexcept {
    std::size_t slashes = 0;
    while (position > slashes && line[position - slashes - 1] == u'\\') {
        ++slashes;
    }
    steps.add(slashes + 1);
    return slashes % 2 != 0;
}

/// Length of the run of `$` that starts at `start` (`line[start]` is a dollar).
std::size_t dollarRun(std::u16string_view line, std::size_t start) noexcept {
    std::size_t end = start;
    while (end < line.size() && line[end] == u'$') {
        ++end;
    }
    return end - start;
}

/// One dollar run of the line: where it starts, how long it is, whether a backslash escapes it.
struct DollarRun {
    std::uint32_t start = 0;
    std::uint32_t length = 0;
    bool escaped = false;
};

class InlineMathElement final : public InlineElement {
public:
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return u"$"; }

    void render(std::u16string_view line, const LineContext& context, const InlineScan& scan,
                LineRender& out) const override {
        detail::InlineSteps steps;
        steps.add(line.size());
        const std::vector<DollarRun> runs = dollarRuns(line, scan, steps);
        if (runs.size() < 2) {
            return;
        }
        // For each run, the first run after it that can close an inline (one `$`) or a display
        // (`$$`) formula with no opaque character between them, found right to left, so that every
        // opener finds its closer at once and the line stays linear (S013's rule for inline
        // elements). A run that cannot close is skipped and belongs to the formula (Pandoc).
        std::vector<std::uint32_t> closerSingle(runs.size(), kNone);
        std::vector<std::uint32_t> closerDouble(runs.size(), kNone);
        std::uint32_t single = kNone;
        std::uint32_t dbl = kNone;
        for (std::size_t r = runs.size(); r-- > 0;) {
            closerSingle[r] = single;
            closerDouble[r] = dbl;
            if (closesSingle(line, runs[r])) {
                single = static_cast<std::uint32_t>(r);
            }
            if (closesDouble(runs[r])) {
                dbl = static_cast<std::uint32_t>(r);
            }
            if (r > 0 && opaqueBetween(scan, runs[r - 1], runs[r], steps)) {
                single = kNone;  // the runs before r cannot reach r or anything after it
                dbl = kNone;
            }
        }

        for (std::size_t r = 0; r < runs.size();) {
            const DollarRun& run = runs[r];
            std::uint32_t closer = kNone;
            if (opensSingle(line, run)) {
                closer = closerSingle[r];
            } else if (opensDouble(run)) {
                closer = closerDouble[r];
            }
            if (closer == kNone) {
                ++r;
                continue;
            }
            emit(run, runs[closer], context, out);
            r = closer + 1U;
        }
    }

private:
    static std::vector<DollarRun> dollarRuns(std::u16string_view line, const InlineScan& scan,
                                             detail::InlineSteps& steps) {
        std::vector<DollarRun> runs;
        for (std::size_t i = 0; i < line.size();) {
            if (line[i] != u'$' || scan.isTaken(i)) {
                ++i;
                continue;
            }
            const std::size_t length = dollarRun(line, i);
            runs.push_back(
                {static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(length), escaped(line, i, steps)});
            i += length;
        }
        steps.add(runs.size());
        return runs;
    }

    /// An opaque or hidden character between the end of `left` and the start of `right`; each
    /// character between two neighbouring runs is looked at once.
    static bool opaqueBetween(const InlineScan& scan, const DollarRun& left, const DollarRun& right,
                              detail::InlineSteps& steps) {
        steps.add(right.start - (left.start + left.length));
        for (std::uint32_t i = left.start + left.length; i < right.start; ++i) {
            if (scan.isTaken(i)) {
                return true;
            }
        }
        return false;
    }

    static bool opensSingle(std::u16string_view line, const DollarRun& run) noexcept {
        const std::size_t after = run.start + 1U;
        return run.length == 1 && !run.escaped && after < line.size() && !isWhitespace(line[after]);
    }

    static bool closesSingle(std::u16string_view line, const DollarRun& run) noexcept {
        const std::size_t after = run.start + 1U;
        return run.length == 1 && !run.escaped && run.start > 0 && !isWhitespace(line[run.start - 1]) &&
               (after >= line.size() || !isDigit(line[after]));
    }

    static bool opensDouble(const DollarRun& run) noexcept { return run.length == 2 && !run.escaped; }

    static bool closesDouble(const DollarRun& run) noexcept { return run.length == 2 && !run.escaped; }

    static void emit(const DollarRun& open, const DollarRun& close, const LineContext& context, LineRender& out) {
        const std::uint32_t d = open.length;
        const Span source{open.start + d, close.start - open.start - d};
        if (source.length == 0) {
            return;
        }
        out.runs.push_back({source, Role::Math, 0});
        // Markers are styled when revealed; when concealed the registry drops them under the hidden ranges.
        out.runs.push_back({{open.start, d}, Role::MathMarker, 0});
        out.runs.push_back({{close.start, d}, Role::MathMarker, 0});
        if (!context.revealed) {
            out.hidden.push_back({{open.start, d}, HideMode::Collapse});
            out.hidden.push_back({{close.start, d}, HideMode::Collapse});
        }
        out.opaqueSpans.push_back({open.start, close.start + d - open.start});
        out.math.push_back({source, d == 2});
    }
};

// Display blocks. The element state is 1 while a block is open.
constexpr std::uint32_t kOpen = 1;
constexpr std::size_t kMaxIndent = 3;  // four spaces make indented code (CommonMark 4.4), as for fences

/// Where the `$$` of an opening line starts, or npos: up to three spaces, `$$`, and no other `$$`
/// after it on the line (that would be display math on one line, the inline element's).
std::size_t openingDollars(std::u16string_view line) noexcept {
    std::size_t start = 0;
    while (start < line.size() && line[start] == u' ') {
        ++start;
    }
    if (start > kMaxIndent || line.substr(start, 2) != u"$$" || dollarRun(line, start) != 2) {
        return std::u16string_view::npos;
    }
    if (line.substr(start + 2).find(u"$$") != std::u16string_view::npos) {
        return std::u16string_view::npos;
    }
    return start;
}

/// Where the `$$` that closes a block starts, or npos: the line ends in `$$` (blanks after it
/// allowed) that is not escaped and not part of a longer run.
std::size_t closingDollars(std::u16string_view line) noexcept {
    std::size_t end = line.size();
    while (end > 0 && isBlank(line[end - 1])) {
        --end;
    }
    if (end < 2 || line.substr(end - 2, 2) != u"$$" || (end > 2 && line[end - 3] == u'$')) {
        return std::u16string_view::npos;
    }
    std::size_t slashes = 0;
    while (end - 2 > slashes && line[end - 3 - slashes] == u'\\') {
        ++slashes;
    }
    return slashes % 2 == 0 ? end - 2 : std::u16string_view::npos;
}

bool onlyBlanks(std::u16string_view text) noexcept {
    return std::all_of(text.begin(), text.end(), isBlank);
}

class DisplayMathElement final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "display-math"; }

    [[nodiscard]] std::optional<BlockLine> parseWithNextLine(std::u16string_view line, std::uint32_t state,
                                                             std::u16string_view /*nextLine*/) const noexcept override {
        if (state == 0) {
            return openingDollars(line) != std::u16string_view::npos ? std::optional{BlockLine{kOpen, true}}
                                                                     : std::nullopt;
        }
        return closingDollars(line) != std::u16string_view::npos ? BlockLine{0, true} : BlockLine{kOpen, false};
    }

    void render(std::u16string_view line, const BlockLine& parsed, const LineContext& context,
                LineRender& out) const override {
        out.mathBlock = true;
        out.mathDelimiter = parsed.delimiter;
        const auto length = static_cast<std::uint32_t>(line.size());
        if (!parsed.delimiter) {
            if (length > 0) {
                out.runs.push_back({{0, length}, Role::Math, 0});
                out.math.push_back({{0, length}, true});
            }
            return;
        }
        // The `$$` with the blanks on its outer side is the marker; text on its inner side is
        // formula source.
        Span marker;
        Span source;
        if (parsed.stateAfter != 0) {
            const auto dollars = static_cast<std::uint32_t>(openingDollars(line));
            marker = {0, dollars + 2};
            source = {marker.end(), length - marker.end()};
        } else {
            const auto dollars = static_cast<std::uint32_t>(closingDollars(line));
            marker = {dollars, length - dollars};
            source = {0, dollars};
        }
        const bool pure = onlyBlanks(line.substr(source.start, source.length));
        if (pure) {
            marker = {0, length};
        } else {
            out.runs.push_back({source, Role::Math, 0});
            out.math.push_back({source, true});
        }
        if (context.revealed) {
            out.runs.push_back({marker, Role::MathMarker, 0});
        } else {
            out.hidden.push_back({marker, pure ? HideMode::KeepSpace : HideMode::Collapse});
        }
    }
};

}  // namespace

std::unique_ptr<InlineElement> makeInlineMathElement() {
    return std::make_unique<InlineMathElement>();
}

std::unique_ptr<MultiLineElement> makeDisplayMathElement() {
    return std::make_unique<DisplayMathElement>();
}

}  // namespace hitokoto::core
