---
story: S044
title: "Objection record: S044–S047 LaTeX math (pull request 78)"
mode: code
spec: docs/stories/044-latex-math.md
branch: feature/latex-notation
commits_examined: "pull request 78 at 4e5b6b1"
date: 2026-11-24
reviewer: Claude (code review of pull request 78, the session that also makes the fixes)
dispositioned: 2026-11-24 (Claude, after Daniel's go-ahead to fix O1–O5; O6–O9 not yet decided)
objections:
  - id: O1
    category: failure
    severity: major
    title: "Layout pointers of a line dangle when the formula cache drops its entries"
    disposition: fixed
    disposition_rationale: "MathCache hands out shared_ptr<const MathLayout>; EditorFormats keeps them for the line it formats, so dropping the cache at kCapacity frees nothing still in use. UI test aLayoutHeldOutlivesTheCacheDroppingIt (tst_s046)."
  - id: O2
    category: cost
    severity: major
    title: "Padding short rows makes a ragged matrix quadratic in its source"
    disposition: fixed
    disposition_rationale: "A grid with more cells than the source has characters (or more than 65535 rows or columns) is refused with \"matrix too large\" before any padding. Core test S045_MathLayout.ARaggedMatrixStaysLinearInItsSource."
  - id: O3
    category: failure
    severity: major
    title: "\\dfrac, \\tfrac, \\dbinom and \\tbinom ignore the style they force"
    disposition: fixed
    disposition_rationale: "The fraction node carries the forced style (display for d*, text for t*) and the layout sets the whole fraction in it. Core test S045_MathLayout.DfracAndTfracForceTheirStyle."
  - id: O4
    category: failure
    severity: major
    title: "The head of a formula starting outside the BMP splits a surrogate pair"
    disposition: fixed
    disposition_rationale: "EditorFormats::formulaHeadLength gives 2 units for a surrogate pair; the format boundary and the measured width use it. UI test aFormulaStartingOutsideTheBmpKeepsItsFirstCharacterWhole (tst_s046)."
  - id: O5
    category: failure
    severity: major
    title: "MathBlockEmbedder::claims() answers with block numbers from before an edit"
    disposition: fixed
    disposition_rationale: "claims() rebuilds its numbers from the claimed cursors when the block count, the revision or the count of contents changes differs from the last build. UI test anEditAboveMovesTheClaimWithTheBlockBeforeTheRefresh (tst_s047)."
  - id: O6
    category: failure
    severity: minor
    title: "An explicit \\limits is ignored outside display style"
    disposition: open
    disposition_rationale: "Not decided yet."
  - id: O7
    category: failure
    severity: minor
    title: "A fraction with scripts loses the Inner class for spacing"
    disposition: open
    disposition_rationale: "Not decided yet."
  - id: O8
    category: failure
    severity: minor
    title: "An array clamped to 65535 rows or columns lays out as nothing without an error"
    disposition: fixed
    disposition_rationale: "Covered by the check of O2: such a grid now fails with \"matrix too large\"."
  - id: O9
    category: operational
    severity: minor
    title: "Layout errors always report position 0"
    disposition: open
    disposition_rationale: "Not decided yet."
---

# Review S044–S047 — LaTeX math (code)

Scope: pull request 78 (`feature/latex-notation`, head `4e5b6b1`), stories S044–S047, read against
CLAUDE.md, ADR 0014 and the stories' decisions.

## O1 - failure - major

`EditorFormats::rangesOf` kept `&mathLayout(...)` for every formula of a line, while
`MathCache::layout` cleared its whole map at `kCapacity`. When the lookup of a later formula on the
line filled the cache, the pointer of an earlier one referred to a destroyed node, and
`applyFormula` read it: a use-after-free in a document with many formulas.

## O2 - cost - major

`environment()` padded every row to the widest one. One row of n `&` and n rows of `\\` is about 3n
characters of source and n² cells; S045 promises linear time for any input.

## O3 - failure - major

`\dfrac`, `\tfrac`, `\dbinom` and `\tbinom` went to the same handler as `\frac` and `\binom` and laid
out in the surrounding style: `$\dfrac{1}{2}$` came out as small as `$\frac{1}{2}$`.

## O4 - failure - major

The boundary after a formula's first character was `start + 1` in UTF-16 units, so a source starting
with a surrogate pair (𝑥 typed directly) had its halves in two formats, and the measured width
was taken the same way.

## O5 - failure - major

`claims()` looked blocks up by the numbers of the last refresh, although the claimed cursors follow
edits. Between an edit above a typeset block and the queued refresh, the wrong blocks counted as
claimed for BlockLayout and the other claimants.

## O6 - failure - minor

The parser keeps an explicit `\limits` in the operator's default field, and the layout sets limits
only in display style, so `$\sum\limits_{k}^{n}$` keeps its scripts beside the operator.

## O7 - failure - minor

`atomWithScripts` gives a scripts node the class of its base node's field, Ord for a fraction,
where `classOf` gives a bare fraction Inner: `x\frac{a}{b}^2` loses its thin space.

## O8 - failure - minor

`rows` and `columns` were clamped to 65535 while every cell stayed, so the layout found
`children.size() != rows * columns` and returned an empty box without an error.

## O9 - operational - minor

`Layout::fail` always reports position 0; only parse errors carry the position of their cause.
