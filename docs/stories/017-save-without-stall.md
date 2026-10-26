# S017 — Saving never stalls the UI

- **Status:** Open · **Order:** 18 · **Depends on:** S004, S007

Follow-up of S007 (`docs/performance.md`, "Known gaps"): saving a 10 MB file blocks the UI thread
while the text is copied out of the `QTextDocument` (CI run #18: Linux 6 ms, macOS 24 ms, Windows
36 ms; development VM 19–25 ms). The brief (section 1) asks for saves that keep the UI responsive,
and the budget says saving never blocks the UI for more than 16 ms; the Windows baseline already
exceeds it without CI noticing, because CI compares with baselines only.

## Story

As a writer with a huge file, I want saving to happen without a hitch in my typing, so that I
save often instead of putting it off.

## Acceptance scenarios

**Scenario: saving a 10 MB file does not stall the UI**
- Given the 10 MB file from S007 is open
- When I save and keep typing
- Then no event on the UI thread takes longer than 16 ms, key-to-paint latency stays within the
  typing budget (p95 ≤ 16 ms) and no keystroke is lost

**Scenario: the file is a state the document had**
- Given a save is in progress and its copy of the text is not complete yet
- When I edit lines before, inside and after the part already copied, including inserting and
  deleting lines
- Then the written file is byte for byte the document as it stood the moment the copy completed:
  never a mixture of older and newer lines

**Scenario: saving always finishes**
- Given I keep inserting lines near the top of the 10 MB file while a save is in progress
- When the copy has been set back more than a few times or has run for longer than about 500 ms
- Then the rest is copied in one go (a single short stall) and the save completes

**Scenario: save, save as and quit behave as before**
- Given a save is in progress
- When I save again, choose Save As, or quit (which saves synchronously)
- Then the running copy is dropped and the newer request wins; quitting waits for the write;
  the title and the modified flag change only when the file written matches the document
  (S004, S008)

**Scenario: the benchmark gate measures the stall, not the call**
- Given `hitokoto-bench` saves the 10 MB file
- When it reports `save_10mb_ui_blocked_ms`
- Then the value is the longest event-loop stall observed during the save (a 1 ms timer's
  largest delay), compared with the baseline as before and additionally checked against the
  16 ms budget in CI
- And `save_10mb_total_ms` is the median of the `--runs` saves, compared with the baseline for
  information only and failing only above an absolute bound of 1000 ms

## Design

The snapshot (`DocumentSession::snapshot()`) is taken block by block in time slices of at most
8 ms on the UI thread, like loading (S007), with the editor editable throughout. Edits during the
copy are tracked through `QTextDocument::contentsChange`:

- an edit that changes the content of an already copied block without changing the number of
  blocks replaces that block's copy;
- an edit that inserts or removes blocks at or before the copied part sets the copy back to that
  block;
- edits after the copied part need nothing: those blocks are copied later with their new content.

So when the copy completes, every block's copy was taken after its last edit and no line moved
in front of it since: the copy equals the document at that moment. Only the completed copy goes
to the worker thread, which encodes and writes it atomically as today. A time limit and a limit
on set-backs end the sliced copy with one synchronous copy of the rest, so a save cannot be
starved by continuous edits. `saveNow()` (quit, tests) stays synchronous.

Alternatives considered: copying faster (a block loop with `reserve` instead of `toRawText()`)
halves the stall at best and leaves Windows above 16 ms; a shadow copy maintained on every
keystroke makes every keystroke pay for the save. Both rejected.

## Tests

- UI: `tst_s017_save_without_stall` — stall bound and typing during a save; the consistency
  scenario (edits at several places while the copy runs, written bytes compared with
  `toPlainText()` at completion); set-back limit; save during save; Save As and quit.
- Core: nothing new; encoding and atomic writing stay in `core::encodeUtf8` and
  `core::writeFileAtomically` (S004).
- Bench: `S007_save_10mb` measures the longest event-loop stall and the median total of the
  `--runs` saves; CI fails on the 16 ms budget for the stall and on 1000 ms for the total.
