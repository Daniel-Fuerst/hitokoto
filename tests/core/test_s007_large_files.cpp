// SPDX-License-Identifier: GPL-3.0-or-later
// S007: rendering stays cheap for the lines of large files.
#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

#include <memory>

using namespace hitokoto::core;

namespace {
// Counts how often the registry asks it to render.
class CountingElement final : public InlineElement {
public:
    CountingElement(std::u16string_view triggers, int& calls) : triggers_(triggers), calls_(calls) {}
    [[nodiscard]] std::string_view id() const noexcept override { return "test-counting"; }
    [[nodiscard]] std::u16string_view triggerCharacters() const noexcept override { return triggers_; }
    void render(std::u16string_view, const LineContext&, LineRender&) const override { ++calls_; }

private:
    std::u16string_view triggers_;
    int& calls_;
};
}  // namespace

// Most lines contain none of the characters that can start an inline element; the registry
// scans each line once and skips those elements instead of letting each one scan the line.
TEST(S007_LargeFiles, InlineElementsRunOnlyWhenTheirCharactersOccur) {
    int backtick = 0;
    int always = 0;
    int nonAscii = 0;
    ElementRegistry r;
    r.addInline(std::make_unique<CountingElement>(u"`", backtick));
    r.addInline(std::make_unique<CountingElement>(u"", always));
    r.addInline(std::make_unique<CountingElement>(u"§", nonAscii));

    (void)r.render(u"plain prose without any trigger", {});
    EXPECT_EQ(backtick, 0);
    EXPECT_EQ(always, 1) << "no trigger characters declared: runs on every line";
    EXPECT_EQ(nonAscii, 0);

    (void)r.render(u"with `code`", {});
    EXPECT_EQ(backtick, 1);
    (void)r.render(u"§ 1", {});
    EXPECT_EQ(nonAscii, 1) << "non-ASCII triggers work too";
    EXPECT_EQ(always, 3);
}

TEST(S007_LargeFiles, BuiltinInlineElementsDeclareTheirTriggers) {
    EXPECT_EQ(makeInlineCodeElement()->triggerCharacters(), u"`");
    EXPECT_EQ(makeEmphasisElement()->triggerCharacters(), u"*_~");
    EXPECT_EQ(makeEscapeElement()->triggerCharacters(), u"\\");
}
