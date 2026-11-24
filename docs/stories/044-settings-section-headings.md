# S044 — Section headings in the settings flyout stand out

- **Status:** In progress · **Order:** 49 · **Size:** S · **Depends on:** S039, S043
- **Basis:** User request on 2026-11-24: in the settings flyout the section headings (**Appearance**,
  **Feel**) have the same weight and colour as the row labels beside the controls ("Theme",
  "Smooth scrolling"), so the sections are hard to tell apart. Wanted: the heading in the accent
  colour, followed by a line to the right edge, like `Appearance ─────────`. Branch
  `feature/settings-section-headings`.

## Story

As a writer, I want the sections of the settings flyout to be clearly set apart, so that I can see
at a glance where one group of settings ends and the next begins.

## Decisions

1. **Heading and rule in one row.** Each section heading of `SettingsMenu` is a row: the heading
   label (`appearanceLabel`, `feelLabel`, unchanged names) on the left, then a horizontal line
   (`appearanceRule`, `feelRule`) that takes the rest of the width, centred vertically on the
   heading text. The row labels ("Theme", "Smooth scrolling") keep their look.
2. **Accent colour and a heavier weight.** The heading is drawn in the accent colour with a semi-bold
   weight (600), so it differs from the row labels in both colour and weight; the line is in the
   border colour so it separates without competing with the heading.
3. **Theming baseline.** The heading row has its own tokens in a new section `[settings-section]`:
   `text` (`colors.accent`), `weight` (600), `rule` (`colors.border`), `rule-width` (1) and
   `rule-gap` (`layout.spacing`, the space between the heading and the line), resolved by
   `settingsSectionStyle()`. The font family and size stay those of the flyout (UI font).

## Acceptance scenarios

**Scenario: section headings stand apart from the row labels**
- Given the settings flyout is open with a built-in theme
- Then **Appearance** and **Feel** are drawn in `colors.accent` with weight 600, while "Theme" and
  "Smooth scrolling" keep `colors.text-muted` with the normal weight
- And to the right of each heading a line in `colors.border` runs to the right edge of the content,
  centred vertically on the heading

**Scenario: theming baseline**
- Given a theme that sets the tokens of `[settings-section]`
- When it is applied
- Then the headings take their colour and weight, and the lines their colour, width and distance
  from the heading, from those tokens; unset tokens fall back as documented in `docs/theming.md`

## Tests

- **Core (`tests/core/test_s044_settings_section_headings.cpp`):**
  `S044_SettingsSectionHeadings.TokensFallbackAndResolve`; the tokens are also in `resolvedTokens()`
  of `tests/core/test_s005_theme.cpp`.
- **UI (`tests/ui/tst_s044_settings_section_headings.cpp`):** `headingsStandApartFromRowLabels`,
  `headingsFollowTheirThemeTokens`.

## Implementation

On `feature/settings-section-headings` (2026-11-24):

- **Core:** `theme.hpp` / `theme.cpp` (`SettingsSectionTokens`, `Theme::settingsSection`,
  `SettingsSectionStyle`, `settingsSectionStyle`, the `[settings-section]` tokens).
- **UI:** `settings_menu.*` (heading rows with `SectionRule`, styled from `settingsSectionStyle`).
- **Docs:** `docs/theming.md`, `docs/architecture.md`.
