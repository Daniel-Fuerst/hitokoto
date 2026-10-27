# ADR 0002: Text buffer ownership, encoding and line endings

- **Status:** Accepted
- **Date:** 2026-10-24
- **Deciders:** project owner (decisions 3 and 4 of the MVP backlog)

## Context

The brief demands that the plain Markdown text is the only source of truth and that no second
document model can drift from it (section 1). ADR 0001 chose `QPlainTextEdit`, which already
keeps the text in a `QTextDocument`. Keeping another copy in the core (for example a piece table)
would mean two buffers that must be synchronised on every keystroke.

Saving must write exactly the text, byte for byte (S004), including BOM and line endings.

## Decision

1. **One copy of the text at runtime:** the editor's `QTextDocument`. The core is stateless with
   respect to the open document: it renders single lines (`ElementRegistry::render`), computes
   reveal ranges, and converts between bytes and text.
2. **Formatting lives only in the layout.** Styles are applied as `QTextLayout` formats per block,
   which are not part of the document's content, not part of undo, and never written to disk.
3. **The core owns file I/O:** UTF-8 decoding/encoding (`text_codec`) and atomic writing
   (`file_io`: temporary file in the same directory, flushed, renamed over the target).
4. **Encoding:** UTF-8 only, with or without BOM. Invalid UTF-8 is refused with the byte offset,
   never repaired. U+2029 (paragraph separator) is refused too, because `QTextDocument` would turn
   it into a line break and the file could not be written back unchanged.
5. **Line endings:** LF, CRLF and CR are preserved. Files with one kind keep it for all lines, new
   lines included. Files with mixed endings keep each existing line's ending (stored per block in
   `QTextBlockUserData`); new lines get the file's majority ending. New documents use LF.
6. **One exception, for compatibility (added 2026-10-27, S018):** when a file is opened, a line
   without `|` that directly follows a table's rows gets a blank line before it. GFM (and
   GitHub) would make that line a row of the table; Obsidian, which writes such files, ends the
   table there. With the blank line the text is a paragraph in every one of them. The document
   is marked modified and the user is told how many lines were added; the file is written only
   when the user saves. Decided by the project owner as a compromise for the best compatibility.

## Consequences

- Opening and saving a file without editing writes it byte for byte, except for decision 6: a
  file with text directly below a table is saved with the added blank lines (and the editor
  says so when opening it). The added lines take the file's majority ending.

- No synchronisation code and no drift by construction.
- Saving copies the document text once on the UI thread (`QTextDocument::toRawText`, about 25 ms
  for 10 MB on the Phase 0 VM); encoding and writing run on a worker thread.
- Mixed-ending files: lines that are never touched keep their ending byte for byte. Lines
  created or changed by editing (typing a line break, joining lines, paste, and lines restored by
  undo, because Qt's undo does not record block user data) take the majority ending.
- U+FDD0 and U+FDD1 are refused as well: Qt uses them as frame markers and would also turn them
  into line breaks.
- Saving keeps the file's metadata: permissions and, where allowed, owner (POSIX); attributes, ACL
  and alternate streams via `ReplaceFileW` (Windows). Files marked read-only are refused on every
  platform. On macOS the data is flushed with `F_FULLFSYNC`.
- Other encodings (UTF-16, Latin-1) are out of scope; such files are refused, not altered.

## Reconsider if

- A future feature needs text access off the UI thread at high frequency (e.g. background search
  over huge files): then a core-owned immutable snapshot structure may pay off.
- Users need non-UTF-8 encodings.
