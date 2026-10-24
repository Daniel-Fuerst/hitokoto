# S008 — Unsaved changes are never lost silently

- **Status:** Proposed · **Order:** 4 · **Depends on:** S004

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

**Scenario: opening another file with changes**
- Given unsaved changes
- When I choose File › Open
- Then the same question appears before the current document is replaced

**Scenario: undo back to saved state**
- Given I typed and then undid everything since the last save
- Then the modified marker is gone

## Tests

- UI: `tst_s008_unsaved` – close event with modified document, dialog answered via a test hook.

## Notes

- No autosave or crash recovery in the MVP; candidate for a later story.
