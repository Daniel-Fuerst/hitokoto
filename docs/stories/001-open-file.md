# S001 — Opening a Markdown file shows its content

- **Status:** Done in v0.1 · **Order:** 2 · **Depends on:** S000

**Implementation note:** Done (v0.1, 2026-10-24). ADR 0002 written. Tests: core `S001_Open`, UI `tst_s001_open`.

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

**Scenario: something that is not a file one can open**
- Given a path that names a folder, a pipe, a device or a socket (also through a symbolic link),
  or a file larger than 128 MiB
- When I try to open it
- Then I see a message that says which of these it is, nothing waits on a pipe, the current
  document is unchanged and stays editable, and the program goes on
- And if memory runs out while a file is read or decoded, I see a message that says so instead of
  the program ending

## Tests

- Core: `S001_Open` – decoding (with/without BOM, LF, CRLF), invalid UTF-8 rejected with position,
  empty file, file without trailing newline; metadata (BOM, line ending) recorded for S004.
- Core property test: decode → encode of valid UTF-8 input returns the identical bytes.
- UI: `tst_s001_open` – start with a fixture path, compare `toPlainText()` with the fixture.
- Core: `S001_ReadFile` – a regular file, an empty file and a link to a file are read byte for
  byte; a missing file, a folder, a link to a folder, a pipe and a device (POSIX) are refused with
  their message, the pipe without waiting; a sparse file one byte over the bound is refused from
  its size without being read; the bound holds at exactly its size and while reading (files
  under `/proc` report a size of 0, Linux).
- UI: `tst_s001_open` – opening a folder shows "Cannot open file" with the reason, keeps the
  document and opens the next file; a failure while decoding (test seam on the worker thread)
  shows "not enough memory" and keeps the document.

## Notes

- Writes **ADR 0002 – text buffer ownership** before coding (see backlog, decision 3).
- File reading runs off the UI thread from the start (needed for S007 anyway).

## Rework on 2026-11-15

Branch `agents/s001-read-what-can-be-read`, from main ed91faa.

- Opening a folder ended the program instead of showing "Cannot open file": `core::readFile`
  read it through a stream iterator, whose read error (libstdc++: "Is a directory") left
  `readFile` as an exception, and the loading lambda on `QThreadPool` did not catch it; Qt's
  thread pool does not catch either, so the program ended. A file larger than the free memory
  ended it the same way with `std::bad_alloc`, and a pipe blocked the worker thread at opening.
- `readFile` now looks at what the path names after following symbolic links before it opens
  anything and reads only regular files. It refuses a file larger than a bound
  (`core::kMaxReadBytes`, 128 MiB) from its size and stops at the bound while reading, for a
  file that grows meanwhile or reports no size. No exception leaves it: running out of memory
  and stream errors become an `IoError` with a message.
- The bound: opening takes about 27 times the file size in memory and about 0.3 s per MiB
  (measured with 10, 50 and 100 MiB files: 0.37, 1.4 and 2.7 GB peak, 3, 16 and 30 s until fully
  loaded, cloud VM with 2 vCPU). At 128 MiB a file needs about 3.5 GB, which a machine with 8 GB
  still has; more cannot be edited usefully. It is more than twelve times the 10 MB of the brief.
  `readFile` takes another bound as a parameter, for files that should be smaller.
- The loading lambda catches every exception and reports it through `loadFailed`; the document
  stays as it was and editable.
- On Windows, `std::filesystem::status` of the Microsoft STL knows only folders, links and
  regular files: a pipe or a device name (`CON`, `NUL`) is not refused as such. What happens there
  depends on opening and reading it, within the bound and without an exception.
