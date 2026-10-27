# Architecture

hitokoto is C++20 with Qt 6 Widgets, built with CMake (ADR 0001). This page describes the layers,
the interfaces between them, and where a new feature goes.

## Layers

```
src/app    main(): command line, theme manager, main window           (Qt)
src/ui     thin adapter: editor widget, styler, file session, theming  (Qt)
src/core   all logic: Markdown elements, reveal, codec, file I/O, theme (C++ standard library only)
```

- `core` never includes or links Qt. CMake fails the configure step if it does, and CI builds and
  tests the core on a machine without Qt (`core-sanitize` preset).
- `ui` converts between core and Qt types in exactly one place: `src/ui/qt_bridge.hpp`.
- Text inside the core is UTF-16 (`std::u16string_view`), so positions map one-to-one to Qt's
  `QString` indices without conversion.

## Core interfaces

| Interface | File | Purpose |
|---|---|---|
| `LineElement` | `markdown.hpp` | One Markdown element: `parse()` (parse rule) and `render()` (render rule) |
| `MultiLineElement` | `markdown.hpp` | A block spanning lines (fenced code, tables): `parse(line, state)` / `parseWithNextLine()` and `render()`; owns every line while open |
| `InlineElement` | `markdown.hpp` | Adds inline style ranges, inline-code runs and concealed delimiter ranges after block rendering |
| `ElementRegistry` | `markdown.hpp` | Holds the elements; `render(line, {revealed, blockState, nextLine})` returns a `LineRender`; `blockStateAfter()` walks lines without rendering |
| `LineRender` | `markdown.hpp` | How one line is shown: heading level, table cells, block and inline style runs, hidden ranges, decorations |
| `BlockState` | `markdown.hpp` | Opaque state from one line to the next while a multi-line element is open |
| `revealedLines`, `revealChanges` | `reveal.hpp` | Which lines show raw syntax; which lines must re-render when that changes |
| `decodeUtf8`, `encodeUtf8` | `text_codec.hpp` | Bytes to text and back, preserving BOM and line endings (ADR 0002) |
| `readFile`, `writeFileAtomically` | `file_io.hpp` | Reading and atomic saving |
| `Theme`, `parseTheme`, `loadTheme` | `theme.hpp` | Theme tokens, theme file format, built-in themes |

`LineRender` is the contract between core and UI. It speaks in roles (`Marker`, `HeadingText`),
inline style flags (bold, italic, strikethrough), hide modes (`Collapse`, `KeepSpace`) and
decorations (`Bullet`), never in colours or fonts. The UI maps these to theme formats.

## UI pieces

| Class | Responsibility |
|---|---|
| `EditorWidget` | `QTextEdit` subclass (ADR 0003) with rich text off; the `QTextDocument` inside is the only copy of the text. Keeps the text on screen still when blocks above the viewport change height. Updates the revealed range on caret/selection changes, paints padded code-block panels and decorations, handles S010 clicks, centres the text column, and hands tables to `TableEmbedder`. |
| `TableEmbedder` | S018 / ADR 0005: finds the tables on and near the screen (±200 lines), lets the header line reserve the height of a `TableWidget` in `BlockLayout` and hides the other lines; places the widgets on scroll and layout changes. Moves the caret into and out of tables, switches to the source view (Ctrl+Shift+M) and changes rows and columns in the Markdown lines, each change one undo step. |
| `TableModel` | `QAbstractTableModel` over a table's Markdown lines: reads cells through the core (`tableRowCells`, `tableCellToText`) and writes an edited cell back into its line (`replaceTableCell`), so the lines stay the only copy of the text (ADR 0002). Alignment from the delimiter row; theme colours and fonts per row. |
| `TableView`, `TableWidget`, `TableDelegate`, `CellEditor` | The table on screen: a `QTableView` sized to its content (columns shrink to the text column width, rows to wrapped content) with hover `+` / `−` buttons for rows and columns. The delegate draws a cell with `QTextDocument::setMarkdown`; `CellEditor` (`QPlainTextEdit`) edits it, Shift+Enter is a line break (`<br>`), Enter, Tab and Backtab finish the cell. |
| `BlockLayout` | The document's layout (ADR 0003, `QAbstractTextDocumentLayout`): one `QTextLayout` per block, laid out when drawn or queried; a keystroke lays out its block at once, wider changes and restyling only drop layouts. Heights in pixels per block with prefix sums (Fenwick tree), estimated from the default font until a block is laid out. A block can be hidden or reserve a height for an embedded widget. Size changes during the styler's slices are reported once per slice (`Batch`). |
| `BlockStyler` | Applies `LineRender` to blocks as `QTextLayout` formats. A line containing `|` is styled with the next line in view, because a GFM table starts only when its delimiter row follows (S018). Edited lines and lines entering/leaving the revealed range are styled at once; the rest lazily: visible blocks first, then idle slices of at most 6 ms. Each block's user state records what it was styled for and caches the block state after the line; only lines that leave a multi-line block open keep that state in a `BlockData`, so plain lines allocate nothing. Edits propagate restyling only while the state changes. |
| `EditorFormats` | Theme tokens to `QTextCharFormat`s; `LineRender` to format ranges. |
| `DocumentSession` | Open/save. Reading, decoding, encoding and writing run on a worker thread (`QThreadPool`, one thread, so operations stay in order). Large files are inserted in slices of about 8 ms; the first 64 KB appear at once. |
| `application_icon` | Embeds the SVG for Qt windows and supplies derived native icons for Windows and macOS bundles. |
| `ThemeManager`, `theme_applier` | Load the theme file, fall back to the built-in theme, apply Fusion + palette + generated style sheet. |
| `MainWindow` | Menus, title, unsaved-changes prompts. Dialogs are replaceable for tests. |

## Concealing markers

Qt has no "invisible" character format. Collapsed markers (`#`) get a 1 px transparent font
(ADR 0001); kept-space markers (`-`) only a transparent colour, with the bullet dot painted on top,
so list text does not move when the caret enters the line. Story S009 keeps checking the 1 px
technique; since S019 it runs on `QTextEdit` with `BlockLayout` (ADR 0003).

## Adding a Markdown element

1. Implement `LineElement` for line-local block constructs, `MultiLineElement` for blocks that
   span lines (fenced code) or `InlineElement` for inline constructs in
   `src/core/src/elements/<name>.cpp`, with a factory function. Inline code runs take precedence
   over other inline styles. A multi-line element keeps a non-zero number of its own as state;
   the registry wraps it in `BlockState`, which the UI caches per block without knowing the
   element. While a block is open it owns each line; line and inline elements do not run there.
2. Register it in `ElementRegistry::withBuiltins()` (`add`, `addMultiLine`, `addInline`).
   Give it its own theme section with a token for every visible property and a fallback for
   each (ADR 0004, theming baseline); the UI reads resolved values (`codeBlockStyle()`, ...).
3. Add tests `S<nnn>_<Name>` in `tests/core`; if it needs a new role or decoration, extend
   `LineRender` and map it in `EditorFormats` / `EditorWidget::paintDecorations`.

Order per line: an open multi-line block, else the first multi-line element that opens a block,
else the first matching line element followed by the inline elements. An inline element declares
its trigger characters (`triggerCharacters()`, e.g. `` ` `` for inline code); the registry scans
the line once and runs only the elements whose characters occur, so most lines run none. The
registry then removes styled runs under hidden ranges. Inline elements run in registration order
and see the runs of the earlier ones: emphasis skips the characters that inline code claimed, so
inline code is registered first (`addInline` documents this). Nothing else connects two elements.
`S014_FencedCodeBlocks.MultiLineElementsAreAddedByRegistrationOnly` adds a test-only element
this way, `S013_InlineEmphasis.InlineElementsAreAddedByRegistrationOnly` a test-only inline element.

## Platform shims

The only OS-specific code is `src/core/src/platform.cpp`: flushing a file to disk before the
atomic rename (`fsync` / `FlushFileBuffers`) and syncing the directory on POSIX. The C++ standard
library cannot do this, and Qt may not be used in the core.

## Dependencies

| Dependency | Use | Where |
|---|---|---|
| Qt 6.8+ (Core, Gui, Widgets, Svg) | UI and SVG application icon | runtime |
| Qt Test | UI tests, end-to-end benchmark | development |
| GoogleTest 1.18 | core tests | development (installed package or fetched by tag) |
| Google Benchmark 1.10 | core benchmarks | development |
| libFuzzer (LLVM) | core fuzzing | development, Clang only |

## Threads

The UI thread owns the document. Worker work never touches Qt widgets or the document: it receives
copies (file bytes, a snapshot of the text) and posts results back with queued invocations.
`DocumentSession` waits for its worker in the destructor and before closing the window.
