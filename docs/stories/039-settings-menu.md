# S039 — Settings menu and persistent configuration

- **Status:** In review · **Order:** 40 · **Size:** M · **Depends on:** S005, S024, S026, S027, S030
- **Basis:** User requests on `feature/settings` (2026-11-15): an "Open settings" button (`openSettingsButton`) with a gear icon on the floating toolbar (S026) that toggles a non-modal floating `SettingsMenu` flyout (`settingsMenu`) to the left of the toolbar; an **Appearance** section with a theme selector (`themeSelector` `QComboBox`) listing built-in themes (`hitokoto Light`, `hitokoto Dark`) plus `.toml` themes from the platform configuration directory and its `themes/` subfolder; a subtle version hint (`settingsVersionLabel`, `v<version>`) at the bottom right instead of a debug info table; and persistence of user settings (`[appearance]` `theme` and `[editor]` options `smooth-scrolling`, `front-matter-as-properties`, `report-blank-lines-added`) in `config.toml` inside the OS configuration directory (`~/.config/hitokoto` on Linux, `%LOCALAPPDATA%\hitokoto` on Windows, `~/Library/Preferences/hitokoto` on macOS via `QStandardPaths::GenericConfigLocation`).

## Story

As a writer, I want a non-modal settings flyout reachable from the floating toolbar where I can switch themes immediately and check the app version without interrupting my writing, and have my chosen settings saved to `config.toml` in the OS configuration directory so they are restored on restart.

## Decisions

1. **Toolbar button and non-modal flyout.** `FloatingToolbar` (S026) adds a checkable `openSettingsButton` with a gear icon below the table and inline-code buttons. Clicking it toggles the non-modal `SettingsMenu` flyout anchored immediately to the left of `FloatingToolbar` inside `EditorWidget`. All controls in `SettingsMenu` and `FloatingToolbar` use `Qt::NoFocus` so editor focus and typing continue uninterrupted; pressing `Escape` in the editor or clicking `openSettingsButton` again closes the flyout and unchecks the button.
2. **Theme discovery and live switching.** The **Appearance** section exposes a `themeSelector` `QComboBox` listing built-in `hitokoto Light` and `hitokoto Dark` followed by `.toml` theme files discovered from the configuration folder and its `themes/` subfolder, explicitly excluding `config.toml` and `settings.toml` (case-insensitive). Picking an entry applies the theme immediately across the application.
3. **Subtle version hint.** Instead of a debug information table, `SettingsMenu` renders a single subtle right-aligned `settingsVersionLabel` at the bottom (`v<version>` from `QCoreApplication::applicationVersion()` falling back to `HITOKOTO_UI_VERSION`) in `colors.text-muted` at 85% of the UI font point size.
4. **Configuration file (`config.toml`) in the OS config directory.** `hitokoto::core` (`settings.hpp` / `settings.cpp`) parses and serializes a TOML subset (`Settings` with `theme`, `smoothScrolling`, `frontMatterAsProperties`, `reportBlankLinesAdded`), reports `SettingsDiagnostic` warnings on invalid/unknown keys or sections while preserving all valid entries, and writes `config.toml` atomically via `writeFileAtomically`, creating missing parent directories. Selecting a valid theme saves `[appearance] theme` while preserving existing `[editor]` options; broken theme files fall back without overwriting the previously saved selection.
5. **Startup precedence.** On startup, CLI `--theme <path>` takes precedence for the current session without overwriting `config.toml`; otherwise the saved theme in `config.toml` is loaded, falling back to legacy `theme.toml` and then the built-in theme matching the system colour scheme.
6. **Theming baseline.** `SettingsMenu` draws its panel background, border, border width, rounded corners, padding, and spacing from `toolbarStyle(theme)` (`[toolbar]` tokens falling back to general tokens), uses `ui.font-family` / `ui.font-size`, renders text/muted labels with `colors.text` / `colors.text-muted`, and relies on `theme_applier.cpp` for themed `QComboBox` and popup list styling.

## Acceptance scenarios

**Scenario: opening and closing the settings flyout from the toolbar**
- Given an open editor window with the focus in the editor
- When I click "Open settings" (`openSettingsButton`) in the floating toolbar
- Then the non-modal `SettingsMenu` flyout (`settingsMenu`) opens immediately to the left of the floating toolbar inside the editor bounds, showing a **Settings** heading (`settingsTitleLabel`), an **Appearance** section with `themeSelector` (`QComboBox`), and a subtle bottom version hint (`settingsVersionLabel` starting with `v` and smaller than `settingsTitleLabel`), with no debug info table labels (`debugAppValue`, `debugQtValue`, `debugPlatformValue`, `debugThemeValue`, `debugDocumentValue`)
- And the "Open settings" button is checked while the editor retains keyboard focus so typing continues uninterrupted
- When I press `Escape` in the editor or click "Open settings" again
- Then `SettingsMenu` closes and `openSettingsButton` is unchecked

**Scenario: discovering and switching themes from SettingsMenu**
- Given `.toml` theme files in the configuration folder and its `themes/` subfolder alongside `config.toml` and `settings.toml`
- When I open `SettingsMenu`
- Then `themeSelector` lists built-in `hitokoto Light` and `hitokoto Dark` plus the custom themes (and excludes `config.toml` / `settings.toml`)
- When I select a custom light/dark `.toml` theme or built-in `hitokoto Light` / `hitokoto Dark`
- Then the application palette, style sheet, and editor theme update immediately to match that theme

**Scenario: persisting the selected theme across restart**
- Given a user configuration directory with a custom `.toml` theme
- When I select that theme (or built-in `hitokoto Light` / `hitokoto Dark`) in `themeSelector`
- Then `config.toml` is saved atomically in the configuration directory and excluded from `themeSelector`
- When a new `MainWindow` / `ThemeManager` instance starts pointing at the same configuration directory
- Then the saved theme is loaded automatically on startup and preselected in `themeSelector`

**Scenario: loading and preserving `[editor]` options in config.toml**
- Given a `config.toml` setting `[appearance] theme` and `[editor]` flags (`smooth-scrolling`, `front-matter-as-properties`, `report-blank-lines-added`)
- When the editor initializes its configuration directory
- Then those `[editor]` flags and theme are applied to `EditorWidget` and `MainWindow`
- When I subsequently pick another theme in `SettingsMenu`
- Then the new theme is saved to `config.toml` while the existing `[editor]` options remain unchanged

**Scenario: picking a theme changes only the theme setting in config.toml**
- Given a `config.toml` edited by hand, with comments, unknown keys, or a `theme` key under another section such as `[editor]`
- When I pick a theme in `themeSelector`
- Then only the value of `theme` in `[appearance]` changes (or of a top-level `theme` before the first section, which is read as the theme); a `theme` key under any other section stays as it is, and so does every other byte of the file, including its line endings (LF or CRLF) and a byte order mark
- And a value written as any kind of TOML string (basic, literal, multi-line) or as another single value is replaced as a whole
- And if `[appearance]` has no `theme`, the key is added below its header; without `[appearance]`, the section is added at the end
- When the file defines the theme in a form hitokoto does not read (a dotted or quoted key, a table header with spaces or quotes, an array), or the changed text would not give the picked theme with all other settings unchanged
- Then `config.toml` is left untouched and saving the theme fails with a reason

**Scenario: CLI `--theme` override precedence**
- Given a `config.toml` with a saved theme and `--theme <path>` passed at startup
- When `ThemeManager` initializes with CLI override active
- Then the CLI theme is applied for the session and `config.toml` is not overwritten until the user interactively selects another theme

**Scenario: malformed entries in config.toml preserve valid settings**
- Given a `config.toml` containing valid keys alongside unknown sections, unknown keys, wrong value types, or malformed lines
- When `parseSettings` / `loadSettingsFile` reads the file
- Then `SettingsDiagnostic` entries describe each problem with line numbers while all valid keys keep their parsed values

**Scenario: theming baseline**
- Given built-in or custom themes with `[toolbar]`, `[colors]`, and `[ui]` tokens
- When a theme is applied
- Then `SettingsMenu` updates its painted surface, border, fonts, label colours, spacing, and `themeSelector` `QComboBox` styling from those tokens

## Tests

- **Core (`hitokoto_core_tests`, `tests/core/test_s039_settings_menu.cpp`):** `S039_SettingsMenu.SettingsParseAndSerializeRoundTrip`, `S039_SettingsMenu.SettingsParseDiagnosticsPreserveValidEntries`, `S039_SettingsMenu.SettingsSaveCreatesDirectoriesAndLoadsAtomically`, `S039_SettingsMenu.BuiltinSchemeFromSettingMatchesLightAndDarkCaseInsensitively`; picking a theme: `S039_SettingsMenu.UpdateThemeChangesOnlyTheThemeKeyOfAppearance`, `S039_SettingsMenu.UpdateThemeChangesATopLevelKeyBeforeAnySection`, `S039_SettingsMenu.UpdateThemeReplacesTheWholeValue`, `S039_SettingsMenu.UpdateThemeKeepsLineEndingsAndAByteOrderMark`, `S039_SettingsMenu.UpdateThemeLeavesTheFileWhenOnlyTheThemeCannotBeChanged`.
- **UI (`tests/ui/tst_s039_settings_menu.cpp`):** `settingsButtonOpensAndClosesSettingsMenuWithDebugInfo`, `settingsMenuListsAndSelectsThemesFromConfigFolder`, `selectedThemePersistsInConfigFileAcrossRestart`, `editorOptionsInConfigFileLoadAndSurviveThemeSelection`.
- **Theme manager persistence (`tests/ui/tst_s005_theme.cpp`):** `themeSelectionPersistsAcrossThemeManagerRestart`.

## Implementation

On `feature/settings` (2026-11-15):

- **Core:** `src/core/include/hitokoto/core/settings.hpp`, `src/core/src/settings.cpp` (`kSettingsFileName`, `SettingsDiagnostic`, `Settings`, `SettingsLoadResult`, `parseSettings`, `serializeSettings`, `loadSettingsFile`, `saveSettingsFile`, `builtinSchemeFromSetting`).
- **UI:** `src/ui/settings_menu.hpp` / `.cpp` (`SettingsMenu`), `src/ui/floating_toolbar.hpp` / `.cpp` (`openSettingsButton`), `src/ui/theme_manager.hpp` / `.cpp` (exclusion of `config.toml`/`settings.toml`, `defaultConfigFilePath`, `configFilePath`, `saveSelectedTheme`, `loadSavedThemeEntry`, `setCliThemeOverride`, `activeThemePath`, `reload()` precedence), `src/ui/theme_applier.cpp` (`QComboBox` styling), `src/ui/editor_widget.hpp` / `.cpp`, `src/ui/main_window.hpp` / `.cpp`, and `src/app/main.cpp`.
