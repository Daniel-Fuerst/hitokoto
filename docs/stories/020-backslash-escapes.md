# S020 — Backslash escapes for ASCII punctuation

- **Status:** In review · **Order:** 21 · **Depends on:** S013, S015

## Story

As a writer, I want backslash escapes (`\*`, `\[`, `\\`, etc.) to conceal the backslash and render the escaped ASCII punctuation character literally, with the backslash revealed on the active line, so that Markdown syntax characters can be written without triggering their formatting.

## Acceptance scenarios

**Scenario: backslash before ASCII punctuation conceals the backslash**
- Given a backslash followed by an ASCII punctuation character (`! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~`)
- When the caret is elsewhere
- Then the backslash is concealed
- And the following character is visible and rendered as regular text
- And the document still contains the original Markdown

**Scenario: active line reveals the backslash as a syntax marker**
- Given a line containing backslash-escaped characters
- When the caret or selection is on that line
- Then the backslash is visible and styled with the marker colour

**Scenario: backslash before non-punctuation remains literal**
- Given a backslash followed by an alphanumeric character, whitespace, or at the end of the line (e.g. `\a`, `\1`, `\ `, trailing `\`)
- When the line is rendered
- Then the backslash remains visible and is not concealed or styled as a marker

**Scenario: escaped backslash produces literal backslash**
- Given `\\` followed by an escaped character or formatting (e.g. `\\\\`, `\\*bold*`)
- When the line is rendered with the caret elsewhere
- Then the first backslash is concealed and the second backslash is visible as text
- And following formatting such as `*bold*` applies normally

**Scenario: backslash inside code blocks and inline code stays literal**
- Given backslashes inside a fenced code block or an inline code span (e.g. `` `\n` ``, `` `C:\` ``, `` `\*` ``)
- When the line is rendered
- Then the backslashes remain literal and are neither concealed nor styled as escape markers

**Scenario: backslash escapes prevent block and inline Markdown syntax**
- Given `\# not a heading`, `\* not a bullet`, `\- not a bullet`, `\+ not a bullet`, `\*not italic\*`, `\`not code``
- When the line is rendered
- Then no heading, bullet, emphasis or inline code formatting is produced

**Scenario: backslash escape is themeable (theming baseline, ADR 0004)**
- Given a theme that sets `escape.marker`
- When an escape is revealed on the active line
- Then the backslash uses the `escape.marker` colour
- And a theme that leaves `escape.marker` unset falls back to `colors.marker`

## Tests

- Core: `S020_BackslashEscapes` verifies concealment of backslashes before all 32 ASCII punctuation characters, literal rendering of non-punctuation escapes, consecutive backslashes (`\\`, `\\\`), preservation of literal backslashes inside inline code and fenced code blocks, suppression of heading/bullet/emphasis/inline code syntax, and theme token parsing.
- UI: `tst_s020_backslash_escapes` verifies that concealed backslashes have 1 px transparent format, revealed backslashes use the marker colour, and mouse clicks near escaped characters place the caret at the expected position.

## Implementation

Implemented with `EscapeElement` registered as an `InlineElement` in `ElementRegistry::withBuiltins()`.
Backslashes preceding ASCII punctuation characters are concealed when the line is not active and revealed
with `Role::EscapeMarker` on the active line. Backslashes inside inline code and fenced code blocks
remain literal. Theming baseline (ADR 0004) is satisfied by `escape.marker` falling back to `colors.marker`.
All core and UI test suites pass.
