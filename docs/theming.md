# Theming

hitokoto's look comes entirely from a theme: colours, fonts, heading sizes, radii and spacing.
A theme is a plain text file you can edit, share and keep in version control. No recompiling.

## Where the theme file lives

hitokoto reads `hitokoto/theme.toml` from your configuration folder:

| System | Path |
|---|---|
| Linux | `~/.config/hitokoto/theme.toml` |
| Windows | `%LOCALAPPDATA%\hitokoto\theme.toml` |
| macOS | `~/Library/Preferences/hitokoto/theme.toml` |

Start with `--theme path/to/file.toml` to use another file. **View › Reload Theme**
(Ctrl+Shift+R) applies changes without restarting. Without a theme file, hitokoto uses the
built-in light or dark theme, following the system colour scheme.

## Format

The format is a small subset of TOML:

- `key = value`, one per line; `# comment` anywhere outside a string.
- `[section]` starts a section; `name` and `base` come before the first section.
- Strings in double quotes (`\"` and `\\` escape a quote and a backslash).
- Numbers like `12`, `1.5`, `-3` (always with a dot, regardless of language settings).
- Colours as strings: `"#rgb"`, `"#rrggbb"` or `"#rrggbbaa"` (alpha last).

A theme only needs the values it changes. Everything else comes from the built-in theme named
by `base` (`"light"` or `"dark"`; default: the system colour scheme).

```toml
# ~/.config/hitokoto/theme.toml - warm dark variant
name = "Ember"
base = "dark"

[colors]
accent = "#ff8a3d"
primary = "#ff8a3d"
bullet = "#ff8a3d"
selection = "#5a3a22"

[fonts]
editor-family = "Iosevka"
editor-size = 13

[code-block]
font-family = "Iosevka Term"
radius = 8

[headings]
h1-scale = 2.2

[layout]
editor-max-width = 900
radius = 4
```

## Errors

hitokoto checks every value. If the file has any error, the whole file is ignored, the built-in
theme is used, and a message lists each problem with line and column, for example
`line 4, column 10: colors.accent: invalid colour "orange" (use #rgb, #rrggbb or #rrggbbaa)`.
A broken theme never prevents startup.

## Tokens

All tokens with their type and meaning. Sizes in `pt` follow the system's font scaling; `px`
values are device-independent pixels. Hairlines (1 px separators) are fixed.

| Token | Type | Meaning |
|---|---|---|
| `name` | string | Display name of the theme |
| `base` | "light" or "dark" | Built-in theme that supplies every token not set in this file; default: the system colour scheme |
| `colors.primary` | color | Main brand colour (focus rings, default buttons) |
| `colors.secondary` | color | Second brand colour for less prominent highlights |
| `colors.accent` | color | Accent colour (links, checked states, scroll handles on hover) |
| `colors.background` | color | Editor and window background |
| `colors.surface` | color | Menus, dialogs and other raised surfaces |
| `colors.border` | color | Separators and outlines |
| `colors.text` | color | Main text |
| `colors.text-muted` | color | Secondary text (disabled items, hints) |
| `colors.selection` | color | Selection background |
| `colors.selection-text` | color | Selected text |
| `colors.marker` | color | Markdown syntax markers when revealed (e.g. '#') |
| `colors.bullet` | color | Bullet dots of list items |
| `fonts.ui-family` | string | Font family for menus and dialogs; "" = system default |
| `fonts.ui-size` | number (pt) | Font size for menus and dialogs; 0 = system default, else 6 to 72 |
| `fonts.editor-family` | string | Font family of the editor text; "" = system default |
| `fonts.editor-size` | number (pt) | Font size of the editor text, 6 to 72 |
| `headings.h1-scale` | number | Size of level-1 headings relative to the editor font, 0.5 to 4 |
| `headings.h2-scale` | number | Same for level 2 |
| `headings.h3-scale` | number | Same for level 3 |
| `headings.h4-scale` | number | Same for level 4 |
| `headings.h5-scale` | number | Same for level 5 |
| `headings.h6-scale` | number | Same for level 6 |
| `headings.weight` | number | Font weight of headings, 100 to 900 |
| `layout.radius` | number (px) | Corner radius of buttons, menus, scroll handles, 0 to 32 |
| `layout.spacing` | number (px) | Base spacing; paddings in menus and buttons derive from it, 0 to 64 |
| `layout.editor-padding` | number (px) | Space around the editor text, 0 to 256 |
| `layout.editor-max-width` | number (px) | Maximum width of the text column (centred); 0 = full width, else 200 to 10000 |
| `buttons.style` | "flat" or "outlined" or "filled" | Look of push buttons in dialogs |

## Element tokens

Each Markdown element has its own section (ADR 0004). An element token that is not set takes
its fallback, which is resolved when the value is used: a theme that only changes
`colors.surface` also changes the code block background, unless it sets
`code-block.background`. The built-in themes leave these tokens unset, so everything below
follows the general tokens above.

| Token | Type | Meaning | Fallback |
|---|---|---|---|
| `code-block.background` | color | Background of the code block panel | `colors.surface` |
| `code-block.text` | color | Code text | `colors.text` |
| `code-block.border` | color | Border of the code block panel | `colors.border` |
| `code-block.border-width` | number (px) | Border width of the panel, 0 to 8 | 0 |
| `code-block.radius` | number (px) | Corner radius of the panel, 0 to 32 | `layout.radius` |
| `code-block.padding` | number (px) | Space between the panel edge and the code on the left and right, 0 to 64; above and below, the fence lines give the space | `layout.spacing` |
| `code-block.margin` | number (px) | Space above and below the panel, 0 to 64 | 3 |
| `code-block.font-family` | string | Font family of the code; "" = system fixed-width font | "" |
| `code-block.font-scale` | number | Font size relative to the editor font, 0.5 to 2 | 1 |
| `code-block.marker` | color | Fence lines (fence characters and info string) when revealed | `colors.marker` |
| `code-block.badge-text` | color | Language badge on the opening fence while it is concealed | `code-block.marker` |
| `code-block.badge-font-family` | string | Font family of the language badge; "" = editor font | "" |
| `code-block.badge-font-scale` | number | Size of the language badge relative to the editor font, 0.5 to 2 | 1 |
| `inline-code.background` | color | Background of inline code | `code-block.background` |
| `inline-code.text` | color | Inline code text | `code-block.text` |
| `inline-code.border` | color | Border of the inline code background | `code-block.border` |
| `inline-code.border-width` | number (px) | Border width, 0 to 8 | 0 |
| `inline-code.radius` | number (px) | Corner radius of the background, 0 to 32 | `code-block.radius` |
| `inline-code.padding` | number (px) | Horizontal space inside the background on each side, 0 to 16; moves the following text | 2 |
| `inline-code.font-family` | string | Font family; "" = system fixed-width font | `code-block.font-family` |
| `inline-code.font-scale` | number | Font size relative to the surrounding text, 0.5 to 2 | 1 |
| `inline-code.marker` | color | Backticks when revealed | `colors.marker` |
| `emphasis.bold-weight` | number | Font weight of bold text, 100 to 900 | 700 |
| `emphasis.bold-text` | color | Colour of bold text (takes precedence over italic and strikethrough) | text colour of the surrounding element |
| `emphasis.italic-text` | color | Colour of italic text (takes precedence over strikethrough) | text colour of the surrounding element |
| `emphasis.strikethrough-text` | color | Colour of struck-through text | text colour of the surrounding element |
| `emphasis.marker` | color | Delimiters (* _ ~~) when revealed | `colors.marker` |
| `table.background` | color | Background of table body rows | `colors.background` |
| `table.header-background` | color | Background of the table header row | `colors.surface` |
| `table.stripe-background` | color | Background of alternating (even-numbered) body rows | `table.background` |
| `table.text` | color | Text colour of table body cells | `colors.text` |
| `table.header-text` | color | Text colour of table header cells | `table.text` |
| `table.header-weight` | number | Font weight of table header cells, 100 to 900 | `headings.weight` |
| `table.border` | color | Outer border, cell grid lines and row/column controls | `colors.border` |
| `table.border-width` | number (px) | Width of table and cell borders, 0 to 8 | 1 |
| `table.radius` | number (px) | Corner radius of the outer table border, 0 to 32 | `layout.radius` |
| `table.padding` | number (px) | Horizontal and vertical space inside cells, 0 to 64 | `layout.spacing` |
| `table.font-family` | string | Font family of table cells; "" = editor font | "" |
| `table.font-scale` | number | Font size of table cells relative to the editor font, 0.5 to 2 | 1 |
| `escape.marker` | color | Backslash when revealed | `colors.marker` |
| `link.text` | color | Colour of link text | `colors.accent` |
| `link.marker` | color | Delimiters and destination when revealed | `colors.marker` |

Bold text is never lighter than the text around it: inside a heading the weight is the larger of
`headings.weight` and `emphasis.bold-weight`. The built-in themes set both to 700, so bold inside a
heading looks like the heading, as it does in Obsidian with its default settings; a theme that
wants it visible sets `emphasis.bold-weight` above `headings.weight` (or lowers the heading weight).

## The built-in light theme

The complete built-in light theme, a good starting point for your own:

```toml
# hitokoto built-in light theme
name = "hitokoto Light"
base = "light"

[colors]
primary        = "#6d5dfc"
secondary      = "#0f9d9a"
accent         = "#6d5dfc"
background     = "#ffffff"
surface        = "#f5f5f7"
border         = "#e2e2e8"
text           = "#1f2328"
text-muted     = "#6b6f76"
selection      = "#dcd8fe"
selection-text = "#1f2328"
marker         = "#9a9ca5"
bullet         = "#6d5dfc"

[fonts]
ui-family     = ""
ui-size       = 0
editor-family = ""
editor-size   = 12

[headings]
h1-scale = 1.9
h2-scale = 1.55
h3-scale = 1.3
h4-scale = 1.15
h5-scale = 1.05
h6-scale = 1.0
weight   = 700

[layout]
radius           = 6
spacing          = 8
editor-padding   = 32
editor-max-width = 760

[buttons]
style = "filled"
```

## How it works

The theme file is the single styling source. hitokoto maps the tokens onto Qt's own styling:
the Fusion style, the application palette, a style sheet generated from the tokens, and the
editor's text formats. Users never write Qt style sheets, so the native style engine and the
theme never pull in different directions (ADR 0001). UI code must not contain styling values;
a test (`S005_NoStylingLiteralsInUi`) enforces that.

Code blocks are drawn as one panel behind their lines, inline code as a box behind the span;
both are painted by the editor, because character formats can neither round corners nor pad.
The panel spans the fence lines too; while the caret is outside the block they are concealed but
keep their height, and the opening fence shows the language, the first word of its info string,
at the right (S021). The margin above and below the panel belongs to the first and last line, so
the text below moves by it, as margins do in CSS.
Inline code padding is reserved after the concealed backticks, so the following text moves by
it; on a revealed line the box covers the backticks instead.
