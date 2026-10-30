# ADR 0006: Context beyond one line (paragraphs, setext headings, front matter)

- **Status:** Accepted (Armin, 2026-10-30; from review objection O3 of S023)
- **Date:** 2026-10-30
- **Deciders:** project owner (Armin, Daniel)

## Context

Until now a line looked the way the line itself and an open multi-line block decided (ADR 0003,
`docs/architecture.md`): `LineElement` is "decided by one line", `BlockState` is "opaque state
from one line to the next while a multi-line element is open", and "nothing else connects two
elements". The brief (section 5) requires that a new element is added without editing existing
elements or the editor's core loop.

S023 (thematic breaks and setext headings, CommonMark 0.31.2, 4.1 and 4.3) needs more:

1. **From above:** `---` is a thematic break, unless the line above is paragraph text, then it
   underlines a level-2 heading. Inside a list item the underline must be indented to the item's
   content column. Which lines are paragraph text depends on every block element.
2. **From below:** every line of a paragraph that ends in a setext underline is a heading line.
   A paragraph can be arbitrarily long (hard-wrapped text), so this is an unbounded lookahead.
3. **Document start:** front matter (`---` on the first line up to the next `---`, Obsidian) is
   recognised only at the start of the document and only when it is closed.

The UI renders lazily (S007, S019): visible lines first, the rest in idle slices of at most 6 ms,
and the walk over block states must stay cheap (S021).

## Decision

1. **A whole-document reference.** `ElementRegistry::renderDocument(lines, revealed)` renders a
   whole document and defines how every line looks, given the lines that hold the caret or
   selection. Core tests specify S023 against it. The UI keeps rendering lazily, but must produce
   the same `LineRender` for every line as the reference (except `blockState`, which is internal);
   `tst_s023_thematic_breaks` compares both. Until S023 it chains `render()` line by line.
2. **Paragraph context from above travels in `BlockState`**, as a field of its own next to
   `element` and `value` (0 = the line is no paragraph text, else 1 + the content column of the
   paragraph). `active()` keeps meaning "a multi-line block is open", so existing users of it
   (`nextLineFor`, fence search, code-block margins) do not change. A changed value restyles the
   next line through the existing chain in `BlockStyler`. The styler caches the value in the
   block's user state bits, never in a `BlockData`: plain lines stay without allocation
   (`tst_s004_save`, `tst_s014_fenced_code_blocks`).
3. **Elements declare their paragraph role.** `LineElement` gets a virtual function with a default:
   a matched line ends a paragraph (headings). The bullet element overrides it: a bullet with text
   opens paragraph text at its content column; an empty bullet opens none. Lines no element
   matches are paragraph text, except lines that start a block hitokoto does not render yet
   (ordered list item, block quote, indented code, HTML block, link reference definition); one
   shared core function decides that (today `startsOtherBlock` in `table.cpp`). A new element
   therefore declares its own role and does not edit the rule; brief section 5 holds.
4. **Setext headings from below** are found by the UI with a forward walk over the paragraph,
   starting at the line, ending at the first line that is no paragraph text; the walk runs within
   the synchronous budget and continues in idle slices (like the S021 fence search). The result
   reaches the core as a context field (`LineContext`, the level the paragraph ends in), so the
   core still decides how the line looks. When an underline appears or disappears, the UI
   restyles the paragraph above it.
5. **Front matter at document start**: the UI renders the first line with a document-start state
   and confirms the closing line with a forward search bounded by the front matter's own length.
   Front matter is a `MultiLineElement` whose lines render as plain text (S024 will show it as
   properties).
6. **One thematic-break recogniser** in the core (CommonMark 4.1); bullet, table and S023 use it.
   Each caller decides indentation.

## Consequences

- `blockStateAfter()` runs the line elements' paragraph role as well as the multi-line elements;
  the walk gets somewhat slower. The benchmark gate gets a document with breaks, setext headings
  and long hard-wrapped and lazily continued paragraphs (S023, O9).
- An edit that changes the paragraph context (typing `- ` before a long hard-wrapped paragraph)
  restyles the rest of that paragraph in idle slices.
- Adding a block element now includes declaring its paragraph role; `docs/architecture.md`
  ("Adding a Markdown element", `BlockState`) is updated with the implementation of S023, when the
  interfaces exist.

## Implementation (S023, 2026-10-30)

- `BlockState::paragraph` is 0, 1 + the content column, or `kLazyParagraph` for text after an
  ordered list item or block quote (not rendered yet), which lines below continue lazily but never
  underline; `underlinable()` tells the two apart. The UI caches it in bits 4-19 of the block's user
  state.
- The paragraph role is `LineElement::paragraphColumn()`; the shared rules (thematic breaks, setext
  underlines, starts of unrendered blocks, content after a container marker) live in
  `src/core/src/block_rules.hpp`, used by the registry, the bullet and the table element.
- `ElementRegistry::paragraphLink()` tells the UI's walk whether a line continues the paragraph
  above or underlines it. The UI caches each line's link and the level of its paragraph in its user
  state; a level is reused from the line below, so typing in a paragraph walks nothing, and passed
  up the paragraph when an underline appears or goes. The walk runs at most 2 ms on a key, then in
  idle slices; only styling starts it (painting and tables use the level a line is shown with).
- Front matter: `MultiLineElement::documentStartState()` and `ElementRegistry::documentStart()`; the
  caller confirms the closing line by walking `blockStateAfter()`, the UI within the synchronous
  budget and then in idle slices, again when an edit touches the first line, the lines up to the
  closing one or, while it is open, a line that could close it.
- `renderDocument()` also reveals the fences of the code block the range is in (S021).

## Alternatives considered

- **`LineContext::previousLine`**: the line above as text. Rejected: an edit above would not
  restyle the line below through the existing chain, and "paragraph text" depends on more than one
  line (lazy continuation, list content column).
- **Parse the whole document on every change**: simplest and exact, but against the budgets for
  10 MB files (S007, S017).
- **Leave `Foo` / `---` as plain text**: rejected by the owner (review O1); setext headings are part
  of S023.
