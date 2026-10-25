// SPDX-License-Identifier: GPL-3.0-or-later
// S014: fenced code blocks preserve Markdown source and render across lines.
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, FenceState state = {}, bool revealed = false) {
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
    EXPECT_EQ(opening.fenceState, (FenceState{u'`', 3}));
    EXPECT_EQ(hiddenSpans(opening), (std::vector<Span>{{0, 6}}));
    EXPECT_EQ(opening.runs, (std::vector<StyleRun>{{{0, 6}, Role::CodeBlock, 0}}));

    const auto content = show(u"# **literal** - item", opening.fenceState);
    EXPECT_TRUE(content.codeBlock);
    EXPECT_FALSE(content.fenceLine);
    EXPECT_TRUE(content.hidden.empty());
    EXPECT_TRUE(content.inlineStyles.empty());
    EXPECT_EQ(content.headingLevel, 0);
    EXPECT_EQ(content.runs, (std::vector<StyleRun>{{{0, 20}, Role::CodeBlock, 0}}));
    EXPECT_EQ(content.fenceState, opening.fenceState);

    const auto closing = show(u"```", content.fenceState);
    EXPECT_TRUE(closing.codeBlock);
    EXPECT_TRUE(closing.fenceLine);
    EXPECT_TRUE(closing.fenceState.empty());
    EXPECT_EQ(hiddenSpans(closing), (std::vector<Span>{{0, 3}}));
}

TEST(S014_FencedCodeBlocks, RevealedFenceShowsSourceWithoutLosingCodeStyle) {
    const auto opening = show(u"  ~~~~ c++", {}, true);
    EXPECT_TRUE(opening.codeBlock);
    EXPECT_TRUE(opening.hidden.empty());
    EXPECT_EQ(opening.fenceState, (FenceState{u'~', 4}));
    EXPECT_EQ(opening.runs.front().role, Role::CodeBlock);

    const auto closing = show(u"   ~~~~~  ", opening.fenceState, true);
    EXPECT_TRUE(closing.codeBlock);
    EXPECT_TRUE(closing.hidden.empty());
    EXPECT_TRUE(closing.fenceState.empty());
}

TEST(S014_FencedCodeBlocks, FenceRulesAndUnterminatedBlocksAreHandled) {
    EXPECT_TRUE(show(u"   ```").fenceState.active());
    EXPECT_TRUE(show(u"    ```").fenceState.empty());
    EXPECT_TRUE(show(u"```` `bad").fenceState.empty());

    const auto opened = show(u"~~~~");
    EXPECT_TRUE(show(u"~~~", opened.fenceState).fenceState.active());
    EXPECT_TRUE(show(u"~~~~~~", opened.fenceState).fenceState.empty());
    EXPECT_TRUE(show(u"~~~ trailing", opened.fenceState).fenceState.active());
    EXPECT_TRUE(show(u"still code", opened.fenceState).codeBlock);
}

TEST(S014_FencedCodeBlocks, FenceEditsChangeFollowingLinesBlockMembership) {
    const auto opening = show(u"```");
    const auto codeLine = show(u"**literal**", opening.fenceState);
    EXPECT_TRUE(codeLine.codeBlock);

    const auto noOpening = show(u"```");
    EXPECT_TRUE(noOpening.fenceState.active());
    const auto outside = show(u"**literal**", {});
    EXPECT_FALSE(outside.codeBlock);
    EXPECT_FALSE(outside.inlineStyles.empty());
}
