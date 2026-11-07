# ADR 0008: Find in the core, with simple case folding from Unicode data

- **Status:** Accepted (Armin, 2026-11-07, with the confirmation of S028; proposed in its spec review)
- **Date:** 2026-11-07
- **Deciders:** project owner (Armin, Daniel)

## Context

S028 adds find in the open file. It searches the concealed view (S028 decision 1), table cells and
front matter as text without syntax (decision 3), treats whitespace runs as equal (decision 4) and
compares case-insensitively with Unicode case folding unless "Match case" is on (decision 6).

CLAUDE.md puts all logic into `src/core`, which uses the C++ standard library only and is tested
without Qt (`core-sanitize` preset). The standard library has no Unicode case folding: `towlower`
and `std::tolower` depend on the C locale, work on `wchar_t` or `char`, and differ between platforms.
Qt has `QChar::toCaseFolded` and `QString::toCaseFolded`, but only in the UI.

## Options

1. **Matcher in the UI** with `QString::toCaseFolded`. Simple, but the rules of S028 (units,
   boundaries, whitespace, mapping to source ranges) would live in the UI, untested by the core
   tests, against CLAUDE.md.
2. **Matcher in the core, folding passed in from the UI** (`std::function<char32_t(char32_t)>` or a
   table filled from `QChar::toCaseFolded`). The rules stay in the core, but the core tests cannot
   check real case folding without Qt, and the result follows the Unicode version of the Qt in use
   (which may differ between Qt 6.8 and 6.12).
3. **Matcher in the core with a folding table generated from Unicode's `CaseFolding.txt`.** Only the
   simple mappings (status C and S, one code point to one) are needed. A script in `scripts/`
   generates a compact, sorted range table as a C++ source file in `src/core`; the generated file
   and the Unicode version are checked in. The data file is under the Unicode License v3, which is
   compatible with GPL-3.0-or-later; the generated file carries its notice.

## Decision

Option 3, pinned to the Unicode version current when S028 is implemented (Unicode 18.0.0, released
in September 2026; checked again then).

- Interface in the core: a compiled query (search text, match case on or off) and a matcher that
  walks units with their source offsets and reports matches as source ranges; the folding function
  `foldCaseSimple(char32_t)` is internal to the core.
- The generator is a small script without dependencies beyond the language's standard library; it
  is run by hand when the Unicode version is raised, not in the build. A core test checks a sample of
  mappings (`Ä`, `Σ`/`ς`, `ẞ`, Cherokee, Deseret outside the BMP) and that every mapping is one code
  point to one.
- A UI test compares `foldCaseSimple` with `QChar::toCaseFolded` for every code point and lists the
  differences that come from Qt's Unicode version, so they are known rather than surprising.
- Normalisation and diacritic folding are not part of it (S028 rules).

## Consequences

- No new library dependency; the core stays standard library only and the find rules are tested in
  `core-sanitize`.
- A generated table of roughly 1,500 mappings in ranges, a few kilobytes in the binary.
- Raising the Unicode version is a manual step with a review of the diff of the generated file.
- Find and Qt may disagree on a code point whose folding changed between Unicode versions; the UI
  test lists such code points.

## Reconsider if

- Another feature needs broader Unicode data (normalisation, word boundaries for "whole word",
  grapheme clusters for the caret); then a maintained library (ICU, or a small one such as utf8proc)
  may beat several generated tables, against the brief, section 3.
- Users ask for full folding (`ß` = `ss`) or diacritic-insensitive search; both change the mapping
  of matches back to source ranges and need their own decision.
