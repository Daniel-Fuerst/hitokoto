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

**Scenario: half of a character does not enter the text** (rework of 2026-11-21)
- Given text from another program holds half of a character (a high or low surrogate without its
  other half), pasted, dropped, typed by a key or an input method, in a pasted table or in a cell
  of a shown table
- When it enters the editor or the cell
- Then U+FFFD (REPLACEMENT CHARACTER, �) stands in place of each half, every whole character
  stays, and the text can be saved

**Scenario: a save that meets half of a character names its place** (rework of 2026-11-21)
- Given the text holds half of a character all the same (a way in no one knows of)
- When I save, also when quitting
- Then the message names its line and column ("line 3, column 7 holds half of a character …"), the
  text stays in the editor and marked as modified, and the file on disk is unchanged

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
- Rework of 2026-11-21: core `S004_Save` for half of a character becoming U+FFFD (a first half
  without its second, a second without its first, at either edge of a text; pairs and other
  characters staying), the first one found, its line and column, and a property over random code
  units (the result always encodes, keeps its length, differs only at halves). UI `tst_s004_save`
  for each way in: Ctrl+V, Paste as Plain Text and a drop in the editor, a table pasted from HTML
  and from Excel's plain text, a key and an input method in the editor, Ctrl+V, Paste as Plain
  Text, a key and an input method in a cell and the value a cell hands to its table; a whole emoji
  typed stays, the caret moves over a pair as a whole, and a save and a save on quitting that meet
  half of a character name its line and column. `S028_FindInFile.AMatchNeverHoldsHalfOfACharacter`.
  `fuzz_codec` reads its input also as UTF-16 code units and checks the replacement.

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

## Rework on 2026-11-21

Branch `agents/s004-text-that-saves`, from main a24d67d.

- Text with half of a surrogate pair (a lone surrogate) could not be saved: UTF-8 cannot encode
  it, and every save failed with "internal error: text could not be encoded", without saying
  where the invisible character was; the user could not save until it was found and removed.
  Files never hold one (decoding refuses invalid UTF-8, ADR 0002), but text from other programs
  can: a paste whose plain text is not checked UTF-16 (the Windows clipboard's Unicode text is
  not), an input method's commit, a key whose text holds a half after its first character (Qt
  drops a key text that starts with one; its Windows key mapping passes a second half without a
  first one on as a key), or HTML whose character reference names half of one (Qt 6.8.3 imports
  it as it is, Qt 6.12.0 replaces it).
- The rule (decided by Armin, 2026-11-21): half of a character does not enter the text. Each way
  in inserts U+FFFD for it, where the user sees it; whole characters stay
  (`core::replaceLoneSurrogates`, called through `ui::insertableText`). The ways: every paste and
  drop in the editor (`EditorWidget::insertFromMimeData`: Ctrl+V, Edit › Paste, the context menu,
  the primary selection, Paste as Plain Text, a drop), the Markdown of a pasted table, keys and
  input methods in the editor, the same in a cell's editor, and the value a cell hands to its line
  (`TableModel::setData`). A key or a commit that brings only half a character types U+FFFD (Qt
  alone typed nothing for a key text that starts with a half).
- A text is judged on its own: a half at its start or end becomes U+FFFD as well. Nothing next to
  an insertion could complete it: the document holds no half, and the caret never stands inside
  a pair (Qt moves and hits by whole characters, a find match is whole code points). Qt hands a
  pair over in one piece; on Windows its key mapping joins the two halves of `WM_CHAR` into one
  input method commit.
- Should a half still get in, by a way not covered (Qt's accessibility interface writes into the
  document directly, `docs/architecture.md`), the save names its place: "notes.md: line 3,
  column 7 holds half of a character (a lone surrogate), which UTF-8 cannot store. Delete it and
  save again; the file was not changed." Lines are the file's lines, columns count characters.
