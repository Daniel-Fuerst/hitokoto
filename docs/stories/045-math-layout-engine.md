# S045 — Math layout engine in the core

- **Status:** Draft (the font is in, decision 6) · **Order:** 50 · **Size:** L · **Depends on:** S044
- **Basis:** ADR 0014 (an engine of our own, Daniel, 2026-11-24). Everything the sample
  `tests/data/s044-latex.md` writes must typeset.

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
