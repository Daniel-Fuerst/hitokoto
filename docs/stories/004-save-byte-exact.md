# S004 — Save and Save As write the text byte for byte, atomically

- **Status:** Done in v0.1 · **Order:** 3 · **Depends on:** S001

**Implementation note:** Done (v0.1, 2026-10-24). Mixed line endings as proposed (existing lines keep theirs, new lines take the majority). Tests: core `S004_Save` incl. property tests, UI `tst_s004_save` incl. a random round trip through the real editor; fuzzer `fuzz_codec`. Saving 10 MB still blocks the UI for about 25 ms (docs/performance.md).

## Story

As a writer, I want saving to write exactly my Markdown text and never damage the file, so that
hitokoto is safe to use on any file.

## Acceptance scenarios

**Scenario: unchanged file round-trips**
- Given any valid UTF-8 file (with or without BOM, LF or CRLF, with or without final newline)
- When I open it and save without editing
- Then the file on disk is byte-identical to the original

**Scenario: edited file keeps its conventions**
- Given a file with CRLF line endings and a BOM
- When I add a line and save
- Then all lines end with CRLF, the BOM is kept, and no rendering artifact (hidden-marker
  formatting, bullet glyphs) appears in the file

**Scenario: atomic write**
- Given a save is interrupted after the temporary file was written but before the rename
- Then the original file is intact, and the temporary file lives in the same directory

**Scenario: a save that fails keeps text and file**
- Given a save fails on its way, also because memory runs out while the text is encoded, in the
  background or when quitting
- Then I see a message, the text stays in the editor and marked as modified, the file on disk is
  unchanged, and the program goes on; the next save works

**Scenario: Save As**
- When I choose File › Save As and pick a new path
- Then the file is written there, the title changes, later saves go to the new path

**Scenario: save does not block typing**
- When I save a 10 MB file
- Then typing continues without a frame drop (checked again in S007)

## Tests

- Core: `S004_Save` – encode with BOM/line-ending metadata; mixed line endings (see notes);
  atomic writer with an injected failure between write and rename.
- Core fuzz test: arbitrary bytes that decode as UTF-8 survive decode → encode unchanged.
- UI: `tst_s004_save` – open fixture, edit headings/bullets, save, compare bytes with expectation.
  With mixed endings only lines whose ending differs from the majority store it per block.
- UI: `tst_s004_save` – running out of memory while encoding (test seam on the worker thread,
  `std::bad_alloc`), for `save()` and for `saveNow()`: message, text and modified flag kept, file
  unchanged, no temporary file left, the next save writes.

## Notes

- Mixed line endings: proposal – keep each existing line's ending, new lines get the file's
  majority ending. Needs confirmation.
- Windows: rename over an open file can fail if another process holds it; report clearly, retry once.

## Rework on 2026-11-15

Branch `agents/s001-read-what-can-be-read`, from main ed91faa.

- The lambda that `DocumentSession::startWrite` gives to `QThreadPool` let an exception leave the
  worker thread; Qt's thread pool does not catch, so `std::bad_alloc` from `core::encodeUtf8`
  ended the program while the editor held the only copy of the text. `saveNow()` (quitting) calls
  the same code on the calling thread, where an exception would leave through Qt's event loop.
- `DocumentSession::write` now turns every failure into an outcome with a message ("not enough
  memory to save the file"), reported through `saveFailed` as before. The file is only ever
  replaced by a complete temporary file, so a failure before that leaves it as it was.
