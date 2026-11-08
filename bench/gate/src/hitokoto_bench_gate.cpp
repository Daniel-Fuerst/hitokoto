// SPDX-License-Identifier: GPL-3.0-or-later
// S029: hitokoto-bench-gate decides from files whether the Linux benchmark job is green (exit 0),
// fails (1) or needs the main stage (3). Stub for the red tests.
//
//   hitokoto-bench-gate --kind pr|main --class <runner class> --results e2e-bench.json
//                       --data <checkout of bench-data> [--base-rounds a.json,b.json]
//                       [--pr-rounds a.json,b.json] [--changed-paths paths.txt] [--accepted]
//                       [--report report.md]
#include <QCoreApplication>

#include <cstdio>

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);
    std::fprintf(stderr, "hitokoto-bench-gate: not implemented yet\n");
    return 2;
}
