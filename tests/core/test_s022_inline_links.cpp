// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links and titles.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include "line_work.hpp"
#include "support.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {

using hitokoto::test::show;
}  // namespace

// User example 1: Standard Link
TEST(S022_InlineLinks, StandardLinkConcealsDelimitersAndExposesLinkTarget) {
    const auto r = show(u"* [Standard Link](https://example.com)", false);
    // Bullet marker: [0, 2)
    // Link: [2, 38)
    // '[' is at 2 (len 1). Link text is [3, 16) (len 13: "Standard Link").
    // '](https://example.com)' is at 16 (len 22).
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{3, 13}));
    EXPECT_EQ(r.links[0].full, (Span{2, 36}));
    EXPECT_EQ(r.links[0].destination, u"https://example.com");
    EXPECT_TRUE(r.links[0].title.empty());

    // Concealed hidden ranges: bullet {0, 1}, '[' {2, 1}, '](https://example.com)' {16, 22}
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::KeepSpace},
                            {{2, 1}, HideMode::Collapse},
                            {{16, 22}, HideMode::ZeroWidth},
                        }));

    // Visible runs: link text at {3, 13}
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{3, 13}, Role::LinkText, 0}}));

    // When revealed:
    const auto rev = show(u"* [Standard Link](https://example.com)", true);
    EXPECT_TRUE(rev.hidden.empty());
    EXPECT_EQ(rev.runs, (std::vector<StyleRun>{
                            {{0, 1}, Role::Marker, 0},
                            {{2, 1}, Role::LinkMarker, 0},
                            {{3, 13}, Role::LinkText, 0},
                            {{16, 22}, Role::LinkMarker, 0},
                        }));
}

// User example 2: Link with Title
TEST(S022_InlineLinks, LinkWithTitleParsesTitleAndConcealsDelimiters) {
    const auto r = show(u"* [Link with Title](https://example.com \"Hover over me to see the title!\")", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{3, 15}));
    EXPECT_EQ(r.links[0].destination, u"https://example.com");
    EXPECT_EQ(r.links[0].title, u"Hover over me to see the title!");

    // '[' at 2 (len 1); '](https://example.com "Hover over me to see the title!")' at 18 (len 56)
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::KeepSpace},
                            {{2, 1}, HideMode::Collapse},
                            {{18, 56}, HideMode::ZeroWidth},
                        }));
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{3, 15}, Role::LinkText, 0}}));
}

// User example 3: Link with Parentheses in URL
TEST(S022_InlineLinks, LinkWithParenthesesInUrlHandlesBalancedParensAndPreservesUnderscores) {
    const auto r =
        show(u"* [Link with Parentheses in URL](https://en.wikipedia.org/wiki/Markdown_(markup_language))", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{3, 28}));
    EXPECT_EQ(r.links[0].destination, u"https://en.wikipedia.org/wiki/Markdown_(markup_language)");
    EXPECT_TRUE(r.links[0].title.empty());

    // Emphasis must NOT match the underscore inside the destination URL!
    EXPECT_TRUE(r.inlineStyles.empty());

    // '[' at 2 (len 1); tail from ']' at 31 to end ')' at 90 (len 59)
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::KeepSpace},
                            {{2, 1}, HideMode::Collapse},
                            {{31, 59}, HideMode::ZeroWidth},
                        }));
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{3, 28}, Role::LinkText, 0}}));
}

// User example 4: Link with Spaces in URL (Angle Brackets)
TEST(S022_InlineLinks, LinkWithSpacesInUrlInAngleBrackets) {
    const auto r =
        show(u"* [Link with Spaces in URL (Angle Brackets)](<https://example.com/path with spaces/index.html>)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{3, 40}));
    EXPECT_EQ(r.links[0].destination, u"https://example.com/path with spaces/index.html");
    EXPECT_TRUE(r.links[0].title.empty());

    // '[' at 2 (len 1); tail from ']' at 43 to end ')' at 95 (len 52)
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::KeepSpace},
                            {{2, 1}, HideMode::Collapse},
                            {{43, 52}, HideMode::ZeroWidth},
                        }));
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{3, 40}, Role::LinkText, 0}}));
}

// User example 5: Link with Bold, Italics, and Code Inside Brackets
TEST(S022_InlineLinks, LinkWithBoldItalicsAndCodeInsideBrackets) {
    const auto r = show(u"* [Link with **Bold**, *Italics*, and `Code` Inside Brackets](https://example.com)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].destination, u"https://example.com");

    // Text inside brackets has bold, italic and inline code:
    // Bold is on "Bold"
    // Italic is on "Italics"
    // Inline code is on "Code"
    bool hasBold = false;
    bool hasItalic = false;
    for (const auto& s : r.inlineStyles) {
        if (s.style == InlineStyle::Bold) {
            hasBold = true;
        }
        if (s.style == InlineStyle::Italic) {
            hasItalic = true;
        }
    }
    EXPECT_TRUE(hasBold) << "bold style inside link brackets";
    EXPECT_TRUE(hasItalic) << "italic style inside link brackets";

    bool hasInlineCode = false;
    for (const auto& run : r.runs) {
        if (run.role == Role::InlineCode) {
            hasInlineCode = true;
        }
    }
    EXPECT_TRUE(hasInlineCode) << "inline code inside link brackets";

    // Delimiters (** around Bold, * around Italics, ` around Code, and [ ](url) of the link)
    // are all in exact hidden ranges when concealed.
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::KeepSpace},
                            {{2, 1}, HideMode::Collapse},
                            {{13, 2}, HideMode::Collapse},
                            {{19, 2}, HideMode::Collapse},
                            {{23, 1}, HideMode::Collapse},
                            {{31, 1}, HideMode::Collapse},
                            {{38, 1}, HideMode::Collapse},
                            {{43, 1}, HideMode::Collapse},
                            {{60, 22}, HideMode::ZeroWidth},
                        }));
}

TEST(S022_InlineLinks, TitleWithSingleQuotesAndParentheses) {
    const auto singleQuote = show(u"[Link](https://example.com 'Single Title')", false);
    ASSERT_EQ(singleQuote.links.size(), 1u);
    EXPECT_EQ(singleQuote.links[0].title, u"Single Title");

    const auto parenTitle = show(u"[Link](https://example.com (Paren Title))", false);
    ASSERT_EQ(parenTitle.links.size(), 1u);
    EXPECT_EQ(parenTitle.links[0].title, u"Paren Title");
}

TEST(S022_InlineLinks, EscapedBracketsInsideLinkText) {
    const auto r = show(u"[link with \\[escaped\\] brackets](https://example.com)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{1, 30}));
    EXPECT_EQ(r.links[0].destination, u"https://example.com");
}

TEST(S022_InlineLinks, EscapedOpeningBracketIsNotALink) {
    const auto r = show(u"\\[Not a link](https://example.com)", false);
    EXPECT_TRUE(r.links.empty());
    // S020: backslash is concealed
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{{{0, 1}, HideMode::Collapse}}));
}

TEST(S022_InlineLinks, MultipleLinksOnOneLine) {
    const auto r = show(u"[One](https://one.com) and [Two](https://two.com)", false);
    ASSERT_EQ(r.links.size(), 2u);
    EXPECT_EQ(r.links[0].destination, u"https://one.com");
    EXPECT_EQ(r.links[1].destination, u"https://two.com");
}

TEST(S022_InlineLinks, CodeSpanContainingLinkSyntaxRemainsCode) {
    const auto r = show(u"`[not a link](url)`", false);
    EXPECT_TRUE(r.links.empty());
    ASSERT_EQ(r.runs.size(), 1u);
    EXPECT_EQ(r.runs[0].role, Role::InlineCode);
}

TEST(S022_InlineLinks, LinkInsideHeadingPreservesHeadingLevel) {
    const auto r = show(u"## [Heading Link](https://example.com)", false);
    EXPECT_EQ(r.headingLevel, 2);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].destination, u"https://example.com");
}

TEST(S022_InlineLinks, ThemeTokensAreParsedWithFallbacks) {
    const auto r = loadTheme(
        "[link]\n"
        "text = \"#112233\"\n"
        "marker = \"#445566\"\n",
        ColorScheme::Light);
    ASSERT_FALSE(r.usedFallback);
    const LinkStyle s = linkStyle(r.theme);
    EXPECT_EQ(s.text, (Color{0x11, 0x22, 0x33, 0xff}));
    EXPECT_EQ(s.marker, (Color{0x44, 0x55, 0x66, 0xff}));

    // Fallbacks
    const auto defaultTheme = loadTheme("", ColorScheme::Light);
    const LinkStyle defStyle = linkStyle(defaultTheme.theme);
    EXPECT_EQ(defStyle.text, defaultTheme.theme.accent);
    EXPECT_EQ(defStyle.marker, defaultTheme.theme.marker);
}

TEST(S022_InlineLinks, UnclosedLinkWithOpeningParenRemainsLiteral) {
    const auto r = show(u"[unclosed link(url)", false);
    EXPECT_TRUE(r.links.empty());
    EXPECT_TRUE(r.hidden.empty());
    EXPECT_TRUE(r.runs.empty());
}

TEST(S022_InlineLinks, CommonMarkNegativesEmphasisAcrossLinkBoundary) {
    // *[foo*](/uri): literal '*' and link 'foo*'
    {
        const auto r = show(u"*[foo*](/uri)", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"/uri");
        EXPECT_EQ(r.links[0].span, (Span{2, 4}));
        EXPECT_TRUE(r.inlineStyles.empty()) << "delimiters across link boundary must not pair";
        // Hidden ranges cover only '[' and '](/uri)'; neither '*' is hidden
        EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                                {{1, 1}, HideMode::Collapse},
                                {{6, 7}, HideMode::ZeroWidth},
                            }));
    }

    // *foo [bar* baz](/uri): literal '*foo ' and link 'bar* baz'
    {
        const auto r = show(u"*foo [bar* baz](/uri)", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"/uri");
        EXPECT_EQ(r.links[0].span, (Span{6, 8}));
        EXPECT_TRUE(r.inlineStyles.empty());
        EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                                {{5, 1}, HideMode::Collapse},
                                {{14, 7}, HideMode::ZeroWidth},
                            }));
    }

    // _[a_](u): literal '_' and link 'a_'
    {
        const auto r = show(u"_[a_](u)", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"u");
        EXPECT_EQ(r.links[0].span, (Span{2, 2}));
        EXPECT_TRUE(r.inlineStyles.empty());
        EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                                {{1, 1}, HideMode::Collapse},
                                {{4, 4}, HideMode::ZeroWidth},
                            }));
    }
}

TEST(S022_InlineLinks, CommonMarkNegativesLinksInsideLinksForbidden) {
    // [foo [bar](/uri)](/uri) in CommonMark: literal '[foo ', link 'bar', literal '](/uri)'
    const auto r = show(u"[foo [bar](/uri)](/uri)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{6, 3}));
    EXPECT_EQ(r.links[0].destination, u"/uri");
    // Outer '[' and outer '](/uri)' are not hidden
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{5, 1}, HideMode::Collapse},
                            {{9, 7}, HideMode::ZeroWidth},
                        }));
}

TEST(S022_InlineLinks, CommonMarkNegativesImagesStayLiteral) {
    const auto r = show(u"![alt](img.png)", false);
    EXPECT_TRUE(r.links.empty());
    EXPECT_TRUE(r.hidden.empty());
    EXPECT_TRUE(r.runs.empty());
}

TEST(S022_InlineLinks, CodeSpanBeforeAndInsideLinkAvoidsUseAfterFree) {
    // Reproducer from review O2: `x` [a `b`](u)
    const auto r = show(u"`x` [a `b`](u)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].span, (Span{5, 5}));
    EXPECT_EQ(r.links[0].destination, u"u");
    // Visible runs: `x` inline code, "a " link text, `b` inline code
    EXPECT_EQ(r.runs, (std::vector<StyleRun>{
                          {{1, 1}, Role::InlineCode, 0},
                          {{5, 2}, Role::LinkText, 0},
                          {{8, 1}, Role::InlineCode, 0},
                      }));
}

TEST(S022_InlineLinks, LinearBracketMatchingPerformance) {
    std::u16string big(10000, u'[');
    const auto r = show(big, false);
    EXPECT_TRUE(r.links.empty());
}

TEST(S022_InlineLinks, BadgeLinkContainingImageDestination) {
    // [![CI](https://x/badge.svg)](https://github.com/x)
    // CommonMark: outer link to https://github.com/x, containing image ![CI](https://x/badge.svg)
    const auto r = show(u"[![CI](https://x/badge.svg)](https://github.com/x)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].destination, u"https://github.com/x");
    EXPECT_EQ(r.links[0].span, (Span{1, 26}));
    EXPECT_EQ(r.hidden, (std::vector<HiddenRange>{
                            {{0, 1}, HideMode::Collapse},
                            {{27, 23}, HideMode::ZeroWidth},
                        }));
}

TEST(S022_InlineLinks, TextWithInterleavedImageInsideLink) {
    // [a ![b](c) d](e)
    const auto r = show(u"[a ![b](c) d](e)", false);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_EQ(r.links[0].destination, u"e");
    EXPECT_EQ(r.links[0].span, (Span{1, 11}));
}

TEST(S022_InlineLinks, DestinationStartingWithQuotesOrParensParsedAsDestination) {
    // [link]("title") has destination "\"title\"" and no title (CommonMark example 496)
    {
        const auto r = show(u"[link](\"title\")", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"\"title\"");
        EXPECT_TRUE(r.links[0].title.empty());
    }

    // [](()) has destination "()" and no title
    {
        const auto r = show(u"[](())", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"()");
        EXPECT_TRUE(r.links[0].title.empty());
    }

    // [!](()) has destination "()" and no title
    {
        const auto r = show(u"[!](())", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"()");
        EXPECT_TRUE(r.links[0].title.empty());
    }
}

TEST(S022_InlineLinks, BackticksInDestinationOrTitleAreConsumedByLink) {
    // In CommonMark (6.1, 6.3), backticks inside link destinations or titles are consumed
    // by the link syntax and do not form inline code spans.
    {
        const auto r = show(u"[link](http://foo`bar`baz)", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"http://foo`bar`baz");
        EXPECT_TRUE(r.links[0].title.empty());
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{1, 4}, Role::LinkText, 0}}));
        for (const auto& run : r.runs) {
            EXPECT_NE(run.role, Role::InlineCode);
        }
    }
    {
        const auto r = show(u"[link](http://foo \"my `code` title\")", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"http://foo");
        EXPECT_EQ(r.links[0].title, u"my `code` title");
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{{{1, 4}, Role::LinkText, 0}}));
        for (const auto& run : r.runs) {
            EXPECT_NE(run.role, Role::InlineCode);
        }
    }
    {
        // Destination consumes the backtick in URL; subsequent backticks outside the link form code spans
        const auto r = show(u"See [docs](https://x.org/a`b) and `code`.", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"https://x.org/a`b");
        EXPECT_EQ(r.links[0].span, (Span{5, 4}));

        bool hasCode = false;
        for (const auto& run : r.runs) {
            if (run.role == Role::InlineCode) {
                hasCode = true;
                EXPECT_EQ(run.span, (Span{35, 4}));  // "code"
            }
        }
        EXPECT_TRUE(hasCode);
    }
    {
        // Unclosed backtick in URL is not a code span, so it remains a valid link
        const auto r = show(u"[link](http://foo`bar)", false);
        ASSERT_EQ(r.links.size(), 1u);
        EXPECT_EQ(r.links[0].destination, u"http://foo`bar");
    }
}

TEST(S022_InlineLinks, CodeSpanIntersectingLinkDelimitersDoesNotFormLink) {
    // Code spans take precedence over link delimiters (CommonMark 6.1, 6.3).
    // An inline code span crossing '[' or ']' prevents link formation.
    {
        const auto r = show(u"`[link` ](url)", false);
        EXPECT_TRUE(r.links.empty());
        bool hasCode = false;
        for (const auto& run : r.runs) {
            if (run.role == Role::InlineCode) {
                hasCode = true;
                EXPECT_EQ(run.span, (Span{1, 5}));  // "[link"
            }
        }
        EXPECT_TRUE(hasCode);
    }
    {
        const auto r = show(u"[foo `bar](url)`", false);
        EXPECT_TRUE(r.links.empty());
        bool hasCode = false;
        for (const auto& run : r.runs) {
            if (run.role == Role::InlineCode) {
                hasCode = true;
                EXPECT_EQ(run.span, (Span{6, 9}));  // "bar](url)"
            }
        }
        EXPECT_TRUE(hasCode);
    }
    {
        const auto r = show(u"`[foo](url)`", false);
        EXPECT_TRUE(r.links.empty());
        bool hasCode = false;
        for (const auto& run : r.runs) {
            if (run.role == Role::InlineCode) {
                hasCode = true;
                EXPECT_EQ(run.span, (Span{1, 10}));  // "[foo](url)"
            }
        }
        EXPECT_TRUE(hasCode);
    }
    {
        const auto r = show(u"` [link](http://foo` )", false);
        EXPECT_TRUE(r.links.empty());
        bool hasCode = false;
        for (const auto& run : r.runs) {
            if (run.role == Role::InlineCode) {
                hasCode = true;
                EXPECT_EQ(run.span, (Span{1, 18}));  // " [link](http://foo"
            }
        }
        EXPECT_TRUE(hasCode);
    }
}

TEST(S022_InlineLinks, RevealedLinkDoesNotOverlapEmphasisOrEscapeMarkers) {
    // Review objection O19: on revealed lines, LinkText must not overlap EmphasisMarker or EscapeMarker.
    // Fuzz invariant: no two runs or hidden ranges cover the same character.
    {
        const auto r = show(u"[**b**](u)", true);
        EXPECT_TRUE(r.hidden.empty());
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{
                              {{0, 1}, Role::LinkMarker, 0},
                              {{1, 2}, Role::EmphasisMarker, 0},
                              {{3, 1}, Role::LinkText, 0},
                              {{4, 2}, Role::EmphasisMarker, 0},
                              {{6, 4}, Role::LinkMarker, 0},
                          }));
    }
    {
        const auto r = show(u"[a \\* b](u)", true);
        EXPECT_TRUE(r.hidden.empty());
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{
                              {{0, 1}, Role::LinkMarker, 0},
                              {{1, 2}, Role::LinkText, 0},
                              {{3, 1}, Role::EscapeMarker, 0},
                              {{4, 3}, Role::LinkText, 0},
                              {{7, 4}, Role::LinkMarker, 0},
                          }));
    }
    {
        const auto r = show(u"[a `b` c](u)", true);
        EXPECT_TRUE(r.hidden.empty());
        EXPECT_EQ(r.runs, (std::vector<StyleRun>{
                              {{0, 1}, Role::LinkMarker, 0},
                              {{1, 2}, Role::LinkText, 0},
                              {{3, 1}, Role::InlineCodeMarker, 0},
                              {{4, 1}, Role::InlineCode, 0},
                              {{5, 1}, Role::InlineCodeMarker, 0},
                              {{6, 2}, Role::LinkText, 0},
                              {{8, 4}, Role::LinkMarker, 0},
                          }));
    }
}

// Rework of 2026-11-16: a line renders in linear time (steps counted, line_work.hpp). `](` that
// never becomes a link read on to the end of the line each time: parentheses that do not balance,
// a title that does not end.
TEST(S022_InlineLinks, ALineRendersInLinearTime) {
    using hitokoto::test::halves;
    using hitokoto::test::LinePattern;
    using hitokoto::test::of;
    using hitokoto::test::repeated;
    for (const auto& pattern : {
             of("[](", u"[]("),
             of("[a](", u"[a]("),
             of("[a](b) ", u"[a](b) "),
             of("[a](b (", u"[a](b ("),
             of("[a](<b> (", u"[a](<b> ("),
             of("[a](b \"", u"[a](b \""),
             of("[a](b 't' ", u"[a](b 't' "),
             of("[a](<", u"[a](<"),
             of("[a]( ", u"[a]( "),
             of("[a](b\\", u"[a](b\\"),
             of("![", u"!["),
             of("![a](b) ", u"![a](b) "),
             of("[", u"["),
             of("[[a](", u"[[a]("),
             of("[a](b)](", u"[a](b)]("),
             of("[*a*](u) ", u"[*a*](u) "),
             of("[`a`](u) ", u"[`a`](u) "),
             of("*a* `b` [c](d) \\* ~~e~~ ", u"*a* `b` [c](d) \\* ~~e~~ "),
             halves("[ .. ](", u"[", u"]("),
             halves("[ .. ](x) ", u"[", u"](x) "),
             halves("[a]((( .. )))", u"[a](((", u")))"),
             LinePattern{"- [a](b) ", [](std::size_t length) { return u"- " + repeated(u"[a](b) ", length); }},
             LinePattern{"[*a* `b` ..](u)",
                         [](std::size_t length) { return u"[" + repeated(u"*a* `b` ", length) + u"](u)"; }},
             LinePattern{"[a]((b)(b)..)", [](std::size_t length) { return u"[a](" + repeated(u"(b)", length) + u")"; }},
             LinePattern{"[a](b \"*x* ..\")",
                         [](std::size_t length) { return u"[a](b \"" + repeated(u"*x* ", length) + u"\")"; }},
         }) {
        hitokoto::test::expectLinearWork(pattern);
    }
}

// All four inline elements on lines put together at random from pieces of their syntax (a fixed
// seed): no line takes more than kStepsPerCharacter.
TEST(S022_InlineLinks, RandomLinesOfSyntaxRenderInLinearTime) {
    static constexpr std::u16string_view kPieces[] = {
        u"[a](b)",
        u"[",
        u"]",
        u"](",
        u"](<",
        u">",
        u")",
        u"(",
        u" \"t\"",
        u" 't'",
        u" (t)",
        u"\"",
        u"'",
        u"!",
        u"![",
        u"*",
        u"**",
        u"***",
        u"_",
        u"__",
        u"~~",
        u"`",
        u"``",
        u"`a`",
        u"\\",
        u"\\\\",
        u"\\*",
        u"\\[",
        u"\\]",
        u"\\(",
        u"\\)",
        u"a",
        u"b",
        u" ",
        u"  ",
        u"<",
        u"x*y",
        u"a_b",
        u" *a*",
        u"**b**",
        u"~~s~~",
        u"[*a*](u)",
        u"[`c`](u \"t\")",
        u"[x](<u v>)",
        u"[x](u(v)w)",
    };
    std::uint32_t state = 20261116U;
    const auto next = [&state] {
        state = state * 1664525U + 1013904223U;  // a linear congruential generator: the same lines everywhere
        return state >> 8;
    };
    for (int i = 0; i < 120; ++i) {
        std::u16string line;
        // Some lines repeat one short sequence of pieces, the others take each piece at random.
        const std::size_t period = i % 4 == 0 ? 1 + next() % 4 : 0;
        std::vector<std::size_t> sequence;
        for (std::size_t k = 0; k < period; ++k) {
            sequence.push_back(next() % std::size(kPieces));
        }
        for (std::size_t k = 0; line.size() < 3000; ++k) {
            line += kPieces[period != 0 ? sequence[k % period] : next() % std::size(kPieces)];
        }
        const std::uint64_t steps = hitokoto::test::renderSteps(line);
        ASSERT_LE(steps, hitokoto::test::kStepsPerCharacter * line.size())
            << "line " << i << ": " << steps << " steps for " << line.size() << " characters";
    }
}

// The links are the same as before the rework. Each line checked against commonmark.js 0.31.2 and
// markdown-it-py 4.2.0 (mode commonmark).
TEST(S022_InlineLinks, ParenthesesOfADestinationBalanceOrThereIsNoLink) {
    // <a href="b(c(d)e)f">a</a> [g](h(i)
    auto r = show(u"[a](b(c(d)e)f) [g](h(i)");
    ASSERT_EQ(r.links.size(), 1U);
    EXPECT_EQ(r.links[0].destination, u"b(c(d)e)f");
    EXPECT_EQ(r.links[0].full, (Span{0, 14}));

    // <a href="(b)">a</a> [c]((d)
    r = show(u"[a]((b)) [c]((d)");
    ASSERT_EQ(r.links.size(), 1U);
    EXPECT_EQ(r.links[0].destination, u"(b)");
    EXPECT_EQ(r.links[0].full, (Span{0, 8}));

    // <a href="b(c">a</a> <a href="e)">d</a>: an escaped parenthesis neither opens nor closes.
    r = show(u"[a](b\\(c) [d](e\\))");
    ASSERT_EQ(r.links.size(), 2U);
    EXPECT_EQ(r.links[0].destination, u"b(c");
    EXPECT_EQ(r.links[1].destination, u"e)");
    EXPECT_EQ(r.links[1].full, (Span{10, 8}));

    // []([]([](<a href="y">x</a>: the three that never close leave the last one a link.
    r = show(u"[]([]([]([x](y)");
    ASSERT_EQ(r.links.size(), 1U);
    EXPECT_EQ(r.links[0].destination, u"y");
    EXPECT_EQ(r.links[0].full, (Span{9, 6}));
}

TEST(S022_InlineLinks, ATitleEndsAtItsDelimiterOrThereIsNoLink) {
    // <a href="b%20c" title="t">a</a> <a href="e" title="f">d</a> <a href="h" title="i">g</a>
    auto r = show(u"[a](<b c> \"t\") [d](e 'f') [g](h (i))");
    ASSERT_EQ(r.links.size(), 3U);
    EXPECT_EQ(r.links[0].destination, u"b c");
    EXPECT_EQ(r.links[0].title, u"t");
    EXPECT_EQ(r.links[1].destination, u"e");
    EXPECT_EQ(r.links[1].title, u"f");
    EXPECT_EQ(r.links[2].destination, u"h");
    EXPECT_EQ(r.links[2].title, u"i");
    EXPECT_EQ(r.links[2].full, (Span{26, 10}));

    // [a](b "t" <a href="d">c</a>: a title without `)` behind it.
    r = show(u"[a](b \"t\" [c](d)");
    ASSERT_EQ(r.links.size(), 1U);
    EXPECT_EQ(r.links[0].destination, u"d");
    EXPECT_EQ(r.links[0].full, (Span{10, 6}));

    // [a](b (t <a href="d">c</a>: a title in parentheses that does not end before the next link.
    r = show(u"[a](b (t [c](d)");
    ASSERT_EQ(r.links.size(), 1U);
    EXPECT_EQ(r.links[0].destination, u"d");
    EXPECT_EQ(r.links[0].full, (Span{9, 6}));
}

// <a href="u"><em>a</em></a> <em>b <a href="d">c</a> e</em>
TEST(S022_InlineLinks, EmphasisInsideALinkAndAroundALink) {
    const auto r = show(u"[*a*](u) *b [c](d) e*");
    ASSERT_EQ(r.links.size(), 2U);
    EXPECT_EQ(r.links[0].destination, u"u");
    EXPECT_EQ(r.links[1].destination, u"d");
    EXPECT_EQ(r.inlineStyles, (std::vector<InlineStyleRun>{
                                  {{2, 1}, InlineStyle::Italic},
                                  {{10, 2}, InlineStyle::Italic},
                                  {{13, 1}, InlineStyle::Italic},
                                  {{18, 2}, InlineStyle::Italic},
                              }));
}
