# S001 — Opening a Markdown file shows its content

- **Status:** Proposed · **Order:** 2 · **Depends on:** S000

## Story

As a writer, I want to open a Markdown file from the command line or a dialog, so that I can
read and edit it immediately.

## Acceptance scenarios

**Scenario: open from the command line**
- Given a UTF-8 file `notes.md`
- When I start `hitokoto notes.md`
- Then the editor shows its exact text, the window title shows `notes.md`, the caret is at the start

**Scenario: open from the menu**
- Given the app is running with an unmodified document
- When I choose File › Open (Ctrl+O / Cmd+O) and pick a file
- Then its content replaces the current document

**Scenario: start without a file**
- When I start `hitokoto` without arguments
- Then an empty, untitled document is ready for typing

**Scenario: unreadable or invalid file**
- Given a file that does not exist, cannot be read, or is not valid UTF-8
- When I try to open it
- Then I see a clear message, the current document is unchanged, and nothing is written to disk

## Tests

- Core: `S001_Open` – decoding (with/without BOM, LF, CRLF), invalid UTF-8 rejected with position,
  empty file, file without trailing newline; metadata (BOM, line ending) recorded for S004.
- Core property test: decode → encode of valid UTF-8 input returns the identical bytes.
- UI: `tst_s001_open` – start with a fixture path, compare `toPlainText()` with the fixture.

## Notes

- Writes **ADR 0002 – text buffer ownership** before coding (see backlog, decision 3).
- File reading runs off the UI thread from the start (needed for S007 anyway).
