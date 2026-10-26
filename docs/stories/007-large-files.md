# S007 — 10 MB files open within budget and type without lag

- **Status:** Partly done in v0.1 · **Order:** 11 · **Depends on:** S006

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

## Tests

- Bench: `S007_open_10mb`, `S007_typing_10mb`, `S007_save_10mb` (generator script in `bench/`).
- Core: incremental styling scheduler – visible blocks first, the rest in idle chunks; no
  whole-document pass on the UI thread.

## Notes

- Phase 0 showed both whole-document passes (GTK and Qt `QSyntaxHighlighter`) take 0.3–0.6 s on
  10 MB; this story replaces them with lazy styling (ADR 0001, section 6).
