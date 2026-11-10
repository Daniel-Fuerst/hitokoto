# Benchmark baselines

Since S029 (ADR 0009) the Linux job keeps no baseline. Every pull request run builds the base
commit (the first parent of the merge commit CI tests) and measures it alternating with the pull
request on the same runner, in four rounds: base, pull request, pull request, base. How a run is
judged (`hitokoto-bench-gate`, rules in `docs/stories/029-bench-runner-classes.md`):

- **Comparison.** A key fails only if the mean of the pull request's two rounds is more than 25 %
  and more than 2 ms above the base's (20 ms for the first screen of an opening, whose single opens
  jump by one frame on one runner) and its faster round is slower than the base's slower round.
  The report lists every key that is not ok with the four rounds and both means.
- **Label.** `bench-accepted` on a pull request accepts failed comparisons (read at run time, so a
  re-run after adding it is enough). Hard budgets still fail.
- **Hard budgets.** Those of the pull request's rounds fail the job; those of the base's rounds are
  listed for information. A hard budget that a pull request adds is new while the base does not
  measure it (neither base round holds the key): the base's rounds count without it, the pull
  request's rounds must hold it and meet its limit, and the report lists it as on one side only.
  A story with a new hard budget therefore needs no change to the gate, only the key in
  `hardBudgets()`, in `kHardKeys` of `tests/bench/test_s029_runner_classes.cpp` and in the
  fixtures under `tests/bench/data/` (all rounds).
- **Toolchain.** The base is built from its own checkout, so its `CMakePresets.json`, `cmake/` and
  fetched test libraries are its own. If the Qt version of the bench job differs, the base is built
  with its own Qt; a new runner image affects both sides and is named in the report. A toolchain
  change may come only with changes to `docs/` and `*.md` files, otherwise the job fails.
- **Runs of `main`** measure once and check the hard budgets.

The runner class (`linux-<model>-<n>cpu-l2-<L2>-l3-<L3>`, `hitokoto-bench-gate --print-class`) is
reported for information. Baselines per runner class, recorded by CI, were the first version of
S029; they are kept as a deferred option in ADR 0009.

Which keys fail and which only inform: medians and percentiles of the 200 typing samples are
compared; extremes of few samples are reported (`typing_*_max_ms`, `s018_table_typing_max_ms` and
the other maxima, `startup_p95_ms`, the counts `*_over_16ms`, the settle times, and
`save_10mb_total_ms`, which waits for the runner's disk). Where the brief names an absolute limit,
a hard budget holds on every machine: the longest UI stall while saving 10 MB
(`save_10mb_ui_blocked_ms`, 16 ms), the 10 MB file on disk within 1 s (`save_10mb_total_ms`), and no
single key event, caret move or step above 50 ms. The lists live in the gate library
(`bench/gate/`) and `hitokoto-bench` uses them too. Opening (`open_*`, `s018_table_open_ms`) is the
median of 5 opens, each from an empty document; the first screen (`open_*_first_chunk_ms`) is
timed in the `firstChunkShown` signal itself.

`windows.json` and `macos.json` keep values of the measurement before 2026-10-28, when the
Windows and macOS benchmarks were switched off (their runners swing by 20-100 % between runs with
the same code). They are not used; when those jobs come back, the scheme of S029 is meant to
follow them.
