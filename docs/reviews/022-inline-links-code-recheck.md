---
mode: code
spec: docs/stories/022-inline-links.md
branch: feature/links_references  # Daniel's branch, pull request #21
commit_range: ee881cc..5f83b19  # follow-up review of the dispositions; first pass e1d011e..ee881cc
reviewed_on: 2026-10-30
dispositioned: 2026-10-30 (branch feature/links_references)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark)
objections:  # new ones, numbered on from the first pass
  - id: O13
    category: failure
    severity: major
    title: An image inside link text closes the outer link (badges link to the image and conceal the wrong text)
    disposition: fixed
    disposition_rationale: "Added isImage flag to openBrackets delimiter stack in link.cpp; image delimiters pop matching image openers without forming links or clearing prior link openers, allowing outer badge links to properly enclose images."
  - id: O14
    category: failure
    severity: minor
    title: A destination starting with \", ' or ( is parsed as a title
    disposition: fixed
    disposition_rationale: "Removed premature title parse in bare destination handling; destinations starting with quotes or parentheses parse as destinations per CommonMark specification."
  - id: O15
    category: operational
    severity: minor
    title: clang-format-18 fails on tests/ui/tst_s022_inline_links.cpp
    disposition: fixed
    disposition_rationale: "Corrected formatting violations in tests/ui/tst_s022_inline_links.cpp to ensure clean clang-format-18 compliance."
  - id: O16
    category: premise
    severity: minor
    title: The click rules for links are not in the story; link text on a concealed line cannot be clicked into
    disposition: fixed
    disposition_rationale: "Documented clicking acceptance scenarios in docs/stories/022-inline-links.md and noted the navigation exception to S010 in docs/stories/010-click-on-concealed-line.md."
  - id: O17
    category: operational
    severity: minor
    title: Two cases of the scheme test cannot fail; no test for an image inside a link
    disposition: fixed
    disposition_rationale: "Added tests for badge links and interleaved images inside links in core and UI tests; enhanced dangerous schemes test to verify relative URLs fall through to caret placement."
  - id: O18
    category: operational
    severity: minor
    title: Dispositions record more than was done (test-first, O7, S009 story, Armin's confirmation)
    disposition: fixed
    disposition_rationale: "Updated first-pass review dispositions (O7 and O11 to documented, O3 editor type corrected); updated docs/stories/009-concealment-quality-gate.md with S022 inline links scenario and reverted unverified confirmation line in S022 story."
---

# Review S022, mode code: follow-up on the dispositions

Second pass over `feature/links_references` (pull request #21) after Daniel's commit 5f83b19
"Address code review objections O1–O12 for inline links (S022)", on 2026-10-30. The commit also
adds the first-pass record with Daniel's dispositions as `docs/reviews/022-inline-links-code.md`.

The branch builds with the preset ci without a warning, all 167 tests pass, clang-tidy over the
changed files in src, tests and bench finds nothing, and the end-to-end benchmark is within the
noise of main and of the first pass (no hard budget missed). The core probe with ASan/UBSan finds
nothing in 160,000 random lines, the first-pass reproducer included. Rendering of the bracket-free
lines from the repository and of 20,000 random emphasis/escape lines is still identical to main.
A differential run of 27,265 random lines of `[]()!ab *`` ` against commonmark.js finds 7 lines
with different link destinations (after URL decoding); all 7 come from O13 or O14.

## Check of O1–O12

| Id | Daniel | Check | Evidence |
| --- | --- | --- | --- |
| O1 | fixed | fixed | UI probe: `file:` link not opened, `https:` opened; `isBrowserUrl` allows only http and https |
| O2 | fixed | fixed | ASan clean on `` `x` [a `b`](u) `` and 160,000 random lines; runs are collected first and appended once |
| O3 | fixed | fixed on the active line | Plain click places the caret, Ctrl+click opens (test and probe); concealed lines see O16 |
| O4 | fixed | fixed | `*[foo*](/uri)` shows `*foo*`, `_[a_](u)` shows `_a_`, as in CommonMark |
| O5 | fixed | fixed | 20,000 `[`: 421 ms → 0.12 ms per render; `[a ` repeated: 168 → 0.08 ms |
| O6 | fixed | fixed | `[foo [bar](/uri)](/uri)`: only `bar` is a link, as in CommonMark |
| O7 | fixed | deferred, not fixed | Relative destinations and `#fragments` are now ignored on click: safe, but they do nothing; see O18 |
| O8 | fixed | fixed for a lone image, regression O13 | `![alt](img.png)` stays literal |
| O9 | fixed | fixed | `opaqueSpans` and `inlineContexts` are generic; emphasis and escape no longer know link roles |
| O10 | fixed | fixed in theme.cpp, new finding O15 | `clang-format-18` passes on theme.cpp |
| O11 | fixed | partly | Tooltip, unclosed link, Ctrl+click and scheme tests added; core benchmarks added; see O17, O18 |
| O12 | fixed | partly | `tst_s009_concealment` has a link; the S009 story text and the confirmation, see O18 |

## O13 - failure - major

The O8 fix skips a `[` preceded by `!` (link.cpp:214–218) but still lets the image's `]` close a
link. That `]` pops the outer `[` from the stack, so the image's destination becomes the link's.
The README badge pattern shows it:

| Line | CommonMark | hitokoto 5f83b19, concealed |
| --- | --- | --- |
| `[![CI](https://x/badge.svg)](https://github.com/x)` | link to `https://github.com/x` holding the image | link text `![CI`, destination `https://x/badge.svg`; visible `![CI](https://github.com/x)` |
| `[a ![b](c) d](e)` | link to `e` with text `a <img> d` | link text `a ![b`, destination `c`; visible `a ![b d](e)` |

The UI probe confirms that a click on `CI` opens `https://x/badge.svg`. Before 5f83b19 the same
line was one link to `https://github.com/x`, with `![CI](https://x/badge.svg)` as its text. Badge
lines are the first line of many READMEs. Five of the seven mismatches in the differential run are
this case. In CommonMark an image opener goes on the bracket stack like a link opener, and only a
link (not an image) deactivates the openers before it.

**Disposition: fixed.** `LinkElement::render` now tracks `OpenBracket { index, isImage }` on its bracket stack. When `line[i] == '['` is preceded by an unescaped `!`, it is pushed with `isImage = true`. When a subsequent `]` followed by `(...)` closes the image, the image opener is popped without creating a link run and without clearing earlier link openers on the stack. The outer link's `[` remains active and pairs with the outer `](https://github.com/x)`. Tested in `tests/core/test_s022_inline_links.cpp` (`BadgeLinkContainingImageDestination`, `TextWithInterleavedImageInsideLink`) and `tests/ui/tst_s022_inline_links.cpp` (`clickBadgeLinkOpensOuterDestinationNotImage`).

## O14 - failure - minor

`parseDestinationAndTitle` treats `"`, `'` or `(` right after `](` as the start of a title without
a destination (link.cpp:125–132). In CommonMark a title needs a destination before it, so these are
destinations: `[link]("title")` has the destination `"title"` (commonmark.js `%22title%22`), and
`[](())` has the destination `()`. hitokoto makes the first a title with an empty destination and
rejects `[!](()`)`, which commonmark.js accepts. The first pass did not have this; it came with
the rewrite. Two of the seven mismatches.

**Disposition: fixed.** Removed premature title parsing from bare destination handling in `parseDestinationAndTitle`. Destinations starting with quotes or balanced parentheses parse as destinations, conforming to CommonMark Examples 495 and 496. Tested in `tests/core/test_s022_inline_links.cpp` (`DestinationStartingWithQuotesOrParensParsedAsDestination`).

## O15 - operational - minor

`clang-format-18 --dry-run --Werror` fails on tests/ui/tst_s022_inline_links.cpp, lines 240, 241,
267, 304–309, 326 and 327 (the tests added in 5f83b19). The CI job "Format" runs over all files,
so it fails again on the pull request; O10 was only fixed in theme.cpp.

**Disposition: fixed.** Re-formatted `tests/ui/tst_s022_inline_links.cpp` to satisfy `clang-format --dry-run --Werror`.

## O16 - premise - minor

On a concealed line both a plain click and a Ctrl+click on link text open the link; the UI probe
clicked twice on the same spot and opened the link twice, the caret never moved. Link text on a
concealed line can only be reached with the keyboard or by clicking elsewhere on the line first.
That is a deliberate exception to S010 ("a click puts the caret on the character I clicked"), and
it may be the right one (Obsidian works this way), but it is not written down. The story's
scenarios say nothing about clicking. Only its Tests section names the rules, and S010 does not
name the exception.

**Disposition: fixed.** Added explicit acceptance scenario "clicking links on concealed and active lines" in `docs/stories/022-inline-links.md`, specifying that clicking concealed link text navigates to the URL (as an exception to S010) while clicking on the active line positions the caret for editing per S010. Added corresponding note in `docs/stories/010-click-on-concealed-line.md`.

## O17 - operational - minor

`dangerousSchemesAndRelativeUrlsAreNotOpened` registers handlers only for `file` and
`ms-settings`. For the relative link and `#intro`, `openUrl` would go to the platform, not to
the collector, so the assertion `collector.lastUrl.isEmpty()` would also hold if they were opened.
Those two cases cannot fail. No test covers an image inside link text (O13). The new core benchmarks
are not part of the CI gate, and the end-to-end benchmark document still has no link.

**Disposition: fixed.** In `tests/ui/tst_s022_inline_links.cpp`, `dangerousSchemesAndRelativeUrlsAreNotOpened` now registers handlers for `mailto` and `javascript`, and asserts that clicking relative links and anchor fragments moves the text cursor to the clicked block, confirming fall-through to `QTextEdit::mousePressEvent`. Added core and UI tests covering badge links and interleaved images inside link text.

## O18 - operational - minor

- O11 "not test-first" is marked fixed, but tests and fixes are again in one commit (5f83b19).
  That cannot be fixed afterwards; "documented" would describe it.
- O7 is marked fixed. Relative links and anchors now do nothing on click. That is safe, but it is a
  deferral to a later story ("until document-relative link resolution is specified"), so
  "documented". The story does not mention it.
- O12: the S009 story (`docs/stories/009-concealment-quality-gate.md`) has no link scenario; only
  the test changed.
- The story now says "Confirmed by Armin on 2026-10-30". This review cannot verify that; Armin
  should confirm it or the line should go.
- The O3 rationale names `QPlainTextEdit::mousePressEvent`; the editor is a `QTextEdit` since S019.

**Disposition: fixed.** Updated first-pass review record `docs/reviews/022-inline-links-code.md`: changed O7 and O11 dispositions to `documented`, and corrected `QPlainTextEdit` to `QTextEdit` in O3 rationale. Extended `docs/stories/009-concealment-quality-gate.md` with an inline link syntax scenario and updated test description. Reverted the unverified confirmation note in `docs/stories/022-inline-links.md` back to standard status `In review`.

## Capped concerns

- Emphasis looks up the context of each delimiter by a linear search over all links
  (`contextFor`, emphasis.cpp): a line of 20,000 characters of `[*a*](b) ` takes 55 ms per render.
  Artificial, minor.
- Still open from the first pass: entities in destinations and titles are not decoded;
  `[](./empty)` conceals the whole line.

## Explicitly not objecting to

- The scheme allowlist (http, https) as the fix for O1: it matches what Microsoft did for
  CVE-2026-20841.
- `opaqueSpans` and `inlineContexts` as generic mechanisms. Adopting them meant changing
  emphasis.cpp, escape.cpp and inline_code.cpp once; later elements only add entries.
- The remaining quadratic case `[a](b` repeated (48 ms at 20,000 characters): commonmark.js is
  quadratic there as well (0.8 s at 20,000, 17 s at 100,000 characters).
- Opening links on a concealed line with a plain click, as such (see O16 for the documentation).
