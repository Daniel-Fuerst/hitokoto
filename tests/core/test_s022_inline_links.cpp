// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links and titles.
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace hitokoto::core;

namespace {
const ElementRegistry& registry() {
    static const ElementRegistry r = ElementRegistry::withBuiltins();
    return r;
}

LineRender show(std::u16string_view line, bool revealed = false) {
    return registry().render(line, {revealed});
}
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
                            {{16, 22}, HideMode::Collapse},
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
                            {{18, 56}, HideMode::Collapse},
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
                            {{31, 59}, HideMode::Collapse},
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
                            {{43, 52}, HideMode::Collapse},
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
                            {{60, 22}, HideMode::Collapse},
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
                                {{6, 7}, HideMode::Collapse},
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
                                {{14, 7}, HideMode::Collapse},
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
                                {{4, 4}, HideMode::Collapse},
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
                            {{9, 7}, HideMode::Collapse},
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
                            {{27, 23}, HideMode::Collapse},
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
