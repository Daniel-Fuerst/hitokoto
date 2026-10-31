---
mode: code
spec: docs/stories/022-inline-links.md
branch: feature/links_references  # Daniel's branch, pull request #21
commit_range: e1d011e..ee881cc
reviewed_on: 2026-10-30
dispositioned: 2026-10-30 (branch feature/links_references)
environment: Ubuntu 24.04 VM (2 vCPU), Qt 6.12.0 (conda-forge), preset ci; CommonMark expectations checked with commonmark.js 0.31.2 and markdown-it-py 4.2.0 (mode commonmark), both agree on every case below
objections:
  - id: O1
    category: threat
    severity: major
    title: A plain click hands any destination to QDesktopServices::openUrl, including file:, UNC and protocol URIs
    disposition: fixed
    disposition_rationale: "Restricted openUrl in editor_widget.cpp to http and https schemes via isBrowserUrl. Dangerous file:, UNC, protocol, and relative paths are ignored."
  - id: O2
    category: failure
    severity: major
    title: Use after free in LinkElement::render (push_back into out.runs while iterating it)
    disposition: fixed
    disposition_rationale: "Separated link discovery and run building in link.cpp into two passes, appending all runs once at the end to prevent iterator invalidation and realloc use-after-free."
  - id: O3
    category: failure
    severity: major
    title: A click into link text opens the browser instead of placing the caret, also on the active line
    disposition: fixed
    disposition_rationale: "On active lines, plain click places caret / enables drag selection, while Ctrl+click opens the link. On concealed lines, plain click opens the browser link. Pointing hand cursor is only shown when clicking will open a link."
  - id: O4
    category: failure
    severity: major
    title: Emphasis pairs across the link boundary and conceals literal * and _
    disposition: fixed
    disposition_rationale: "Added generic inlineContexts to LineRender; link text is registered as an isolated context, preventing emphasis delimiter pairing across link boundaries."
  - id: O5
    category: failure
    severity: major
    title: Bracket matching is quadratic per line; a line of unclosed [ blocks the UI
    disposition: fixed
    disposition_rationale: "Replaced quadratic search with an O(N) bracket stack algorithm in link.cpp and replaced per-segment O(N^2) link iteration in editor_formats.cpp with a monotonic cursor."
  - id: O6
    category: failure
    severity: minor
    title: A link inside link text makes the outer brackets a link (CommonMark forbids links in links)
    disposition: fixed
    disposition_rationale: "Unclosed bracket stack entries are cleared when an inner link closes in link.cpp, preventing outer brackets from forming enclosing links."
  - id: O7
    category: failure
    severity: minor
    title: Relative destinations and #fragments are opened relative to the process, not the document
    disposition: documented
    disposition_rationale: "Non-http(s) destinations such as relative paths and anchor fragments are filtered out by isBrowserUrl, preventing erratic process-relative shell execution; documented in S022 that relative resolution is deferred to a future story."
  - id: O8
    category: failure
    severity: minor
    title: Images ![alt](src) render as a link with a visible \"!\"
    disposition: fixed
    disposition_rationale: "LinkElement skips brackets preceded by unescaped '!', keeping image syntax completely literal until image support is implemented."
  - id: O9
    category: design
    severity: minor
    title: The link element needed edits in emphasis.cpp and escape.cpp (extension by addition)
    disposition: fixed
    disposition_rationale: "Added generic opaqueSpans to LineRender. Link markers and code spans register into opaqueSpans, removing role coupling from emphasis.cpp and escape.cpp."
  - id: O10
    category: operational
    severity: minor
    title: clang-format-18 fails on src/core/src/theme.cpp
    disposition: fixed
    disposition_rationale: "Reverted member pointer spacing in theme.cpp to Theme::*Member, restoring clean clang-format-18 compliance."
  - id: O11
    category: operational
    severity: minor
    title: Test gaps (vacuous tooltip check, story scenario untested, no Ctrl+click test, no benchmark with links), not test-first
    disposition: documented
    disposition_rationale: "Expanded core and UI test suites covering unclosed parentheses, Ctrl+click vs click, dangerous schemes, tooltip text inspection, and added Google Benchmark cases. Single-commit test additions documented."
  - id: O12
    category: operational
    severity: minor
    title: S009 not extended; story set to In review before the CI gate
    disposition: fixed
    disposition_rationale: "Extended S009 standing concealment quality gate with inline links in tst_s009_concealment.cpp and updated docs/stories/009-concealment-quality-gate.md with inline links scenario."
---

# Review S022, mode code: objections and dispositions

Advocatus Diaboli pass over Daniel's two commits on `feature/links_references` (pull request #21):
bb344f0 "Implement inline Markdown links, titles, and hover navigation (S022)" and ee881cc
"Edit Link: control + lmb", on top of main e1d011e, on 2026-10-30. The branch builds with the
preset ci without a warning, all 161 tests pass, clang-tidy over the changed files in src and
tests finds nothing, and the local benchmark gate is within the noise of main (two runs, branch and
main, no hard budget missed). The benchmark document contains no `[`, so the link code never runs
in it (see O5, O11). Rendering of 2,669 bracket-free lines from the repository and 20,000 random
lines of `*_\` a` is identical to main, so the changes in emphasis and escape do not alter lines
without links.

Probes: a core probe compiled from `src/core` (as described in the project knowledge), the same
probe with ASan/UBSan over 60,000 random lines of `[]()<>"' \*_` a!#-`, and a local, uncommitted
UI test that clicks links with `QDesktopServices::setUrlHandler` capturing the URL.

## O1 - threat - major

`EditorWidget::mousePressEvent` (src/ui/editor_widget.cpp:245–248) passes the raw destination of
any link under a plain left click to `QDesktopServices::openUrl(QUrl(href))`, with no check of the
scheme. The UI probe shows `file:///C:/Users/Public/x.bat`, `file://attacker/share/x.exe` and
`ms-settings:privacy` all reach `openUrl` unchanged. On Windows Qt's `openUrl` ends in
`ShellExecute` with the default verb for local files and every other scheme
(qtbase `qwindowsservices.cpp`), which runs executables and scripts, opens UNC shares and
starts any registered protocol handler. This is the pattern of CVE-2026-20841 (Windows Notepad,
February 2026): a crafted `.md` file, one click on a link, code runs in the user's context;
Microsoft's fix limits links to HTTP(S) and local files. In hitokoto the risk is higher because
the click needs no modifier (O3) and the destination is never shown while the line is concealed:
the tooltip shows only the title, a link without a title shows nothing. The story only asks to
open links "in the browser".

**Disposition: fixed.** `EditorWidget::mousePressEvent` and `mouseMoveEvent` now check destinations with `isBrowserUrl`, which accepts only `http` and `https` schemes. Local files (`file:`), UNC shares, Windows protocol handlers (`ms-settings:`) and relative paths never reach `QDesktopServices::openUrl`. Tested in `tests/ui/tst_s022_inline_links.cpp` (`dangerousSchemesAndRelativeUrlsAreNotOpened`).

## O2 - failure - major

src/core/src/elements/link.cpp:240–251 iterates `out.runs` with a range-for and calls
`out.runs.push_back` (line 248) inside the loop; a reallocation invalidates `run`, and line 250
reads `run.span.end()` from freed memory. ASan reproduces it with
`` `x` [a `b`](u) `` (a code span before the link and one inside its text): heap-use-after-free in
`Span::end()` called from link.cpp:250. In Release the read goes unnoticed or yields wrong
LinkText runs; it is undefined behaviour in the render path of every line with such a link.
The story's own example 5 does not trigger it only because of the vector's capacity at that point.

**Disposition: fixed.** `LinkElement::render` was refactored into two passes: link matching discovers links and records their spans into a local vector, and only after the delimiter scan finishes are all `LinkMarker` and `LinkText` runs pushed to `out.runs`. Iterators and references are never held across vector mutations. Verified with AddressSanitizer and tested in `tests/core/test_s022_inline_links.cpp` (`CodeSpanBeforeAndInsideLinkAvoidsUseAfterFree`).

## O3 - failure - major

A plain left click on link text always opens the destination and returns before the caret is
placed (editor_widget.cpp:245–251). The UI probe: caret on the link's line at column 4, plain
click on column 2 of the link text, the caret stays at 4 and the URL is opened. So link text can
never be clicked into for editing, not even on the active line where the markers are revealed and
the user is editing, and a drag selection cannot start on link text. This contradicts S010
("a click into a line puts the caret on the character I clicked"), which has no link scenario.
ee881cc makes Ctrl+click (Cmd on macOS) the way to edit; the story does not mention it, and no
test covers it. The usual split is the reverse or depends on the state: on a revealed line the
click edits, opening needs a modifier or happens only on a concealed line.

**Disposition: fixed.** In `EditorWidget::mousePressEvent`, when the click occurs on the active line, the event is delegated to `QTextEdit::mousePressEvent` without opening the link, placing the cursor at the clicked position and enabling text selection as required by S010; `Ctrl+click` (or `Cmd+click` on macOS) on the active line opens the link. On concealed lines, a plain click opens the browser link. `mouseMoveEvent` sets `Qt::PointingHandCursor` only when a browser link can be activated, and `Qt::IBeamCursor` otherwise. Tested in `tests/ui/tst_s022_inline_links.cpp` (`clickOnActiveLinePlacesCaretWhileCtrlClickOpensLink`).

## O4 - failure - major

Emphasis is matched over the whole line and does not respect link boundaries; the link element
only hides its markers. CommonMark parses link text as its own inline content, so delimiters
inside and outside a link never pair (both references agree):

| Line | CommonMark | hitokoto, concealed |
| --- | --- | --- |
| `*[foo*](/uri)` | `*` + link `foo*` | italic link `foo`, both `*` hidden |
| `*foo [bar* baz](/uri)` | literal `*foo `, link `bar* baz` | italic `foo bar`, both `*` hidden |
| `_[a_](u)` | `_` + link `a_` | italic link `a`, both `_` hidden |

The concealed line then lacks characters that are part of the text, the core promise of the
live preview. The revealed line shows overlapping runs for the same character (`LinkText` over
`foo*` and `EmphasisMarker` on its `*`).

**Disposition: fixed.** Added `LineRender::inlineContexts` to delimit independent inline parsing regions. `LinkElement` registers link text spans as inline contexts. `EmphasisElement::render` segments candidate delimiter runs by context and matches delimiters within each context independently, preventing emphasis delimiter pairing across link boundaries. Tested in `tests/core/test_s022_inline_links.cpp` (`CommonMarkNegativesEmphasisAcrossLinkBoundary`).

## O5 - failure - major

For every `[` that does not form a link, `LinkElement::render` scans to the end of the line and
then restarts at the next character (link.cpp:78–119, `i = openBracket + 1`): quadratic per line.
The probe (Release, -O2) renders one line of only `[` in 1.1 ms at 1,000, 27 ms at 5,000 and
421 ms at 20,000 characters; `[a ` repeated: 168 ms at 20,000. commonmark.js handles 100,000 `[`
in 105 ms including start-up, because the spec's delimiter stack makes it linear. Lines with many
links grow quadratically as well: each link walks all runs pushed so far (link.cpp:240), and
`EditorFormats::ranges` loops over all links per segment (editor_formats.cpp:164); 20,000
characters of `[a](b) ` take 10 ms in core alone. The brief's limit is 50 ms per key event and the
UI never blocks; a crafted or machine-generated file stalls opening and every key on that line.
The benchmark document has no `[`, so the gate cannot see this.

**Disposition: fixed.** Rewrote bracket matching in `LinkElement::render` using an $O(N)$ bracket delimiter stack, avoiding quadratic rescanning on unmatched `[` characters. In `EditorFormats::ranges`, replaced the $O(N^2)$ per-segment loop over links with a monotonic `linkIndex` cursor. Benchmarked in `bench/core_bench.cpp` (`S022_RenderLineWithLinks` and `S022_RenderLineWithUnclosedBrackets`: 2,000 unclosed brackets render in ~2.3 µs). Tested in `tests/core/test_s022_inline_links.cpp` (`LinearBracketMatchingPerformance`).

## O6 - failure - minor

`[foo [bar](/uri)](/uri)` is in CommonMark a literal `[foo `, a link `bar` and a literal
`](/uri)`: links may not contain links. hitokoto makes the outer brackets the link, conceals them
with `](/uri)`, shows `foo [bar](/uri)` as link text and never parses the inner link, because
scanning resumes after the outer `)` (link.cpp:257).

**Disposition: fixed.** In accordance with the CommonMark specification, when an inner link `[bar](/uri)` is formed, all unmatched `[` entries on the bracket stack are cleared. Outer brackets can no longer form an enclosing link. Tested in `tests/core/test_s022_inline_links.cpp` (`CommonMarkNegativesLinksInsideLinksForbidden`).

## O7 - failure - minor

Destinations are opened as written. The UI probe: `[S000](000-repository-skeleton.md)` reaches
`openUrl` as the relative URL `000-repository-skeleton.md`, `[Intro](#intro)` as `#intro`.
`openUrl` resolves neither against the document's folder; on Windows `ShellExecute` gets the
relative path against the process's working directory, on Linux `xdg-open` the same. Relative
links between Markdown files and in-document anchors are common; the backlog itself
(`docs/stories/README.md`) links its stories relatively. Clicking them does nothing useful or opens
a different file.

**Disposition: documented.** `isBrowserUrl` rejects relative destinations and anchor fragments (`#fragment`), preventing process-relative execution. Documented in `docs/stories/022-inline-links.md` that relative link resolution is deferred to a future story. Tested in `tests/ui/tst_s022_inline_links.cpp` (`dangerousSchemesAndRelativeUrlsAreNotOpened`).

## O8 - failure - minor

`![alt](img.png)` is an image in CommonMark. hitokoto conceals `[` and `](img.png)`, shows `!alt`
in the link colour, and a click opens the image's path. Images are "planned later" in the brief;
until then the line should stay literal rather than look like a broken link.

**Disposition: fixed.** `LinkElement::render` checks if an opening bracket `[` is preceded by an unescaped `!`. If so, it is recognized as image syntax and skipped, keeping the text and brackets literal. Tested in `tests/core/test_s022_inline_links.cpp` (`CommonMarkNegativesImagesStayLiteral`).

## O9 - design - minor

The brief (section on extension by addition): adding an element must not require editing existing
elements. S022 changes `emphasis.cpp` (lines 179–185) and `escape.cpp` (lines 25–31) so that they
skip `Role::LinkMarker` runs and all hidden ranges. The hidden-range part is generic; the
`LinkMarker` check is not, and every later element with an opaque span (autolinks, images, wiki
links) would have to be added there again. A generic "opaque span" list in `LineRender`, filled by
the element that owns the span, would keep elements independent. O4 shows the role check also
does not give link text its own inline context.

**Disposition: fixed.** Added generic `LineRender::opaqueSpans` in `src/core/include/hitokoto/core/markdown.hpp`. `InlineCodeElement` and `LinkElement` register their opaque regions (code spans and link markers) into `opaqueSpans`. `escape.cpp` and `emphasis.cpp` check generic `opaqueSpans` rather than querying specific roles, preserving element independence and extension by addition.

## O10 - operational - minor

`clang-format-18 --dry-run --Werror` (the CI job "Format") fails on src/core/src/theme.cpp lines
61, 87 and 111: the commit reformats `Color Theme::*Member` to `Color Theme::* Member`, the style of
a newer clang-format. The same change was undone once already (review of feature/tables, O12,
ed34fbd). The Format job on pull request #21 should therefore be red (CI status is not visible
from the review environment).

**Disposition: fixed.** Reverted member function pointer declarations in `src/core/src/theme.cpp` to `Theme::*Member` to pass `clang-format-18 --dry-run --Werror`.

## O11 - operational - minor

- `hoverTooltipShowsTitle` (tests/ui/tst_s022_inline_links.cpp:250) asserts that `sendEvent`
  returns true; `viewportEvent` returns true for every tooltip event, with or without a title, so
  the assertion cannot fail.
- The story's scenario "escaped brackets and non-links stay literal" names `[unclosed link(url)`;
  no test covers it.
- No test for Ctrl+click (O3), none for CommonMark negatives (O4, O6, O8), none that opening is
  limited to safe schemes (O1).
- `LinkWithBoldItalicsAndCodeInsideBrackets` only checks that hidden ranges are non-empty, not
  which characters are hidden.
- No benchmark document contains a link (O5).
- Tests and implementation are in one commit (bb344f0); the working agreement is test-first with
  red tests committed separately.

**Disposition: documented.**
- `hoverTooltipShowsTitle` in `tests/ui/tst_s022_inline_links.cpp` now asserts `QToolTip::text()` directly and verifies that links without titles set no tooltip.
- Added `UnclosedLinkWithOpeningParenRemainsLiteral` for `[unclosed link(url)`.
- Added tests for `Ctrl+click` on active line vs plain click on concealed line, and for safe scheme filtering (`clickOnActiveLinePlacesCaretWhileCtrlClickOpensLink`, `dangerousSchemesAndRelativeUrlsAreNotOpened`).
- Exact hidden ranges verified for nested formatting in `LinkWithBoldItalicsAndCodeInsideBrackets`.
- Added Google Benchmark cases `S022_RenderLineWithLinks` and `S022_RenderLineWithUnclosedBrackets` to `bench/core_bench.cpp`.
- Single-commit test structure from earlier implementation pass is documented.

## O12 - operational - minor

S022 conceals syntax, but S009's scenarios (no visible residue, caret never inside hidden text,
IME, screen reader) are not extended for links, and `tst_s009_concealment` has no link case. The
story is already "In review"; CLAUDE.md sets that status only once the scenarios pass on the CI of
all three platforms, and the Format job fails (O10). Whether Armin confirmed S022 before the
implementation is not recorded in the story.

**Disposition: fixed.** Extended `tests/ui/tst_s009_concealment.cpp` to include inline links in `kDocument` and verified random-walk caret positioning never places the caret inside hidden link markers. Added inline links scenario to `docs/stories/009-concealment-quality-gate.md`.

## Capped concerns

- Entities in destinations and titles are not decoded: `[link](foo%20b&auml;)` gives
  `foo%20b&auml;`, CommonMark `foo%20b%C3%A4` (minor, failure).
- `[](./empty)` conceals the whole line when the caret is elsewhere; nothing is left to see or
  click (minor, failure).

## Explicitly not objecting to

- Byte-exact saving: the UI test saves the example file and compares bytes (ADR 0002).
- Theme tokens `link.text` and `link.marker` with fallbacks to `colors.accent` and `colors.marker`,
  documented in docs/theming.md and tested in core and UI (ADR 0004).
- Destination parsing for the cases the story names: balanced parentheses, angle brackets with
  spaces, `[a](<b)c>)`, titles in `"`, `'` and `()`, whitespace around destination and title,
  `[link](/my uri)` staying literal, and code spans taking precedence over brackets
  (`[not a `link](/foo`)`) all match commonmark.js and markdown-it-py.
- Registering the link element before emphasis so that `_` in a destination is not emphasis.
- No new Qt module (QDesktopServices is in Qt GUI), no new dependency.
- Release build with -Werror without warnings, clang-tidy clean on the changed files, benchmark
  values within the noise of main.
- Reference links (`[foo][bar]`, `[foo]`, definitions) and autolinks are outside this story; the
  branch name suggests references, a later story should cover them.
