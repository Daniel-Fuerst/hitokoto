# S005 — A theme file changes the look; light and dark built in

- **Status:** Done in v0.1 · **Order:** 10 · **Depends on:** S003

**Implementation note:** Done (v0.1, 2026-10-24). `docs/theming.md` written; a test checks that it documents every token and that its examples parse. Reload via View > Reload Theme; the theme also follows system light/dark changes. Tests: core `S005_Theme`, UI `tst_s005_theme`, `S005_NoStylingLiteralsInUi`; fuzzer `fuzz_theme`.

## Story

As a user, I want to change hitokoto's look with a plain-text theme file, so that I can make it
mine and share my theme without recompiling.

## Acceptance scenarios

**Scenario: built-in themes**
- Given no user theme
- Then hitokoto uses the built-in light or dark theme following the system colour scheme, with a
  tasteful accent colour (brief, section 8)

**Scenario: custom theme**
- Given a theme file in the user's config directory that sets accent, background, editor font
  and heading scale
- When I start hitokoto (or reload the theme)
- Then window, editor text, headings, markers and bullet dots use those values

**Scenario: partial theme inherits**
- Given a theme that sets only `accent`
- Then every other token comes from the built-in theme it names as `base` (light or dark)

**Scenario: broken theme**
- Given a theme with a syntax error, an unknown token or an invalid colour
- Then hitokoto starts with the built-in theme and shows the file, line, column and reason

**Scenario: no hard-coded styling**
- Given the UI source
- Then no colour, radius, font or spacing literal appears outside the theme mapping
  (checked by a test that scans `src/ui`)

## Tests

- Core: `S005_Theme` – parser, validation messages with line/column, inheritance, defaults;
  fuzz test: arbitrary input never crashes the parser.
- UI: `tst_s005_theme` – apply a fixture theme, assert palette and editor formats, snapshot.
- Bench: theme parsing adds < 5 ms to startup.

## Notes

- Format: TOML-like subset parsed by our own small parser in core (ADR 0001); the UI maps tokens
  to Fusion + `QPalette` + generated QSS + editor formats. Users never write QSS.
- Token list (minimum from the brief): primary, secondary, accent, background, surface, text
  colours, editor font family and sizes, heading scale, border radius, button styles,
  spacing/density, marker style.
- Writes `docs/theming.md` with the token reference and an example theme.
