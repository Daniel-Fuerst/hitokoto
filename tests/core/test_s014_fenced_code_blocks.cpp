// SPDX-License-Identifier: GPL-3.0-or-later
// S014: fenced code blocks preserve Markdown source and render across lines.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <memory>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, BlockState state = {}, bool revealed = false) {
    return registry().render(line, {revealed, state});
}

std::vector<Span> hiddenSpans(const LineRender& render) {
    std::vector<Span> spans;
    for (const auto& hidden : render.hidden) {
        spans.push_back(hidden.span);
    }
    return spans;
}
}  // namespace

TEST(S014_FencedCodeBlocks, FencedContentIsLiteralAndFencesAreConcealed) {
    const auto opening = show(u"```cpp");
    ASSERT_TRUE(opening.codeBlock);
    ASSERT_TRUE(opening.fenceLine);
    EXPECT_TRUE(opening.blockState.active());
    EXPECT_EQ(hiddenSpans(opening), (std::vector<Span>{{0, 6}}));
    EXPECT_TRUE(opening.runs.empty()) << "a concealed fence is hidden, not also styled (no overlapping spans)";

    const auto content = show(u"# **literal** - item", opening.blockState);
    EXPECT_TRUE(content.codeBlock);
    EXPECT_FALSE(content.fenceLine);
    EXPECT_TRUE(content.hidden.empty());
    EXPECT_TRUE(content.inlineStyles.empty());
    EXPECT_EQ(content.headingLevel, 0);
    EXPECT_EQ(content.runs, (std::vector<StyleRun>{{{0, 20}, Role::CodeBlock, 0}}));
    EXPECT_EQ(content.blockState, opening.blockState);

    const auto closing = show(u"```", content.blockState);
    EXPECT_TRUE(closing.codeBlock);
    EXPECT_TRUE(closing.fenceLine);
    EXPECT_FALSE(closing.blockState.active());
    EXPECT_EQ(hiddenSpans(closing), (std::vector<Span>{{0, 3}}));
}

TEST(S014_FencedCodeBlocks, RevealedFenceShowsSourceWithoutLosingCodeStyle) {
    const auto opening = show(u"  ~~~~ c++", {}, true);
    EXPECT_TRUE(opening.codeBlock);
    EXPECT_TRUE(opening.hidden.empty());
    EXPECT_EQ(opening.blockState, show(u"~~~~").blockState) << "indentation and info string do not matter";
    EXPECT_NE(opening.blockState, show(u"~~~").blockState) << "the fence length is part of the state";
    EXPECT_EQ(opening.runs.front().role, Role::CodeBlock);

    const auto closing = show(u"   ~~~~~  ", opening.blockState, true);
    EXPECT_TRUE(closing.codeBlock);
    EXPECT_TRUE(closing.hidden.empty());
    EXPECT_FALSE(closing.blockState.active());
}

TEST(S014_FencedCodeBlocks, FenceRulesAndUnterminatedBlocksAreHandled) {
    EXPECT_TRUE(show(u"   ```").blockState.active());
    EXPECT_FALSE(show(u"    ```").blockState.active());
    EXPECT_FALSE(show(u"```` `bad").blockState.active());

    const auto opened = show(u"~~~~");
    EXPECT_TRUE(show(u"~~~", opened.blockState).blockState.active());
    EXPECT_FALSE(show(u"~~~~~~", opened.blockState).blockState.active());
    EXPECT_TRUE(show(u"~~~ trailing", opened.blockState).blockState.active());
    EXPECT_TRUE(show(u"still code", opened.blockState).codeBlock);
}

namespace {
struct Walk {
    std::vector<bool> code;          ///< per line: belongs to a code block
    std::vector<BlockState> states;  ///< per line: state passed to the next line
};

/// Renders a document line by line, passing each line's block state on, as the editor does.
Walk walk(const std::vector<std::u16string_view>& lines) {
    Walk result;
    BlockState state;
    for (const auto line : lines) {
        const auto render = show(line, state);
        result.code.push_back(render.codeBlock);
        result.states.push_back(render.blockState);
        state = render.blockState;
    }
    return result;
}
}  // namespace

// Editing a fence line changes which of the following lines are code, beyond the original block:
// the old closing fence then opens a new block. The state after the edited line changes, which is
// what makes the editor restyle the lines below.
TEST(S014_FencedCodeBlocks, FenceEditsChangeFollowingLinesBlockMembership) {
    const std::vector<std::u16string_view> original = {u"```", u"a", u"b", u"```", u"after"};
    const std::vector<std::u16string_view> edited = {u"x``", u"a", u"b", u"```", u"after"};  // same length

    const auto before = walk(original);
    EXPECT_EQ(before.code, (std::vector<bool>{true, true, true, true, false}));

    const auto after = walk(edited);
    EXPECT_EQ(after.code, (std::vector<bool>{false, false, false, true, true}))
        << "the former closing fence opens a block, so the last line becomes code";
    EXPECT_NE(after.states.front(), before.states.front());
}

// The registry's shortcut for the UI must agree with the full render, line by line.
TEST(S014_FencedCodeBlocks, BlockStateAfterMatchesRender) {
    const std::u16string_view lines[] = {u"text", u"```cpp", u"code",   u"~~~",   u"````",  u"```",
                                         u"~~~~", u"  ~~~",  u"~~~~ x", u"~~~~~", u"after", u"``` `x`"};
    BlockState state;
    for (const auto line : lines) {
        for (bool revealed : {false, true}) {
            EXPECT_EQ(show(line, state, revealed).blockState, registry().blockStateAfter(line, state));
        }
        state = registry().blockStateAfter(line, state);
    }
}

namespace {
// A multi-line element the core does not know: a "$$" display-math block. Test-only.
class MathBlock final : public MultiLineElement {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "test-math"; }
    [[nodiscard]] std::optional<BlockLine> parse(std::u16string_view line,
                                                 std::uint32_t state) const noexcept override {
        const bool delimiter = line == u"$$";
        if (state == 0) {
            return delimiter ? std::optional<BlockLine>{BlockLine{1, true}} : std::nullopt;
        }
        return BlockLine{delimiter ? 0u : 1u, delimiter};
    }
    void render(std::u16string_view line, const BlockLine& parsed, const LineContext& context,
                LineRender& out) const override {
        if (parsed.delimiter && !context.revealed) {
            out.hidden.push_back({{0, static_cast<std::uint32_t>(line.size())}, HideMode::Collapse});
        }
        // Deliberately also styles the delimiter: the registry keeps runs and hidden ranges apart.
        out.runs.push_back({{0, static_cast<std::uint32_t>(line.size())}, Role::CodeBlock, 0});
    }
};
}  // namespace

// Brief section 5, extension by addition: a new multi-line element is added by registering it;
// the registry, the UI and the existing elements stay unchanged.
TEST(S014_FencedCodeBlocks, MultiLineElementsAreAddedByRegistrationOnly) {
    ElementRegistry r = ElementRegistry::withBuiltins();
    r.addMultiLine(std::make_unique<MathBlock>());

    const std::u16string_view lines[] = {u"$$", u"# not a heading", u"```", u"$$", u"# heading", u"```", u"$$"};
    BlockState state;
    std::vector<LineRender> shown;
    for (const auto line : lines) {
        shown.push_back(r.render(line, {false, state}));
        EXPECT_EQ(shown.back().blockState, r.blockStateAfter(line, state));
        state = shown.back().blockState;
    }
    EXPECT_TRUE(shown[0].blockState.active());
    EXPECT_EQ(hiddenSpans(shown[0]), (std::vector<Span>{{0, 2}}));
    EXPECT_TRUE(shown[0].runs.empty()) << "styled and hidden ranges never overlap";
    EXPECT_EQ(shown[1].headingLevel, 0) << "the open block owns the line";
    EXPECT_FALSE(shown[2].codeBlock) << "a fence inside the math block is math";
    EXPECT_FALSE(shown[3].blockState.active()) << "closed";
    EXPECT_EQ(shown[4].headingLevel, 1);
    EXPECT_TRUE(shown[5].codeBlock) << "built-in fenced code still works";
    EXPECT_TRUE(shown[6].codeBlock) << "and owns the line";
    EXPECT_TRUE(shown[6].blockState.active());
    EXPECT_NE(shown[6].blockState, shown[0].blockState) << "states of different elements differ";
}

// Theming baseline (ADR 0004).
TEST(S014_FencedCodeBlocks, ThemeTokensAreParsed) {
    const auto r = loadTheme(
        "[code-block]\nbackground = \"#010203\"\ntext = \"#040506\"\nborder = \"#070809\"\n"
        "border-width = 2\nradius = 3\npadding = 4\nfont-family = \"Mono X\"\nfont-scale = 0.9\n"
        "marker = \"#0a0b0c\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback) << r.diagnostics.front().toString();
    const CodeStyle s = codeBlockStyle(r.theme);
    EXPECT_EQ(s.background, (Color{1, 2, 3, 255}));
    EXPECT_EQ(s.text, (Color{4, 5, 6, 255}));
    EXPECT_EQ(s.border, (Color{7, 8, 9, 255}));
    EXPECT_DOUBLE_EQ(s.borderWidth, 2);
    EXPECT_DOUBLE_EQ(s.radius, 3);
    EXPECT_DOUBLE_EQ(s.padding, 4);
    EXPECT_EQ(s.fontFamily, "Mono X");
    EXPECT_DOUBLE_EQ(s.fontScale, 0.9);
    EXPECT_EQ(s.marker, (Color{10, 11, 12, 255}));

    EXPECT_TRUE(loadTheme("[code-block]\nborder-width = 9\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[code-block]\nfont-scale = 2.5\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[code-block]\npadding = 65\n", ColorScheme::Light).usedFallback);
    EXPECT_TRUE(loadTheme("[code-block]\nbackground = 3\n", ColorScheme::Light).usedFallback);
}

TEST(S014_FencedCodeBlocks, ThemeTokensFallBackToGeneralTokens) {
    for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
        const Theme& t = builtinTheme(scheme);
        const CodeStyle s = codeBlockStyle(t);
        EXPECT_EQ(s.background, t.surface);
        EXPECT_EQ(s.text, t.text);
        EXPECT_EQ(s.border, t.border);
        EXPECT_DOUBLE_EQ(s.borderWidth, 0);
        EXPECT_DOUBLE_EQ(s.radius, t.radius);
        EXPECT_DOUBLE_EQ(s.padding, t.spacing);
        EXPECT_EQ(s.fontFamily, "");
        EXPECT_DOUBLE_EQ(s.fontScale, 1);
        EXPECT_EQ(s.marker, t.marker);
    }
    // A general token changed in a user theme reaches the element unless the element sets its own.
    const auto general = loadTheme("[colors]\nsurface = \"#123456\"\n[layout]\nradius = 11\n", ColorScheme::Dark);
    EXPECT_EQ(codeBlockStyle(general.theme).background, (Color{0x12, 0x34, 0x56, 255}));
    EXPECT_DOUBLE_EQ(codeBlockStyle(general.theme).radius, 11);
    const auto own =
        loadTheme("[colors]\nsurface = \"#123456\"\n[code-block]\nbackground = \"#654321\"\n", ColorScheme::Dark);
    EXPECT_EQ(codeBlockStyle(own.theme).background, (Color{0x65, 0x43, 0x21, 255}));
}
