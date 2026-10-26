# Benchmark baselines

CI compares `hitokoto-bench` results with `<os>.json` in this folder (`linux`, `windows`,
`macos`) and fails if a value is more than 25 % slower (differences under 2 ms are ignored).
`save_10mb_total_ms` is compared and reported (`info`) but never fails the comparison: it waits
for the file to reach the runner's disk and swings too much on shared CI machines. Two hard
budgets hold on every machine instead and fail the run without a baseline: the longest UI stall
while saving 10 MB (`save_10mb_ui_blocked_ms`, 16 ms) and the time until the 10 MB file is on
disk (`save_10mb_total_ms`, 1000 ms). Both are the median of `--runs` saves (S017).

The baselines come from CI run #18 (pull request #5). CI runners differ from the development
machine, so baselines must come from CI itself. To create or update them:

1. Open a successful CI run (on `main` or on a pull request; both use the same runners), download
   the artefacts `bench-linux`, `bench-windows`, `bench-macos`.
2. Copy each `e2e-bench.json` here as `linux.json`, `windows.json`, `macos.json`.
3. Commit them with a message that names the run, e.g. "Update benchmark baselines from CI run 123".

Only update baselines on purpose (a known, accepted change), never to make a red build green.
