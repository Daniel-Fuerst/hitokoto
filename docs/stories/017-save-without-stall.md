# S017 — Saving never stalls the UI

- **Status:** In progress · **Order:** 18 · **Depends on:** S004, S007

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
- When the copy has been set back more than a few times
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

The snapshot is taken in slices of about 128 K characters on the UI thread, one slice per
event-loop turn, with the editor editable throughout. Each slice copies whole blocks with a
`QTextCursor` (`selectedText()`, raw text as `toRawText()` gives it). Edits during the copy are
tracked through `QTextDocument::contentsChange`:

- an edit at or before the copied part sets the copy back to the start of the slice that holds
  the changed block; the following slices copy from there again;
- edits behind the copied part need nothing: those blocks are copied later with their new content.

So when the copy completes, every block's copy was taken after its last edit and no line moved in
front of it since: the copy equals the document at that moment (`snapshotTaken()`). Only the
completed copy goes to the worker thread, which encodes and writes it atomically as before. After
a few set-backs the rest is copied in one go, so a save cannot be starved by continuous edits;
there is no time limit, because a slow but steady copy must not turn into one big stall.
`saveNow()` (quit, tests) copies everything in one go and writes synchronously. A newer `save()`
drops a running copy; loading another document drops it too.

Cost: `selectedText()` is the only public way to copy a range of a `QTextDocument`, and it costs
about six times `toRawText()` per character (development VM: 124 ms against 21 ms for 10 MB,
`QTextBlock::text()` per block 226 ms). The sliced copy therefore uses more CPU in total than the
old one-shot copy, spread over about 80 event-loop turns of a few milliseconds each; nothing
stalls. Measured in `docs/performance.md`.

Alternatives considered: copying faster (a block loop with `reserve` instead of `toRawText()`)
is slower, not faster, see above; a shadow copy maintained on every keystroke makes every
keystroke pay for the save (moving up to 20 MB) and risks drifting from the document, which a
byte-exact editor cannot afford. Both rejected. Replacing a single changed block inside the copy
instead of setting back would move the rest of the copy on every keystroke; set-backs are cheaper
and simpler.

## Tests

- UI: `tst_s017_save_without_stall` — stall bound (longest gap between 1 ms timer ticks while
  saving, in an event loop without sleeps); the consistency scenario (edits at several places
  while the copy runs, written bytes compared with `toPlainText()` at `snapshotTaken()`);
  set-back limit; save during save; quit while a copy runs. `tst_s004_save` now expects
  keystrokes before `snapshotTaken()` in the file and keystrokes after it to keep the modified
  flag.
- Core: nothing new; encoding and atomic writing stay in `core::encodeUtf8` and
  `core::writeFileAtomically` (S004).
- Bench: `S007_save_10mb` measures the longest event-loop stall and the median total of the
  `--runs` saves; CI fails on the 16 ms budget for the stall and on 1000 ms for the total.
