# Benchmark baselines

CI compares `hitokoto-bench` results with `<os>.json` in this folder (`linux`, `windows`,
`macos`) and fails if a value is more than 25 % slower (differences under 2 ms are ignored).

There are no baselines yet: CI runners differ from the development machine, so the first ones
must come from CI itself. To create or update them:

1. Open a successful CI run (on `main` or on a pull request; both use the same runners), download
   the artefacts `bench-linux`, `bench-windows`, `bench-macos`.
2. Copy each `e2e-bench.json` here as `linux.json`, `windows.json`, `macos.json`.
3. Commit them with a message that names the run, e.g. "Update benchmark baselines from CI run 123".

Only update baselines on purpose (a known, accepted change), never to make a red build green.
