---
mode: code
spec: docs/stories/037-windows-msi.md
commits: 4fa38e7..9bb2e1d (bb3f07d red, 492c1ac green, 13ec2bd/9c96dcb, 4d0ff02/491968f and 9bb2e1d fixes of the dry runs)
reviewer: Advocatus Diaboli (Claude, in the session that wrote the code; not an independent
  subagent)
date: 2026-11-13
objections:
  - id: O1
    category: failure
    severity: major
    title: The dialogs never ran; every check installs without them
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: failure
    severity: minor
    title: Completing a release with "Re-run failed jobs" is untested
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: failure
    severity: minor
    title: A download of windows-* that matches nothing may stop a release that needs no MSI
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: failure
    severity: minor
    title: No lint keeps the check of one version running when another version's build failed
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: operational
    severity: minor
    title: The file list pins the C++ runtime of the image's Visual Studio
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: design
    severity: minor
    title: The Windows logic lives in jobs named after Debian
    disposition: pending
    disposition_rationale: null
---

# Review (code) of S037 — Every version gets a Windows installer (MSI)

Examined: the commits `4fa38e7..9bb2e1d` on `agents/s037-windows-msi` (pull request 45), and the
fourth dry run of "Packaging" on that pull request, green according to Armin (2026-11-13): build
and 416 tests of 0.5.1 on `windows-2022`, the 25 files of `files.txt`, `wix build`,
`wix msi validate` with ICE57 accepted, and `install-check.ps1` per user and per machine.

## O1 - failure - major

`install-check.ps1` runs every `msiexec` with `/qn` (line 46), so the user interface of
`hitokoto.wxs` never ran: the licence page with its checkbox, the scope dialog shown only with
`Privileged` and preselecting "Everyone", the `Publish` events that set `MSIINSTALLPERUSER` from the
choice, and the UAC prompt of a per-machine installation started without elevation. Decision 6 and
the scenarios "the licence is accepted" and "for everyone, from the command line" (its double-click
part) rest on exactly these, and the story's notes leave open whether an administrator under UAC
gets `Privileged` in the dialogs. A mistake there (a dialog that does not advance, a per-machine
choice that installs per user, a preselection that does not depend on the rights) would pass the
dry run and every release. The MSI of the dry run is at hand: the artifact `windows-0.5.1` of the
run on pull request 45 (kept for 7 days).

## O2 - failure - minor

Decision 2 and its scenario complete a version with "Re-run failed jobs": the failed package job
and the release job run again, and the release job takes the other package from the first attempt.
That attempts share their artifacts is known only from another repository (pull request 129 of
`dsb-norge/github-actions-terraform`, 2026-10-21); the path ran nowhere here, and a dry run cannot
reach it (the release job runs only on main). A re-run also runs the dependent `windows-check` of
versions that had passed, which upload `windows-<version>` again; `download-artifact` then keeps one
artifact per name. If the re-run does not find the first attempt's package, the next run of
"Packaging" or a manual run with "publish" still catch the version up.

## O3 - failure - minor

The release job downloads `pattern: windows-*` whenever the Windows plan built something
(`packaging.yml` line 688). If every Windows check failed while another version is due without an
MSI (skipped for Windows, decision 2), the pattern matches nothing. Whether `download-artifact@v8`
fails then is not checked; if it does, the job stops before the release step, and the version that
needs no MSI is not released either. The release step already checks every file it needs, so the
download could be allowed to find nothing.

## O4 - failure - minor

`windows-check` needs `windows-package` and carries `!cancelled() && needs.debian-plan.result ==
'success'` (line 667), so that a failed build of one version does not skip the checks of the others
(decision 2). The lint `S037_OneWindowsVersionDoesNotStopTheOthers` checks fail-fast and the matrix,
not this condition; without `!cancelled()` the default `success()` would skip every check, and no
version would get its MSI, without a test noticing.

## O5 - operational - minor

`files.txt` lists the DLLs of `Microsoft.VC143.CRT` of the image's Visual Studio (14.44.35112, with
`vcruntime140_threads.dll`). An image update that adds or drops a DLL there makes every MSI red until
the list follows. The red is intended (decision 5), and the build prints the installed list, but the
README says nothing about this case.

## O6 - design - minor

The Windows plan is a step of the job `debian-plan`, and the release job, which now releases both
packages, is still `debian-release`. The ids stay because the S036 lints and the history use them,
but a reader of `packaging.yml` looking for the Windows plan or for the release finds them under
Debian's names.

## Explicitly not objecting to

- The file list as the check of what ships: the dry run matched it exactly, and a change shows.
- Accepting ICE57 for the Start menu entry: in a package for both scopes the key path and the folder
  follow the scope; the reason is in the recipe.
- Running the tests again in the Windows build: the packaged build is the tested one (decision 4),
  at about 2 minutes 20 seconds for 416 tests.
- Calling `build-package.ps1` in the step's own session with splatting: the empty ICE list is no
  longer lost (second dry run).
- Accepting WiX's EULA with `wix eula accept` before the first command: required by WiX 7 (first
  dry run).
