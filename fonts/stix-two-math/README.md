# STIX Two Math

The math font hitokoto typesets formulas with (ADR 0014, S045). It is compiled into the program
as a Qt resource (`:/fonts/STIXTwoMath-Regular.otf`), so no user installs a font.

- **Upstream:** https://github.com/stipub/stixfonts, file `fonts/static_otf/STIXTwoMath-Regular.otf`
- **Version:** 2.13 (tag `v2.13`, commit `37308d44583ef882dc8b544aaa2e80ac8c406ebf`), the latest
  release with built fonts; `v2.14` (2025) is an interim tag with sources only
- **Licence:** SIL Open Font License 1.1 (`OFL.txt`, from the same commit). The font keeps its
  own licence: it is distributed together with hitokoto (GPL-3.0-or-later) as data the program
  uses, unchanged, not relicensed under the GPL; the packages carry `OFL.txt`
- **Checksums (SHA-256):**
  - `STIXTwoMath-Regular.otf`: `f2076b9f1676438439dd41e23676f5ab99056e83d6b8f8c27841591ef2ccfa72`
  - `OFL.txt`: `bec17cee6412788db45fd2644b301afbc99cc5d372ddd3345dc4a7bfdefb9f04`
- **In git:** the font is kept in the repository (Daniel, 2026-11-24), so a build needs no
  network and every build has the same font; it changes only with an upstream release.
- **Fetched:** 2026-11-24 (Daniel's OK in conversation); git blob hashes checked against the
  upstream tree (`ed56b48c…` for the font, `96aa460d…` for the licence)

An update replaces both files from a newer upstream release, with the new commit and checksums
here; `S045_BundledMathFont` checks that the font loads and has its `MATH` table.
