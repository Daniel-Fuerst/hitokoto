# S024 — Front matter shown as properties

- **Status:** In progress (since 2026-10-31; confirmed by Armin on 2026-10-31, refined 2026-10-30
  with his decisions and revised after review `docs/reviews/024-front-matter-spec.md`) ·
  **Order:** 25 · **Size:** L ·
  **Depends on:** S023 · **Decision:** ADR 0007
- **Basis:** Obsidian stores note properties as YAML between a `---` on the first line and the next
  `---`, and shows them as a block "Properties" (Obsidian help, "Properties", read 2026-10-30).
  CommonMark and GFM do not know front matter. Since S023 hitokoto recognises the block and leaves
  it as plain text. Armin, 2026-10-30, after using S023 with a review record: the YAML should look
  like it does in Obsidian.
- **Changes accepted behaviour of S023**, whose front matter stays plain text with the caret
  anywhere (`docs/stories/023-thematic-breaks.md`, "Front matter"). Recognition stays as S023 has
  it. The S023 behaviour remains available through a feature toggle (decision 9).

## Story

As a writer who also uses Obsidian, I want the front matter at the top of a note shown as a compact
block of properties while the caret is elsewhere, and as its YAML while the caret is in it, so that
it does not look like body text and I can still edit it as text, while the file stays byte for
byte.

## Decisions (Armin, 2026-10-30)

1. **Types** as in Obsidian, inferred from the value, because hitokoto has no vault-wide list of
   property types: text, number, checkbox, date, date and time, list, tags (review O3); a nested
   value is "unsupported" and shows as JSON, at most three lines (review O1).
2. **Editing** as text: the caret in the block shows its YAML (like the fences of a code block,
   S021); elsewhere the block shows the properties. Nothing is ever written from the properties
   block, so ADR 0002 needs no change.
3. **Adding and changing properties through the block** ("Add property", changing a type, deleting
   a row, ticking a checkbox) is story S025, not part of S024.
4. **Reading YAML** with a parser of our own for a subset of YAML 1.2, in the core, without a new
   dependency (ADR 0007).
5. **Look** as in Obsidian: a title, then one row per property with a type icon, the key and the
   value; icons drawn by the editor, no image assets; tokens in a new section `[properties]`.
6. **Invalid YAML** stays text as in S023, with a badge on the first line naming the line and the
   reason (review O10).
7. **Special cases** as listed under "Rules". Recognition keeps no limit; that YAML-looking body
   text can now be concealed is recorded as a known consequence (review O9).
8. **S009:** switching between properties and YAML changes the block's height; the lines below move
   only at that moment, as when a table switches to its source (S018), and once when a long block
   has been read (review O7).
9. **Feature toggle** `ui::Options::frontMatterAsProperties`, on by default; off gives the S023
   behaviour (Obsidian's "Source"). An options dialog may offer it later; Obsidian's "Hidden" is not
   offered (review O2).
10. **Only the caret reveals:** the YAML shows while the caret and the anchor of the selection are
    both in the block; a selection reaching beyond it leaves the properties block, and while a
    mouse button is held the block does not switch (review O6).

## Rules

Line numbers here are 0-based lines of the document unless they are shown to the user; the badge
and the scenarios count from 1, as an editor does.

**Recognition (unchanged from S023).** `---` on the first line of the document, closed by the next
line `---`; both may end in blanks. No limit on the lines between and none on blank lines (S023
code review O5). A byte order mark before the first `---` does not matter: the decoder removes it
(ADR 0002) and saving writes it back. A blank line before the first `---` means no front matter.
`...` does not close the block (Obsidian; it is a YAML document end marker, see "Invalid").
Known consequence (review O9): a CommonMark document that starts with a thematic break and later
has text underlined with `---` is front matter up to that line, as in Obsidian; if that text
happens to be a valid YAML mapping (`Status: draft`, blank, `# Chapter`), it is now concealed and
shows as properties, its `# ` lines dropped as comments.

**The YAML** is the text between the two delimiter lines. It is valid for S024 if it is empty,
only blanks and comments, or a block mapping at column 0 whose keys are unique, within the subset
of ADR 0007:

- keys: plain or quoted scalars (`title:`, `"my key":`, `'k2':`), shown as written without quotes;
  `a` and `"a"` are the same key; keys must be unique in nested mappings too;
- values: plain scalars (also over several lines, folded), single- and double-quoted scalars with
  their escapes, literal and folded block scalars (`|`, `>`, with `-` or `+`), block sequences
  (`- a`, also not indented below the key), flow sequences and flow mappings (`[a, b]`, `{a: 1}`),
  nested block mappings and sequences up to 64 levels;
- comments: a line starting with `#` after optional blanks, and ` #` after a value outside quotes;
  never shown;
- `key:` without a value is null.

**Invalid**, with the reason the badge names (the block stays text):

| Reason | Kind | Examples |
|---|---|---|
| duplicate key | invalid | `a: 1` / `a: 2`, `a: 1` / `"a": 2` |
| bad indentation | invalid | a sequence item less indented than its siblings, a key indented in the top mapping or deeper than its siblings (`a: 1` / `  b: 2`), a list started on its key's line (`a: - x`) |
| colon in a plain value (quote it) | invalid | `title: Note: draft`, `a: b:` (code review O6) |
| unknown escape | invalid | `a: "C:\path"`, `"\q"` |
| text after a value | invalid | `a: "x" y`, `a: [x] y`, `a: ["x" y]`, `a: \| text`, a line after a flow mapping, a flow key whose `:` is on the next line |
| reserved character (quote it) | invalid | a value that starts with `@`, `` ` ``, `%`, `,`, `]` or `}` (`author: @armin`) |
| tab in indentation | invalid | a tab before `- x` |
| unclosed quote | invalid | `a: "abc` |
| unclosed flow collection | invalid | `a: [1, 2` |
| not a mapping | invalid | a document that is a sequence (`- a`) or a scalar (`just text`) |
| document end | invalid | a line `...` |
| too deep | invalid | nesting beyond 64 levels |
| anchor or alias | unsupported | `a: &x 1`, `b: *x` |
| tag | unsupported | `a: !!str 1` |
| complex key | unsupported | `? a` |
| directive | unsupported | `%YAML 1.2` |
| indentation indicator | unsupported | `a: \|2` |

"Unsupported" is valid YAML outside the subset of ADR 0007; js-yaml and ruamel.yaml accept it.

**Types**, decided per top-level key from its value (YAML 1.2 core schema; quoted scalars are
always text):

| Value | Type | Shown as | Icon |
|---|---|---|---|
| plain or quoted scalar not matched below, block scalar | text | the scalar's text, wrapped; line breaks of block scalars kept | three lines |
| null (`null`, `Null`, `NULL`, `~`, nothing) | text | empty | three lines |
| plain `true`, `True`, `TRUE`, `false`, `False`, `FALSE` | checkbox | a ticked or empty box (read only) | box with a tick |
| plain integer or float of the core schema: `[-+]?[0-9]+`, `[-+]?0o[0-7]+`, `[-+]?0x[0-9a-fA-F]+` (the sign as in js-yaml and ruamel.yaml), decimal with fraction or exponent, `.inf`, `-.inf`, `.nan` (and their `Inf`, `INF`, `NaN`, `NAN` spellings) | number | as written | `#` |
| plain `YYYY-MM-DD`, a valid calendar date | date | the system's short date format (`QLocale`) | calendar |
| plain `YYYY-MM-DDTHH:MM` or `YYYY-MM-DDTHH:MM:SS`, valid | date and time | short date and short time of the system | clock |
| sequence whose items are all scalars, also empty (`[]`) | list | one chip per item, the item's text | bulleted list |
| key `tags` whose value is a sequence of scalars, a scalar or null | tags | one chip per item; a scalar is one chip, null none | tag |
| key `aliases` or `cssclasses` with a scalar or null | list | one chip, or none for null | bulleted list |
| mapping, or sequence with a mapping or sequence among its items | unsupported | JSON in `properties.unsupported-text`, at most three wrapped lines, then `…` | question mark |

Not numbers, but text: `0b101` (js-yaml and ruamel.yaml: 5) and `1_000` (ruamel.yaml: 1000); the
core schema has neither. Not dates, but text: `2026-02-30`, `2026-10-30 16:28:00` (space instead of
`T`), a time zone or fractions of a second (`…Z`, `…+01:00`, `…:00.5`). Obsidian stores dates as
`YYYY-MM-DD` and `YYYY-MM-DDTHH:MM:SS`.

**JSON for unsupported values** exactly as JavaScript's `JSON.stringify` writes the value, without
spaces: strings quoted with JSON escapes, `null`, `true`/`false`, keys in their order, numbers as
ECMAScript's Number::toString writes them (`007` → `7`, `1e3` → `1000`, `0x1A` → `26`, `1e20` →
`100000000000000000000`, `1e21` → `1e+21`, `1e-6` → `0.000001`, `1e-7` → `1e-7`), `.inf` and `.nan`
→ `null`. Dates inside are strings as written (`"2026-10-30"`; js-yaml would write an ISO time
stamp). The full JSON is the row's accessible name; the row shows at most three wrapped lines and
ends the third with `…` if there is more. Its tooltip shows the JSON as `JSON.stringify(value,
null, 2)` writes it, at most 20 lines, then a line `…` if there is more; a click shows the YAML
for the rest (code review O5).

**The block.** Unless the YAML shows (below), lines 0 to the closing line are concealed and the
block shows, in their place:

- a title "Properties";
- one row per top-level key in document order: type icon, key (in a column of `key-width`), value;
  values wrap within the text column, a key longer than its column wraps too;
- nothing below the rows (no "Add property", that is S025);
- for empty YAML or only comments: the title alone.

For screen readers the block is a title named "Properties" and one element per row, named
`key: value` and described by its type (`text`, `number`, `checkbox`, `date`, `date and time`,
`list`, `tags`, `unsupported`). The value in the name is the text shown, a checkbox `checked` or
`not checked`, the items of a list or tags joined by `, `, the full JSON of an unsupported value;
an empty value leaves `key:`.

**When the YAML shows.** While the caret and the anchor of the selection both lie on lines 0 to the
closing line, every line of the block shows its YAML as plain text, as in S023. A selection with
one end outside the block leaves the properties block shown; copying it copies the YAML lines it
covers, as with tables (ADR 0005). While a mouse button is held, the block keeps the form it had
when the button went down; it switches when the button is released.

A click on a row puts the caret at the first character of that row's value in the YAML (for a value
on the lines below the key, on the first of them; for a block scalar, on its first content line at
the content's indentation, or after its header if it has no content; for a null, directly after
the colon, before a comment if there is one; code review O7); a click
on the title or elsewhere in the block at the start of line 1, the first YAML line. Up from
the first line below the block puts the caret on the closing line; Ctrl+Home, PageUp and the like
reach the YAML lines as any text, so jumping to the top of a note shows its YAML.

**After opening** (Armin, 2026-10-31). Opening puts the caret at the start of the document (S001).
If the document starts with closed front matter whose YAML is valid, the caret stands at the start
of the first line below the closing line instead, so the first screen shows the properties block.
Reading and building take at most one frame (16 ms) then, instead of 4 ms. Front matter
confirmed only in idle slices moves the caret there when it is confirmed, unless the caret has
moved since. Without a line below the closing line, with a table on that line (its first cell
would take the keyboard focus; code review O4, Armin's decision), with invalid YAML or with the
toggle off, the caret stays at the start.

**Invalid YAML.** The lines stay plain text as in S023, revealed or not. While the caret is
outside, the first line `---` shows a badge after it: `Invalid YAML (line n): <reason>` or
`Unsupported YAML (line n): <reason>`, with n the line in the document counted from 1 and the
reason from the table above. The badge is also its accessible name.

**Height.** The block's height is the height of the properties block while concealed and that of
the YAML lines while revealed. It belongs to the block's geometry (ADR 0003): the first line
reserves the height of the properties block, the other lines take none. Switching may move the
lines below; the caret stays in view.

**Long front matter.** Until the closing line is confirmed (S023, idle slices after 4 ms) and the
YAML is read and the block laid out, the lines show as they did in S023. Reading and building take
at most 4 ms on a key and continue in idle slices; when they finish, the block switches once to
properties without the caret moving (an exception to decision 8, review O7). Front matter that
fits in that budget, as every ordinary note does, switches before the first screen is painted.

**Toggle.** With `ui::Options::frontMatterAsProperties` off, front matter shows as in S023: plain
text, no properties block, no badge.

## Acceptance scenarios

**Scenario: front matter shows as properties**
- Given a review record like Armin's example `022-inline-links-code-recheck3.md` (front matter on
  lines 1 to 27 with `mode`, `spec`, `branch` with a comment, `commit_range`, `reviewed_on`,
  `environment` and a nested `objections` list), opened with the caret on a line of its text
- Then these lines show a block "Properties" with the rows `mode`, `spec`, `branch`,
  `commit_range`, `reviewed_on`, `environment` and `objections`
- And `branch` shows `feature/links_references` without the comment
- And `reviewed_on` shows the date in the system's short format with a calendar icon
- And `objections` shows `[{"id":"O19",…` in `properties.unsupported-text` with a question
  mark, at most three lines, its JSON pretty-printed as the tooltip (20 lines at most)
- And the heading below the block renders as a heading

**Scenario: each type has its look**
- Given front matter with a text, a number, `true`, `false`, a date, a date with time, a list in
  block and in flow style, `tags: one`, `tags` as a list, a null and a mapping
- Then each row has the type, icon and display text of the table above

**Scenario: opening shows the properties**
- Given a file that starts with front matter and has a heading below it
- When I open it
- Then the caret stands at the start of the heading and the properties block shows
- And Ctrl+Home shows the YAML
- And if a table follows the front matter directly, the caret stays at the start of the document

**Scenario: the caret in the block shows the YAML**
- Given the properties block
- When the caret moves onto any line of the front matter, by key, by click or by search
- Then all its lines show their YAML as plain text and no properties block is drawn
- And when the caret leaves the block, the properties block returns with the values as edited

**Scenario: a selection across the block keeps the properties**
- Given the properties block, taller than its YAML
- When I drag with the mouse from the body up across the block, Shift+click into the block from
  below, or select all
- Then the properties block stays and nothing below it moves
- And when the drag ends inside the block with its anchor inside too, the YAML shows after the
  button is released

**Scenario: a click on a row edits its value**
- Given the properties block
- When I click the value of `branch`
- Then the YAML shows and the caret stands before `feature/links_references` on the line of
  `branch`

**Scenario: invalid YAML stays text with a badge**
- Given front matter with a duplicate key on line 3 of the document
- Then all its lines are plain text and the first line shows `Invalid YAML (line 3): duplicate key`
- And with the caret on a line of the block the badge is gone
- And an anchor on line 2 shows `Unsupported YAML (line 2): anchor or alias`

**Scenario: special cases**
- Given `---` / `---`, then the properties block is the title alone
- Given a byte order mark before `---`, then the front matter shows as properties and saving
  writes the file byte for byte
- Given a blank line before `---`, or no closing `---`, then there is no front matter (S023)
- Given `...` instead of the closing `---`, then the block reaches the next `---` as in S023, and
  the `...` line makes it invalid
- Given blank lines and comments between properties, then the properties show without them
- Given a document that starts with `---`, then YAML-looking text, a `# Chapter` line and later
  `Section` / `---`, then that text shows as properties (known consequence, review O9)

**Scenario: front matter directly followed by a table**
- Given front matter and directly below it a table
- When I edit the table and scroll, so that the table refreshes
- Then the properties block stays concealed and keeps its height, and the table stays a table
  (review O4)

**Scenario: long front matter**
- Given front matter of 5,000 top-level keys, and front matter with one nested value of 5,000 lines
- Then opening, typing in the body below and leaving the block keep every budget: no key above
  50 ms; until the block is read and built, it shows its YAML

**Scenario: the toggle gives the S023 behaviour**
- Given `frontMatterAsProperties` off
- Then front matter is plain text with the caret anywhere, without block or badge

**Scenario: the file stays byte for byte**
- Given any front matter, valid or not
- When I open, move through the block, click rows and save
- Then the file is unchanged byte for byte (ADR 0002)

**Scenario: the properties block is themeable (theming baseline, ADR 0004)**
- Given a theme with the tokens of `[properties]` below
- Then the block uses them, and a theme without them falls back as listed
- And the margin and the padding are part of the block's geometry (ADR 0003)

**Scenario: concealment quality gate (S009)**
- Given front matter among other elements
- When the caret walks with arbitrary sequences of Up/Down/Left/Right/Home/End/PageUp/PageDown
- Then the YAML shows exactly while the caret is on a line of the front matter, the properties
  block exactly while it is not
- And no line other than those of the front matter changes height, and the lines below move only
  when the caret enters or leaves the block
- And the caret is never on a concealed line
- And the properties block is accessible: one accessible element per row, named `key: value` (for
  unsupported values the full JSON), the title named "Properties"

## Theme tokens (`[properties]`)

| Token | Type | Meaning | Fallback |
|---|---|---|---|
| `properties.background` | color | Background of the block | `colors.background` |
| `properties.text` | color | Values | `colors.text` |
| `properties.key-text` | color | Keys | `colors.text-muted` |
| `properties.title-text` | color | The title "Properties" | `colors.text-muted` |
| `properties.icon` | color | Type icons | `colors.text-muted` |
| `properties.unsupported-text` | color | JSON of unsupported values | `colors.accent` |
| `properties.badge-text` | color | Badge of invalid or unsupported YAML on the first line | `properties.unsupported-text` |
| `properties.badge-font-family` | string | Font family of the badge; "" = editor font | "" |
| `properties.badge-font-scale` | number | Size of the badge relative to the editor font, 0.5 to 2 | 1 |
| `properties.checkbox` | color | Tick and border of a checkbox | `colors.accent` |
| `properties.checkbox-background` | color | Fill of a checkbox | `properties.background` |
| `properties.chip-background` | color | Background of a list item or tag | `colors.surface` |
| `properties.chip-text` | color | Text of a list item or tag | `properties.text` |
| `properties.chip-border` | color | Border of a list item or tag | `colors.border` |
| `properties.chip-border-width` | number (px) | Border width of a list item or tag, 0 to 8 | 0 |
| `properties.chip-radius` | number (px) | Corner radius of a list item or tag, 0 to 32 | `layout.radius` |
| `properties.chip-padding` | number (px) | Horizontal space inside a list item or tag, 0 to 32 | 4 |
| `properties.border` | color | Border of the block | `colors.border` |
| `properties.border-width` | number (px) | Border width, 0 to 8 | 0 |
| `properties.radius` | number (px) | Corner radius of the block, 0 to 32 | `layout.radius` |
| `properties.padding` | number (px) | Space inside the block, 0 to 64 | `layout.spacing` |
| `properties.margin` | number (px) | Space above and below the block, 0 to 64 | 3 |
| `properties.row-spacing` | number (px) | Space between rows, 0 to 32 | 2 |
| `properties.key-width` | number (px) | Width of the key column, 40 to 1000 | 160 |
| `properties.font-family` | string | Font family of the block; "" = editor font | "" |
| `properties.font-scale` | number | Font size relative to the editor font, 0.5 to 2 | 1 |

## Requirements for the implementation

- Core: the parser of ADR 0007, `parseProperties`, returns the rows (key, type, display text,
  list items, date fields, the line of the key and of its value) or the first error with its line
  and reason. It never throws and is linear in the length of the YAML. `LineRender::frontMatter`
  marks the lines of closed front matter, both delimiters included, concealed or not; the
  reference `ElementRegistry::renderDocument` sets it.
- Editor, ownership of hidden lines (review O4): `TableEmbedder::sweep` today unhides every hidden
  line and clears every reserved height in its range that no table owns. Hidden and reserved state
  gets an owner, either per block in `BlockLayout` or through one embedder interface that tables
  and properties share, so that neither undoes the other. This changes `TableEmbedder` once; after
  it, a further embedded element needs no change to the existing ones (brief, section 5).
- Editor: the properties block is embedded like a table in S018: the first line reserves its height
  in `BlockLayout`, the other lines are hidden; it is built when the closing line is confirmed and
  rebuilt only when the caret leaves the block or the lines change. `EditorWidget::propertiesView()`
  returns it (null while there is none or the YAML shows), `EditorWidget::frontMatterBadge()` the
  badge text. Rows on screen are laid out first; building and laying out follow the budget under
  "Long front matter" (review O8). A text value shows in full however long it is: keys and text
  values are laid out once per width into a `QTextLayout`, a long one across idle slices, and only
  their lines on screen are painted (code review O2, Armin's decision).
- The fuzz target `fuzz_render` also feeds the YAML of each closed front matter, and the whole
  decoded input split into lines, to the parser, so the existing CI job covers it without a change
  to `ci.yml` (review O12).
- Benchmark keys `s024_*`: opening the 10 MB document with front matter of 200 properties in front
  (first screen shows the properties block), typing in the body below, entering and leaving the
  block, leaving a block of 5,000 top-level keys and one with a nested value of 5,000 lines; all
  budgets hold.
- S022 (`feature/links_references`, Daniel, PR #21, not merged on 2026-10-30) adds link tokens to
  `theme.hpp`, `theme.cpp` and `docs/theming.md` after the escape and thematic-break tokens, and
  touches `LineRender::empty()` in `markdown.hpp`. Whichever merges second keeps both: the
  condition `frontMatter` and S022's in `empty()`, both token blocks.
- Differences from the reference parsers, found in a differential run of 20,000 random front
  matters per seed against js-yaml 4.3.2 (2026-10-31; ruamel.yaml 0.19.1 to decide where they
  disagree), besides those of the subset (anchors, tags, complex keys, directives, indentation
  indicators, `...`, invalid calendar dates, `0b` and `_` in numbers, a top-level mapping that is
  indented): keys keep their order in JSON, as in ruamel.yaml, where JavaScript puts integer-like
  keys first; a line with only a tab inside a block scalar is an empty line (both references reject
  it); `-.5` and `+.5` are numbers and `1e400` a number whose JSON is `null`, as in ruamel.yaml
  (js-yaml: text; code review O11). In sequences nested deeper than their parent's next item, js-yaml attaches the later items
  to the inner sequence; ruamel.yaml and hitokoto do not. Test `CasesFromTheDifferentialRun`.
- Differences from Obsidian, recorded here and in the tests: types come from values, not from a
  vault-wide list; a quoted date is text; a scalar `tags: a, b` is one chip (Obsidian may split
  it; not verified); dates are not linked to daily notes; text values that are URLs or `[[links]]`
  are not clickable; nested values show at most three lines of JSON; a repeated key or an anchor
  makes the block text with a badge; no "Hidden" display; the badge, not Obsidian's own error view.

## Tests

- Core: `S024_FrontMatter` – the parser (subset, types, display text, JSON, lines, invalid and
  unsupported input with line and reason, linear time), `LineRender::frontMatter` in the reference,
  theme tokens and fallbacks, the tokens in `docs/theming.md`. The expected values were checked
  against js-yaml 4.3.2 and ruamel.yaml 0.19.1 (YAML 1.2) and, for JSON numbers, Node 22; where
  the story decides otherwise (dates, anchors, tags, `0b`, `_`) the test says so.
- UI: `tst_s024_front_matter` – the block for a review record, types and icons, reveal and conceal
  by caret, the caret after opening, selections and mouse drags, click to value, invalid YAML with
  the badge, special cases, front matter above a table, height in the layout, the toggle,
  byte-exact save, tokens, accessibility.
- S009: `tst_s009_concealment::frontMatterWhileWalking`. The S023 walk
  `thematicBreaksAndSetextHeadingsWhileWalking` starts with plain lines instead of front matter,
  because the front matter now changes height when the caret enters it.
- S023: `ClosedFrontMatterStaysText`, `FrontMatterReachesTheNextDelimiter` and
  `tst_s023_thematic_breaks::frontMatterStaysText` keep what S024 leaves of S023 (no break, no
  heading, no inline element in front matter; the YAML as text with the caret in it) and no longer
  require an empty render.
