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
| `ElementRegistry` | `markdown.hpp` | Holds the elements; `render(line, {revealed})` returns a `LineRender` |
| `LineRender` | `markdown.hpp` | How one line is shown: heading level, style runs, hidden ranges, decorations |
| `revealedLines`, `revealChanges` | `reveal.hpp` | Which lines show raw syntax; which lines must re-render when that changes |
| `decodeUtf8`, `encodeUtf8` | `text_codec.hpp` | Bytes to text and back, preserving BOM and line endings (ADR 0002) |
| `readFile`, `writeFileAtomically` | `file_io.hpp` | Reading and atomic saving |
| `Theme`, `parseTheme`, `loadTheme` | `theme.hpp` | Theme tokens, theme file format, built-in themes |

`LineRender` is the contract between core and UI. It speaks in roles (`Marker`, `HeadingText`),
hide modes (`Collapse`, `KeepSpace`) and decorations (`Bullet`), never in colours or fonts. The UI
maps roles to theme formats.

## UI pieces

| Class | Responsibility |
|---|---|
| `EditorWidget` | `QPlainTextEdit` subclass; the `QTextDocument` inside is the only copy of the text. Updates the revealed range on caret/selection changes, draws decorations, handles S010 clicks, centres the text column. |
| `BlockStyler` | Applies `LineRender` to blocks as `QTextLayout` formats. Edited lines and lines entering/leaving the revealed range are styled at once; the rest lazily: visible blocks first, then idle slices of at most 6 ms. Each block's user state records what it was styled for, so no whole-document pass is ever needed. |
| `EditorFormats` | Theme tokens to `QTextCharFormat`s; `LineRender` to format ranges. |
| `DocumentSession` | Open/save. Reading, decoding, encoding and writing run on a worker thread (`QThreadPool`, one thread, so operations stay in order). Large files are inserted in slices of about 8 ms; the first 64 KB appear at once. |
| `ThemeManager`, `theme_applier` | Load the theme file, fall back to the built-in theme, apply Fusion + palette + generated style sheet. |
| `MainWindow` | Menus, title, unsaved-changes prompts. Dialogs are replaceable for tests. |

## Concealing markers

Qt has no "invisible" character format. Collapsed markers (`#`) get a 1 px transparent font
(ADR 0001); kept-space markers (`-`) only a transparent colour, with the bullet dot painted on top,
so list text does not move when the caret enters the line. Story S009 decides whether the 1 px
technique stays or a custom text surface replaces `QPlainTextEdit`; only `src/ui` would change.

## Adding a Markdown element

1. Implement `LineElement` in `src/core/src/elements/<name>.cpp` with a factory function.
2. Register it in `ElementRegistry::withBuiltins()` (one line).
3. Add tests `S<nnn>_<Name>` in `tests/core`; if it needs a new role or decoration, extend
   `LineRender` and map it in `EditorFormats` / `EditorWidget::paintDecorations`.

Nothing else changes. Inline elements (bold, links) will need a second, inline pass over the
content range of a line; the plan is an `InlineElement` interface next to `LineElement`, applied by
the registry after the line element, so existing elements stay untouched.

## Platform shims

The only OS-specific code is `src/core/src/platform.cpp`: flushing a file to disk before the
atomic rename (`fsync` / `FlushFileBuffers`) and syncing the directory on POSIX. The C++ standard
library cannot do this, and Qt may not be used in the core.

## Dependencies

| Dependency | Use | Where |
|---|---|---|
| Qt 6.8+ (Core, Gui, Widgets) | UI | runtime |
| Qt Test | UI tests, end-to-end benchmark | development |
| GoogleTest 1.18 | core tests | development (installed package or fetched by tag) |
| Google Benchmark 1.10 | core benchmarks | development |
| libFuzzer (LLVM) | core fuzzing | development, Clang only |

## Threads

The UI thread owns the document. Worker work never touches Qt widgets or the document: it receives
copies (file bytes, a snapshot of the text) and posts results back with queued invocations.
`DocumentSession` waits for its worker in the destructor and before closing the window.
