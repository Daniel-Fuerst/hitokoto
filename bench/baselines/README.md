# Benchmark baselines

Since S029 (ADR 0009) the Linux job has no baseline file. Its baselines are samples of runs of
`main` (push, the nightly run, `workflow_dispatch` on `main`) that CI adds to the branch
`bench-data`, one file per run, by runner class:

- `linux/<class>/<start time>-<commit>-<run>-<attempt>.json`: the context of the run (class, model
  name, CPU count, L2 and L3 sizes, clock, commit, run, event, start time) and its `values`, the
  results of `hitokoto-bench`.
- `linux/accepted/<start time>-<commit>.json`: an acceptance marker, written for a merged pull
  request that carried the label `bench-accepted` or for `workflow_dispatch` on `main` with
  `accept_drift`.

The runner class is `linux-<model>-<n>cpu-l2-<L2>-l3-<L3>` from `/proc/cpuinfo`, `nproc` and the
cache sizes in sysfs (`hitokoto-bench-gate --print-class`). How a run is judged
(`hitokoto-bench-gate`, rules in `docs/stories/029-bench-runner-classes.md`):

- **Class stage.** A class with 3 or more samples is established; its baseline is the median per
  key of its newest 5 samples. A key fails when it is more than 25 % and more than 2 ms slower
  (20 ms for the first screen of an opening, whose samples jump by one frame on one runner).
- **Main stage.** If the class is not established, or a key fails the class stage, the job builds
  the base commit and measures base, pull request, pull request, base on the same runner. A key
  fails only if the mean of the pull request's rounds is more than 25 % and 2 ms above the base's
  and its faster round is slower than the base's slower round.
- **Label.** `bench-accepted` on a pull request accepts failed comparisons (read at run time, so a
  re-run after adding it is enough). Hard budgets still fail.
- **Drift.** Each class has an anchor, the median of its oldest 3 samples after the newest
  acceptance marker. A run of `main` whose class baseline is more than 25 % and 2 ms above the
  anchor fails; pull requests list the drift for information. Find the cause, or accept the drift
  with `workflow_dispatch` on `main` (`accept_drift`); the anchor then forms anew.

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

Nothing here is edited by hand: deleting `bench-data` resets every class to "not established",
which is safe (the main stage decides until new samples come in).

`windows.json` and `macos.json` keep values of the measurement before 2026-10-28, when the
Windows and macOS benchmarks were switched off (their runners swing by 20-100 % between runs with
the same code). They are not used; when those jobs come back, the scheme of S029 is meant to
follow them.
