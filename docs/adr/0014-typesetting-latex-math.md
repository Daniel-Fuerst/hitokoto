# ADR 0014: Typesetting LaTeX math

- **Status:** Accepted (Daniel, 2026-11-24: option 3, an engine of our own)
- **Date:** 2026-11-24
- **Deciders:** project owner (Daniel, Armin)

## Context

S044–S047 show LaTeX formulas typeset. Laying out mathematics (fractions, radicals, limits, matrices,
stretchy delimiters, `aligned`, `cases`, extensible arrows) is a typesetting engine in itself.
Constraints: no web technology (brief, section 2), so no KaTeX or MathJax in a webview; dependencies
must be well maintained and minimal (section 3); typing within one frame and no idle CPU
(section 4); every visible property themeable (section 8); `src/core` is standard C++ only
(ADR 0001), so anything that paints lives in `src/ui` behind our own interface.

## Options

1. **JKQTMathText** (part of JKQTPlotter, LGPL-2.1-or-later). A hand-written LaTeX math renderer in
   C++ on Qt that paints with `QPainter`, for Qt 5 and Qt 6, documented as a self-contained library
   (trunk v5.0.0), packaged in Fedora. Supports a large LaTeX subset with bundled or system math
   fonts (XITS, STIX, Fira Math). It fits the stack: Qt types, CMake, painting into our own layout.
   Risks: one principal maintainer (needs an explicit exception under section 3, or a plan to
   vendor it); it is built from the JKQTPlotter tree (whether the `JKQTMathText` and
   `JKQTCommon` libraries build alone without the plotter, and how big they are, is to be measured);
   coverage of `aligned`, `pmatrix`, `cases`, `\xrightarrow` and `\binom` is to be checked against
   the sample.
2. **MicroTeX** (MIT, C++, port of JLaTeXMath, with a Qt backend). Good coverage, but the original
   repository has had no release for years, a rewrite branch is unfinished and several forks exist
   with none clearly in charge. Fails section 3 on maintenance.
3. **An engine of our own.** Box-and-glue layout following TeX's rules for math (The TeXbook,
   appendix G) driven by the OpenType `MATH` table of one bundled font (STIX Two Math or Latin
   Modern Math, SIL OFL), with the parser and layout in `src/core` (testable, no dependency) and only
   glyph painting in the UI. Full control over tokens, caching and performance, no third-party
   risk. Cost: several L stories before the sample typesets as well as option 1 does, and a
   permanent maintenance load.
4. **An installed TeX (latex + dvisvgm) called per formula.** Best quality, but a TeX installation
   the user must have, a process per formula (hundreds of milliseconds), and images that do not
   follow the theme. Rejected.

## Decision

Option 3, an engine of our own (Daniel, 2026-11-24). No third-party math library: the parser and
the layout live in `src/core` (standard C++, tested without Qt), driven by the OpenType `MATH`
table of one bundled font under the SIL OFL; the UI only supplies the font's bytes and glyph
metrics (`QRawFont`) and paints the glyphs and rules the core places. The work is split into
stories: S044 finds formulas and styles their source, S045 is the engine, S046 shows inline
formulas typeset, S047 display formulas. The subset is what S045 names, starting from the sample.

### The font (Daniel, 2026-11-24)

**STIX Two Math 2.13** (`fonts/stix-two-math/`, SIL OFL 1.1), compiled into the program as a Qt
resource (`:/fonts/STIXTwoMath-Regular.otf`), so it works on every platform without a font the
user installs, and read only when the first formula needs it, so cold start does not change.
Chosen over Latin Modern Math (GUST Font License, the look of TeX's Computer Modern) and Fira Math
(OFL, sans serif, a smaller `MATH` coverage): STIX Two has the most complete `MATH` table of the
three (constants, variants and assemblies for every stretchy delimiter, radical and arrow the
sample uses), is published by the STI Pub companies for scientific publishing, under the licence
with the fewest obligations, and its maintainers still work on it (an interim v2.14 in 2025, the
last commit in 2026). The OTF (CFF outlines, 838 KB) is half the size of the TTF. The packages
carry `OFL.txt` (Debian `copyright`, the AUR package's licences, the MSI's `THIRD-PARTY.txt`).

## Consequences

- The core gains two elements (inline and display math, S044) and a math layout engine (S045); no
  new library.
- The bundled font adds 838 KB to the program and is a file we take from upstream releases
  (`fonts/stix-two-math/README.md` says how to update it); its licence (OFL) goes next to the GPL
  in the packages.
- Quality depends on our own implementation of TeX's rules; S045's tests compare placements with
  the `MATH` constants, and the sample is the acceptance document.
- A larger subset (more environments, macros) is new stories, each small.

## Reconsider if

The engine's maintenance outgrows its use (the subset users ask for keeps growing faster than we
can add to it), or a well-maintained native math library for Qt appears; the core's tree and the
`MathFont` interface keep a replacement local to the UI.
