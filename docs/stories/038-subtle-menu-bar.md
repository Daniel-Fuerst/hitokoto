# S038 — Subtle top menu bar without a dividing line

- **Status:** In review · **Order:** 39 · **Size:** S · **Depends on:** S005
- **Basis:** User request on 2026-11-15 to modernize the top menu bar (`File`, `Edit`, `View`) into a subtle, low-presence menu without the full-width bottom dividing line, keeping its top-left position and core functionality.

## Story

As a writer, I want the top menu bar to blend subtly into the editor without a full-width dividing line or persistent mnemonic underlines, so that the window stays clean and distraction-free while `File`, `Edit` and `View` remain easy to reach in the top-left corner.

## Acceptance scenarios

**Scenario: subtle top-left menu bar without a dividing line**
- Given an open editor window
- Then the menu bar sits at the top of the window with `File`, `Edit` and `View` arranged from the top-left, inset by `menu-bar.padding` and separated by `menu-bar.spacing`
- And by default there is no horizontal dividing line below the menu bar (`menu-bar.border-width` defaults to `0`)
- And the menu bar background uses `menu-bar.background` (falling back to `colors.background`) so it merges seamlessly with the editor
- And idle menu items use muted text (`menu-bar.text`, falling back to `colors.text-muted`) with no pill background and no persistent mnemonic underlines

**Scenario: hover and open menu pills preserve core functionality**
- Given the subtle top menu bar
- When the pointer hovers over a menu item (`File`, `Edit`, or `View`)
- Then a rounded pill (`menu-bar.item-radius`) is drawn behind that item in `menu-bar.hover-background` (`colors.surface`) and its label uses `menu-bar.hover-text` (`colors.text`)
- When a menu is opened
- Then its pill background uses `menu-bar.active-background` (`colors.border`) and its label uses `menu-bar.active-text` (`colors.text`)
- And all existing actions, shortcuts, mnemonics and `window->menuBar()->actions()` remain intact

**Scenario: theming baseline (ADR 0004)**
- Given a theme with `[menu-bar]` tokens (`background`, `border`, `border-width`, `padding`, `spacing`, `text`, `hover-text`, `active-text`, `hover-background`, `active-background`, `item-radius`, `item-padding-horizontal`, `item-padding-vertical`, `font-family`, `font-scale`)
- When the theme is applied
- Then the menu bar uses those tokens, falling back to the documented values when unset
