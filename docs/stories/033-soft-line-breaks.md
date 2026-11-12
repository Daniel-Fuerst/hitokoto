# S033 — Soft line breaks: paragraphs flow

- **Status:** In review (confirmed by the user on 2026-11-09) · **Order:** 34 · **Size:** L ·
  **Depends on:** S003, S009, S019, S020, S023, S028 · **Decision:** ADR 0003, ADR 0006
- **Basis:** `docs/stories/unrefined.md` ("Soft line breaks: paragraphs flow"). In CommonMark 0.31.2
  (sections 4.8 and 6.8), a paragraph is a sequence of non-blank lines that cannot be interpreted as
  other kinds of blocks; line breaks inside it are soft line breaks and paragraphs flow as one block
  of wrapped text, unless a line ends in two or more spaces or in an unescaped `\` (section 6.7,
  hard line break). The repository's own `docs/` files are wrapped at 100 columns; showing every
  source line on its own editor line leaves short lines next to wider lines when a window is resized
  or a line is edited.
- **Changes accepted behaviour of S023 and S009** on setext headings whose paragraph has more than
  one source line (`docs/stories/023-thematic-breaks.md`, "Height and concealment"): while the caret
  and selection are outside the paragraph and its underline, the lines of the paragraph flow as one
  wrapped heading block instead of staying on separate source lines; when the caret or selection
  enters the paragraph or its underline, its raw source lines are revealed on their own lines, which
  may change the heading's height at that moment (as with tables in S018 and front matter in S024).
  Single-line setext headings keep their height.

## Story

As a writer reading or editing a hard-wrapped Markdown note, I want consecutive lines of a paragraph
(including paragraphs in bullet list items and setext headings) to flow together as a single wrapped
paragraph while the caret is outside the paragraph, and to reveal their raw source lines when the
caret or selection enters the paragraph, while hard line breaks (`2+` trailing spaces or a trailing
`\`) start a new line inside the paragraph, so that hard-wrapped files read as continuous prose
without changing the underlying file byte for byte.

## Decisions (2026-11-09)

1. **Paragraph-level reveal.** While the caret and any selection lie completely outside a paragraph
   (and, for a setext heading, its underline), the paragraph flows as a single wrapped block. When
   the caret or selection enters *any* line of the paragraph (or its setext underline), all lines of
   that paragraph reveal their raw source lines (each source line is laid out on its own line, with
   its leading continuation spaces and any trailing hard-break markers visible). Inline markers
   (`**`, `` ` ``, `[text](url)`, `\*`) on the other lines of the revealed paragraph keep their
   per-line reveal rule unless the caret or selection also touches their line, so moving the caret
   onto one line of a paragraph reveals the paragraph's line structure without exposing inline
   syntax on every other line of the paragraph.
2. **CommonMark hard line breaks (section 6.7).** A line inside a paragraph that is followed by
   another line of the same paragraph and ends with two or more spaces (`' '`, U+0020) or an
   unescaped `\` (outside an inline code span) creates a hard line break: while the paragraph is
   flowing, the trailing spaces or trailing `\` are concealed (`HideMode::Collapse`) and the next
   source line starts on a new visual line at the paragraph's left margin (or hanging indent). While
   revealed, the trailing spaces or `\` are shown (`\` styled with `Role::EscapeMarker` /
   `escape.marker`). A trailing `\` or trailing spaces at the very end of the last line of a
   paragraph is not a hard line break (CommonMark 6.7); a trailing `\` there stays literal text.
3. **Bullet list item continuations (CommonMark 5.2).** Continuation lines of a bullet list item's
   paragraph (whether indented to the item's content column or lazy continuation lines, ADR 0006)
   flow with the bullet item's paragraph while the caret and selection are outside the paragraph.
   Leading spaces or tabs on a continuation line (`0 .. firstNonBlank`) are concealed while flowing,
   and all wrapped and hard-broken lines of the bullet item's paragraph align to the bullet item's
   hanging indent (`wrapIndent`).
4. **Lazy paragraph continuations (`1. item`, `> text`, ADR 0006).** Even though ordered list
   markers and blockquote markers are not styled as elements yet (`BlockState::kLazyParagraph` in
   ADR 0006), their text is paragraph text (`BlockState::inParagraph() == true`), so their
   continuation lines conceal leading spaces and flow with the paragraph while the caret is outside.
5. **Single text buffer (ADR 0002 & ADR 0003).** Each source line remains its own `QTextBlock` in
   `QTextDocument`; no characters are inserted, removed or rewritten in the document buffer.
   `BlockLayout` chains the `QTextLine`s of consecutive blocks of a flowing paragraph across block
   boundaries, separating soft-broken lines by the width of a single space and wrapping words across
   lines as one paragraph.

## Rules

All rules follow CommonMark 0.31.2 (sections 4.8 Paragraphs, 5.2 List items, 6.7 Hard line breaks,
6.8 Soft line breaks) within the elements hitokoto implements so far.

- **Which lines belong to a paragraph:**
  - A paragraph starts on a plain paragraph line or a bullet list item line (`BlockState::paragraph`
    with `underlinable() == true`, i.e. `paragraph >= 1 && paragraph != kLazyParagraph`).
  - A subsequent line continues the same paragraph (`BlockLink::continues == true` in
    `block_rules.cpp`) when the previous line's `BlockState` has `underlinable() == true` and the
    subsequent line is a plain paragraph continuation line (not blank, not an ATX heading, not a
    fenced code fence, not a thematic break or setext underline, not a bullet marker, not a pipe
    table start, and not a lazy ordered-list or blockquote marker that starts its own block unless
    it is a lazy continuation).
  - If a setext heading underline (`===` or `---`) immediately follows the paragraph's lines, all
    lines of that paragraph are heading lines (styled with `Role::Heading1` or `Role::Heading2`,
    S023) and still flow together as one wrapped heading while neither the paragraph lines nor the
    underline are in the revealed `LineRange`.
- **Leading spaces on paragraph lines (CommonMark 4.8):**
  - On a continuation line (`continues == true`) of a flowing paragraph, leading spaces and tabs up
    to the first non-blank byte (`0 .. firstNonBlank(line)`) are concealed with `HideMode::Collapse`.
  - On the first line of a plain top-level paragraph (up to 3 leading spaces, not a bullet item),
    leading spaces (`0 .. firstNonBlank(line)`) are also concealed with `HideMode::Collapse` while
    the paragraph is flowing (CommonMark 4.8: leading spaces in a paragraph are skipped).
  - On the first line of a bullet list item, the bullet element (S003) already conceals the prefix
    up to the content column and sets `wrapIndent` to the content column. Continuation lines of a
    bullet item inherit `wrapIndent` from the first line of the paragraph.
  - While the paragraph is revealed, leading spaces on all lines of the paragraph are shown as raw
    text.
- **Soft line breaks vs hard line breaks (CommonMark 6.7, 6.8):**
  - Only a line `i` that is continued by line `i + 1` (`continues` on line `i + 1` is true) has a
    line break inside the paragraph. The last line of a paragraph has neither a soft nor a hard line
    break after it.
  - **Backslash hard break:** Line `i` (when continued by line `i + 1`) ends with a backslash hard
    break if its last byte is `'\\'` (`0x5C`), the preceding consecutive `'\\'` bytes form an odd
    run (so the final `'\\'` is not itself escaped by an immediately preceding `'\\'`, S020), and
    that final `'\\'` byte does not lie inside an `opaqueSpan` (such as an inline code span, S015,
    or a link destination/title, S022).
    - The final `'\\'` byte is always given a `StyledRange` with `Role::EscapeMarker`
      (`escape.marker` token, S020), whether the paragraph is flowing or revealed.
    - While the paragraph is flowing, that final `'\\'` byte (and any spaces or tabs immediately
      before earlier trailing spaces? No: in CommonMark 6.7, a backslash hard break is `\` at the
      end of the line; spaces before `\` are preserved) is concealed in `hidden` with
      `HideMode::Collapse`, and `LineRender::hardBreak` is `true` (`flowsToNext` is `false`).
    - On the last line of a paragraph (not continued by `i + 1`), a trailing unescaped `'\\'` is
      ordinary literal text (CommonMark 6.7: "Hard line breaks cannot occur at the end of a block"):
      it gets neither `Role::EscapeMarker` nor a `HiddenRange`.
  - **Two-space hard break:** Line `i` (when continued by line `i + 1`) ends with a two-space hard
    break if it ends with two or more `' '` (`0x20`) bytes and the last two `' '` bytes do not lie
    inside an `opaqueSpan` (e.g. an inline code span whose closing backtick is before the spaces is
    fine; spaces inside a code span are inside `opaqueSpans`). All trailing `' '` bytes at the end
    of line `i` (outside `opaqueSpans`) are concealed in `hidden` with `HideMode::Collapse` while
    the paragraph is flowing, and `LineRender::hardBreak` is `true` (`flowsToNext` is `false`).
    While the paragraph is revealed, the trailing spaces are not concealed.
    - Note (CommonMark 6.7): tabs at the end of a line do not create a hard line break; a line
      ending in `' \t'` or `'\t\t'` is a soft line break.
  - **Soft line break:** Any other line `i` that is continued by line `i + 1` ends with a soft line
    break. While the paragraph is flowing, any trailing `' '` or `'\t'` bytes at the end of line `i`
    (outside `opaqueSpans`) are concealed in `hidden` with `HideMode::Collapse`, and
    `LineRender::flowsToNext` is `true` (`hardBreak` is `false`).
- **How `BlockLayout` lays out flowing paragraphs (ADR 0003):**
  - When block `k` has `flowsToNext == true`, block `k + 1` continues on the last visual `QTextLine`
    of block `k`, separated by one space width of block `k`'s font (`QFontMetricsF::horizontalAdvance(u' ')`),
    provided the first line of block `k + 1` fits at least one word in the remaining width of the
    text column (`rightEdge - (prevLineEndX + spaceWidth)`).
  - If the first word of block `k + 1` fits in that remaining width, block `k + 1`'s first
    `QTextLine` (`line 0`) is positioned at `x = prevLineEndX + spaceWidth`, `y = 0` (relative to
    block `k + 1`'s top, which equals the `y` of block `k`'s last visual line), with width
    `rightEdge - x`. Any subsequent wrapped lines (`line 1, 2, ...`) of block `k + 1` are positioned
    at the paragraph's hanging indent `subLeft = pad + indent` with width `subWidth = textW - indent`.
  - If the first word of block `k + 1` does not fit in the remaining width on block `k`'s last
    visual line, block `k + 1`'s `line 0` starts on a new visual line below block `k` at
    `x = subLeft`, `y = 0`, with width `subWidth`.
  - When block `k` has `hardBreak == true` (while the paragraph is flowing), block `k + 1` starts on
    a new visual line directly below block `k`'s last visual line, at the paragraph's hanging indent
    `subLeft = pad + indent` with width `subWidth`.
  - When the paragraph is revealed (caret or selection touches any line of the paragraph or its
    setext underline), every block of the paragraph is laid out independently on its own visual line
    starting at `x = pad` (or `pad + indent` on the bullet marker line if unrevealed, though the
    caret inside the paragraph reveals the paragraph's flow).
- **Caret movement, clicking, selection, and find:**
  - Clicking anywhere on a flowing paragraph (S010) hit-tests across all blocks sharing that visual
    line, moves the caret to the clicked character in its `QTextBlock`, and reveals the paragraph's
    source lines while keeping the surrounding viewport still (`EditorWidget::keepContentStill`).
  - Moving the caret into a paragraph by keyboard (`Up`, `Down`, `Left`, `Right`, `Home`, `End`,
    `PageUp`, `PageDown`) reveals the paragraph's source lines; moving the caret out of the
    paragraph causes the paragraph to flow again.
  - Find in file (S028): while the find bar is open and stepping through matches (`Enter` / `F3`),
    the caret does not move, so a match inside a flowing paragraph is highlighted in place on the
    flowing paragraph (including matches whose whitespace spans a soft line break between two source
    lines). Closing the find bar (`Escape`) selects the match, which places the selection inside the
    paragraph and reveals its source lines.
- **Reading position (S019 finding 2):**
  - A new width or font re-estimates every block; the line at the top of the view stays at the top.
    The reading position is the text that starts that line (in the first block of a line that
    blocks share) and how far into the line the view starts, not a share of a block's height: a
    block that shares its first line has more or fewer lines at another width, and the same share
    would land on another of its lines.
  - Consecutive resizes and theme changes return to the same reading position until the view
    scrolls; taken anew at each one, it would drift. At the end of the document the view returns to
    the same line as well, although the scroll range takes in new heights only later.
  - After the caret moved or text was typed at it, the caret stays on screen while blocks are laid
    out and restyled in idle slices, until the reader scrolls it off screen. Undo and Redo from the
    Edit menu count as typing.
  - A resize while the caret is followed: if the new width puts the caret off screen, the view
    follows the caret and lets the reading position go (review O4 of the layout-cost review).

## Acceptance scenarios

**Scenario: consecutive lines of a paragraph flow as one wrapped block**
- Given a document with a three-line paragraph:
  ```markdown
  First short line
  second short line
  third short line.
  ```
  and the caret on a line outside the paragraph, in a viewport wide enough for all three lines to
  fit on one visual line
- Then the three lines are laid out on a single visual line with a single space gap between
  `line` and `second` and between `line` and `third`
- And when the viewport is narrowed, the paragraph wraps across visual lines at word boundaries as
  a single continuous paragraph

**Scenario: caret or selection in any line of the paragraph reveals all its source lines**
- Given the three-line paragraph above with the caret outside it
- When the caret moves onto line 1, line 2, or line 3 of the paragraph (or a selection touches any
  of its lines)
- Then each of the three source lines is shown on its own visual line
- And inline syntax on line 3 (such as `**bold**`) stays concealed while the caret is on line 1,
  and reveals only when the caret or selection touches line 3
- And when the caret leaves the paragraph, the three lines flow together again

**Scenario: backslash hard line break inside a paragraph**
- Given a paragraph:
  ```markdown
  First line with a hard break\
  second line flows into
  third line.
  ```
- When the caret is outside the paragraph
- Then the trailing `\` on line 1 is concealed, line 2 starts on a new visual line below line 1, and
  line 3 flows on the same visual line after line 2
- And when the caret enters the paragraph, the trailing `\` on line 1 is visible and styled with
  `escape.marker`
- And a trailing `\` on the last line of a paragraph (`third line.\`) is not a hard break and is
  shown as literal `\` both when flowing and when revealed

**Scenario: two-space hard line break inside a paragraph**
- Given a paragraph whose first line ends with two or more spaces (`"First line  "`) followed by
  `"second line flows into\nthird line."`
- When the caret is outside the paragraph
- Then the trailing spaces on line 1 are concealed, line 2 starts on a new visual line below line 1,
  and line 3 flows after line 2
- And a line ending in a single space (`"First line "`) or a tab (`"First line\t"`) is a soft line
  break and flows into the next line with a single space gap

**Scenario: bullet list item with continuation lines flows at the bullet's hanging indent**
- Given a bullet list item with indented or lazy continuation lines:
  ```markdown
  - First line of the bullet item
    second line indented two spaces
    third line of the bullet item.
  ```
- When the caret is outside the bullet item's paragraph
- Then the leading spaces on lines 2 and 3 are concealed, the three lines flow together as one
  paragraph after the `• ` bullet, and every wrapped line of that paragraph (and any line after a
  hard line break inside it) starts at the bullet item's hanging indent (`wrapIndent`)
- And when the caret enters any of the three lines, all three lines are shown on separate lines
  with their leading spaces visible

**Scenario: multi-line setext heading flows when unrevealed and reveals when entered**
- Given a two-line setext heading:
  ```markdown
  Heading part one
  heading part two
  ----------------
  ```
- When the caret is outside the heading and its underline
- Then `Heading part one` and `heading part two` flow together as a single wrapped H2 heading and
  the `----------------` underline is concealed
- And when the caret moves onto line 1, line 2, or the underline on line 3, both heading lines and
  the underline are shown on their own lines

**Scenario: clicking a flowing paragraph places the caret at the clicked character**
- Given a multi-line paragraph flowing on screen with the caret outside it
- When I click on a word that belongs to the second or third source line of the paragraph
- Then the caret lands on that word's character in its `QTextBlock` and the paragraph reveals its
  source lines without jumping the surrounding viewport

**Scenario: find in file across a soft line break in a flowing paragraph**
- Given a flowing paragraph `"First line\nsecond line"` and the caret outside it
- When I search for `"line second"` in the find bar (S028)
- Then the match is found and highlighted across the two lines while the paragraph stays flowing
- And when I press `Escape` to close the find bar, the match is selected and the paragraph reveals
  its source lines

**Scenario: large hard-wrapped paragraph keeps performance budgets (S007)**
- Given a 20,000-line hard-wrapped paragraph (no blank lines)
- When I type inside it, move the caret into it, or move the caret out of it
- Then every keystroke and caret move completes within the 50 ms budget (synchronous work bounded to
  4 ms, any remaining restyling completed in idle slices)

**Scenario: resizing keeps the top line inside a flowing paragraph (S019)**
- Given a long document of flowing paragraphs whose short source lines share visual lines
- And the view scrolled so that its top line belongs to a block that shares its first line with
  several blocks before it
- When I make the window narrower and then wider again
- Then the same block is at the top of the view after each resize

**Scenario: the file stays byte for byte (ADR 0002)**
- Given a document with flowing paragraphs, hard line breaks (`\` and `2+` spaces), and bullet
  continuations
- When I open, walk the caret through every line, and save
- Then the saved file is identical byte for byte to the original

**Scenario: concealment quality gate (S009)**
- Given a document mixing flowing paragraphs, hard line breaks, bullet continuations, setext
  headings, code blocks, and inline formatting
- When the caret walks through every line with Up/Down/Left/Right/Home/End
- Then the caret is always visible, lines outside the entered/exited paragraph never change height,
  and the viewport stays still (`keepContentStill`) when a paragraph reveals or conceals

## Theme tokens

No new theme tokens are required:
- Paragraph text uses the existing editor/heading/inline tokens.
- A trailing `\` hard line break uses the existing `escape.marker` token from S020 (`Role::EscapeMarker`).

## Requirements for the implementation

- **Core (`src/core`):**
  - Extend `LineRender` in `src/core/include/hitokoto/core/markdown.hpp` with:
    - `bool flowsToNext{false}`: true when this line is part of an unrevealed paragraph and flows
      across a soft line break into the next line (`i + 1`).
    - `bool hardBreak{false}`: true when this line is part of an unrevealed paragraph, continues to
      line `i + 1`, and ends with a CommonMark hard line break (`2+` spaces or unescaped `\`).
    - `std::size_t wrapIndent{0}`: set on all lines of an unrevealed bullet list item's paragraph
      (not only the first line) so continuation and hard-broken lines know their hanging indent.
    - Update `LineRender::empty()` and `operator==` accordingly.
  - In `ElementRegistry::renderDocument`:
    - Identify contiguous paragraph ranges (`start .. end`, plus any attached setext underline at
      `end + 1`).
    - A paragraph is revealed (`paragraphRevealed`) if `revealed` overlaps `start .. end` (or
      `end + 1` when underlined as a setext heading).
    - Per-line inline markers still use `revealed.contains(i)`, while paragraph flow
      (`flowsToNext`, `hardBreak`, leading space concealment on paragraph lines, trailing space or
      `\` concealment on lines `start .. end - 1`, and `wrapIndent` propagation on bullet
      continuations) uses `!paragraphRevealed`.
    - On any line `i` (`start <= i < end`) that ends with an unescaped `'\\'` outside `opaqueSpans`,
      emit a `StyledRange` with `Role::EscapeMarker` for that `'\\'` byte (both when flowing and
      when revealed) and, when `!paragraphRevealed`, a `HiddenRange` with `HideMode::Collapse`.
  - In `src/core/src/find.cpp` (`lineUnits`):
    - Ensure that concealing leading/trailing spaces on flowing paragraph lines does not change
      `findInDocument` / `DocumentFinder` results (including queries with leading or trailing
      whitespace across soft or hard line breaks).
- **UI (`src/ui`):**
  - `BlockData`: store paragraph flow state (`flowsToNext`, `hardBreak`, `wrapIndent` / paragraph
    continuation state) so `BlockLayout` and `BlockStyler` can inspect it in $O(1)$ per block.
  - `BlockStyler`:
    - Produce the exact same `LineRender` as `ElementRegistry::renderDocument` for every styled
      block.
    - When the caret or selection enters or leaves a paragraph, restyle all lines of that paragraph
      (bounded by the 4 ms budget, continuing in idle slices for pathological 20,000-line
      paragraphs, just like S023).
    - When block `i` is edited or restyled, also update block `i - 1` if block `i` starts or stops
      continuing block `i - 1`'s paragraph (because block `i - 1`'s `flowsToNext`, `hardBreak`, and
      trailing `\` role depend on whether block `i` continues its paragraph).
  - `BlockLayout`:
    - Lay out consecutive blocks with `flowsToNext == true` as chained lines of one paragraph, and
      blocks after `hardBreak == true` at the paragraph's hanging indent `subLeft` on a new visual
      line.
    - Keep `heights_[k] >= 0` for every block in `HeightIndex` by assigning the shared visual line's
      vertical advance to the last block on that visual line (and `0` to earlier blocks whose only
      line is shared with the next block).
    - Update `draw`, `hitTest`, `blockBoundingRect`, and `visibleBlockRange` to step back to the
      first block sharing a visual line (`firstBlockOnVisualLine`) so all blocks on a shared visual
      line are painted and hit-tested together.
    - `blockOfTextAt(y)` returns the block whose own advance covers `y`, without stepping back;
      `EditorWidget::captureAnchor` takes the scroll anchor from it.
- **Benchmarks (`s033_*`):**
  - Add core and UI benchmarks for opening, typing in, and entering/leaving a flowing paragraph
    (including a 20,000-line hard-wrapped paragraph).

## Tests

- Core: `tests/core/test_s033_soft_line_breaks.cpp` (`S033_SoftLineBreaks`) – soft line breaks,
  leading and trailing space concealment, two-space hard breaks, backslash hard breaks (including
  escaped `\\`, code spans, links, and trailing `\` on the last line of a paragraph), bullet list
  continuations and `wrapIndent` propagation, setext headings with multi-line paragraphs, paragraph
  reveal vs per-line inline reveal, find in file across soft and hard breaks.
- UI: `tests/ui/tst_s033_soft_line_breaks.cpp` (`tst_s033_soft_line_breaks`) – visual line chaining
  and word wrapping across blocks, caret/selection reveal and conceal of paragraphs, hard breaks
  (`\` and `2+` spaces), bullet continuations at hanging indent, clicking words on continuation
  blocks, a continuation line whose first word does not fit beside the line above starting a new
  visual line of the paragraph (`orderedListContinuationThatDoesNotFitStartsANewLine`, widths from
  the editor's font), Up/Down/Left/Right navigation, find-bar highlighting in flowing paragraphs,
  20,000-line paragraph latency budget, byte-exact save, reading position on resize inside a shared
  visual line (`resizingKeepsTheTopLineInsideAFlowingParagraph`), the same top line after a resize
  and back at many widths, in the middle and at the end of the document
  (`aResizeAndBackShowsTheSameTopLine`, `aResizeAndBackAtTheEndShowsTheSameTopLine`), a new reading
  position once the view scrolls (`scrollingAfterAResizeTakesTheNewTopLine`), one restore of the
  reading position per resize although Qt nests a second `resizeEvent`
  (`aResizeRestoresTheReadingPositionOnce`), only the screen styled at once after an edit of many
  lines (`insertingManyLinesStylesOnlyTheScreenAtOnce`), each block of a screen laid out about once
  (`layingOutTheScreenLaysOutEachBlockAboutOnce`), a block that stops sharing a line
  (`aBlockThatStopsSharingALineGetsItsTopWithoutTheBlockBeforeLaidOut`), estimates close to the
  laid-out height and a paste that follows the caret once, also with a font whose own average
  character width is off (`theEstimatedHeightOfAFlowingParagraphIsCloseToItsLaidOutHeight`,
  `theEstimatedHeightHoldsWithAFontWhoseAverageWidthIsOff`,
  `pastingManyShortLinesLaysOutTheScreenAtTheCaretAboutOnce`,
  `pastingManyShortLinesWithAFontWhoseAverageWidthIsOff`), the caret on screen while a long
  paragraph is restyled in slices (`theCaretStaysOnScreenWhileALongParagraphIsRestyledInSlices`),
  a long paragraph whose flow changes again laid out no further
  (`changingTheFlowOfALongParagraphAgainLaysOutNoMoreOfIt`, with fixed slices), a key before a
  long paragraph that searches its extent about as far as the screen
  (`aKeyBeforeALongParagraphSearchesItsExtentAboutAsFarAsTheScreen`), and in steps of the slices
  for tests when they are set (`withSlicesForTestsAKeyWalksALongParagraphInStepsOfThatSize`), a
  chunk inserted below lines not yet styled that leaves them to the idle styler
  (`aChunkBelowLinesNotYetStyledLeavesThemToTheIdleStyler`), the order of the layout's signals
  (`aHeightChangeIsReportedBeforeTheNewSize`) and the caret on screen through Undo and Redo from the
  Edit menu (`theCaretStaysOnScreenThroughUndoAndRedoFromTheMenu`).
- S027: `tst_s027_smooth_scrolling::shiftDuringExternalSetValueUsesCurrentBarValue` – a shift while
  the program sets the scroll bar starts from the bar's value.
- S009 & S023: update `tst_s009_concealment` and `tst_s023_thematic_breaks` where they previously
  asserted that a 2-line setext heading had the same height while unrevealed as while revealed, and
  add `flowingParagraphsWhileWalking` to `tst_s009_concealment`.
- S002: `tst_s002_headings::onlyAffectedLinesAreRestyled` accounts for entering the 9-line flowing
  paragraph `101..109` (revealing its 9 lines on entry, then restyling only 2 lines on `101 -> 102`
  and 2 lines per key on `"abc"`).

## Implementation

Implemented on 2026-11-09 following ADR 0003 and ADR 0006:

- Core (`src/core`): `LineRender::flowsToNext` and `LineRender::hardBreak`; `LineContext::paragraphRevealed`
  and `LineContext::nextNextLine`; `ElementRegistry::render` and `ElementRegistry::renderDocument`
  implement CommonMark 4.8 leading space concealment, 6.7 hard line breaks (`2+` trailing spaces or
  unescaped trailing `\` with `Role::EscapeMarker`), 6.8 soft line breaks (`flowsToNext`), and 5.2
  bullet continuation `wrapIndent` propagation.
- UI (`src/ui`): `BlockLayout` chains consecutive flowing blocks onto shared visual `QTextLine`s
  (`layoutContinuationBlock`, `layoutParagraphChain`, `flushDirtyChains`, `firstBlockOnVisualLine`,
  `advanceOf` vs `heightOf`) with `HeightIndex` invariants preserved (`heights_[k] >= 0`);
  `BlockStyler` tracks `revealedParagraph_` via bounded `ParagraphExtent` walks and caches
  `kStyledParagraphRevealed` in `QTextBlock::userState()` so caret movement and typing inside a
  paragraph remain $O(1)$.

Rework on 2026-11-10 (branch `agents/s033-resize-anchor`; CI job Build and test (Linux min Qt) was
red in `tst_s019_editor_surface::resizingKeepsTheReadingPosition`):

- `SmoothScroller::shift` without a running animation starts from the scroll bar's value, not from
  a target that the scroller's own `valueChanged` slot has not updated yet (763b39c).
- Qt lays the viewport out with the old margins before `EditorWidget::resizeEvent` sets the new
  ones, so a second `resizeEvent` runs inside the first; it no longer captures or restores the
  scroll anchor, and the margins are set before `QTextEdit::resizeEvent` (763b39c).
- The scroll anchor is the block covering the top of the view (`BlockLayout::blockOfTextAt`), not
  the first block of a visual line it shares (`BlockLayout::firstBlockOfLineAt`; the two were
  `blockCovering` and `blockAt` until review O5 named them by what they return).
- The guard against the nested `resizeEvent` has a test of its own (review O3): one resize restores
  the reading position once; `EditorWidget::anchorRestores()` counts the restores for tests.
- Known limit (review `docs/reviews/033-soft-line-breaks-code-resize-anchor.md`, O1), left to this
  story's owner (decision Armin, 2026-11-11): inside a paragraph of more than 64 source lines the
  top line can move by one block on a resize, because a paragraph is laid out from at most 64
  blocks back (`kMaxLookback`). Review O2 was fixed in the rework of 2026-11-11.

Rework on 2026-11-11 (branch `agents/s033-layout-cost`, from be81762; the benchmark of PR #37
against main was red, `s031_paste_no_table_5000_lines_ms` 29 -> 139 ms among others, and the CI
on the follow-up was red on Windows and macOS):

- The visible range adds up the blocks' own advances (63cddaf). Between Qt's `contentsChange` and
  the layout's `documentChanged` the height index still has the old size, so every new block of an
  edit got the same top and seemed to be on screen; the whole edit was styled and laid out at once.
- Laying a changed chain out again lays out only what had lines (7ca2a47, `ChainScope::Extend` and
  `LaidOutOnly`): each island of laid-out blocks grew by the lookback on every change of flow, and
  restyling cost more each time (`s023_underline_settled_ms` 97, 127, 170 ... 496 ms).
- A chain is laid out from the last block with lines, and the forward pass stops at a block whose
  lines did not change (7f459d1): a screen took about 20 layouts per block.
  `BlockLayout::blockLayouts()` counts them for tests.
- A block that stops sharing a line updates the advance of the block before it, also when that one
  has no lines (7371461; MSVC evaluates the operands of a comparison in the other order and found
  it).
- Review O2 fixed (ed0b7b3, 7787bdd): a block that goes on in the last line of the block before is
  estimated at its share of a line, measured with the width of running text in the editor's font
  (a sample, not `QFontMetricsF::averageCharWidth`, which is 20-30 % too wide for Arial, Helvetica
  and their metric twins).
- The reading position is the text that starts the top line plus how far into that line the view
  starts (28cb865), held across consecutive resizes and theme changes until the reader scrolls; at
  the end of the document the restore is not cut short by a scroll range that has not taken in the
  new heights yet, and it is shown again once the layout settles (24acd13).
- The layout reports a height change, and the end of a batch, before the new document size, so the
  view keeps its content still before the scroll range shrinks; the other way round, the end of the
  document cut the value short and the shift moved it a second time. The view follows the caret
  after the caret moved or text was typed at it, until a scroll leaves the caret off screen: while
  a long paragraph is restyled in idle slices, the caret no longer ends off screen.
  `BlockStyler::setSliceBlocksForTests` makes the idle slices end after a fixed number of blocks
  in tests.
- On a key, the reveal of the paragraph at the caret searches the paragraph's extent only about as
  far as the screen (and 512 lines at least) instead of for up to 2 ms; idle slices walk on to its
  ends as before (`s023_bullet_typing_median_ms`: "- " before a paragraph of 20,000 lines).
  `BlockStyler::extentSteps()` counts the lines walked, for tests.
- Known limit, left to this story's owner: how a chain wraps depends on which blocks before it
  already have lines (`Extend` stops at the first block with lines, a block without lines whose
  block before has none either stays without lines). A block partly on screen at the top can be
  laid out again later with other line breaks and move by a line; the reading position after a
  resize is restored again when that happens, the view at other times is not.
- Known limit: a scroll of the reader's that ends exactly on the maximum in the same turn of the
  event loop as a new, smaller scroll range counts as a clamp and keeps the held reading position.
- Opening a file in chunks no longer works out the block states of the lines not yet styled a
  second time: a line above an edit that is not styled yet is left to the idle styler instead of
  styled at once (f504457 on GitHub; the line came with the flow, ad6b6ec). In the cloud the plain
  10 MB benchmark file loaded in 1.95-2.01 s before and 1.77-1.79 s after (main 1.73-1.80 s), and
  was styled in 2.96-3.12 s before and 2.77-2.84 s after (main 2.58-2.66 s).
  `BlockStyler::stateSteps()` counts the states worked out on their own, for tests.
- Accepted (review O1, decision Armin 2026-11-12; the pull request carries the label
  `bench-accepted`): `s023_open_10mb_styled_ms` and `s031_paste_no_table_5000_lines_ms` stay SLOW
  against main. CI on f504457 (against main on the same runner): 2.34-2.37 s -> 2.96-2.99 s
  (+26 %) and 28.8-29.3 ms -> 39.0-39.4 ms (+35 %); the first screen of the 10 MB file is shown
  as on main, and the plain 10 MB file is within the gate (loaded +1 %, styled +10 %). In the
  opening most of the difference is painting (callgrind: 59 instead of 37 line pieces per frame,
  each cheaper than on main, and the scroll bar painted again in almost every frame because the
  styling lets paragraphs off screen flow anew); in the paste of 5,000 short lines, which become
  one flowing paragraph, about 4.6 times as many source lines are on a screen. Both follow from the
  flow; only the scroll bar's share could be reduced, through an estimate that takes the flow into
  account.

Review of the rework (`docs/reviews/033-soft-line-breaks-code-layout-cost.md`, 2026-11-12):

- O3 fixed: `BlockStyler::setSliceBlocksForTests` ends every walk of the styler that otherwise ends
  after a time (setext walks, level jobs, the searches for a paragraph's extent, fences and front
  matter) after the given number of lines, not only the idle styling slices.
- O5 fixed: Undo and Redo of the Edit menu (`EditorWidget::undoEdit`, `redoEdit`) count as typing.
- O8 fixed: the extent test allows what the code may walk (512 lines past the screen, plus the
  screen), not twice as much.
- O9 fixed: a test pins that a height change, and the end of a batch, is reported before the new
  size.
- O7 and O10 documented in the code: why a walk takes at least 512 lines (`kWalkedAtOnce`), and why
  `settleAnchor` stops after four passes (`kMaxSettlePasses`).
- Known limit (O2): a clamp of the scroll value nested inside another clamp consumes the flag; the
  outer one counts as a scroll of the reader's and lets the held reading position go, and the
  followed caret if it is off screen. Rare; a test that reproduces the nesting reliably would be
  costly.
- Known limit (O6): estimates measure the width of a Latin sample; CJK characters are about twice as
  wide, so CJK paragraphs not yet laid out are estimated at about half their height, and the scroll
  bar moves until they are laid out. `averageCharWidth` before had the same bias.
- Known limit (O11): `tst_s033` takes about 15 s in the cloud (3 s on 24acd13; 13.6 s on Windows
  before O3): the fixtures of 3,000 and 20,000 lines and the fixed slices that make the tests the
  same on every machine.
