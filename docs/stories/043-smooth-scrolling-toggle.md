# S043 — Toggle switches and a "Feel" section for smooth scrolling

- **Status:** In progress · **Order:** 48 · **Size:** M · **Depends on:** S027, S039
- **Basis:** User request on 2026-11-23: smooth scrolling (S027) can only be turned off by editing
  `config.toml` by hand (`[editor] smooth-scrolling`, S039). The settings flyout gets toggle
  switches, which it did not have, with an ease-in-out animation of the knob, and a new section
  **Feel** in which a switch "Smooth scrolling" sits. Branch `feature/smooth-scrolling-toggle`.

## Story

As a writer, I want to turn smooth scrolling on or off with a switch in the settings flyout, so
that I can choose how scrolling feels without editing a configuration file, and the choice is kept
across restarts.

## Decisions

1. **A toggle switch of our own (`ToggleSwitch`).** Qt Widgets has no switch control; a
   `QCheckBox` styled through a style sheet cannot animate its indicator. `ToggleSwitch` is a small
   checkable `QAbstractButton` that paints a rounded track and a round knob. It keeps
   `Qt::NoFocus` like every control of the flyout (S039, decision 1), so the editor keeps the
   keyboard focus; it is reachable for assistive technology through its accessible name and the
   checkable button role.
2. **Ease-in-out.** When the state changes by a click, the knob travels from one end of the track to
   the other and the track colour blends between its two colours, both driven by one
   `QVariantAnimation` from 0 to 1 with `QEasingCurve::InOutCubic` over 160 ms. A change of state
   while the animation runs reverses it from where the knob is. A change by the program
   (`setChecked` while the flyout is closed, or a theme change) shows the end state at once. The
   duration is behaviour, not look, like the 200 ms of S027, so it is no theme token.
3. **The section "Feel".** `SettingsMenu` gets a section **Feel** (`feelLabel`) below
   **Appearance**, with the row "Smooth scrolling" (`smoothScrollingSwitch`). The section label and
   the row label use the same look as **Appearance** and "Theme".
4. **Saving the choice changes only that value.** Flipping the switch turns smooth scrolling on or
   off at once (`EditorWidget::setSmoothScrolling`, `Options::smoothScrolling`) and writes
   `smooth-scrolling` in `[editor]` of `config.toml`, changing nothing else in the file, with the
   same rules as picking a theme (S039, "picking a theme changes only the theme setting"): the
   change of the theme is generalised in the core to one key of a section
   (`updateSettingInSettingsSource`), and `updateThemeInSettingsSource` uses it. Without such a key
   it is added below the first `[editor]` header, or a new `[editor]` section is added at the end.
   A change that cannot be saved keeps the choice for the session and shows one message "Setting
   not saved" with the file, the reason and the problems of the file, like "Theme not saved".
5. **Theming baseline.** The switch has its own tokens in a new section `[switch]`: `track`
   (`colors.border`), `checked-track` (`colors.accent`), `knob` (`colors.background`),
   `checked-knob` (`colors.background`), `border` (`colors.border`), `border-width` (0), `width`
   (36), `height` (20), `radius` (10) and `knob-margin` (2), resolved by `switchStyle()`.

## Acceptance scenarios

**Scenario: the Feel section shows the smooth scrolling switch**
- Given an editor window whose `config.toml` sets `smooth-scrolling = false` (or has none)
- When I open the settings flyout
- Then below **Appearance** there is a section **Feel** with a switch "Smooth scrolling"
  (`smoothScrollingSwitch`) that is off (on without the key, the default of S027)
- And the switch takes no keyboard focus; the editor keeps it

**Scenario: flipping the switch turns smooth scrolling on and off**
- Given the settings flyout is open and smooth scrolling is on
- When I click the switch
- Then it is off, the editor scrolls without animation, and `MainWindow::options().smoothScrolling`
  is false
- When I click it again
- Then smooth scrolling is on again

**Scenario: the knob moves with an ease-in-out animation**
- Given the switch is off and its knob rests at the left end of the track
- When I click it
- Then the knob does not jump: shortly after the click it stands between both ends, nearer to its
  start than a linear movement would put it at the start of the movement, and nearer to the end
  than a linear movement would put it near the end of it (ease in, ease out)
- And after the animation (160 ms) it rests at the right end and the track has its checked colour
- When I click again while the knob is moving
- Then it turns back from where it is, without jumping to an end first

**Scenario: the choice is saved and restored**
- Given a `config.toml` written by hand, with comments, a theme and other `[editor]` keys
- When I flip the switch
- Then only the value of `smooth-scrolling` in `[editor]` changes; every other byte stays, including
  the line endings and a byte order mark
- And without the key it is added below `[editor]`, without `[editor]` the section is added at the
  end, and without a file one is written
- When a new window starts with the same configuration folder
- Then smooth scrolling and the switch have the saved state

**Scenario: a choice that cannot be saved is reported**
- Given a `config.toml` that sets `smooth-scrolling` in a form hitokoto does not change (for example
  a dotted key `editor.smooth-scrolling`), or a folder of that name
- When I flip the switch
- Then the choice is used for the session, `config.toml` stays untouched, and one message
  "Setting not saved" names the file and gives the reason

**Scenario: theming baseline**
- Given a theme that sets the tokens of `[switch]`
- When it is applied
- Then the switch draws its track, knob, border, size, corner radius and knob margin from those
  tokens; unset tokens fall back as documented in `docs/theming.md`

## Tests

- **Core (`tests/core/test_s043_smooth_scrolling_toggle.cpp`):**
  `S043_SmoothScrollingToggle.SwitchTokensFallbackAndResolve`,
  `S043_SmoothScrollingToggle.UpdateSettingChangesOnlyThatKeyOfItsSection`,
  `S043_SmoothScrollingToggle.UpdateSettingAddsTheKeyBelowTheSectionOrTheSectionAtTheEnd`,
  `S043_SmoothScrollingToggle.UpdateSettingKeepsLineEndingsAndAByteOrderMark`,
  `S043_SmoothScrollingToggle.UpdateSettingRefusesAFormItCannotChange`,
  `S043_SmoothScrollingToggle.UpdateSmoothScrollingInFileWritesANewFile`,
  `S043_SmoothScrollingToggle.EaseInOutProgressIsSlowAtBothEnds`,
  `S043_SmoothScrollingToggle.KnobTravelsInsideTheTrackAndColoursBlend`; the switch tokens are also in
  `resolvedTokens()` of `tests/core/test_s005_theme.cpp`; the S039 tests of the theme pick
  run unchanged against the generalised change.
- **UI (`tests/ui/tst_s043_smooth_scrolling_toggle.cpp`):** `feelSectionShowsTheSwitchInItsSavedState`,
  `flippingTheSwitchTurnsSmoothScrollingOnAndOff`, `theKnobMovesWithAnEaseInOutAnimation`,
  `theChoiceIsSavedAndRestored`, `aChoiceThatCannotBeSavedIsReported`, `switchFollowsItsThemeTokens`.

## Implementation

On `feature/smooth-scrolling-toggle` (2026-11-23):

- **Core:** `toggle_switch.hpp` / `.cpp` (`easeInOut`, `toggleGeometry`, `blendColors`); `theme.hpp` /
  `theme.cpp` (`SwitchTokens`, `Theme::toggleSwitch`, `SwitchStyle`, `switchStyle`, the `[switch]`
  tokens); `settings.hpp` / `settings.cpp` (the change of one key generalised from the theme;
  `updateSmoothScrollingInSettingsSource`, `updateSmoothScrollingSettingInFile`).
- **UI:** `toggle_switch.hpp` / `.cpp` (`ToggleSwitch`); `settings_menu.*` (section Feel,
  `smoothScrollingSwitch`, `smoothScrollingToggled`); `editor_widget.*` (`smoothScrollingChosen`, the
  switch follows `setSmoothScrolling`); `main_window.*` (`applySmoothScrollingChoice`, message
  "Setting not saved", shared with "Theme not saved").
- **Docs:** `docs/theming.md` (the `[switch]` tokens and how the switch is drawn),
  `docs/architecture.md`.
