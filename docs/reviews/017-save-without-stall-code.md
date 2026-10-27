---
mode: code
spec: docs/stories/017-save-without-stall.md
commit_range: ca801b7..8e4adaf  # S017 on main (pull request #8)
head_examined: 8e4adaf (main)
date: 2026-10-27
source: review feedback relayed by Daniel's session on feature/tables, 2026-10-27
dispositioned: 2026-10-27 (branch agents/s017-save-without-stall, follow-up pull request)
objections:
  - id: O1
    category: failure
    severity: major
    disposition: documented
    disposition_rationale: Not reproducible. markContentsDirty() and QTextLayout::setFormats() emit no contentsChange in Qt 6.8.3 and 6.12.0 (probe), so the styler cannot set the copy back. Guard added anyway, mirroring EditorWidget; a test keeps the styler out of the copy.
  - id: O2
    category: failure
    severity: minor
    disposition: fixed
    disposition_rationale: An invalid block (findBlock() past the end) now sets the copy back to the start instead of walking off the slice list; empty slice list guarded.
  - id: O3
    category: failure
    severity: major
    disposition: fixed
    disposition_rationale: A running copy is completed in one go and written when the window closes, when another document is loaded or created, and in saveNow(); before S017 the snapshot was taken at once, so the write always happened.
---

# Review S017, mode code: objections and dispositions

The objections were relayed on 2026-10-27 by Daniel's session working on `feature/tables`, as
one sentence each, examined at main 8e4adaf. They are reproduced here as received, then checked.

## O1 - failure - major

"A missing `styler().isBusy()` guard triggers a ~124 ms synchronous UI freeze on save": the
styler's `markContentsDirty()` would emit `contentsChange`, each styled block would count as an
edit and set the copy back, and after eight set-backs the rest is copied in one go.

**Disposition: documented, guard added.** Not reproducible: a probe with Qt 6.12.0 and 6.8.3
shows that `QTextLayout::setFormats()`, `markContentsDirty()` and `setUserState()` emit no
`contentsChange` at all, and `copySetBacks()` stays 0 while the styler restyles the whole
10 MB document during a save (`stylerWorkDuringTheCopyIsNotAnEdit`, three saves after
`invalidateAll()`). `EditorWidget`'s `isBusy()` guard for `editCount` is a precaution of the same
kind; `onContentsChange` now has it too, so a future styler that does emit the signal cannot set
the copy back. `copySetBacks()` is new instrumentation for tests.

## O2 - failure - minor

"An out-of-bounds iterator decrement (UB)": in `onContentsChange` the search for the slice that
holds the changed block decrements past `begin()` when no slice qualifies, which happens when
`findBlock(position)` returns an invalid block (block number -1).

**Disposition: fixed.** Qt does not deliver a position outside the document here, so no test
reproduces it; the block number is clamped to 0 (set back to the start rather than trust the
position) and an empty slice list returns early.

## O3 - failure - major

"Silent Save As data loss on window close in `MainWindow::closeEvent`": with the copy pending in
the event loop, `closeEvent` → `waitForIdle()` waited only for the worker thread; an unmodified
document closed without the pending Save As ever being written. Likewise loading another file or
File › New dropped the copy (`cancelCopy()`), and "Save changes?" → Save wrote to the old path
because `path_` changes only after the write.

**Disposition: fixed.** `waitForIdle()` first completes a running copy in one go and hands it to
the worker thread, then waits (covers `closeEvent`, `saveNow()`); `load()` and `newDocument()`
do the same instead of dropping the copy (`finishCopy()`). Only a newer `save()` replaces a
running copy. Tests `pendingSaveIsWrittenWhenTheWindowCloses` and
`pendingSaveIsWrittenWhenAnotherFileIsOpened` (red on 8e4adaf);
`quitSavesSynchronouslyWhileACopyRuns` now expects the pending copy to be written before the
synchronous save.

## Also changed

- The stall tests take the smallest of three saves' longest stalls: the development VM preempts
  the thread for 20 ms and more now and then, seen inside one `QTextCursor::selectedText()` call
  of an otherwise ordinary slice; a stall the mechanism causes shows up in every run. The CI gate
  already uses the median of ten saves.
