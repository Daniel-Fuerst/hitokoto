# S052 — Task list items show as checkboxes that the mouse ticks

- **Status:** In progress (confirmed by the user on 2026-11-27) · **Order:** 57 · **Size:** M ·
  **Depends on:** S003, S009, S010, S019
- **Basis:** Request of the user on 2026-11-27: "`[]` and `[x]` should act as checkboxes. There
  should not be a markdown-view of them. Once created the only interaction is with the mouse."
  Task lists are in the brief's list of later features (section 1); this story adds them. Decisions
  1 to 4 are the user's answers of the same day.

## Story

As a writer keeping a to-do list, I want `- [ ]` and `- [x]` items to show as checkboxes that I tick
with a click, so that the list reads and works like a checklist while the file stays plain Markdown.

## Decisions

1. **Syntax: GitHub task list items, plus `[]`.** A bullet item (`-`, `*`, `+`, any indentation,
   S003) whose text starts with `[ ]`, `[x]` or `[X]` followed by a blank (space or tab) is a task
   item. `[]` followed by a blank is accepted as an unchecked box too (other renderers show it as
   text). Without the blank after `]` (`- [ ]` at the end of the line, `- [x]foo`, `- [](url)`) the
   line stays a bullet.
2. **No source view.** The box replaces the marker, the brackets and the blank after them on every
   line, with or without the caret or a selection on it; the raw `- [ ] ` is never shown. The
   characters stay in the file byte for byte (ADR 0002).
3. **The mouse ticks, the keyboard does not.** A left click on the box toggles it: `[ ]` and `[]`
   become `[x]`, `[x]` and `[X]` become `[ ]`, one undo step; the caret and selection stay where
   they were. Space, Enter and every other key never toggle. The pointer is a pointing hand over the
   box.
4. **The keyboard treats the box as one piece.** The caret never stands inside the hidden prefix
   (from the line start to the text): arrows, Home, clicks and Up/Down that would put it there put
   it at the start of the text; Left (and Ctrl+Left) at the start of the text goes to the end of the
   line above. Backspace (and Ctrl+Backspace) at the start of the text removes the box and the
   blanks after it, so the line becomes a plain bullet (`- [ ] milk` → `- milk`), one undo step. A
   selection may start at the line start (Shift+Left from the text, a drag); deleting it deletes
   the box with it.
5. **Created on the blank.** Typing `- [ ] ` (or `- [x] `, `- [] `) turns the line into a task item
   at the moment the blank after `]` is typed, with the caret after it; nothing waits for the caret
   to leave the line.
6. **Lists continue as tasks.** Enter in a task item with text continues the list with an unchecked
   task item of the same indentation and marker (`  * [x] done` → `  * [ ] `). Enter in an empty task
   item (no text after the box) ends it as Enter ends an empty bullet (S003): a nested item is
   outdented, a top-level item becomes a blank line. Tab and Shift+Tab indent and outdent task items
   as bullets.
7. **Checked items read as done:** their text takes `task.done-text` and, unless
   `task.done-strikethrough` is 0, a line through it.
8. **Paragraph rules as for bullets** (S023, ADR 0006): a task item opens paragraph text at the
   bullet's content column and starts after paragraph text (its text is never empty).
9. **Theming baseline:** a new section `[task]` with a token for every visible property of the box
   and of done text (`docs/theming.md`).
10. **Deliberately out of scope:** numbered task items (`1. [ ]`, numbered lists are not rendered
    yet), Delete at the end of the line above (joins the lines as everywhere; the joined `- [ ]` is
    then text in the middle of a line and shows as written), a keyboard shortcut to toggle.

## Acceptance scenarios

**Scenario: unchecked and checked boxes**
- Given `- [ ] milk`, `- [x] bread`, `* [X] eggs`, `  + [] tea`
- Then each line shows a box where the marker is (empty for milk and tea, ticked for bread and
  eggs) followed by its text; no `-`, `*`, `+`, bracket or bullet dot is shown

**Scenario: not a task item**
- Given `- [ ]`, `- [x]foo`, `- [](url)`, `[ ] milk`, `-[ ] milk`, `- [y] milk`
- Then none shows a box; the bullets among them show as bullets (S003)

**Scenario: no source view**
- Given `- [ ] milk` with the caret on that line, or a selection over it
- Then the box is shown and no character of `- [ ] ` is visible

**Scenario: a click toggles**
- Given `- [ ] milk` and the caret elsewhere
- When I click the box
- Then the line reads `- [x] milk`, the caret has not moved, and one undo restores `- [ ] milk`
- And clicking again gives `- [ ] milk`; `- [] tea` clicked gives `- [x] tea`

**Scenario: keys never toggle**
- Given the caret at the start of the text of `- [ ] milk`
- When I press Space or Enter
- Then the box stays unchecked (Space types a space into the text, Enter continues the list)

**Scenario: the caret skips the box**
- Given `a` / `- [ ] milk` with the caret before `m`
- When I press Home, or click left of the box's middle
- Then the caret stays before `m`
- When I press Left
- Then the caret is after `a`
- When I press Right from there
- Then the caret is before `m`

**Scenario: Backspace removes the box**
- Given the caret before `m` in `  - [x] milk`
- When I press Backspace
- Then the line reads `  - milk` and shows a bullet; one undo restores `  - [x] milk`

**Scenario: typing creates a task item**
- Given an empty line
- When I type `- [ ] milk`
- Then after the blank behind `]` the line shows a box, and the text `milk` follows it

**Scenario: Enter continues and ends task lists**
- Given `  * [x] done` with the caret at its end
- When I press Enter
- Then a new line `  * [ ] ` follows and shows an empty box with the caret after it
- When I press Enter again
- Then the new line reads `* [ ] `; pressing Enter once more leaves it empty

**Scenario: checked text reads as done**
- Given `- [x] bread` and `- [ ] milk`
- Then `bread` is drawn in `task.done-text` and struck through, `milk` in the normal text colour

**Scenario: wrapped task items**
- Given a task item whose text wraps
- Then the continuation lines start where the text after the box starts

**Scenario: theme tokens**
- Given a theme that sets every `[task]` token
- Then the box, its tick and the done text take them; unset, each takes its documented fallback

## Tests

- **Core (`tests/core/test_s052_task_checkboxes.cpp`):** `S052_TaskCheckboxes.` `RecognisesTaskItems`,
  `NotTaskItems`, `HidesThePrefixWhetherRevealedOrNot`, `CheckedTextIsDone`, `WrapIndentAfterTheBox`,
  `ParagraphRulesAsBullets`, `ToggleWritesTheOtherState`, `RemoveBoxLeavesABullet`,
  `ContinuedPrefixIsUnchecked`, `ThemeTokensFallbackAndResolve`.
- **UI (`tests/ui/tst_s052_task_checkboxes.cpp`):** a click on the box toggles and undoes, the caret
  stays; Space and Enter do not toggle; the caret skips the prefix (Home, Left, Right, a click left
  of the box); Backspace removes the box; typing creates a task item; Enter continues and ends task
  lists; the revealed line keeps the box (no visible prefix glyphs); done text format.

## Implementation

- **Core:** `src/core/src/elements/task.cpp` (registered before the bullet element): parse, render
  (prefix hidden as zero width whatever the reveal, a `Checkbox` decoration, `LineRender::doneText`),
  and the line functions of `hitokoto/core/task.hpp` (`parseTaskLine`, `toggledTaskLine`,
  `taskLineWithoutBox`, `continuedTaskPrefix`). Theme section `[task]` (`taskStyle()`).
- **UI:** `EditorFormats` gives the blank before the text the box's width and formats done text;
  `paintCheckboxes` draws the boxes also on revealed lines; `EditorWidget` toggles on a click,
  keeps the caret out of the prefix and handles Left and Backspace at the start of the text;
  `ListEditing` continues and ends task items.
