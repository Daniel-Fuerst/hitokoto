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
