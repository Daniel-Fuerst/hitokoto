# S007 — 10 MB files open within budget and type without lag

- **Status:** Partly done in v0.1 · **Order:** 40 · **Depends on:** S006

**Implementation note:** Partly done (v0.1, 2026-10-24). Loading off-thread and in slices, lazy styling, typing p95 under 2 ms on 10 MB (development VM). Open: the editor is read-only until the whole file is inserted (2.2 s for 10 MB on the VM), saving 10 MB blocks the UI for about 25 ms (follow-up: S017), scrolling smoothness is not measured yet. Tests: UI `tst_s007_large_files`, benchmarks `hitokoto-bench`, `hitokoto_core_bench`.

## Story

As a writer with a huge notes file, I want hitokoto to stay fast with 10 MB+ files, so that file
size never decides whether I can use it.

## Acceptance scenarios

**Scenario: open a large file**
- Given a generated 10 MB Markdown file (headings, bullets, paragraphs)
- When I open it
- Then the first screen of text is visible and editable in under 2–3 s on the reference machines,
  and the UI stays responsive while the rest loads and is styled

**Scenario: typing stays within one frame**
- Given the 10 MB file, caret in the middle and at the end
- When I type 200 characters, press Enter and move across heading lines
- Then key-to-paint latency has p95 ≤ 16 ms and no single event above 50 ms

**Scenario: saving does not stall**
- When I save the 10 MB file while typing
- Then no keystroke is lost and latency stays within budget

**Scenario: scrolling**
- When I scroll from top to bottom
- Then scrolling stays smooth (no frame above 16 ms after the initial styling has finished)

**Scenario: a line above a bound shows as text**
- Given a line longer than 10,000 characters, or a line with more than 300 characters that can
  start inline syntax (`*`, `_`, `~`, `` ` ``, `\` and `[`)
- When it is shown, with the caret on it or elsewhere
- Then it shows its characters as they are written: no inline formatting, nothing concealed
- And a heading, a bullet and the flow of a paragraph into its next line still apply to it
- And lines within the bounds beside it look as ever
- And typing in it and saving keep the file byte for byte (ADR 0002)

## Tests

- Bench: `S007_open_10mb`, `S007_typing_10mb`, `S007_save_10mb` (generator script in `bench/`).
- Core: incremental styling scheduler – visible blocks first, the rest in idle chunks; no
  whole-document pass on the UI thread.
- Core: `S007_LargeFiles` checks the bounds of a line: a line at a bound shows its formatting and
  one character more shows as text, concealed, revealed and in a table cell; characters that start
  no syntax do not count; no inline element runs above the bounds; block elements still apply.
- UI: `tst_s007_large_files` `aLineAboveTheBoundsShowsAsTextAndIsSavedByteForByte`.

## Notes

- Phase 0 showed both whole-document passes (GTK and Qt `QSyntaxHighlighter`) take 0.3–0.6 s on
  10 MB; this story replaces them with lazy styling (ADR 0001, section 6).

## Rework on 2026-11-16

Branch `agents/s013-inline-in-linear-time`, from main 0b69199.

- One long line made every key slow, whatever the size of the file: each key renders its line
  again, and so does each repaint for every line on screen. A line of 5,000 characters `a*a*a*`
  took 44 ms per key and up to 58 ms, one of 20,000 characters 490 ms; opening a file with such a
  line of 200,000 characters blocked the window for minutes.
- Two bounds now guard a line (`core::kInlineBounds`): its length, and the number of its
  characters that can start inline syntax (4,000 and 500 at first, 10,000 and 300 since the inline
  elements are linear, see below). The registry counts both in the pass over the line that it makes
  anyway, before any inline element runs; above either bound none runs, and the line shows as
  text. Block elements do not depend on the bounds. Nothing is written to the document, so the
  file stays as it is.
- Why two bounds: what a line costs has two sources. Laying out and drawing it grows with the
  number of formatted pieces, whatever the length (about 25 µs for a link, about 12 µs for a
  concealed `*`); the count of syntax characters bounds the pieces. Finding the syntax grows with
  the length. A bound on the length alone that kept dense lines within the budget would have to
  be so low (about 2,000 characters) that it would take the formatting from ordinary long
  paragraphs written on one line.
- The numbers come from measurements against the 50 ms that no key event may take
  (`docs/performance.md`, 2026-11-16). With the bound of 4,000 characters, which the inline
  elements needed while their work grew with the square of the length, a key at the bounds took at
  most 14 ms on the development VM (Qt 6.12.0 and 6.8.3) and the slowest of 30 keys 26 ms.
- Since the inline elements render a line in linear time (rework of S013, S015, S020 and S022 on
  the same day), the length adds little to a key, and the bound is 10,000 characters. A longer
  bound would only buy formatting for lines that Qt's layout makes slow anyway (20 ms per key at
  50,000 characters).
- The bound on the syntax characters went down to 300 at the same time. With 500 the slowest of
  30 keys took up to 28 ms on the development VM; a CI runner can be half as fast in laying out
  text (S031 measured factors of 1.1 to 2), and on such a machine that is at the 50 ms. With 300 a
  key in a line at the bounds takes at most 8 ms with Qt 6.12.0 and Qt 6.8.3, a key on the line
  above it 9 ms and 12 ms, and the slowest single key 15 ms and 18 ms.
- A registry can be given other bounds (`ElementRegistry::setInlineBounds`); tests use small ones.
- Not covered: a line without any syntax still costs Qt's layout of the whole line on every key
  (about 0.2 µs per character: 4 ms for 10,000 characters, 43 ms for 200,000). See
  `unrefined.md`, "Very long lines".
