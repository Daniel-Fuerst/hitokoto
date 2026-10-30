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

CI runs both on Linux, Windows and macOS and compares with `bench/baselines/<os>.json`
(tolerance 25 % on Linux, 50 % on Windows and macOS whose runners vary that much; differences
under 2 ms ignored). Hard budgets fail the run on any machine:
saving 10 MB stalls the UI for at most 16 ms, the file is on disk within 1 s, and no key event
takes longer than 50 ms. Maxima of many samples (`typing_*_max_ms`, `save_10mb_total_ms`) are
compared with the baseline for information only; p99 of the key-to-paint samples is the compared
value. See `bench/baselines/README.md`.

## Measurements

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

Styling all of a 10 MB file takes about 10 % longer than on main: every line is classified once
more for its paragraph link, and each paragraph is walked to its end once to find its setext
level. Key-to-paint in the 10 MB file stays in the range of main; in the 10 KB file p95 and p99 are
up to 1 and 3 ms higher (the gate ignores differences under 2 ms). A first version drew the line of
a break in a pass of its own that rendered every visible line once more; drawn in the decorations
pass, typing in a separate probe of 1,000 keys per run was as fast as on main. A walk over a
paragraph costs about 0.3 µs per line; it runs for at most 2 ms on a key and goes on in idle
slices, and a changed level is passed up at most 2,048 lines on a key (the rest in idle slices).
Before that bound (code review O2), typing `-` under a paragraph of 150,000 lines took 37–44 ms per
key on this VM, and over 50 ms from about 230,000 lines. The `s023_*` values are not in
`bench/baselines/linux.json` yet; like `s021_codeblock_*`, they are added from the first CI run of
the pull request, and until then only their hard budgets apply.

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
