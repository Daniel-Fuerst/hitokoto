# S008 — Unsaved changes are never lost silently

- **Status:** Done in v0.1 · **Order:** 4 · **Depends on:** S004

**Implementation note:** Done (v0.1, 2026-10-24). Tests: UI `tst_s008_unsaved`.

## Story

As a writer, I want to be warned before unsaved changes are discarded, so that I never lose work.

## Acceptance scenarios

**Scenario: modified indicator**
- Given an open document
- When I type a character
- Then the window title shows a modified marker; after saving, the marker disappears

**Scenario: closing with changes**
- Given unsaved changes
- When I close the window or quit
- Then I am asked Save / Don't Save / Cancel, and Cancel keeps the app open with the text unchanged

**Scenario: the question shows the name as its characters** (rework of 2026-11-21)
- Given a document whose file name looks like markup (`<b>notes</b>.md`, or an image tag with a
  data URL; Linux and macOS allow such names)
- When I am asked whether to save it
- Then the question shows the name as it is written, as plain text; nothing in it is drawn as
  formatting or as a picture

**Scenario: opening another file with changes**
- Given unsaved changes
- When I choose File › Open
- Then the same question appears before the current document is replaced

**Scenario: undo back to saved state**
- Given I typed and then undid everything since the last save
- Then the modified marker is gone

## Tests

- UI: `tst_s008_unsaved` – close event with modified document, dialog answered via a test hook.
- Rework of 2026-11-21: `tst_s008_unsaved` asks the real question (`MainWindow::askSaveInDialog`)
  with a name holding a tag, a name holding a picture and a plain name: the text as written, plain
  text, no red pixel of the picture drawn; title, icon, buttons, default button and every answer
  as before; and closing a file whose name holds a picture (not on Windows).

## Notes

- No autosave or crash recovery in the MVP; candidate for a later story.

## Rework on 2026-11-21

Branch `agents/s004-text-that-saves`, from main a24d67d.

- The question "Save changes to %1 before closing it?" came from `QMessageBox::question`, which
  guesses from its text whether it is rich text. A file name may hold a tag on Linux and macOS:
  `<b>notes</b>.md` showed in bold, and a name holding an image tag with a data URL (a slash
  spelled `%2F`) made the box decode and draw the picture the name carries.
- `MainWindow::askSaveInDialog` builds the box itself, shows its text as plain text like the
  messages beside it, and otherwise asks as before (icon, title, buttons Save, Discard and Cancel,
  Save as the default, Escape and closing as Cancel). It uses the theme's existing style of
  message boxes; no new element.
