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

Fenced code blocks use the theme's `colors.surface` background and `colors.text` foreground,
with a rounded panel padded by `layout.spacing` and rounded by `layout.radius`. Single-backtick
inline code uses the same surface and text colors with a fixed-width style hint. No additional
theme tokens are needed.
