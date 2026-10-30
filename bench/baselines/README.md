# Benchmark baselines

CI compares `hitokoto-bench` results with `<os>.json` in this folder (`linux`, `windows`,
`macos`) and fails if a value is more than 25 % slower on Linux or 50 % on Windows and macOS
(differences under 2 ms are ignored). The wider tolerance is for the runners, not the code: the
same commit ran 20-50 % slower across every value on a second Windows and macOS runner
(2026-10-26); a baseline is one sample of a runner's speed. If this keeps failing, the
comparison should normalise by the run's overall slowdown instead.
`save_10mb_total_ms` is compared and reported (`info`) but never fails the comparison: it waits
for the file to reach the runner's disk and swings too much on shared CI machines. Two hard
budgets hold on every machine instead and fail the run without a baseline: the longest UI stall
while saving 10 MB (`save_10mb_ui_blocked_ms`, 16 ms) and the time until the 10 MB file is on
disk (`save_10mb_total_ms`, 1000 ms). Both are the median of `--runs` saves (S017). Likewise
the maximum of the 200 key-to-paint samples (`typing_*_max_ms`, `s018_table_typing_max_ms`) is `info` in the comparison, one
scheduler hiccup on a shared runner would fail it, and has a hard budget of 50 ms instead (S007:
no single event above 50 ms); `typing_*_p99_ms`, the maximum after dropping the worst 1 %, is
compared with the baseline. `typing_*_over_16ms` and `s018_table_typing_over_16ms` count the samples above the 16 ms budget, for
information. `startup_p95_ms`, the p95 of 10 cold starts, is `info` as well (31.6 -> 127.2 ms on
a Windows runner with the median unchanged); `startup_median_ms` is the gate. The rule: medians
and percentiles of the 200 typing samples are compared, extremes of few samples are reported,
and where the brief names an absolute limit, a hard budget applies. Opening (`open_*`,
`s018_table_open_ms`) is the median of 5 opens, each from an empty document (S018: one open of
10 MB gave a first screen after 51 and 89 ms in two runs without a code change). The first
screen (`open_*_first_chunk_ms`) is painted and timed in the `firstChunkShown` signal itself:
waiting for it with `QTest::qWaitFor` had added the wait's polling and further slices of text
inserted meanwhile, 15-40 ms that swung between runs. A runner that is slower as a whole still
fails the comparison; the baselines changed with this measurement and come from the first run
with it.

The baselines come from the review run of the S017 pull request (2026-10-27; Windows and macOS from
single re-runs of their jobs); Linux since 2026-10-28 from the run on `e5ae913` of
`feature/tables`, the first with the opening medians timed in the signal, which also added the
`s018_*` values; the `s021_codeblock_*` values (moving, entering and typing in a 10 MB code block)
were added on their own from the CI run of pull request #19 (2026-10-28), the other values kept.
The `s023_*` values (a 10 MB document of breaks, setext headings and long hard-wrapped
paragraphs: opening, typing on a break, `- ` before and an underline below a 20,000-line
paragraph) were added the same way from the CI run of pull request #22 (2026-10-30); their
maxima and the times until restyling are `info` only, the keys have a hard budget of 50 ms.
Since 2026-10-28 the Windows and macOS benchmarks are off until further notice (user decision,
`ci.yml`): their runners swing too much for a gate, and a comparison that only reports is read by
nobody. `windows.json` and `macos.json` keep values of the earlier measurement and need new ones
from the first run after the jobs come back. Until then the hard budgets (saving, key events) are
not checked on Windows and macOS either, nor the code that only runs there (`ReplaceFileW`,
`F_FULLFSYNC`), nor the `s021_codeblock_*` values. CI runners differ from the development machine,
so baselines must come from CI itself. To create or update them:

1. Open a successful CI run (on `main` or on a pull request; both use the same runners), download
   the artefacts `bench-linux`, `bench-windows`, `bench-macos`.
2. Copy each `e2e-bench.json` here as `linux.json`, `windows.json`, `macos.json`.
3. Commit them with a message that names the run, e.g. "Update benchmark baselines from CI run 123".

Only update baselines on purpose (a known, accepted change), never to make a red build green.
