# Performance

Budgets come from the brief, section 4. They are checked by two tools:

- `hitokoto_core_bench` (Google Benchmark): core costs for 10 MB files and theme parsing.
- `hitokoto-bench`: end to end. Cold start (separate processes, time from exec to the first painted
  frame with the file shown), opening 1 MB and 10 MB files, key-to-paint latency while typing,
  pressing Enter and moving across headings, and saving 10 MB (longest stall of the UI thread and
  time until the file is on disk, median of `--runs` saves).

```sh
cmake --preset bench && cmake --build --preset bench
QT_QPA_PLATFORM=offscreen build/bench/bench/hitokoto-bench --runs 10 --output results.json
build/bench/bench/hitokoto-bench --fail-on-budget   # on a reference machine, with a real display
```

CI runs both on Linux (Windows and macOS are off since 2026-10-28). Since S029 (ADR 0009) every
pull request run builds the base commit and measures it alternating with the pull request on the
same runner (four rounds: base, pull request, pull request, base); `hitokoto-bench-gate` compares
the rounds. A key fails when the mean of the pull request's rounds is more than 25 % and more than
2 ms above the base's (20 ms for the first screen of an opening) and the rounds of the two sides do
not overlap. Hard budgets fail the run on any machine: saving 10 MB stalls the UI for at most
16 ms, the file is on disk within 1 s, and no key event takes longer than 50 ms. Maxima of many
samples (`typing_*_max_ms`, `save_10mb_total_ms`) are compared for information only; p99 of the
key-to-paint samples is the compared value. Runs of `main` measure once and check the hard budgets.
A toolchain change (Qt version or runner image of the bench job, `CMakePresets.json`, `cmake/`)
comes in its own pull request. See `bench/baselines/README.md`.

`hitokoto-bench --baseline file.json --tolerance 0.25` still compares two runs on one machine, for
local measurements.

## Measurements

### 2026-11-21, development VM, S000: libstdc++'s assertions in every build (release build, offscreen, Qt 6.12.0)

`_GLIBCXX_ASSERTIONS` adds a bounds or precondition check to each `operator[]`, `front()`,
`back()` and similar call of the standard library; it changes no layout and no complexity. Three
sets of four rounds against main (`hitokoto-bench --runs 10`, `hitokoto-bench-gate --kind pr`), and
as a control main against itself with the same binaries:

| Set (order) | Keys ok | Hard budget missed | Compared value slower |
|---|---|---|---|
| 1 (main, branch, branch, main) | 99 of 102 | `s031_paste_outlook_40x10_ms` 71.8 ms in one branch round | `s031_paste_outlook_40x10_ms` |
| 2 (branch, main, main, branch) | 100 of 102 | `s007_bound_lines_typing_max_ms` 79 ms, `s028_table_field_typing_max_ms` 807 ms, one branch round each | none |
| 3 (main, branch, branch, main) | 94 of 102 | `s028_field_typing_max_ms` 114 ms in one branch round | `s018_table_typing_p99_ms`, `s031_paste_excel_40x10_ms` |
| main against main | 95 of 102 | `s028_field_typing_max_ms`, `s007_bound_lines_typing_max_ms` | four keys, among them `s031_paste_excel_40x10_ms` |

The keys that fail change from set to set, and main against itself fails the same way: on this VM
(2 vCPU) single keys and the middle rounds take outliers the gate cannot tell from a cost. Over all
six rounds of each side, the branch's medians are 1 % above main's, p95 8 %, p99 4 % below, maxima
8 % above; no key has every branch round above every round of main. The core benchmarks
(`hitokoto_core_bench`, median of three) move between −9 % and +16 % per benchmark, −2 % in the
median. Control Flow Guard and `/sdl` (MSVC) are not measured: the Windows benchmarks are off. The
gate of the pull request on the CI's runner decides.

### 2026-11-20, development VM, S031: lists in Qt's HTML import (release build, offscreen, Qt 6.12.0 and 6.8.3)

Qt marks every block of a list again each time a block joins it (`QTextBlockGroup::blockInserted`
calls `markBlocksDirty`), so one list costs its import the square of its blocks. Qt's import alone
of the HTML the scan gets, a cell of a table of four, Qt 6.12.0 (the estimate before this change in
brackets; the import may take 25 ms):

| Paste | 2,000 | 4,000 | 8,000 |
|---|---|---|---|
| one `ul` (or `ol`) of that many items | 71 ms (12 ms) | 384 ms (25 ms) | 1.74 s (49 ms) |
| items without end tags, or a list inside an item | 71 to 72 ms | 384 to 399 ms | 1.72 to 1.76 s |
| lines of a `pre` in one item | 71 ms (0.1 ms) | 389 ms (0.1 ms) | 1.73 s (0.1 ms) |
| that many lists of one item | 14 ms | 28 ms | 61 ms |
| items without a list, or lines of a `pre` outside a list | 3.6 to 4.9 ms | 7.5 to 24 ms | 16 to 23 ms |

Paragraph separators in an item, also as `&#8233;`, and `&#10;` in a `pre` in an item cost the same
as the lines of a `pre` (4,000: 374 to 387 ms, 8,000: 1.68 to 1.80 s, both Qt versions); line breaks
in an item outside a `pre` and `<div>` in an item cost nothing extra. One list of 16,000 items took
7.4 / 7.6 s (Qt 6.12.0 / 6.8.3), of 24,000 items 17.2 / 17.5 s: about 60 ns per pair of blocks;
another machine took 10.3 to 10.9 s for 16,000.

The scan now charges each block that joins a list 100 ns for every block that joined before it since
the outermost open list started. The largest paste of one kind it lets through, and Qt's import of
it (three runs, both Qt versions, highest share of what the cells allow):

| Form, in a table of | 20 cells | 2,000 cells | 8,000 cells | 50,000 cells |
|---|---|---|---|---|
| one list | 644 items, 32 % | 1,336, 34 % | 2,732, 55 % | 6,920, 66 % |
| a list inside an item | 643, 47 % | 1,334, 49 % | 2,730, 52 % | |
| lines of a `pre` in an item | 702, 20 % | 1,395, 51 % | 2,791, 55 % | 6,980, 62 % |
| paragraph separators in an item | 702, 18 % | 1,395, 37 % | 2,791, 52 % | |

The table of 2,000 cells with one list of 15,908 items (181 KB), 10.5 s before, stays plain text in
2 ms in the window (Qt 6.8.3), the one of 8,000 cells with 63,637 items (724 KB) in 9 ms; the
largest list let through in 2,000 cells pastes in 51 ms. The verdict of every sample, every table in
their markup, 900 random mixes (seeds 1 to 6) and the pastes of the formatting rework is the same as
before; only the list forms above change from Flat to OverBudget. Of the 900 mixes, Qt's import of
those the scan lets through took at most 86 % of what their cells allow in one run (Qt 6.12.0, 63 to
74 % on five reruns of the same paste) and 57 % with 6.8.3.

### 2026-11-20, development VM, S031: formatting before Qt's HTML import (release build, offscreen, Qt 6.12.0 and 6.8.3)

Three costs of Qt's import that the estimate below did not count, all from formatting a pasted table
does not keep. Qt's import alone (`QTextDocument::setHtml` of the HTML the scan judged `Flat`),
before (the HTML without style attributes, as on `a394434`) and now (without what only formats,
`core::htmlWithoutFormatting`), Qt 6.12.0 / 6.8.3:

| Paste | Size | Import may take | Before | Now |
|---|---|---|---|---|
| `<style>b ~ span{…}</style>`, 2,000 spans in a cell of four | 28 KB | 25 ms | 464 / 480 ms | 3.1 / 2.6 ms |
| the same rule, 8,785 empty spans in a row of ten cells | 62 KB | 25 ms | 29.8 / 31.2 s | 14 / 10 ms |
| the same rule across a comment, or read at `</div>` | 28 KB | 25 ms | 445 to 481 ms | 2.6 to 3.2 ms |
| `+`, child, descendant, attribute selectors, `:first-child`, `:last-child`, `* ~ *` | 2,000 spans | 25 ms | 4.2 to 8.0 ms | 2.6 to 3.5 ms |
| tables of 2 x 2 cells, `width` of the first cell different | 10,000 | 2.5 s | 6.9 / 7.7 s | 0.38 / 0.31 s |
| the same in percent, or on th | 10,000 | 2.5 s | 7.2 to 7.6 s | 0.31 to 0.40 s |
| the same with one width, or `width` of col or table, or `bgcolor` | 10,000 | 2.5 s | 0.31 to 0.44 s | 0.31 to 0.37 s |
| `<font size=… color=…>` with the same sum of size and colour | 4,000 in a cell | 25 ms | 141 / 139 ms | 6.6 / 5.6 ms |
| the same with random sizes and colours | 4,000 in a cell | 25 ms | 17 / 13 ms | 7.3 / 5.7 ms |
| the same sum, eight fonts in each of 2,000 / 8,000 cells | 0.7 / 2.8 MB | 125 / 500 ms | 2.9 / 5.3 s, 46 / 58 s | 47 / 38 ms, 0.22 / 0.17 s |

A rule `b ~ span` checks each span against every sibling before it, finding each with a search of
their parent (`QTextHtmlStyleSelector::previousSiblingNode`); a table keeps its column widths in a
`QList<QTextLength>`, and `QTextFormatCollection::indexForFormat` hashes such a list by its type
alone, so each table is compared with every one before it; and the hash of a format is the sum of
the hashes of its properties, an integer by its value and a colour by its RGBA value, so a font size
and a colour with the same sum share it. Qt 6.8.3 and 6.12.0 have the same code here. In the window
(Ctrl+V of the HTML until the event loop is idle, Qt 6.8.3), before / now: 8,785 spans 30.8 s / 12
ms, 2,000 spans 472 / 4 ms, 10,000 tables with different widths 7.4 s / 0.51 s (with one width 0.49
/ 0.53 s), 4,000 fonts with the same sum 136 / 9 ms, 8,000 cells of them 0.22 s now.

The results stay. Qt's import of the 16 samples, of eight tables in their markup, of 300 random
mixes of 37 elements, 25 attributes, style rules and tables and of the pastes above gives the same
blocks with and without what only formats, with both Qt versions; the one difference is a style
element setting `white-space: pre`. In the editor the samples and the tables in their markup give
the same Markdown as before; 36 of the 300 mixes, which the estimate of their style rules kept plain
text, now convert. Of 600 random mixes (four seeds), Qt's import of each the scan lets through took
at most 65 % of what its cells allow (Qt 6.12.0; 61 % with 6.8.3).

The samples' markup in the estimate, without what only formats: 40 x 10 to 500 x 100 cells cost 54
to 55 % of what the cells allow with Outlook's markup and 25 to 26 % with Excel's (before 59 % and
30 %); Outlook's import took 39 to 62 % of its estimate, Excel's 39 to 44 %. Writing the HTML
without what only formats and scanning it takes 85 to 94 ms for 50,000 cells in Outlook's markup (21
MB) and 26 to 41 ms for 50,000 in Excel's (5.6 MB), against 50 to 56 ms and 20 ms for removing the
style attributes and scanning before.

### 2026-11-20, development VM, S031: bounds on Qt's HTML import (release build, offscreen, Qt 6.12.0 and 6.8.3)

Some pastes make Qt's HTML import build far more than their cells. A first cell spanning 20,000
columns above 2,000 rows (58 KB, 4,002 cells) and 2 MB of nested `<div>` in a cell were still
running after 130 s on main with both Qt versions. Pastes repeating one thing, Qt's import alone
(`ui::readPastedHtml`), the larger of the two Qt versions:

| Paste | Size | Import |
|---|---|---|
| `<td colspan=100>` above rows of two cells | 1,000 / 4,000 rows | 0.41 / 2.2 s |
| 1,000 cells, then rows without cells | 100 / 300 rows | 0.45 / 1.6 s |
| cells spanning 10,000 rows in a table of 10,000 rows | 10 / 40 of them | 1.2 / 11 s |
| `<div>` nested in each other | 2,000 / 4,000 / 8,000 | 42 / 170 / 760 ms |
| `.cN{…}` rules and as many `<span class=cN>` | 1,000 / 2,000 | 47 / 186 ms |
| `*{…}` with 1,000 declarations, `<span>` 50 deep | 1,000 spans | 0.7 s |
| `<img>` in one cell / one per cell | 10,000 | 5.1 s / 0.15 s |
| `<a name>` or `<font face>`, all different | 10,000 / 20,000 | 1.8 / 6.2 s |
| `<b>x</b>` in one cell | 80,000 / 640,000 | 0.26 / 2.2 s |
| `<p>x</p>` in one cell | 80,000 / 640,000 | 0.44 / 3.5 s |
| text, entities, comments | 4 MB / 6.8 MB / 10 MB | 63 / 122 / 49 ms |

The scan before the import (`core::scanHtml`) reads the HTML without style attributes as Qt's
parser reads it and estimates the import from what it counts, with the costs per unit of
`core::kPasteHtmlBounds`; above half of what S031 grants the cells the paste stays plain text.
Estimate against the measured import, for 108 pastes of 40 kinds and sizes: 0.86 to 2 times it
for 82 of them (below 1 only where a run was slower than other runs of the same paste), up to 5
times for style elements, whose rules the estimate takes as matching every element, and up to 13
times for spans that overlap. Pastes just under and just over the bound of one kind each, from
Ctrl+V until the table shows (median of three):

| Paste just under the bound | Cells | main, 6.12.0 / 6.8.3 | now, 6.12.0 / 6.8.3 |
|---|---|---|---|
| `<td colspan=100>` above 33 rows | 68 | 22.2 / 24.0 ms | 22.5 / 23.0 ms |
| 11 cells spanning 1,000 rows | 2,011 | 82.8 / 79.5 ms | 81.2 / 79.5 ms |
| 1,000 cells, 7 rows without cells (not shown, too wide) | 1,000 | 27.4 / 27.1 ms | 27.4 / 27.0 ms |
| 2,460 `<b>x</b>` inside 200 nested spans | 4 | 24.5 / 21.6 ms | 22.8 / 21.5 ms |
| 6,932 `<b>x</b>` | 4 | 31.8 / 27.5 ms | 31.4 / 29.2 ms |
| 4,089 `<p>x</p>` (a cell of 4,089 lines) | 4 | 93.2 / 102.1 ms | 89.8 / 93.4 ms |
| 856 spans and 100 rules | 4 | 10.3 / 10.0 ms | 10.4 / 10.4 ms |
| 1,578 spans and a rule of 100 declarations | 4 | 42.5 / 32.5 ms | 32.3 / 36.3 ms |
| 576 images in one cell | 4 | 20.5 / 19.4 ms | 20.9 / 20.7 ms |
| 1,013 `<a name>` | 4 | 17.5 / 18.9 ms | 17.6 / 18.4 ms |

Just over the bound the same pastes stay plain text in 0.3 to 1.0 ms with both Qt versions; on
main they took 10 to 93 ms. The two pastes above take 0 ms (the span) and 4 to 5 ms (the nesting:
removing the style attributes from 2.2 MB) instead of more than 130 s. The cell of 4,089 lines
takes 90 ms on main as now: the time goes into the table's layout of its lines, which is content,
not markup; the bound leaves text, lines and other content of the cells alone.

The markup of the samples stays well within: tables of 40 x 10 to 500 x 100 cells cost 59 % of
what their cells allow with Outlook's markup and 30 % with Excel's, in the estimate (Outlook's
import itself took 57 % of its estimate). The cells themselves are not bounded: the whole paste
grows linearly with them, Excel's markup 45 µs per cell at 4,000 cells and 53 µs at 250,000
(13.1 s), Outlook's 63 µs at 4,000 and 67 µs at 50,000 (3.4 s). The scan takes 37 ms for 50,000
cells in Outlook's markup (21 MB) and at most 0.32 s for 10 MB of any markup tried, less than a
plain paste of 10 MB (about 0.85 s).

### 2026-11-16, development VM, long lines (S007) (release build, offscreen, Qt 6.12.0 and 6.8.3)

A file of five lines whose third line is one pattern repeated; the caret in the middle of that
line; 20 or 30 keys (a character and Backspace in turn), each with the repaint, as `typing_*`
measures them. A probe outside the repository; main 0b69199, Qt 6.12.0, median per key:

| Line of | 1,000 | 2,000 | 5,000 | 10,000 | 20,000 characters |
|---|---|---|---|---|---|
| text without syntax | 1.0 ms | 1.6 ms | 2.5 ms | 4.2 ms | 5.9 ms |
| `a*` | 6.2 ms | 13.9 ms | 44 ms | 138 ms | 487 ms |
| `[](` | 1.9 ms | 5.2 ms | 26 ms | 100 ms | 362 ms |
| `[a](b) ` | 3.6 ms | 6.9 ms | 15.6 ms | 33 ms | 84 ms |
| `` `a `` | 4.9 ms | 8.8 ms | 19.4 ms | 36 ms | 75 ms |
| `\*` | 5.4 ms | 11.4 ms | 36 ms | 99 ms | 336 ms |

Two costs add up. Up to about 2,000 characters nearly all of the time is Qt's: laying out and
drawing a line grows with its formatted pieces, about 25 µs for each `[a](b)` and about 12 µs for
each `*` of `a*` (a concealed marker has a font of its own, a style changes the font). Beyond that
the core's share grows with the square of the length (the inline elements and the registry's merge
of their results), and every key renders its line several times: the styler once, each repaint
once more for the code-block panels and once for the decorations.

The bounds of S007 follow from this (`core::kInlineBounds`; a line above either shows as text).
The time for a key has to stay under 50 ms with a margin of two, so under about 25 ms for the
slowest key on this VM:

| Line | Key in the line, median / slowest of 30 | Key on the line above it |
|---|---|---|
| 500 links `[a](b) ` (3,500 characters) | 13 / 26 ms | 14 / 23 ms |
| 1,000 links | 22 / 32 ms | 28 / 47 ms |
| 2,000 links | 50 / 70 ms | 61 / 81 ms |
| 500 `[](`, then text without blanks up to 4,000 characters | 12 / 17 ms | 9 / 9 ms |
| the same up to 8,000 characters | 25 / 33 ms | 18 / 20 ms |
| the same up to 16,000 characters | 55 / 114 ms | 35 / 50 ms |

A link is the dearest piece per syntax character, and `[](` is the pattern that reads on to the
end of the line each time it starts. So: at most 500 characters that can start inline syntax, and
at most 4,000 characters. With the bounds in place, lines at the bounds (500 syntax characters of
each of twelve patterns, alone and with text up to 3,990 characters) take at most 13 ms per key
with Qt 6.12.0 and 14 ms with Qt 6.8.3 (links in both); the slowest single key in the line was
26 ms and 18 ms. A key on the line above, which renders the line again, took at most 14 ms and
17 ms; the slowest single one 28 ms and, once in 30 keys, 39 ms (the next slowest 25 ms).

A line without any syntax is not helped by the bounds: 43 ms per key (up to 53 ms) with 200,000
characters, all of it Qt's layout of the block (`docs/stories/unrefined.md`, "Very long lines").

The inline elements and the registry's merge of their results are linear since the rework of
S013, S015, S020 and S022 on the same day. The core alone, one render concealed and one revealed
of one line (a harness outside the repository, an optimised build, with the bounds lifted so that
the elements run; best of three after the rework):

| Line of | 50,000 | 100,000 | 200,000 characters | Before, 200,000 |
|---|---|---|---|---|
| `[](` | 0.6 ms | 1.4 ms | 2.9 ms | 18.9 s |
| `[a](` | 0.6 ms | 1.2 ms | 2.5 ms | 11.2 s |
| `a*` | 5.6 ms | 11.9 ms | 27.2 ms | 9.5 s |
| `` `a `` | 2.9 ms | 5.8 ms | 12.4 ms | 4.6 s |
| `\` | 2.9 ms | 6.1 ms | 12.9 ms | 6.9 s |
| `~~a` | 4.1 ms | 8.6 ms | 18.0 ms | 4.1 s |

Counted in steps (`core::inlineSteps`, three renders of a line), 53 patterns take between 3 and
47 steps per character whatever the length; 40 of them took 59 to 64 times the steps for eight
times the length before.

With linear elements the length of a line adds little to a key, so the bound on the length rose
from 4,000 to 10,000 characters. 500 syntax characters of a pattern, then words up to the given
length; a key in the line, median / slowest of 30, Qt 6.12.0:

| Line | 10,000 | 20,000 | 32,000 | 50,000 | 100,000 characters |
|---|---|---|---|---|---|
| 500 links `[a](b) ` | 11 / 13 ms | 13 / 22 ms | 14 / 20 ms | 18 / 20 ms | 30 / 51 ms |
| 500 `` ` `` (250 code spans) | 8 / 9 ms | 12 / 15 ms | 14 / 26 ms | 19 / 22 ms | 33 / 49 ms |
| emphasis, code, link, escape and strikethrough in turn | 8 / 17 ms | 12 / 21 ms | 15 / 21 ms | 20 / 31 ms | 34 / 37 ms |
| words without syntax | 4 / 7 ms | 6 / 11 ms | 8 / 11 ms | 13 / 16 ms | – |

The bound on the syntax characters went from 500 to 300 at the same time. With 500, the slowest
of 30 keys in a line at the bounds took up to 26 ms with Qt 6.12.0 and 25 ms with Qt 6.8.3 on
this VM, a key on the line above up to 28 ms (the numbers of the first table of bounds above). A
CI runner can be half as fast as this VM in laying out text (factors of 1.1 to 2 in the S031
measurements below), which would put such a key at the 50 ms.

At the bounds as they are now (300 syntax characters of each of thirteen patterns, alone and with
words or with unbroken text up to 9,990 characters): a key in the line takes at most 8 ms with
both Qt versions (links), a key on the line above 9 ms and 12 ms; the slowest single key was
15 ms and 18 ms. One key of 30 on a line above took 49 ms in one run with Qt 6.8.3 and at most
12 ms in three more runs of the same file.

Opening a file whose third line has 200,000 characters, until the first screen shows / until
styled (the window does not answer until the first screen shows):

| Line of | main, Qt 6.12.0 | main, Qt 6.8.3 | now, Qt 6.12.0 | now, Qt 6.8.3 |
|---|---|---|---|---|
| words without syntax | 0.07 / 0.10 s | 0.07 / 0.08 s | 0.07 / 0.09 s | 0.07 / 0.08 s |
| `[](` | 22 / 84 s | 28 / 98 s | 0.07 / 0.09 s | 0.06 / 0.08 s |
| `[a](` | 18 / 66 s | 18 / 67 s | 0.07 / 0.09 s | 0.07 / 0.09 s |
| `` `a `` | 19 / 63 s | 14 / 46 s | 0.09 / 0.11 s | 0.07 / 0.08 s |
| `\` | 24 / 82 s | 17 / 58 s | 0.07 / 0.09 s | 0.08 / 0.10 s |
| `a*` | 90 / 168 s | 82 / 145 s | 3.0 / 3.0 s | 2.7 / 2.7 s |
| `~~a` | 42 / 75 s | 39 / 68 s | 3.9 / 4.0 s | 3.8 / 3.8 s |
| `x` (one word, no syntax) | 3.0 / 3.0 s | 3.4 / 3.4 s | 2.8 / 2.8 s | 3.3 / 3.3 s |

`a*` and `~~a` hold no place where a line may break, so they open like one word of 200,000
characters: the three seconds that remain are Qt's layout of that word, as for `x`.

`hitokoto-bench` measures long lines since then (`s007_*`), three runs on this VM with Qt 6.12.0:

| Measurement | Value | Budget |
|---|---|---|
| Key in lines at the bounds, median / p95 / max (`s007_bound_lines_typing_*`) | 5.4–5.8 / 8.2–9.6 / 8.6–26.9 ms | 50 ms max (hard) |
| Key in lines above the bounds, median / p95 / max (`s007_long_lines_typing_*`) | 4.3–4.4 / 8.7–9.3 / 10.6–18.8 ms | 50 ms max (hard) |
| Open four lines of 200,000 characters, first screen / styled (`s007_200k_lines_open_*`) | 63–70 / 82–89 ms | – |
| Open one line of 200,000 characters `a*`, styled (`s007_200k_word_open_styled_ms`) | 3.0–3.1 s | reported only |

The lines at the bounds are seven lines, one for each of `[](`, `[a](`, `a*`, `` `a ``, `\`, `~~a`
and `[a](b) `: the pattern until the line holds 300 syntax characters, then words up to 9,990
characters, so they show their formatting. The lines above the bounds repeat each pattern up to
10,010 characters and show as text. 30 keys in the middle of each line, a character and Backspace
in turn. The four lines of 200,000 characters are the patterns that hold a place to break a line
(`[](`, `[a](`, `` `a ``, `\`; median of five opens); the line of `a*` is one word to Qt and is
opened once, its time is reported and never fails a run. The maxima are compared for information,
as those of the other typing measurements. With the earlier bound of 500 syntax characters the
slowest key at the bounds was 27 ms and the p95 19 ms in one run.

Text without blanks is a case of its own, with or without syntax and on main as well: Qt lays out
one long word in a time that grows with the square of its length. A key in a line of 10,000
characters `x` takes 8 ms, of 20,000 characters 21 ms, of 32,000 characters 45 ms, of 50,000
characters 100 ms (`docs/stories/unrefined.md`, "Very long lines").

### 2026-11-09, development VM, S031 (release build, offscreen, Qt 6.12.0)

`s031_paste_*`: Ctrl+V in an empty document until the table view shows all rows, median of five
pastes. The clipboard holds a table generated from the markup of a sample (`outlook-t1-example`,
`excel-t6-example`; `tests/ui/paste_samples.hpp`) and its plain text. Story S031: 500 cells within
50 ms, above that at most 100 µs per cell; hard budgets of the gate.

| Table | Outlook markup | Excel markup | Budget (hard) |
|---|---|---|---|
| 50 × 10 (500 cells) | 44 ms | 31 ms | 50 ms |
| 500 × 10 (5,000 cells) | 344 ms | 206 ms | 500 ms |
| 50 × 100 (5,000 cells) | 356 ms | 233 ms | 500 ms |

Of the 44 ms for 50 × 10 with Outlook's markup, Qt's HTML import takes 20 ms (198 KB), the core and
the insertion 1.5 ms; the rest is styling and showing the table (S018). With Excel's markup (44 KB)
the import takes 5 ms.

On the CI runner (AMD EPYC 7763, dispatch on 7a742f6) the same pastes took 72 / 59 ms, 515 / 370 ms
and 539 / 396 ms (Outlook / Excel): four hard budgets missed. Most of the time for Excel's markup
went into sizing the cells (`TableView::fit`: a `QTextDocument` with `setMarkdown` per cell, for the
width and again for the height). Since then a cell that GFM shows as exactly its characters
(`core::tableCellIsPlainText`) is laid out as plain text, an unwrapped cell takes its natural height
without a second layout, and each cell's Markdown is read once. Development VM afterwards (median of
five):

| Table | Outlook markup | Excel markup | Before |
|---|---|---|---|
| 50 × 10 | 35 ms | 23 ms | 44 / 31 ms |
| 500 × 10 | 311 ms | 176 ms | 344 / 206 ms |
| 50 × 100 | 331 ms | 193 ms | 356 / 233 ms |

`s018_table_open_ms` (a 1,000-row table) went from 96 to 61 ms. Qt's HTML import of Outlook's
markup stays: about 18 ms for 500 cells and 200 ms for 5,000 on the VM.

### 2026-11-10, development VM, S031 review (code) (release build, offscreen, Qt 6.12.0)

Review `docs/reviews/031-paste-tables-code.md`, O10 step 1: the generated tables now hold cells
like the samples' (`tests/ui/paste_samples.hpp`, in turn one word, two lines, `R&D`, and two lines
with `09:30` and `a_b`). Two-line cells and cells with `&`, `:` or `_` take Qt's Markdown layout,
not the plain-text path of the sizing fix. Median of five pastes:

| Table | One-word cells (2178d93) | Sample-like cells, with the red tests of the review | With the review fixes | Budget (hard) |
|---|---|---|---|---|
| Outlook 50 × 10 | 35 ms | 54 ms | 56 ms | 50 ms |
| Outlook 500 × 10 | 311 ms | 462 ms | 480 ms | 500 ms |
| Outlook 50 × 100 | 331 ms | 498 ms | 514 ms | 500 ms |
| Excel 50 × 10 | 23 ms | 36 ms | 37 ms | 50 ms |
| Excel 500 × 10 | 176 ms | 285 ms | 306 ms | 500 ms |
| Excel 50 × 100 | 193 ms | 318 ms | 339 ms | 500 ms |

With sample-like cells Outlook 50 × 10 and 50 × 100 miss their budgets on the VM; Excel holds them.
The review fixes add up to 7 % (the scan of the HTML before the import, the display escape on the
Markdown path); runs of the same build differ by about 5 %.

Step 2 (Armin, after these numbers): a cell holding only plain characters, backslash escapes and
`<br>` is laid out as plain text with U+2028 between its lines (`core::tableCellPlainLayout`).
Cells with `R&D` or `09:30` still take Qt's Markdown layout. Median of five:

| Table | Sample-like cells, step 1 | Step 2 | Budget (hard) |
|---|---|---|---|
| Outlook 50 × 10 | 56 ms | 51 ms | 50 ms |
| Outlook 500 × 10 | 480 ms | 424 ms | 500 ms |
| Outlook 50 × 100 | 514 ms | 475 ms | 500 ms |
| Excel 50 × 10 | 37 ms | 35 ms | 50 ms |
| Excel 500 × 10 | 306 ms | 267 ms | 500 ms |
| Excel 50 × 100 | 339 ms | 316 ms | 500 ms |

Step 3 (Armin, 2026-11-10): a colon not followed by `//` and an ampersand that starts no entity
are text, so `R&D` and `09:30` cells are laid out as plain text too (`core::tableCellIsPlainText`;
links with `://`, e-mail addresses and entities still take the Markdown layout; a link form that
GFM does not recognise is an accepted risk, a follow-up story if practice needs it). The same rule
as a spike on the same VM: Outlook 42 / 386 / 453 ms, Excel 27 / 220 / 265 ms (50 × 10, 500 × 10,
50 × 100), every budget held. The implementation was measured after the session moved to a VM about
1.3 times slower (`s018_table_open_ms` about 80–95 instead of 62 ms, the paste without a table 33
instead of 25 ms), step 2 and step 3 in turn:

| Table | Step 2 | Step 3 | Budget (hard) |
|---|---|---|---|
| Outlook 50 × 10 | 68 ms | 57–59 ms | 50 ms |
| Outlook 500 × 10 | 541 ms | 487–523 ms | 500 ms |
| Outlook 50 × 100 | 598 ms | 532–536 ms | 500 ms |
| Excel 50 × 10 | 44 ms | 36–37 ms | 50 ms |
| Excel 500 × 10 | 344 ms | 278–283 ms | 500 ms |
| Excel 50 × 100 | 402 ms | 337–347 ms | 500 ms |

Step 3 takes 15–20 % off step 2. Whether the Outlook budgets hold on the CI runner shows its run;
on the earlier VM they held with a margin of about 1.2 (Outlook 50 × 10).

Margin of the hard budgets (review O8): Outlook 50 × 10 with one-word cells took 35 ms on the VM
(factor 1.4 to the budget) and 32 ms on the CI runner for 2178d93 (factor 1.55). Runner and VM
compared on the same code before and after the S018 sizing fix: `s018_table_open_ms` 158–160 / 96
ms = 1.65, then 60.2 / 61 ms = 1.0; Outlook 50 × 10 71.8 / 44 ms = 1.63, then 32.2 / 35 ms = 0.92;
Excel 50 × 10 59.1 / 31 ms = 1.91, then 23.1 / 23 ms = 1.0. Split into import and layout, the
runner was about 1.1 times slower in Qt's HTML import and about 2 times slower in `setMarkdown`
layout, which plain cells no longer take; cells that still take it can be that much slower on the
runner (an estimate from two dispatches). If an `s031_paste_*` hard budget trips in a pull
request, first re-run the failed jobs; if it trips again, the numbers go to Armin before any budget
changes.

A paste without a table (review O9, `s031_paste_no_table_5000_lines_ms`: 5,000 lines of code, three
spans per line, about 620 KB of HTML): 142.6 ms before the fixes, 26.9 ms after them (no import).
Copies made in hitokoto itself carry HTML that Qt generates from the selection when it is asked
for; a probe with the review fixes (lines of text with emphasis, code and a link, median of three):

| Copied in hitokoto | Generating `html()` | Scan | `Ctrl+V` of the copy | `Ctrl+V` of the same plain text |
|---|---|---|---|---|
| 5,000 lines | 41.8 ms | 1.0 ms | 76.7 ms | 37.4 ms |
| 50,000 lines | 447.5 ms | 14.1 ms | 818.2 ms | 285.8 ms |

Qt's data for a selection (`QTextEditMimeData`) makes HTML, Markdown, ODF and plain text all at the
first request, also when only the plain text is asked for; a marker alone, which skips the HTML,
gave 71.4 and 668.2 ms. hitokoto's own copy (`OwnCopy` in `editor_widget.cpp`, review (code) O9)
makes each format only when asked for and carries the marker; the same probe then measured:

| Copied in hitokoto | Generating `html()` | Scan | `Ctrl+V` of the copy | `Ctrl+V` of the same plain text |
|---|---|---|---|---|
| 5,000 lines | 11.4 ms | 0.7 ms | 32.2 ms | 32.4 ms |
| 50,000 lines | 125.9 ms | 9.0 ms | 255.2 ms | 255.8 ms |

### 2026-11-10, CI runner, S031 after the review (dispatch on 5a1e4d0)

Runner class `linux-intel-r-xeon-r-platinum-8573c-4cpu`, the two rounds of the pull request with
the old table sizes and sample-like cells:

| Table | Outlook markup | Excel markup | Budget (hard) |
|---|---|---|---|
| 50 × 10 | 49.3 / 48.0 ms | 34.2 / 34.2 ms | 50 ms |
| 500 × 10 | 441.7 / 450.2 ms | 259.2 / 267.1 ms | 500 ms |
| 50 × 100 | 502.1 / 492.2 ms | 308.1 / 316.9 ms | 500 ms |

Outlook 50 × 100 missed its budget in the first round, the Outlook keys kept only 1–4 % margin.
`s031_paste_no_table_5000_lines_ms` 29.3 / 28.2 ms; `s018_table_open_ms` 72 / 68 ms against
111 / 108 ms of main on the same runner (the plain-text layout of steps 2 and 3 of review O10).

Armin, 2026-11-10: the times stay, the tables shrink to 40 × 10 (400 cells, 50 ms), 400 × 10 and
40 × 100 (4,000 cells, 500 ms); the story's rule is now 400 cells in 50 ms, then at most 125 µs
per cell. From the run above (about 4 ms plus 88 µs per cell for Outlook 500 × 10, about 100 µs
per cell for 50 × 100) the runner should take about 39, 356 and 400 ms with Outlook's markup, a
margin of about 1.25. On the slower development VM of 2026-11-10 (median of five): Outlook
46.9 / 430.7 / 434.2 ms, Excel 33.3 / 227.0 / 276.0 ms (40 × 10, 400 × 10, 40 × 100).

### 2026-11-10, development VM, S031: where a paste's time goes, and HTML without styles

The CI run on 712f652 (AMD EPYC 7763) missed `s031_paste_outlook_40x10_ms` with 54.7–57.4 ms;
400 × 10 took 437 ms and 40 × 100 483 ms. A probe on the VM (outside the repository, median of 7)
found no fixed cost worth naming (a paste of 20 cells takes 7 ms in all); the time grows with the
cells. About half of it is Qt's HTML import of Outlook's markup, which holds about 1 KB per cell,
mostly inline CSS; the rest is embedding and showing the table, the same for both markups:

| Outlook markup | HTML | Qt's import | Ctrl+V handler | Until shown after it | Total |
|---|---|---|---|---|---|
| 20 cells | 91 KB | 1.0 ms | 1.4 ms | 5.6 ms | 7.0 ms |
| 400 cells | 401 KB | 20.2 ms | 20.7 ms | 23.9 ms | 44.6 ms |
| 4,000 cells | 3,350 KB | 227 ms | 231 ms | 148 ms | 379 ms |
| Excel markup, 400 cells | 90 KB | 6.1 ms | 7.5 ms | 23.4 ms | 30.9 ms |

Armin, 2026-11-10: the style attributes leave the HTML before the import
(`core::htmlWithoutStyles`); every sample gives the same Markdown, and Qt 6.12's import ignores
`display: none` anyway (inline, by class and on blocks). The import alone: Outlook 400 cells
17.9 → 8.9 ms, 4,000 cells 208 → 115 ms; Excel 4.6 → 3.3 and 65 → 47 ms. `hitokoto-bench` on the
same VM (median of five, before → after):

| Table | Outlook markup | Excel markup | Budget (hard) |
|---|---|---|---|
| 40 × 10 | 46.9 → 40.1 ms | 33.3 → 29.4 ms | 50 ms |
| 400 × 10 | 430.7 → 320.1 ms | 227.0 → 206.0 ms | 500 ms |
| 40 × 100 | 434.2 → 327.1 ms | 276.0 → 247.1 ms | 500 ms |

The CI run on 1096c0b landed on the slower runner class, AMD EPYC 7763 (pull request rounds 1 / 2):
Outlook 43.5 / 43.8 ms (40 × 10), 332.8 / 327.6 ms (400 × 10), 376.7 / 377.8 ms (40 × 100); Excel
38.4 / 38.7, 264.1 / 264.1, 312.4 / 314.0 ms; the paste without a table 28.5 / 28.8 ms. Against
the run on 712f652 on the same class (56.4, 437.5, 483.3 ms Outlook) that is 22–25 % less; the
smallest margin is now Outlook 40 × 10 with about 1.14.
The budgets follow one system when they change (Armin, 2026-11-10): the tables grow by a factor of
ten (400, 4,000 and 40,000 cells) and the times with them (50, 500 and 5,000 ms).

### 2026-11-09, CI runner, S029 revised (pull request #31, run #125)

The bench job of the revised S029 (base build and four rounds in every pull request run) took
7 min 58 s on an AMD EPYC 7763 runner (4 CPUs); the base was main 705c578, built with the same Qt.
All 86 compared keys were ok; the report noted the changed harness (`bench/hitokoto_bench.cpp`).

### 2026-11-08, CI runner, S029 gate (pull request #31)

The Linux benchmark job with the base build and four rounds (then the "main stage" of the first
version of S029, which also kept baselines per runner class) took 7 min 52 s, of which the build of
the pull request 57 s and the core benchmarks 9 s. The job's `timeout-minutes` is 30. The first run
(AMD EPYC 9V45, 4 CPUs) failed on `open_10mb_first_chunk_ms` alone: the round medians were 48.1 and
47.9 ms for the base and 64.6 and 64.6 ms for the pull request, and every round had single opens at
both levels, one frame apart. Since then a first-screen key needs 20 ms of difference (story S029,
rule "First screen of an opening").

### 2026-10-25, CI runners (run #18, pull request #5)

GitHub-hosted runners (ubuntu-24.04 with Qt 6.12, windows-2022 with Qt 6.8.3, macos-15 with
Qt 6.12), release build, offscreen, 10 runs. These are the baselines in `bench/baselines/`.

| Measurement | Linux | Windows | macOS | Budget |
|---|---|---|---|---|
| Cold start, median | 25 ms | 23 ms | 252 ms | < 1000 ms |
| Open 1 MB, fully loaded | 140 ms | 171 ms | 202 ms | < 500 ms |
| Open 10 MB, first screen editable | 65 ms | 79 ms | 79 ms | < 2–3 s |
| Open 10 MB, fully loaded | 1.7 s | 1.9 s | 3.3 s | |
| Key to paint, 10 MB file (median / p95) | 0.9 / 1.0 ms | 0.9 / 1.0 ms | 1.2 / 2.2 ms | ≤ 16 ms |
| Save 10 MB: UI thread blocked (duration of the save() call; S017 measures the longest stall instead) | 6 ms | 36 ms | 24 ms | imperceptible |

### 2026-11-07, development VM, S028 after the code review (release build, offscreen, Qt 6.12.0)

The same measurements as below, two new ones (code review O8, O9): Enter five times right after
the last letter of `ipsum`, before the count is complete (five runs), and the 1,000-row table of
S018 (`tableDocument(1000)`) with "Highlight all" on and the caret in the middle: 100 keys in the
field (`note with` typed and erased), then 200 times Enter in the table.

| Measurement | After the code review | Budget |
|---|---|---|
| Key in the find bar, median / p95 / max | 5.6 / 6.0–6.4 / 7.5–8.2 ms | ≤ 16 ms p95, 50 ms max (hard) |
| Until the current match | 4.6 ms | – |
| Until the full count | 1.17–1.27 s | informational: < 1 s on the CI runner |
| Next match, median / max | 2.7 / 3.4 ms | 50 ms max (hard) |
| Next match before the count is complete, median / max | 0.9 / 9.7 ms | 50 ms max (hard) |
| 1,000-row table, highlight all: key in the field, median / max | 5.1–5.7 / 17.2 ms | 50 ms max (hard) |
| 1,000-row table, highlight all: next match, median / max | 2.7–3.3 / 3.6–8.7 ms | 50 ms max (hard) |
| Typing in the document with highlight all, median / p95 / max | 0.9–1.1 / 1.7–2.0 / 5.0–6.0 ms | ≤ 16 ms p95, 50 ms max (hard) |
| After typing until the count is complete again | 11–12 ms | – |

Before the fixes, a step before the count was complete worked the pass for up to twice 30 ms (62 ms
in the reviewer's probe), and in a 4,000-row table with "Highlight all" one step took 206 ms,
because every lookup read the table from its header. Now a step works at most 8 ms, and a lookup
starts at its row. The first run of the table benchmark still showed 88 ms for a key: the table
that held the current match was built again (about 90 ms for 1,000 rows) each time the match left
it. A table that shows its source only for the find bar now keeps its view hidden and shows it again
as it was (`TableEmbedder::park`).

On the Linux CI runner (pull request #27, 2026-11-07, an ordinary runner: core decoding 20 ms at
3.25 GHz) every hard budget held: key in the find bar median / p95 / max 6.1 / 6.5 / 7.1 ms, next
match max 4.2 ms, before the count is complete max 9.9 ms, the 1,000-row table with "Highlight
all" 7.5 / 22.7 ms per key (median / max) and 4.9 ms max per step, typing in the document
1.0 / 1.3 / 8.7 ms. The full count took 1.5 s, above the informational target of 1 s. The
`s028_*` values in `bench/baselines/linux.json` (removed with S029) came from that run, added like
`s023_*`.

### 2026-11-07, development VM, S028 (release build, offscreen, Qt 6.12.0)

`s028_*` (story S028, review O4 and O6): the 10 MB document (`largeDocument`, 445,000 lines) with
the caret in the middle; 200 keys in the find bar's field (`dolor sit` typed and erased again,
each key a new search of the whole document), the search text `ipsum` typed five times until the
current match and until the full count, 200 times Enter (next match), then 200 keys typed in the
document with "Highlight all" on and the time until the count is complete again.

| Measurement | S028 | Budget |
|---|---|---|
| Key in the find bar, median / p95 / max | 6.0 / 6.6 / 14.9 ms | ≤ 16 ms p95, 50 ms max (hard) |
| Until the current match (pass ahead from the caret) | 4.6 ms | – |
| Until the full count (pass from the start, idle slices) | 1.1–1.4 s | informational: < 1 s on the CI runner |
| Next match, median / max | 2.6–3.4 / 5.0–9.2 ms | 50 ms max (hard) |
| Typing in the document with highlight all, median / p95 / max | 0.9–1.1 / 1.0–1.7 / 7.3–8.8 ms | ≤ 16 ms p95, 50 ms max (hard) |
| After typing until the count is complete again | 11–12 ms | – |

Three things made these numbers. First, the key budget: a key in the field searches for at most
8 ms (half of it for the pass ahead), the rest goes on in idle slices of 6 ms. Second, an edit
restarts the pass a little above the edited line and stops it where the block state and the
results meet the earlier ones; an edit while such a pass still runs goes on from where that pass
is, so typing never sets off a pass over the whole document (before: every key in the document
cost a 6-ms slice and the count settled only after 1.2–1.5 s). Third, the pass ahead from the
caret (review O6) uses the styler's block states once it has styled the document, so the current
match comes after 5 ms instead of after the pass from the start reached the middle (about 0.6 s).
Lines without any character of the search text are rendered for their state but get no units;
with `ipsum` in this document that saves little, every paragraph holds those letters.

### 2026-10-31, development VM, S024 (release build, offscreen, Qt 6.12.0)

`s024_*` (story S024, review O7 and O8, code review O1 to O3): the 10 MB document
(`largeDocument`) with 200 properties of every type in front of it; opening it until the
properties block shows (median of 5), 200 keys typed on the line directly below the closing line,
where opening puts the caret (a character and Backspace in turn; code review O1), Up into the
block and Down out of it (10 rounds); then two documents whose front matter has 5,000 top-level
keys or one nested value of 5,000 lines, and one with a block scalar of 2,000 lines of about 70
characters in front of 1 MB of text (code review O2): Up into the block, a character typed and
removed on its closing line, Down out of it (5 rounds), so that the block is read and built
again; in the last one also 200 keys typed below the block while the end of its value is on
screen. Five runs of the branch against five runs of main 3a17d89, and three runs of the state
before the code review (4aecf63) against three of the branch, on the same VM, `--runs 10`.

| Measurement | main 3a17d89 | S024 | Budget |
|---|---|---|---|
| Open 10 MB, first screen / styled | 67–73 ms / 3.76–4.13 s | 61–76 ms / 3.79–4.09 s | < 2–3 s first screen |
| Key to paint, 10 MB, median / p95 / p99 | 1.1–1.2 / 1.3–1.8 / 2.2–4.3 ms | 1.1–1.3 / 1.6–4.6 / 2.8–6.1 ms | ≤ 16 ms |
| Open 10 MB with 200 properties, until the block shows | – | 45–54 ms; the first screen shows it | < 2–3 s |
| Typing below the block, median / p95 / max | – | 2.3–2.5 / 2.6–4.6 / 2.8–8.5 ms | ≤ 16 ms p95, 50 ms max (hard) |
| Entering and leaving the block, median / max | – | 1.4–1.6 / 2.7–4.2 ms | 50 ms max (hard) |
| Leaving 5,000 edited keys, key max; until built | – | 9–18 ms; 105–126 ms | 50 ms max (hard) |
| Leaving a nested value of 5,000 lines, key max; until built | – | 20–34 ms; 19–21 ms | 50 ms max (hard) |
| Typing below a text value of 2,000 lines, p95 / max | – | 2.6–4.4 / 4.6–23.5 ms | ≤ 16 ms p95, 50 ms max (hard) |
| Leaving that value edited, key max; until built | – | 8–18 ms; 61–84 ms | 50 ms max (hard) |

Every other value stays within the spread of main. Key to paint in the 10 MB document without
front matter has the same median as main, but a higher p95 in some runs (up to 4.6 against 1.8
ms); the state before the code review shows the same spread (1.6–3.4 ms), and the difference stays
below the 2 ms from which the gate compares. After the merge of main 90a15b2 (S022), three runs of
the branch against three of that main: every value within the spread of main, also key to paint
in 10 MB (p95 1.7–3.7 against 1.8–3.2 ms); the `s024_*` values as in the table. The first
version left 5,000 edited keys in
about 110 ms: deleting 5,000 row widgets, laying out the old rows again before they were replaced,
showing a widget with 5,000 children, and two passes over the hidden lines (laid out by
`blockBoundingRect` when the view scrolled past them, rendered by the panels and decorations on
every repaint, about 60 and 35 ms; both fixed for all hidden lines, tests in S019). The rows are now
painted by one widget and are accessible through interfaces of their own. Reading 5,000 keys takes
about 4 ms (`parseProperties`), copying their lines out of the document about 2 ms; laying out the
rows about 30 µs each, in idle slices. A key or value that fits on one line is drawn as it is; a
longer one is laid out once per width (code review O2). Before that, a block scalar of 2,000
lines cost about 45 ms on every repaint near the block and 147 ms on leaving it (code review,
indicative). A new width or theme lays out all rows at once: about 40 ms for 5,000 keys, on a
resize, not on a key.

Reading the YAML is not sliced (code review O3): the key that leaves the block, or one idle slice
while the closing line is still being confirmed, copies and parses all of it. After editing a key
and leaving (separate probe, 3 rounds each): 5,000 keys 10 ms on the key and 8 ms in the longest
idle pass; 10,000 keys 14 and 13 ms; 20,000 keys 9 and 30 ms; 30,000 keys 20 and 64 ms; 50,000 keys
37 and 132 ms. From about 25,000 keys (some 500 KB of front matter) one pass takes more than 50 ms.
The `s024_*` values in `bench/baselines/linux.json` (removed with S029) came from the four CI runs
of pull request #24 (2026-10-31), the slowest value of each key, added like `s023_*`. The first run
alone was on a fast runner (core decoding 12 ms at 3.7 GHz against 20 ms at 3.25 GHz in the other
three; the older keys 10–45 % below their baseline), and the three runs after it failed
`s024_leave_5000_keys_built_ms` (63 → 83–84 ms) and `s024_leave_long_text_built_ms` (37 → 55 ms).
The three ordinary runs agree within a few per cent, except `s024_open_10mb_properties_ms` (31, 55
and 61 ms; 49 ms in the first run).

### 2026-10-30, development VM, S023 (release build, offscreen, Qt 6.12.0)

`s023_*` (review O9 of the story, O2 and O6 of the code review): a 10 MB document of sections with
a setext heading, twelve hard-wrapped lines, a thematic break, a list item continued lazily and two
more breaks, with one paragraph of 20,000 lines without a blank line in the middle. Opening it
(the cold walk, median of 5), 200 keys typed on a break (a character and Backspace in turn, so the
line changes between a break and paragraph text), `- ` typed before the long paragraph and removed
(10 rounds), and `-` typed on the blank line below it, which makes all 20,000 lines a setext
heading, and removed (10 rounds). Three runs of the branch against three runs of main e1d011e on
the same VM.

| Measurement | main e1d011e | S023 | Budget |
|---|---|---|---|
| Open 10 MB (`largeDocument`), styled | 3.24–3.57 s | 3.68–3.78 s | |
| Key to paint, 10 MB, median / p95 / p99 | 1.2–1.6 / 1.5–2.5 / 2.2–4.3 ms | 1.2–1.4 / 1.9–2.1 / 2.8–3.4 ms | ≤ 16 ms |
| Key to paint, 10 KB, p95 / p99 | 1.3–1.6 / 1.5–2.1 ms | 1.9–2.5 / 2.5–4.9 ms | ≤ 16 ms |
| Open the S023 document, first screen / styled | – | 58–61 ms / 3.1–3.3 s | < 2–3 s first screen |
| Typing on a break, median / p95 / max | – | 1.7–2.0 / 2.1–4.2 / 5.4–29.7 ms | ≤ 16 ms p95, 50 ms max (hard) |
| `- ` before the paragraph, keys median / max; restyled after | – | 4.0–4.4 / 4.9–14.6 ms; 47–51 ms | 50 ms max (hard) |
| Underline below the paragraph, keys median / max; restyled after | – | 2.6–3.0 / 4.2–4.7 ms; 134–140 ms | 50 ms max (hard) |

Styling all of a 10 MB file takes about 10 % longer than on main: every line is classified once more
for its paragraph link, and each paragraph is walked to its end once to find its setext level.
Key-to-paint in the 10 MB file stays in the range of main; in the 10 KB file p95 and p99 are up to 1
and 3 ms higher (the gate ignores differences under 2 ms). A first version drew the line of a break
in a pass of its own that rendered every visible line once more; drawn in the decorations pass,
typing in a separate probe of 1,000 keys per run was as fast as on main. A walk over a paragraph
costs about 0.3 µs per line; it runs for at most 2 ms on a key and goes on in idle slices, and a
changed level is passed up at most 2,048 lines on a key (the rest in idle slices). Before that bound
(code review O2), typing `-` under a paragraph of 150,000 lines took 37–44 ms per key on this VM,
and over 50 ms from about 230,000 lines. The `s023_*` values in `bench/baselines/linux.json`
(removed with S029) came from the CI run of pull request #22 (2026-10-30), added like
`s021_codeblock_*`. In that run `open_10mb_styled_ms` was 3.06 s against 2.60 s in the baseline (+18
%, inside the 25 % tolerance) and `typing_small_p95_ms` 1.43 ms against 0.93 ms (under the 2 ms the
comparison ignores).

### 2026-10-28, development VM, S021 (release build, offscreen, Qt 6.12.0)

A 10 MB document that is one fenced code block (about 134,000 lines), `s021_codeblock_*`:
200 caret moves in the middle of the block (Down, Up, End, Home, Right, Left), 40 times entering
and leaving it at either fence, 200 keys typed in it. Daniel's commit b00ea24 walked the whole
block on every caret move; the rework walks it once per block (review O2).

| Measurement | b00ea24 | rework | Budget |
|---|---|---|---|
| Caret step, median / p95 / max | 71.8 / 87.1 / 108.8 ms | 1.8 / 2.6–7.4 / 5.3–26.4 ms | ≤ 16 ms p95, 50 ms max (hard) |
| Entering the block, median / max | 8.5 / 134.6 ms | 2.8–6.0 / 6.5–8.6 ms | 50 ms max (hard) |
| Typing, median / p95 / max | 72.4 / 83.5 / 102.8 ms | 1.9 / 2.3–2.5 / 4.8–8.5 ms | ≤ 16 ms p95, 50 ms max (hard) |

A walk over cached block states costs about 150 ns per line here (about 20 ms for this block),
over states not computed yet about 0.9 µs; so the walk runs for at most 4 ms on a key and goes
on in idle slices. b00ea24 was measured once, merged with main 759a59d; the rework twice, on
main 7f1a163 (ranges). The VM swings between runs: against one run of main (tolerance 25 %),
two further runs of main itself failed the comparison (table typing p95 6.5 → 9.9 and 10.6 ms,
save stall 4.1 → 6.6 ms) and the rework failed it in the same values by the same amounts.

### 2026-10-27, development VM, S019 (release build, offscreen, 5 runs, Qt 6.12.0)

`QTextEdit` with `BlockLayout` (ADR 0003) against the `QPlainTextEdit` surface it replaced, same
machine and run.

| Measurement | QPlainTextEdit | QTextEdit + BlockLayout |
|---|---|---|
| Cold start, median | 34.8 ms | 34.6 ms |
| Open 1 MB, fully loaded | 165 ms | 179 ms |
| Open 10 MB, first screen / loaded / styled | 120 ms / 2.35 s / 4.0 s | 101 ms / 2.35 s / 3.6 s |
| Key to paint, 10 MB (median / p95 / p99 / max) | 1.3 / 2.4 / 3.6 / 41.0 ms | 1.2 / 3.5 / 5.2 / 5.3 ms |
| Save 10 MB, longest UI stall | 3.5 ms | 3.9 ms |

Two things decided the numbers. Restyling must not lay out lines nobody sees: a first version
laid out every restyled block and needed 17.6 s to style 10 MB; like Qt's own plain layout,
`BlockLayout` now drops the layouts and lays a block out when it is drawn or queried. And size
changes are reported once per styler slice, not per block (each report relayouts the scroll
bars). Enter rebuilds the prefix sums of all blocks, O(n) for 427,000 lines; whether that explains
the higher p95 and p99 is not measured yet. All values stay well inside the budgets.

### 2026-10-27, development VM, S019 follow-up (release build, offscreen)

CI on feature/tables: `typing_10mb_p99_ms` 1.00 -> 3.88 ms. Enter in the 10 MB file (427k
blocks) rebuilt `BlockLayout`'s prefix sums over every block, 2.5-3.5 ms per Enter here. With
`HeightIndex` (a gap at the last edit) Enter costs about 0.6 ms; the first Enter after the caret
jumps far moves the gap once (3.6 ms). `typing_10mb_p99_ms` over four runs: main 3.2-3.6 ms,
feature/tables 2.7-3.5 ms (one run with a 55 ms VM hiccup, six further runs below 8 ms).

### 2026-10-27, development VM, S018 (release build, offscreen)

Tables (`hitokoto-bench`): opening a 1000-row table until the table view shows it 139 ms (budget
500 ms); typing in a cell of a 200-row table, every 25th key Tab (writes the cell into its line
and opens the next), key to paint median / p95 / p99 / max 3.5 / 7.3 / 8.5 / 12.0 ms (budget
16 ms p95/p99, hard 50 ms max).

Opening a file adds blank lines below tables first (ADR 0002, decision 6), on the loading thread
before the first screen. A file without `|` skips it after a `memchr` over its bytes (about 1 ms
for 10 MB); looking in the decoded text instead had cost about 10 ms and failed the CI
regression check of `open_10mb_first_chunk_ms` (66 -> 87 ms, tolerance 25 %). A 10 MB file with a
table walks every line's block state once: 18-22 ms (one VM outlier at 56 ms).

### 2026-10-26, development VM, S017 (release build, offscreen, 5 runs)

Saving 10 MB: longest UI stall 6.7 ms (was 18.9 ms, measured as the duration of the save() call),
until on disk 190 ms (was 111 ms: the sliced copy costs about six times the CPU of a one-shot
copy, see "Known gaps").

### 2026-10-24, development VM (not reference hardware)

Cloud VM, 2 vCPU x86-64, 7 GiB RAM, no GPU, Ubuntu 24.04, Qt 6.12.0, release build,
`QT_QPA_PLATFORM=offscreen` unless noted. Indicative only; S011 measures on real machines.

| Measurement | Result | Budget |
|---|---|---|
| Cold start, exec to first frame (offscreen, median of 10) | 36 ms | < 1000 ms |
| Cold start, main() to first frame, X11 (Xvfb) | 36–52 ms | |
| Cold start, main() to first frame, native Wayland (weston headless) | 21–29 ms | |
| Open 1 MB, fully loaded | 179 ms | < 500 ms |
| Open 10 MB, first screen editable | 95 ms | < 2–3 s |
| Open 10 MB, fully loaded | 2.2 s | |
| Open 10 MB, styling finished in the background | 3.8 s | |
| Key to paint, small file (median / p95 / max) | 1.0 / 1.4 / 22 ms | ≤ 16 ms |
| Key to paint, 10 MB file (median / p95 / max) | 1.1 / 1.8 / 3.7 ms | ≤ 16 ms |
| Save 10 MB: UI thread blocked | 25 ms | imperceptible |
| Save 10 MB: total until written | 108 ms | |
| Binary size (stripped, Qt shared) | 310 KB | |

Core (Google Benchmark): decode 10 MB 24 ms, encode 10 MB 32 ms, render every line of 10 MB
21 ms, parse the built-in theme 8 µs, render one heading line 60 ns.

With S013–S015 (emphasis, fenced code, inline code; measured 2026-10-25 on the review VM, same
machine for both): render every line of 10 MB 33 ms (v0.1: 18 ms), render one heading line
91 ns (v0.1: 50 ns). Inline elements run only on lines containing their trigger characters;
without that they cost 61 ms and 184 ns.

Per-block metadata (2026-10-25, review VM, Qt 6.12, debug build): every line of the 10 MB
document from S007 used to carry a 32-byte `BlockData` (427,204 lines, 13.0 MB of about 300 MB
heap). Now only lines that leave a code block open carry one, and in files with mixed line
endings only lines whose ending differs from the majority: none in that document, and 61,029 of
488,233 lines (1.9 MB) in a variant where every eighth line ends in CRLF.

### Known gaps and next steps

- **Saving 10 MB used to block the UI for about 25 ms** (copying the text out of
  `QTextDocument` in one go). S017 copies in slices of about 128 K characters with the editor
  editable: longest stall 4–5 ms on the development VM (Debug build), snapshot complete after
  about 160 ms. The only public range copy, `QTextCursor::selectedText()`, costs about six times
  `toRawText()` per character (10 MB: 124 ms against 21 ms; `QTextBlock::text()` per block
  226 ms; `QTextDocumentFragment::toRawText()` 1.6 s), so the sliced copy spends more CPU in
  total, spread so that nothing stalls.
- **Loading and background styling of 10 MB are dominated by Qt** (inserting text into
  `QTextDocument` and relayouting styled blocks), not by the core (21 ms for all lines). The editor
  is usable after 95 ms, but read-only until the whole file is inserted (2.2 s here).
- **After typing, the styler's catch-up stalls the event loop for about 17 ms** (development VM,
  release build, seen as the longest gap between 1 ms timer ticks right after the 200 keystrokes
  of the typing benchmark on the 10 MB file). Key-to-paint latency does not include it, because
  the styling is deferred. Not measured before S017's save benchmark waited for the styler;
  worth a look in S007's remaining work.
- Measurements on reference hardware with a real display (S011), including cold-cache starts.
