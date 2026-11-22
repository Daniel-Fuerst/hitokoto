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
- `src/ui/qt_bridge.hpp` converts what passes between core and UI on every line: a `QString` seen
  as `std::u16string_view` without a copy, text back to `QString`, and core colours as `QColor`.
  File paths, font families, theme names, link destinations and messages are converted where
  they are used, with Qt's own functions (`QString::fromStdString`, `fromStdU16String`,
  `toStdU16String`); nothing checks where such a conversion stands.
- Text inside the core is UTF-16 (`std::u16string_view`), so positions map one-to-one to Qt's
  `QString` indices without conversion.

## Core interfaces

| Interface | File | Purpose |
|---|---|---|
| `LineElement` | `markdown.hpp` | One Markdown element: `parse()` (parse rule), `render()` (render rule), `paragraphRole()` and `startsAfterParagraph()` (its paragraph rules, ADR 0006) |
| `MultiLineElement` | `markdown.hpp` | A block spanning lines (fenced code, tables, front matter): `parseWithNextLine(line, state, nextLine)` and `render()`; owns every line while open; `documentStartState()` for a block that may only start the document |
| `InlineElement`, `InlineScan` | `markdown.hpp` | Adds inline style ranges, inline-code runs and concealed delimiter ranges after block rendering; `InlineScan` tells it which characters the elements before it made opaque or hidden |
| `ElementRegistry` | `markdown.hpp` | Holds the elements; `render(line, {revealed, blockState, nextLine, setextLevel})` returns a `LineRender`; `renderInline(text)` runs only the inline elements (the text of a table cell, S028); both run no inline element on a line above the registry's bounds (`InlineBounds`, `kInlineBounds`, `setInlineBounds`; S007); `blockStateAfter()` walks lines without rendering, `paragraphLink()` tells how a line relates to the paragraph above, `documentStart()` the state the first line may be entered with; `renderDocument()` renders a whole document and is the reference for the lazy rendering in the UI (ADR 0006) |
| `LineRender` | `markdown.hpp` | How one line is shown: heading level, table cells, thematic break, setext underline, closed front matter, block and inline style runs, hidden ranges, decorations |
| `BlockState` | `markdown.hpp` | Opaque state from one line to the next: the multi-line element that is open (`element`, `value`; `active()`) and, in a field of its own, the paragraph the line belongs to (`paragraph`: 0, or 1 + its content column; ADR 0006) |
| `parseBulletLine`, `indentBulletLine`, `outdentBulletLine` | `bullet.hpp` | S003: the prefix of a bullet item and its indentation, as lines |
| `tableRowCells`, `replaceTableCell`, `separateTextAfterTables` and the other table functions | `table.hpp` | GFM pipe tables as lines (S018, S026, S031): rows and cells to read, change and lay out; the table element itself is registered like any other (`markdown.hpp`) |
| block rules | `src/core/src/block_rules.hpp` | Internal: the one thematic-break recogniser, setext underlines, the start of blocks hitokoto does not render yet, the content column after a container marker (S023) |
| `revealedLines`, `revealChanges` | `reveal.hpp` | Which lines show raw syntax; which lines must re-render when that changes |
| `decodeUtf8`, `encodeUtf8` | `text_codec.hpp` | Bytes to text and back, preserving BOM and line endings (ADR 0002) |
| `replaceLoneSurrogates`, `findLoneSurrogate`, `placeOf` | `text_codec.hpp` | S004: text on its way into the editor with each half of a surrogate pair replaced by U+FFFD, so that it can always be encoded (ADR 0002, addendum of 2026-11-21); the first half left in a text and its line and column |
| `readFile`, `writeFileAtomically` | `file_io.hpp` | Reading and atomic saving |
| `FindQuery`, `FindUnitBuilder`, `scanUnits`, `documentUnits`, `findInDocument` | `find.hpp` | S028, ADR 0008: the concealed view as units (lines without their hidden ranges, link destinations as side units, table cells with only the inline elements run on them, keys and values of front matter) with soft and hard boundaries and a map back to the source; a search text as words between whitespace runs; scanning in pieces that resume where a match needs more units; simple Unicode case folding from a table generated from `CaseFolding.txt` (`foldCaseSimple`, `scripts/generate-case-folding.py`). `documentUnits` is the reference the UI's sliced pass must equal. `FindUnitBuilder::continueTable` lets a lookup start at a table row (S028 code review O8) |
| `pastedTablesToMarkdown`, `parseExcelPlainText`, `placePastedBlocks` | `paste.hpp` | S031, ADR 0011: the model of a pasted table (cells with spans, each a list of lines), whether it counts (two rows and two columns), its GFM Markdown (escaping, lines as `<br>`, merged cells, width), the text around tables as plain text, Excel's plain text as a table, and the blank lines around the tables at the caret |
| `scanHtml`, `kPasteHtmlBounds`, `htmlWithoutFormatting` | `paste.hpp` | S031, ADR 0011: one linear pass over a paste's HTML, read as Qt's HTML parser reads it: whether it holds tables side by side or nested, its cells, and the time Qt's import would take, estimated per unit of what it counts (elements by kind, attributes, nesting, the grid of each table with its spans, images in one block, the blocks of each list against those before them); a style sheet it meets is beyond the estimate. `htmlWithoutFormatting`, the same reading, writes the HTML Qt imports: of the attributes only the spans of cells and meta's `name` and `content` stay, and style elements lose their sheet. The import may take half of what S031 grants the cells |
| `parseProperties` | `properties.hpp` | S024, ADR 0007: the YAML of front matter read with a parser of our own for a subset of YAML 1.2 into the top-level properties (key, type, display text, list items, date, lines of key and value), or the first error with its line and reason (invalid or unsupported). Never throws, at most 64 levels deep, linear |
| `HeightIndex` | `height_index.hpp` | Block heights and their prefix sums for `BlockLayout` (Fenwick tree): block <-> y in O(log n). Keeps a gap of free slots at the last edit, like a gap buffer, so a new line costs O(log n) instead of rebuilding the sums of every block |
| `Settings`, `parseSettings`, `serializeSettings`, `loadSettingsFile`, `saveSettingsFile`, `updateThemeInSettingsSource`, `updateThemeSettingInFile` | `settings.hpp` | S039: persistent user configuration (`config.toml`) in the platform config folder (selected theme and editor options) with TOML-subset parsing, serialization, and atomic writes; picking a theme changes only the value of the theme key the parser reads and keeps the file unchanged when the parser would read anything else from the changed text |
| `Theme`, `parseTheme`, `loadTheme` | `theme.hpp` | Theme tokens, theme file format, built-in themes |

`LineRender` is the contract between core and UI. It speaks in roles (`Marker`, `HeadingText`),
inline style flags (bold, italic, strikethrough), hide modes (`Collapse`, `KeepSpace`) and
decorations (`Bullet`), never in colours or fonts. The UI maps these to theme formats.

## UI pieces

| Class | Responsibility |
|---|---|
| `EditorWidget` | `QTextEdit` subclass (ADR 0003) with rich text off; the `QTextDocument` inside is the only copy of the text. Updates the revealed range on caret/selection changes, paints padded code-block panels, decorations and the line of concealed thematic breaks (whole device pixels, S023), handles S010 clicks, centres the text column, and hands tables to `TableEmbedder` and front matter to `PropertiesEmbedder`. |
| `ViewAnchor` | S033: what the reader sees stays where it is. Keeps the text on screen still when blocks above the viewport change height, holds the reading position across resizes and theme changes until the reader scrolls (`readingAnchor`, `restore`, `settleLater`), and follows the caret while it is on screen, also while blocks are restyled in idle slices. Owns the flags for that and the connections to the vertical scroll bar; `EditorWidget` marks keys, input methods, pastes, undo and redo (`Input`) and its resizes and theme changes (`Reshape`). |
| `TableEmbedder` | S018 / ADR 0005: finds the tables on and near the screen (±200 lines), lets the header line reserve the height of a `TableWidget` in `BlockLayout` and hides the other lines; places the widgets on scroll and layout changes. Moves the caret into and out of tables, switches to the source view (Ctrl+Shift+M) and changes rows and columns in the Markdown lines, each change one undo step. A table that shows its source only because it holds the find bar's current match keeps its view hidden (`park`) and shows it again unchanged, without building it again (S028). When the find bar moves its hold in the middle of a change of the document (it moves its hold when lines move), the embedder finds the tables again after the change, queued (`EditorWidget::insideEdit`), and at once outside a change. Its own changes to a table's lines look the table up again by its header line afterwards and keep no reference into the list of tables. |
| `PropertiesEmbedder` | S024/S025: keeps valid closed front matter as a `PropertiesView` regardless of caret, selection, or search position; clicks in the source area outside the view are trapped, and Up from the first body line does not enter the YAML. Like a table, the first line reserves the block's height (with its margins) in `BlockLayout` and the other lines are hidden. Reads the YAML through the core (`parseProperties`) at once, not in slices (S024 code review O3), and builds the rows for at most 4 ms on a key, the rest in idle slices; the existing graphical block stays in place during updates. Keys, scalar values, and individual list/tag chips edit in place; checkboxes toggle directly. The compact, text-only Add property action starts an inline key editor, while hover-only remove controls delete rows and chips. No property editor opens a dialog or popup. Each committed edit writes only its property source range or inserts before the closing delimiter, using the core's YAML scalar/list writers (ADR 0007 amendment). A change or delete replaces the lines from the key to the end of the value (comment and blank lines after it stay), a rename only the key's source; the entries of a flow mapping are not edited. Each write is one edit block, and one Ctrl+Z in the block undoes one step; a line that would start a new block or holds half of a surrogate pair is not written. Invalid YAML stays text with a badge after the first line. |
| `PropertiesView` | The table-like properties block: a title and one row per property (type icon drawn with `QPainter`, key, value as text, checkbox, removable/addable chips or at most three lines of JSON, whose tooltip shows at most 20 lines of it pretty-printed), followed by a compact text-only Add property action. A cell gets a themed `QLineEdit` overlay when edited, without its default length limit; its text leaves as `insertableText` makes it. Return commits and ends in the field, and the block accepts every key it does not use, so that no key reaches the editor behind it. One widget paints the rows that intersect the area to paint; a key or text value that fits on one line is drawn as it is, a longer one is laid out once per width into a `QTextLayout` (a long one across slices) and only its lines in the area are drawn. Each row is an accessible element without a widget: a `QAccessibleInterface` of its own, registered on demand as a child of the block's interface, named `key: value` and described by its type, and deleted on `reset` and in the destructor (S024 code review O10). |
| `readPastedHtml` | S031, ADR 0011 (`table_paste.hpp`): reads the HTML of a paste, without what only formats (`core::htmlWithoutFormatting`), with Qt's HTML import into the core's model (text blocks outside tables, `QTextTable` cells with spans, a table in a cell marks the outer table); every resource is refused (`loadResource` returns nothing). `EditorWidget::insertFromMimeData` asks the core whether the paste converts (not in a code block, front matter, a table's lines, a block quote or a list item; not for a drop or Paste as Plain Text; not for HTML without a table, with nested tables or whose import would take longer than its cells allow, found by `core::scanHtml` on the HTML without what only formats before any import) and inserts the Markdown in one step under the wait cursor, which lasts until the pasted table is shown. Paste as Plain Text (`Ctrl+Shift+V`) acts where the focus is: the editor, a cell editor or a text field. What does not convert is inserted as the paste's or drop's plain text, in the editor and in a cell editor (`EditorWidget::pastedText`; the Markdown as it is written if the data holds no plain text): Qt is handed that text alone, because it reads data whose first format is `text/markdown` as Markdown and inserts what it shows of it. A copy made in the editor (`EditorWidget::createMimeDataFromSelection`) offers Qt's formats, each made only when asked for, and a marker (`EditorWidget::ownCopyFormat`); pasting it skips the HTML. |
| `TableModel` | `QAbstractTableModel` over a table's Markdown lines: reads cells through the core (`tableRowCells`, `tableCellToText`) and writes an edited cell back into its line (`replaceTableCell`), so the lines stay the only copy of the text (ADR 0002). Alignment from the delimiter row; theme colours and fonts per row. |
| `TableView`, `TableWidget`, `TableDelegate`, `CellEditor` | The table on screen: a `QTableView` sized to its content (columns shrink to the text column width, rows to wrapped content) with hover `+` / `−` buttons for rows and columns. The delegate draws a cell with `QTextDocument::setMarkdown` (Qt's GitHub dialect without front matter: a cell is never front matter, and Qt 6.8.0 to 6.8.3 crash on a cell they take for it; and without raw HTML, which shows as its characters as outside tables), a block marker at the cell's start escaped for it (`core::escapeTableCellBlockStart`: GFM reads a cell as inline text, S031 review (code) O3), an image's brackets escaped so that it shows as its characters and a space for each `<br>` that becomes U+2028 after the import (`core::tableCellImport`; should the import still hold an image, the cell shows its characters), or as plain text when GFM shows exactly its characters and line breaks (`core::tableCellPlainLayout`, review (code) O10: also cells with `<br>` and backslash escapes). The current cell always has its `CellEditor` (`QPlainTextEdit`, a persistent editor) open, and it writes every keystroke to the Markdown line at once (no editing mode); Shift+Enter is a line break (`<br>`). Keys at a cell's edges go to `TableView::cellKey`: Tab, Enter, arrows across cells and out of the table, Escape, row and column shortcuts, Ctrl+Z in the document (S018, kept from the first version). |
| `BlockLayout` | The document's layout (ADR 0003, `QAbstractTextDocumentLayout`): one `QTextLayout` per block, laid out when drawn or queried; a keystroke lays out its block at once, wider changes and restyling only drop layouts. Heights in pixels per block in a `HeightIndex` (core), estimated from the default font until a block is laid out. A block can be hidden or reserve a height for an embedded widget; embedded elements (tables, properties) register as `BlockLayout::Claimant` and ask the layout before they undo hidden or reserved state they find, so none undoes another's (S024 review O4). A block hidden, shown or given a reserved height in the middle of a change of the document, while the heights still have the old numbering (before `documentChanged`), gets its height when the change is applied (S028). Size changes during the styler's slices are reported once per slice (`Batch`). |
| `BlockStyler` | Applies `LineRender` to blocks as `QTextLayout` formats. A line containing `|` is styled with the next line in view, because a GFM table starts only when its delimiter row follows (S018). Edited lines and lines entering/leaving the revealed range are styled at once; the rest lazily: visible blocks first, then idle slices of at most 6 ms. Each block's user state records what it was styled for and caches the block state after the line, its paragraph included; only lines that leave a multi-line block open keep that state in a `BlockData`, so plain lines allocate nothing. Edits propagate restyling only while the state changes. For setext headings (S023, ADR 0006) the user state also caches the level of the line's paragraph and the line's link to the paragraph above; a level is found by walking down the paragraph (at most 2 ms on a key, the rest in idle slices), reused from the line below, and passed up the paragraph when an underline appears or goes. Front matter is confirmed at the document start the same way; the confirmed closing line is reported to `PropertiesEmbedder` (`frontMatterClose`, `frontMatterConfirmed`), whose idle work `isComplete` includes (`addPendingWork`). The revealed range also reveals the fences of the code block it is in (S021) and the underline of the setext heading it is on (S023). |
| `drawCodeBox`, `paintInlineCodeBoxes`, `paintThematicBreak`, `paintBullets` | `editor_painters.hpp`: what the editor paints beside the text Qt draws, each for one block or one panel from its `LineRender` and its resolved tokens: the panel and the boxes of code (S014, S015, S021), the line of a concealed thematic break (S023), the dots and rings of bullet items (S003). `EditorWidget` walks the blocks on screen (`paintCodeBlockPanels`, `paintDecorations`) and calls them. |
| `EditorFormats` | Theme tokens to `QTextCharFormat`s; `LineRender` to format ranges. |
| `SmoothScroller` | S027: animates a scroll bar towards a target with an easing curve (200 ms, `OutCubic`); a wheel step in the same direction adds to the target, one in the other direction turns the animation round. `EditorWidget` owns one for its vertical scroll bar, animates wheel steps with it while the smooth-scrolling setting is on (a touchpad's pixel steps and wheel steps with Ctrl or Shift go straight), stops it on a key, a click and a resize, and moves it with the content when blocks above the viewport change height (`shift`). |
| `FloatingToolbar` | S026: vertical floating toolbar centered on the right edge of `EditorWidget` with custom-drawn buttons for inserting a table, inserting an inline code span, and toggling the settings flyout, styled from `toolbarStyle(theme)`. |
| `SettingsMenu` | S039: non-modal floating settings flyout anchored beside `FloatingToolbar` in `EditorWidget`, toggled by the settings button or closed with `Escape`, displaying an **Appearance** section (`themeSelector` `QComboBox` listing built-in themes plus `.toml` themes discovered from the config folder, excluding `config.toml`/`settings.toml`) and a subtle version hint (`settingsVersionLabel`), styled from `toolbarStyle(theme)` and `[colors]` tokens. Selecting a theme persists it to `config.toml` in the platform config folder. |
| `insertableText`, `insertableKey`, `insertableInput` | S004 (`text_input.hpp`): text, a key's text and an input method's commit as they may enter a document, through `core::replaceLoneSurrogates`. Every way text from outside comes in passes here: `EditorWidget::insertFromMimeData` (every paste and drop) and the Markdown of a pasted table, `keyPressEvent` and `inputMethodEvent` of `EditorWidget` and `CellEditor`, `CellEditor::insertFromMimeData` and `TableModel::setData`. Qt's accessibility interface (`QAccessibleTextEdit`) writes into a document directly and passes nowhere hitokoto could see it; a save that meets a half names its line and column. |
| `DocumentSession` | Open/save. Reading, decoding, encoding and writing run on a worker thread (`QThreadPool`, one thread, so operations stay in order). Large files are inserted in slices of about 8 ms; the first 64 KB appear at once. |
| `application_icon` | Embeds the SVG for Qt windows and supplies derived native icons for Windows and macOS bundles. |
| `ThemeManager`, `theme_applier` | Discover `.toml` themes in the config folder (excluding `config.toml` and `settings.toml`), persist the selected theme and preserve `[editor]` options in `config.toml` inside the platform config folder, load the saved/configured theme file on startup (unless overridden by `--theme`), fall back to the built-in theme, and apply Fusion + palette + generated style sheet (including `QComboBox`). |
| `FindBar` | S028: the find bar below the editor (search field, previous, next, the toggles "Match case" and "Highlight all" with their platform shortcuts, the count, close), drawn from `[find]`. Not modal. |
| `FindController` | S028: one pass over the lines in slices (at most 6 ms in idle time, 8 ms on a key) builds the units with the core and scans them; per line it keeps the entering block state and the number of matches starting there, positions only for the lines on screen and the current match. The current match is the first at or after the caret (or the previous current match while typing); the caret moves only on Escape. An edit restarts the pass a little above the edited line and stops it where the state and the results meet the earlier ones (S028 review O4). Highlights are the editor's extra selections: a background over the match, the text colour only on visible characters (review O9). A table holding the current match shows its source (`EditorWidget::setFindHold`, review O8); the properties block stays graphical and marks matching keys, values or chips. With "Highlight all", cells on screen and the key, value or chip of a property with a match get a background (`TableModel::setFindCells`, `PropertiesView::setFindMarks`, code review O3, O8). Front matter is found and parsed once per change of it (code review O6, O12); steps before the count is complete work at most 8 ms and wait in a queue (O9). Announces the current match with its context for screen readers (`QAccessibleAnnouncementEvent`). Never changes the document. |
| `MainWindow` | Menus, title, unsaved-changes prompts, the find bar below the editor and Edit › Find, Find Next, Find Previous. Dialogs are replaceable for tests. |
| `MenuBar` | S038: subtle top menu bar (`File`, `Edit`, `View`) without a dividing line or persistent mnemonic underlines; rounded pill hover/active states styled from `menuBarStyle(theme)`. |
| `ListEditing` | S003 (`list_editing.hpp`): Enter, Tab and Shift+Tab in bullet items as edits through a cursor, each key one undo step: Enter continues the list with the item's prefix or ends it on an empty item, Tab and Shift+Tab indent or outdent the bullet lines of the selection as `core::indentBulletLine` and `core::outdentBulletLine` say; not in a fenced code block. `EditorWidget::handleBulletKey` decides which key it is and places the caret. |

## Concealing markers

Qt has no "invisible" character format. Collapsed markers (`#`) get a 1 px transparent font
(ADR 0001); kept-space markers (`-`) only a transparent colour, with the bullet dot painted on top,
so list text does not move when the caret enters the line. Link syntax uses a zero-width conceal
format so the hidden destination does not leave a gap before following text. Story S009 keeps
checking the concealment technique; since S019 it runs on `QTextEdit` with `BlockLayout` (ADR 0003).

## Adding a Markdown element

1. Implement `LineElement` for line-local block constructs, `MultiLineElement` for blocks that
   span lines (fenced code) or `InlineElement` for inline constructs in
   `src/core/src/elements/<name>.cpp`, with a factory function. Inline code runs take precedence
   over other inline styles. A multi-line element keeps a non-zero number of its own as state;
   the registry wraps it in `BlockState`, which the UI caches per block without knowing the
   element. While a block is open it owns each line; line and inline elements do not run there.
   A line element declares its paragraph rules (ADR 0006): `paragraphRole()` says whether a matched
   line ends paragraph text (the default, as a heading does), opens it at a content column (a
   bullet with text) or has text that continues lazily (an ordered item or block quote, once
   rendered); `startsAfterParagraph()` whether it may start directly after paragraph text (not an
   empty list item; otherwise the line continues the paragraph). Blocks hitokoto does not render
   yet follow the same rules in `block_rules.hpp`.
2. Register it in `ElementRegistry::withBuiltins()` (`add`, `addMultiLine`, `addInline`).
   Give it its own theme section with a token for every visible property and a fallback for
   each (ADR 0004, theming baseline); the UI reads resolved values (`codeBlockStyle()`, ...).
3. Add tests `S<nnn>_<Name>` in `tests/core`; if it needs a new role or decoration, extend
   `LineRender` and map it in `EditorFormats` / `EditorWidget::paintDecorations`.
4. An element shown as a widget over its lines (an embedded element: tables in S018, properties
   in S024) has an embedder class in `src/ui` that registers as `BlockLayout::Claimant`, reserves
   the height on its first line and hides the others. It is wired into the UI by hand:
   `EditorWidget` connects it in its constructor (caret and selection changes, `contentsChange`,
   the styler's signals, `documentSizeChanged`, `blocksGoneUnreported`), passes theme and width
   in `applyTheme` and `resizeEvent`, repositions it in `resizeEvent` and `scrollContentsBy`, hands
   it mouse and key events (`handleEditorClick`, a held button for properties, `handleEditorKey`
   in `keyPressEvent`) and offers accessors for tests (`tableViewAt`, `propertiesView`,
   `frontMatterBadge`). Properties also register their idle work with
   `BlockStyler::addPendingWork`, so that `isComplete` waits for it, and `DocumentSession` asks
   them where the caret goes after opening (`placeCaretAfterOpening`). The paint loops of
   `EditorWidget` skip hidden blocks for all of them. S024's code review (O8) recorded this
   instead of an interface: the third embedded element (an image, a math block) introduces an
   embedder interface that `EditorWidget` iterates, and moves tables and properties to it.

Order per line: an open multi-line block, else the first multi-line element that opens a block,
else a setext underline (paragraph text above), else a thematic break, else the first matching
line element followed by the inline elements; paragraph text that ends in an underline is a
heading line (`LineContext::setextLevel`, found below it). An inline element declares
its trigger characters (`triggerCharacters()`, e.g. `` ` `` for inline code); the registry scans
the line once and runs only the elements whose characters occur, so most lines run none. The
same scan counts the characters that can start inline syntax: a line with more of them than
`kInlineBounds` allows, or longer than it allows, runs no inline element and shows as text (S007:
what a line costs on every key grows with both). The
registry then removes styled runs under hidden ranges. Inline elements run in registration order
and see the runs of the earlier ones; which characters those made opaque (code spans, link
syntax) or hidden, the registry marks once per line in an `InlineScan` and hands to each element.
Emphasis skips the characters that inline code claimed, so inline code is registered before it
(`addInline` documents this). Besides the multi-line state, only
the paragraph context connects two elements (ADR 0006): which lines are paragraph text and at
which content column, carried in `BlockState::paragraph` from above, and the setext level from
below; elements contribute to it only through their declared paragraph role.
An inline element renders a line in linear time: it may look at each character a fixed number of
times, however the line is made up. It counts its steps (`src/core/src/inline_steps.hpp`: one for
each character it looks at and each entry of a list it walks), `core::inlineSteps()` reads the
sum, and the tests of its story compare the steps for a short and a long line of each pattern
(`tests/core/line_work.hpp`). The registry's merge of the elements' results follows the same rule.
`S014_FencedCodeBlocks.MultiLineElementsAreAddedByRegistrationOnly` adds a test-only element
this way, `S013_InlineEmphasis.InlineElementsAreAddedByRegistrationOnly` a test-only inline element.

## Platform shims

OS-specific code stands in three places:

- `src/core/src/platform.cpp`: flushing a file to disk before the atomic rename (`fsync` /
  `FlushFileBuffers`) and syncing the directory on POSIX. The C++ standard library cannot do this,
  and Qt may not be used in the core.
- `src/core/src/properties.cpp`: where the standard library has no floating-point
  `std::from_chars` (Apple's libc++), a number in front matter is read with `strtod_l` in the C
  locale (`<xlocale.h>` on Apple), so that the locale Qt sets does not change it.
- `src/ui/find_bar.cpp`: on macOS the find bar's toggles take Ctrl+Alt+letter instead of
  Alt+letter, because Option+letter types a character there (`Q_OS_MACOS`).

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
