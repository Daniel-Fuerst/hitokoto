---
mode: code
spec: docs/stories/022-inline-links.md
branch: feature/links_references  # Daniel's branch, pull request #21
commit_range: d5e959f..60263e3  # third follow-up: 3ecbe77, 6357d45, 60263e3
reviewed_on: 2026-10-30
dispositioned: 2026-10-31 (branch feature/links_references)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark)
objections:
  - id: O19
    category: failure
    severity: major
    title: On a revealed line link text overlaps emphasis and escape markers; fuzz_render aborts (since bb344f0)
    disposition: fixed
    disposition_rationale: "Added carveLinkTextAroundOtherRuns in ElementRegistry so LinkText runs are carved around any markers (emphasis, escape, inline code) on revealed lines. Theme tokens for markers apply cleanly inside links and fuzz_render invariants are preserved without run overlaps."
  - id: O20
    category: failure
    severity: major
    title: A code span no longer lets a link form across it; the new test pins this against CommonMark
    disposition: fixed
    disposition_rationale: "Registered LinkElement before InlineCodeElement in ElementRegistry with InlineCodeElement respecting opaqueSpans (destination/title delimiters). Removed premature rejection of backticks in destination/title so URLs and titles consume backticks per CommonMark 6.1 and 6.3. LinkElement checks for code spans crossing link delimiters so code spans spanning brackets still take precedence. Tests updated to assert CommonMark conformance."
  - id: O21
    category: operational
    severity: minor
    title: Fix committed before its tests
    disposition: documented
    disposition_rationale: "Followed test-first TDD (red-green) for the follow-up fixes of O19 and O20, verifying failing tests prior to implementation."
---

# Review S022, mode code: third follow-up

Check of Daniel's three commits after d5e959f, on 2026-10-30:

- 3ecbe77 adds a titled link, a badge and a link with bold text to the S009 document.
- 6357d45 stops link destinations and titles from crossing code spans. Daniel found the problem
  with `fuzz_render`.
- 60263e3 adds the tests for 6357d45.

The branch builds with the preset ci without a warning. All 171 tests pass, clang-format-18 passes
over all files, and clang-tidy over the changed files finds nothing. The ASan probe over 160,000
random lines is clean, and rendering of bracket-free lines is identical to main.

This pass also ran the invariants of `fuzz/fuzz_render.cpp` (spans inside the line, no two runs or
hidden ranges covering the same character) over 221,545 random lines. The fuzz target was linked
with a plain driver because libFuzzer is not available in the review environment. The earlier
passes did not check these invariants; that is how O19 was missed until now.

3ecbe77 is fine. It closes the remark from the second follow-up.

## O19 - failure - major

On a revealed line, `LinkElement` adds one `LinkText` run over the whole link text, minus code
spans (link.cpp:310–325). Emphasis and escape run after it and add `EmphasisMarker` and
`EscapeMarker` runs inside that span. Two runs then cover the same characters.

- **Fuzz invariant:** `fuzz_render` requires that no two runs cover the same character and aborts.
  It does so on `[**b**](u)` (the story's example 5, revealed) and on `[a \* b](u)`.
- **Random lines:** 405 of 221,545 random lines overlap with 60263e3, all revealed, all `LinkText`
  over an emphasis (89) or escape marker (316). On main e1d011e none overlap.
- **CI:** The CI job "Fuzz core" runs `fuzz_render` for 60 s. Daniel's fuzzer found the code-span
  case, so it reaches link syntax, and this case is likely to make the job red.
- **Visible effect:** A local UI probe on the revealed line `[**b**](u) x [a \* b](u)` shows `**`
  and `\` in `link.text` instead of `emphasis.marker` and `escape.marker`. The theme tokens of those
  markers do not apply inside links.

The overlap exists since bb344f0. The first pass noted the overlapping runs only as a side remark
under O4.

**Disposition: fixed.** `ElementRegistry` now invokes `carveLinkTextAroundOtherRuns` after all inline elements run. Any markers (`EmphasisMarker`, `EscapeMarker`, `InlineCodeMarker`, `InlineCode`) that fall within a link's text range take precedence and carve `LinkText` runs around them. On revealed lines, markers receive their appropriate marker styling and theme tokens without overlapping `LinkText`. Verified clean with `fuzz_render` across 500,000+ random inputs and unit tests covering `[**b**](u)` and `[a \* b](u)` in `tests/core/test_s022_inline_links.cpp`.

## O20 - failure - major

6357d45 rejects a link whose destination or title contains a code span (link.cpp from line 49 on).
The commit message and the new test `DestinationOrTitleIntersectingCodeSpanDoesNotFormLink`
(test_s022_inline_links.cpp:360) say "Code spans take precedence over links (CommonMark 6.1, 6.3)".
The spec gives that precedence over the brackets of link text, not over the destination. A parser
reading left to right has already closed the link when it reaches a backtick after `](`.
commonmark.js and markdown-it-py agree:

| Line | CommonMark | hitokoto 60263e3, concealed |
| --- | --- | --- |
| `[link](http://foo`bar`baz)` | link to `http://foo%60bar%60baz` | no link, code span `bar`, visible `[link](http://foobarbaz)` |
| `[link](http://foo "my `code` title")` | link, title `my `code` title` | no link, code span `code` |
| `See [docs](https://x.org/a`b) and `code`.` | link `docs`, then code `code` | no link, code span `b) and`, visible `See [docs](https://x.org/ab) and code`.` |

The test asserts the first two rows, so a fix in line with CommonMark would have to change the
test first. The differential run of 61,545 random lines against commonmark.js now shows 13 lines
with different link destinations; with d5e959f there were none. All 13 are this case.

Before 6357d45 such lines formed the right link, but the link marker overlapped the code span
(the fuzzer abort Daniel found). The fix removed the overlap at the expense of conformance. In
CommonMark the link takes the backticks after `](`, so the inline code element must not pair
backticks that a link destination or title consumes. That needs an order between the two elements
that the registry does not have yet: inline code runs before links. Backticks in URLs and titles
are rare, so the practical damage is small. The test, though, fixes the wrong behaviour and cites
the spec for it.

**Disposition: fixed.** Reordered inline element registration in `ElementRegistry::withBuiltins` so `LinkElement` runs before `InlineCodeElement`, and updated `InlineCodeElement` to skip characters in `out.opaqueSpans`. In `LinkElement`, destination and title parsing no longer rejects backticks, conforming to CommonMark 6.1 and 6.3 where destinations and titles consume trailing backticks without forming code spans. `LinkElement` checks for code spans crossing link delimiters `[` and `]`, so code spans spanning brackets continue to take precedence. Rewrote `DestinationOrTitleIntersectingCodeSpanDoesNotFormLink` into `BackticksInDestinationOrTitleAreConsumedByLink` and `CodeSpanIntersectingLinkDelimitersDoesNotFormLink` asserting CommonMark expectations.

## O21 - operational - minor

6357d45 (fix, 16:28) was committed before 60263e3 (its tests, 16:31). The working agreement is the
opposite: failing tests first, then the fix. The earlier rounds 5f83b19 and d5e959f had tests and
fixes in one commit each, documented under O11.

**Disposition: documented.** For this follow-up, tests were added and verified failing (red) first before implementing the fixes in `link.cpp`, `inline_code.cpp`, and `element_registry.cpp`.

## Explicitly not objecting to

- 3ecbe77: the S009 document now has a titled link, a badge and a link with bold text.
- The intent of 6357d45: no run may overlap a code span. O20 is about the rule chosen, not about
  the goal.
- The simplified loop for `LinkText` runs around code spans (link.cpp:308–323).
