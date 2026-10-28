# ADR 0003: Editor surface: QTextEdit with our own document layout

- **Status:** Accepted (2026-10-27, on the Linux results of S019; the user waived the Windows and
  macOS CI runs for this decision)
- **Date:** 2026-10-27
- **Deciders:** project owner

## Context

ADR 0001 started with a `QPlainTextEdit` adapter and planned an exit ("plan B": our own surface on
`QAbstractScrollArea` + `QTextLayout`); S009 is the gate that decides. The tables story (S018)
forced the question:

- `QPlainTextDocumentLayout` supports no tables or frames (Qt documentation), ignores line height
  and paragraph margins, and derives a block's height only from its layout lines. A block cannot
  reserve space for anything else (probe with Qt 6.12.0).
- `QPlainTextEdit` sizes its scroll bar in lines. The tables branch laid out each cell as its own
  `QTextLine` and set the block's line count to 1; the mismatch made the scroll bar flip without
  end (review objection O2). Matching the counts ends the loop but makes scrolling jump inside
  tables (O8).
- Table cells need line breaks (`<br>` in GFM). Qt sets rich text with tables, wrapping and
  `<br/>` itself (`QTextDocument::setMarkdown`, `QTableView`); we do not want to implement that.

The project wants to use as much of Qt as possible.

## Decision

1. **`EditorWidget` derives from `QTextEdit`** instead of `QPlainTextEdit`, with
   `setAcceptRichText(false)`. Caret, selection, mouse, keyboard editing, clipboard, drag and
   drop, undo, input methods and accessibility stay Qt's (`QTextEdit` and its text control).
2. **The document gets our own layout**, `BlockLayout` (`src/ui`), a subclass of the public
   `QAbstractTextDocumentLayout`, replacing `QTextDocumentLayout`:
   - one `QTextLayout` per block, as before; blocks are laid out lazily (visible blocks first,
     the rest in idle slices), never the whole document at once;
   - geometry in pixels: a height per block, estimated from the line height until the block is
     laid out, and prefix sums so that block ↔ y lookups cost O(log n) for 10 MB files;
   - a block can be hidden (height 0) or reserve a given height for an embedded widget.
3. **The text stays one plain `QTextDocument`** (ADR 0002): one block per line, no frames or
   tables in the document. Embedded widgets (S018 tables) read and write those lines through
   `QTextCursor`, so undo stays in the document.
4. **Concealment is unchanged** (1-px technique from ADR 0001). S009 keeps checking it on the new
   surface.

## Alternatives

| Option | Why not |
|---|---|
| Keep `QPlainTextEdit` | Line-based scrolling and no reserved space; tables need the cell-line workaround (O2, O8). |
| `QTextEdit` with Qt's `QTextDocumentLayout` and `QTextTable` | Tables become document structure, not Markdown lines (ADR 0002 would need a second model and our own byte-exact writer); `QTextDocumentLayout` lays out the whole document, which ADR 0001 avoided for 10 MB files. |
| Own surface on `QAbstractScrollArea` | Caret, selection, IME, accessibility, clipboard and undo handling would be ours; `QTextEdit` already provides them on top of any `QAbstractTextDocumentLayout`. |

## Consequences

- `QTextEdit` computes the scroll range from the layout's document size in pixels and stops
  re-adjusting after a few passes, so a scroll bar that changes the width cannot loop.
- Code that used `QPlainTextEdit` API (`firstVisibleBlock`, `blockBoundingGeometry`,
  `contentOffset`, `QPlainTextDocumentLayout`) moves to `BlockLayout`.
- The document height of unlaid blocks is an estimate; the scroll bar may adjust slightly while
  idle layout proceeds.
- Risk: parts of `QTextEdit` assume Qt's own layout. `QTextEdit::documentSize` falls back to
  `QAbstractTextDocumentLayout::documentSize` for other layouts (Qt source); anything else that
  shows up is fixed in `BlockLayout` or reported here.

## Reconsider if

- The budgets (brief section 4) or the S009 scenarios fail on the new surface and cannot be met
  in `BlockLayout`.
- `QTextEdit` relies on `QTextDocumentLayout` internals in a way we cannot work around.
