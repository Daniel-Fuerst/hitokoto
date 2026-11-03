# ADR 0007: Reading YAML front matter with a parser of our own

- **Status:** Accepted (Armin, 2026-10-31, with the confirmation of S024; decided 2026-10-30)
- **Date:** 2026-10-30
- **Deciders:** project owner (Armin, Daniel)

## Context

S024 shows the front matter of a note (S023) as properties, as Obsidian does. That needs the YAML
between the delimiter lines read into keys, types and values. hitokoto only reads it: the
properties block writes nothing (editing stays text, S024 decision 2), so the bytes of the file
never depend on a YAML library (ADR 0002). Writing properties through the block is S025; it will
change only the bytes of one value and needs no serialiser either.

Front matter in notes is small and flat: keys with text, numbers, booleans, dates and lists of
text. Obsidian does not support nested values and shows them as JSON (help, "Properties").

The brief (section 3) asks to minimise dependencies and to add one only when it clearly beats our
own code. The core uses the C++ standard library only (`CLAUDE.md`).

## Options

1. **rapidyaml** (MIT, release 0.12.1 in 2026, active): full YAML 1.2 tree parser, fast, C++
   without Qt. A new dependency on three platforms, in conda-forge, vcpkg or FetchContent, in the
   core-only CI job and the fuzz build.
2. **yaml-cpp** (MIT, 0.9.0): widely packaged, slower, reports errors by exceptions, which the core
   avoids on the render path. Same footprint as 1.
3. **A parser of our own** for a subset of YAML 1.2 in the core, about the size of the table
   element: block mappings and sequences by indentation, flow collections, plain, quoted and block
   scalars, comments, and the core schema for null, booleans and numbers.

## Decision

Option 3. The subset is exactly what S024 lists under "The YAML"; everything outside it is
reported as an error with its line, and the block stays text with a badge. In particular anchors,
aliases, tags, complex keys, directives, `...` and explicit indentation indicators are outside the
subset, although they are valid YAML: none of them appears in Obsidian's own properties, and each
would add code paths for no visible gain.

- Interface: `parseProperties(lines)` in the core returns the top-level rows (key, type, display
  text, list items, date fields, lines) or the first error with its line and reason; a reason is
  either "invalid" (wrong YAML) or "unsupported" (valid YAML outside the subset), so the badge can
  tell them apart (S024 review O10). Behind it a small tree of scalars, sequences and mappings,
  used for types and for the JSON of unsupported values. Replacing it with a library later changes
  only this function.
- Never throws, no recursion deeper than a fixed limit (64 levels; deeper is an error), linear in
  the length of the YAML.
- Reference: the expected values of the tests are checked against js-yaml 4 and ruamel.yaml 0.19
  (both YAML 1.2), as CommonMark expectations are checked against commonmark.js. A differential
  run over random front matter compares validity and values with js-yaml; differences that the
  subset decides on purpose (anchors, tags, invalid calendar dates) are listed in S024.
- `fuzz_render` also feeds the YAML of each closed front matter, and its whole input split into
  lines, to the parser (S024 review O12).

## Consequences

- No new dependency, no change to the build on any platform, the core stays standard library only.
- We own a parser and its tests. js-yaml's loader alone has about 1,800 lines with anchors, tags
  and directives; the subset still needs flow collections, quoted escapes, folding, block scalars
  with chomping, a JSON writer with ECMAScript number formatting and calendar checks, so expect
  roughly 800 to 1,200 lines (S024 review, capped concern). Bugs in it can only hide properties or
  show them wrongly; they cannot change the file.
- Files that use anchors or tags in their front matter show it as text with a badge. If that turns
  out to matter, a library (option 1) can replace the parser behind the same interface; this ADR
  would then be superseded.
- S025 (editing properties through the block) builds on the lines that the parser reports for
  each key and value.

## Reconsider if

- Notes in real use carry anchors, tags or other YAML outside the subset often enough that the
  badge annoys more than it helps.
- S025 or a later story needs to write YAML beyond replacing one value, such as reformatting a
  block, which a library with a serialiser does better.
- The parser keeps producing differences from js-yaml in the differential run that are not
  decided on purpose.
