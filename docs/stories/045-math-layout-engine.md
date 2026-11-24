# S045 — Math layout engine in the core

- **Status:** In review since 2026-11-24 (CI green on Linux, Linux with the minimum Qt, Windows,
  macOS, ASan, clang-tidy, fuzzing and the benchmarks on pull request 78) · **Order:** 50 ·
  **Size:** L · **Depends on:** S044
- **Basis:** ADR 0014 (an engine of our own, Daniel, 2026-11-24). Everything the sample
  `tests/data/s044-latex.md` writes must typeset. Confirmed by Daniel on 2026-11-24.

## Story

As a writer, I want the LaTeX of my formulas laid out the way TeX lays out mathematics, so that
fractions, roots, limits, matrices and aligned equations read as they do in print.

## Decisions (proposed)

1. **In `src/core`, standard C++ only.** A parser turns a formula's source into a tree (atoms,
   groups, scripts, fractions, radicals, `\left…\right`, environments); a layout pass turns the
   tree into boxes with width, height and depth and a flat list of glyphs and rules at positions.
   Never throws; a formula it cannot read gives an error with its position and reason.
2. **Fonts through an interface.** The layout asks a `MathFont` for glyph ids, advances, bounds,
   italic corrections and the OpenType `MATH` table (constants, glyph variants and assemblies for
   stretchy delimiters and radicals, extensible arrows). The core reads the `MATH` table from its
   bytes; the UI supplies the bytes and glyph metrics from Qt (`QRawFont`). Tests use a fake font
   with fixed metrics and the bundled font's real table.
3. **TeX's rules.** Atom classes and the spacing table (thin, medium, thick space), styles
   (display, text, script, scriptscript) with cramping, the placement rules of The TeXbook,
   appendix G, driven by the `MATH` constants as OpenType math does.
4. **The subset.** Greek letters and symbols (`\nabla`, `\partial`, `\cdots`, `\vdots`, `\ddots`,
   `\leq`, `\neq`, `\uparrow`, …), `^` `_`, `\frac`, `\binom`, `\sqrt[n]`, `\sum` `\int` with
   limits, `\left` `\right`, `\mathbf` `\mathrm` `\mathit` `\text`, `\quad` `\qquad` `\,` `\;`,
   `\sin` and the other operator names, `\xrightarrow[below]{above}`, environments `aligned`,
   `cases`, `matrix`, `pmatrix`, `bmatrix`, `vmatrix`, with `&` and `\\`.
5. **Cost.** A formula of the sample lays out in well under a millisecond; results are cached by
   the UI (S046).
6. **The bundled font (done 2026-11-24).** STIX Two Math 2.13 under the SIL OFL, compiled into the
   program as a Qt resource and read on the first formula (`ui::bundledMathFont`, ADR 0014), so no
   user installs a font; `tst_s045_bundled_math_font` checks that it loads, has its `MATH` table
   and the glyphs of the sample, and that its licence is inside too. `math.font-family` stays the
   font of formula source (S044); the typeset formula always uses the bundled font until a token
   for another math font is asked for.

## Acceptance scenarios (outline)

- Every formula in the sample lays out without error with the bundled font.
- `\frac{a}{b}` centres numerator and denominator over a rule of the `MATH` thickness.
- `x^2_1` places scripts by the `MATH` shifts; `\sum_{k=1}^n` puts limits above and below in
  display style and beside in text style.
- `\left( \frac{a}{b} \right)` picks a delimiter variant at least as tall as its content, or an
  assembly.
- `\frac{a` gives an error naming the missing `}` and its position.
- Fuzzing: any input never crashes and stays within linear time and bounded depth.

## In review

- **What changed:** `core::layoutMath` (ADR 0014, parser in `math_parse.cpp`) takes the subset of
  decision 4 and lays it out from the bundled STIX Two Math's MATH table (`math_table.cpp`):
  TeX spacing, scripts, limits, fractions, radicals, stretchy delimiters, matrices, `aligned`,
  `cases` and extensible arrows, with an error and its position for everything else; the UI reads
  the font's metrics through `QtMathFont` (`fonts/stix-two-math`, OFL).
- **Tests added:** `S045_MathLayout` (core), `tst_s045_bundled_math_font` (UI), `fuzz_math`
  (formulas and MATH tables), benchmark `S045_LayoutMaxwellEquations`.
- **Next:** Daniel checks the sample's formulas in the app and marks the story Done; accents,
  macros, `ssty` and MathKern wait in `unrefined.md`.

## Tests

- Core (`tests/core/test_s045_math_layout.cpp`, `S045_MathLayout`): the MATH table of the bundled
  font (constants, italic corrections, variants and assemblies of `(`, the radical sign and the
  arrow), malformed tables and every prefix of the real one, every formula of the sample with the
  real table, italic and bold letters, TeX's spacing (binary operators, relations, unary minus,
  punctuation, operator names, explicit spaces), scripts by the MATH shifts and the gap between
  them, limits in display style, fractions centred on the axis and `\binom`, radicals with and
  without a degree, delimiters from glyph to variant to assembly, matrices, aligned relations,
  cases, extensible arrows, every error with its reason and position, the nesting bound, the
  subset of decision 4, and 3,000 random inputs.
- UI (`tst_s045_bundled_math_font`): the sample laid out with the real glyph metrics through
  `QtMathFont`; `HITOKOTO_MATH_PREVIEW=<file.png>` writes a picture of it.
- Fuzzing (`fuzz/fuzz_math.cpp`, in CI's fuzz job): any bytes as a formula and as a MATH table.

## Implementation

- `src/core/include/hitokoto/core/math.hpp`: `MathFont` (the UI's glyph metrics), `MathTable`
  (read once from the font's MATH table), `layoutMath()` giving glyphs and rules in pixels, or an
  error. `openTypeTable()` finds a table in a font file.
- `src/core/src/math_parse.cpp`: one pass over the source into a tree, at most 64 levels; tables
  for symbols, large operators, operator names, spaces, environments and commands with arguments.
- `src/core/src/math_layout.cpp`: TeX's atom classes and spacing table (with its rules for unary
  operators), styles with cramping, scripts and limits, fractions and stacks, radicals with a
  degree, delimiters (`\delimiterfactor` 901, `\delimitershortfall` 5pt) from variants or
  assemblies, arrays (`\arraystretch` struts, `\jot` for aligned), extensible arrows, the
  `\LaTeX` logo.
- `src/core/src/math_table.cpp`: the MATH table, bounds-checked.
- `src/ui/math_font.cpp`: `QtMathFont` reads metrics from `QRawFont` loaded at one pixel per
  design unit; `bundledMathTable()` and `bundledMathMetrics()` are made on first use.

Not yet: script-style glyph variants (`ssty`), cut-ins (MathKern), accents (`\hat`, `\vec`),
macros. A benchmark of the layout comes with S046, where the UI caches layouts.
