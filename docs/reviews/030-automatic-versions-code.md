---
story: S030
title: "Objection record: S030 a defined version step for every merge into main (code)"
mode: code
spec: docs/stories/030-automatic-versions.md
branch: agents/s030-versioning
commits_examined: "d56b118..5dad514 (7b275d1 scripts under scripts/version .. 5dad514 pull request 32 labelled version:patch); code: scripts/version/, CMakeLists.txt, src/app/, .github/workflows/version.yml, tests/version/"
date: 2026-11-09
reviewer: Advocatus Diaboli (Claude, in the session of the implementation)
objections:
  - id: O1
    category: failure
    severity: major
    title: "An empty pull request lookup counts as a commit pushed directly: the merge stays without a version and the job is green"
    disposition: pending
    disposition_rationale: null
  - id: O2
    category: failure
    severity: minor
    title: "The GitHub side of the tag job runs for the first time on the merge of this pull request"
    disposition: pending
    disposition_rationale: null
  - id: O3
    category: threat
    severity: minor
    title: "The check Version label runs the pull request's own copy of NextVersion.cmake"
    disposition: pending
    disposition_rationale: null
  - id: O4
    category: operational
    severity: minor
    title: "HITOKOTO_VERSION stays in the cache and silently wins over git in every later configure"
    disposition: pending
    disposition_rationale: null
  - id: O5
    category: failure
    severity: minor
    title: "Git environment variables redirect the version and the throwaway repository of the test to another repository"
    disposition: pending
    disposition_rationale: null
  - id: O6
    category: failure
    severity: minor
    title: "CPACK_PACKAGE_VERSION with build metadata does not fit the Windows installer generators of S012"
    disposition: pending
    disposition_rationale: null
  - id: O7
    category: operational
    severity: minor
    title: "Documentation says .dirty is appended; on a tagged commit the version is 0.4.0+dirty"
    disposition: pending
    disposition_rationale: null
  - id: O8
    category: failure
    severity: minor
    title: "On Windows no test reads the version the running program reports"
    disposition: pending
    disposition_rationale: null
---

# Objection record: S030 a defined version step for every merge into main (code)

Mode `code`. Examined: `scripts/version/HitokotoVersion.cmake` and `NextVersion.cmake`, the wiring
in `CMakeLists.txt` and `src/app/` (`hitokoto_version.hpp.in`, `main.cpp`), the workflow
`.github/workflows/version.yml`, and the `ctest` cases in `tests/version/`, against the story and
ADR 0010. Evidence beside the code: the 27 cases `S030_*` (all red on 02a1c79, all green on
5dad514; dev with Qt 6.12, ci with Qt 6.12 and 6.8.3, 334/334 each), a local run of the tag job's
script against a bare repository with a stand-in for `gh` (first run tags only the merge of
pull request 32; a second run changes nothing; a merge without a label stops the job and is tagged
after the label is set), PR runs on 5dad514 ("Version label" red without a label, green with
`version:patch`; tag job skipped in pull request runs), and the source of go-gh v2.13.0
(`pkg/jq/jq.go` compiles with `gojq.WithEnvironLoader`, so `env.SHA` works in `gh api --jq`).

## O1 - failure - major

**An empty pull request lookup counts as a commit pushed directly: the merge stays without a version
and the job is green.**

The tag job asks `repos/$REPO/commits/$sha/pulls` for every candidate (`version.yml:71-76`). An
empty answer writes only `commit <sha>`, and `NextVersion.cmake` reports "no merge of a pull
request into main, no version" and goes on (`decide_candidate`). The job cannot tell "pushed
directly" from "the lookup found nothing": the run starts seconds after the merge, and whether
GitHub has associated the merge commit with its pull request by then is not checked (the endpoint
is documented as listing the merged pull request that introduced the commit; how soon after the
merge it does is not documented, and this was not measured). If the answer is empty, the merge of a
labelled pull request gets no tag, and the job is green. The next merge's run picks
it up again only because the range starts at the last tag; until then the build of main reports
`v<last>+1.g…`, and nobody is told. Since the ruleset forbids direct pushes, every first-parent
commit on main after `v0.3.0` is a merge of a pull request (0cf5e28, 0286646), and the merge commit
names its pull request in its message ("Merge pull request #31 from …"). A cross-check would close
the gap: take the number from the message, read that pull request (`pulls/<n>`) and require
`merge_commit_sha` and base `main`; a commit whose message names a pull request that the lookup
does not confirm fails the job instead of being skipped. The local simulation could not show the
timing, because the stand-in answers at once.

## O2 - failure - minor

**The GitHub side of the tag job runs for the first time on the merge of this pull request.**

What the simulation replaced is untested: the answer of `commits/{sha}/pulls` for merge commits of
this repository, `--paginate` with `--jq`, `pull-requests: read` for a private repository, and
`git push origin refs/tags/…` with the credentials that `actions/checkout@v7` keeps outside
`.git/config` (`version.yml:43-90`). A workflow with `workflow_dispatch` can be started only once
it is on the default branch, so there is no run before the merge. A failure is repairable (the job
is red, nothing is tagged, a re-run catches up), but a wrong answer of the lookup would put a tag
on the wrong commit, and tags are never moved (ADR 0010). The first run after the merge has to be
checked by hand: tags `v0.3.1` on the merge of pull request 32 only, none on 0cf5e28 and 0286646.

## O3 - threat - minor

**The check Version label runs the pull request's own copy of `NextVersion.cmake`.**

The label job checks out the merge ref of the pull request (`version.yml:28`, default of
`actions/checkout` for `pull_request`) and runs `scripts/version/NextVersion.cmake` from it
(`version.yml:34`). A pull request that changes the script, by mistake or on purpose, changes its
own required check: a script that always succeeds lets the pull request through without a label.
The tag job runs the script from main (`ref: main`, `version.yml:51`) and would still stop after the
merge, so the damage is a red tag job instead of a red check, and only collaborators open pull
requests. Checking out the base (`pull_request.base.sha`) would close it, but would make the check
of this pull request fail, because main has no script yet.

## O4 - operational - minor

**`HITOKOTO_VERSION` stays in the cache and silently wins over git in every later configure.**

`HitokotoVersion.cmake:116-121` declares the option as a cache string and passes it as `OPTION`;
a non-empty value skips git entirely (`:65-70`). Configured once with `-DHITOKOTO_VERSION=0.4.0` in
a checkout (to try a package build, say), the build directory reports `0.4.0` after every later pull
and commit, and the status line `hitokoto version 0.4.0` does not say where the value came from.
The README only says builds without git take the option. Naming the source in the status line
(`from HITOKOTO_VERSION` or `from git describe`) and in the README would make it visible.

## O5 - failure - minor

**Git environment variables redirect the version and the throwaway repository of the test to
another repository.**

Both the configure step (`HitokotoVersion.cmake:88-92`) and the case `S030_GitRepository`
(`s030_version_cases.cmake:357-387`) call `git -C <dir>`. `-C` changes the directory, but
`GIT_DIR` and `GIT_WORK_TREE` in the environment win over discovery, as they do in git hooks and
in some IDE or script setups. Then configure describes the repository named by `GIT_DIR`, and the
test runs `init`, `add`, `commit` and `tag -a v0.3.0` against it: a `ctest` started from a hook
would commit `a.txt` and set version tags in the developer's real repository. Clearing `GIT_DIR`,
`GIT_WORK_TREE`, `GIT_INDEX_FILE` and `GIT_CEILING_DIRECTORIES` in the test, and passing
`--git-dir`/`--work-tree` or clearing them for the describe call, would make both independent of
the caller's environment.

## O6 - failure - minor

**`CPACK_PACKAGE_VERSION` with build metadata does not fit the Windows installer generators of
S012.**

`CMakeLists.txt:64` sets `CPACK_PACKAGE_VERSION` to the full version (`0.4.0+3.gabc1234`), as the
story asks. DEB and TGZ accept it. The Windows installers that S012 names (CPack WIX, INNOSETUP,
see hitokoto-Wissen, "Windows-Installer: Bausteine für S012") need a numeric version for the
product version and the version resource; a `+` breaks or truncates them. S012 then has to set the
generator-specific version from `HITOKOTO_VERSION_CORE`. Nothing fails today; the point is that S012
inherits it without a note.

## O7 - operational - minor

**Documentation says `.dirty` is appended; on a tagged commit the version is `0.4.0+dirty`.**

README (line 67) and the story (decision 4, scenario "the build reports the version") say that
uncommitted changes append `.dirty`. On a tagged commit there is no build metadata to append to;
`hitokoto_version_from_describe` gives `0.4.0+dirty` (`S030_DescribeDirty`, `S030_GitRepository`).
That is valid SemVer and sorts after `0.4.0` in Debian, but a reader of the README expects
`0.4.0.dirty`, which is not a SemVer build at all. One sentence in the README and the story would
match the behaviour.

## O8 - failure - minor

**On Windows no test reads the version the running program reports.**

`S030_TheAppReportsTheVersion` runs `hitokoto --version` and compares the output, but not on
Windows (`tests/version/CMakeLists.txt:50`), because a GUI program shows the version in a message
box there. On Windows only `S030_TheAppIsBuiltWithTheVersion` remains, which reads the generated
header, not the program. A change that leaves `main.cpp` without the header on Windows only (an
`#ifdef`, a resource) would go unnoticed. The rest of the wiring is the same on all platforms, so
the gap is small.

## Explicitly not objecting to

- **Scripts in CMake instead of a script language.** ADR 0010 and the brief (section 3) want no new
  language; the scripts are short, and the cases run on all three CI platforms.
- **Tests that compare the build with the value its own configure computed**
  (`S030_TheConfiguredVersion`, `S030_CPackCarriesTheFullVersion`,
  `S030_TheAppIsBuiltWithTheVersion`). They check the wiring; the derivation itself is tested with
  fixed `git describe` outputs and with a real repository (`S030_GitRepository`).
- **`git describe` takes the nearest tag, `NextVersion.cmake` the highest.** All version tags sit on
  main's first-parent line in merge order, so both give the same tag; a tag elsewhere would be one
  set by hand, which ADR 0010 rules out.
- **The skipped tag job in every pull request run.** Both jobs live in one workflow file as the
  story asks; the skipped entry costs nothing.
- **`workflow_dispatch` runs the tag job with `contents: write` from any branch.** Collaborators
  can push tags anyway; ADR 0010 records a tag rule in the repository settings as a later option.
- **The CMake warning `0.0.0+unknown` in every CI test job with a shallow checkout.** The story
  accepts it for jobs whose artefacts carry no version.
