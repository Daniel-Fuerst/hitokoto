---
story: S030
mode: spec
spec: docs/stories/030-automatic-versions.md
also_read: docs/adr/0010-versions-from-pull-request-labels.md, .github/workflows/ci.yml, CMakeLists.txt, src/app/CMakeLists.txt, docs/stories/unrefined.md
date: 2026-11-08
dispositioned: 2026-11-08 (Armin; O1 open until Armin and Daniel decide, O6 documented by Armin's choice, the others as proposed by Claude, before confirmation of the story)
reviewer: Advocatus Diaboli (same session as the author; no independent subagent available in this chat)
objections:
  - id: O1
    category: premise
    severity: minor
    title: "Every pull request into main makes a version, also those that change nothing users get"
    disposition: pending
    disposition_rationale: "Open until Armin and Daniel decide whether every pull request into main makes a version or a fourth label version:none exists (story, open question 1)."
  - id: O2
    category: design
    severity: major
    title: "Which commits the tag job tags is ambiguous: an associated pull request is not the same as the merge of a pull request into main"
    disposition: fixed
    disposition_rationale: "Story, requirement 'Workflow' and scenario 'only merges of pull requests into main make versions': candidates are the commits on main's first-parent line after the last version tag; one counts only if a pull request with base main has it as its merge_commit_sha; direct pushes and merges reached through a second parent (like e75458c) get no version. ADR 0010, mechanics."
  - id: O3
    category: failure
    severity: major
    title: "No rule for later merges while an earlier merge lacks its label"
    disposition: fixed
    disposition_rationale: "Story, requirement 'Workflow' and scenario 'a later merge waits for an earlier merge without a label': versions follow merge order; the tag job stops at the first merge without exactly one label, is red, names its pull request and tags nothing after it; after the label is set a re-run tags them in order. ADR 0010, mechanics and consequences."
  - id: O4
    category: failure
    severity: major
    title: "Without tags in the checkout the tag job would restart the numbering"
    disposition: fixed
    disposition_rationale: "Story, requirement 'Checkouts' and scenario 'no version tag, no new version': the tag job checks out with fetch-depth 0 (history and tags); NextVersion.cmake fails when no version tag is reachable instead of assuming v0.0.0. ADR 0010, mechanics."
  - id: O5
    category: failure
    severity: minor
    title: "The version is computed at configure time and goes stale in incremental builds"
    disposition: documented
    disposition_rationale: "Story, decision 4, scenario 'the build reports the version' (configured three commits later) and scenario 'documented': the version is that of the last configure; CI and package builds always configure afresh. A build step running git describe on every build is left until a case needs it. ADR 0010, consequences."
  - id: O6
    category: threat
    severity: minor
    title: "A version tag set by hand becomes the base of every later version"
    disposition: documented
    disposition_rationale: "Armin, 2026-11-08: documented. Story, decision 5 and scenario 'documented': no v* tag by hand apart from the starting tag v0.3.0. ADR 0010, consequences: nothing enforces it; a tag rule in the repository settings could later."
  - id: O7
    category: failure
    severity: minor
    title: "macOS bundle versions are equal for all builds between two tags"
    disposition: documented
    disposition_rationale: "Story, requirement 'macOS bundle versions', and ADR 0010, consequences: bundle versions carry the numeric core; harmless for builds by hand; S012 distributes tagged builds only."
  - id: O8
    category: operational
    severity: minor
    title: "The label is checked for presence, not for fitting the final content"
    disposition: documented
    disposition_rationale: "Story, scenario 'documented': CLAUDE.md and README.md say that the label is part of the review before the merge and must fit the final content. ADR 0010, consequences."
---

# Objection record: S030 (spec)

Read: the story, ADR 0010, `.github/workflows/ci.yml`, `CMakeLists.txt`, `src/app/CMakeLists.txt`,
the entry "Versioning, and packages and installers for every version" in `unrefined.md`, and the
first-parent history of main up to 705c578. Note on independence: written in the same session that
drafted the story; a second pass by an independent reviewer before the confirmation is advisable.

## O1 - premise - minor

Decision 1 makes every pull request into main a version. Main's history shows pull requests that
users would not see as a version: #19 and #20 both came from `agents/s014-fences-in-block`
(51f95fb, e1d011e) and would make two versions for one feature; pull requests that change only
stories or CI would make versions too. The story keeps this as open question 1, but the scenarios
and ADR 0010 assume it is settled. Possible remedy: decide before the confirmation; if not every
pull request should count, a fourth label `version:none` with the same check.

## O2 - design - major

The requirement says the tag job "finds the pull request of each untagged merge commit through the
GitHub API". For a commit on the default branch, the endpoint for the pull requests of a commit
returns the merged pull request that brought the commit into the repository, whatever its base.
Since PR #26, main contains e75458c, the merge of PR #30 into `feature/smooth_scrolling`; walking
all ancestors since the last tag would find it with PR #30 and could tag it. The story does not say which commits are candidates. Possible remedy: candidates
are only the commits on main's first-parent line since the last version tag; a candidate counts
only if a pull request with base `main` has it as its merge commit (`merge_commit_sha`); direct
pushes on that line (such as 083b03e) are skipped. A scenario with e75458c-like history.

## O3 - failure - major

The scenarios cover one merge without a label and quick merges with labels, not both together.
If merge A has no label and merge B after it has one, the story leaves open whether B is tagged:
tagging B first and A later would give A a higher version than B although it came first; skipping A
for good loses a version. Possible remedy: versions follow merge order; the job stops at the first
merge without exactly one label, tags nothing after it, is red and names that pull request; after
the label is set, a re-run tags A and B in order. Scenario for it.

## O4 - failure - major

`actions/checkout` fetches one commit without tags by default (every job in `ci.yml` uses it that
way). The requirement for checkouts names "jobs whose artefacts carry the version", not the tag job
itself. A tag job without tags finds no base and, by the rule "highest reachable tag", would start
over at `v0.0.1` on main. Possible remedy: the tag job checks out with full history and tags;
`NextVersion.cmake` fails when no version tag is reachable, instead of assuming `v0.0.0` (the
starting tag `v0.3.0` exists from decision 5 on). Scenario: no reachable version tag → red, no tag.

## O5 - failure - minor

Decision 4 computes the version at configure time. Incremental builds that do not reconfigure
(Visual Studio in folder mode, `cmake --build` after a pull) keep the old string: a build said
"0.4.0+3.gabc1234" although it contains later commits. The scenario "three commits later" silently
assumes a reconfigure. Possible remedy: a build step that runs `git describe` on every build and
rewrites a generated header only when the string changes (no rebuild otherwise), or the story
states that the version is that of the last configure. The cost of the build step is a `git` call
per build.

## O6 - threat - minor

Anyone with write access can push a tag by hand, as `v0.3.0-tulip` shows. A hand-set tag of the
exact form (say `v1.0.0` on main) would become the base of every later version without anyone
noticing. The story says which tags count, not who may set them. Possible remedy: the tag job
accepts as base only annotated tags whose tagger is the workflow, apart from the starting tag
`v0.3.0`, and is red otherwise; or a tag rule in the repository settings (whether GitHub Actions can
then still create tags must be checked first).

## O7 - failure - minor

The scenario "the build reports the version" sets `CFBundleShortVersionString` and
`CFBundleVersion` to the numeric core, so every build between `v0.4.0` and `v0.4.1` carries `0.4.0`
on macOS. Launch Services and installers compare `CFBundleVersion`, so different builds cannot be
told apart there. For hand builds this is harmless; for a `.dmg` from S012 it matters. Possible
remedy: accept for now and note it for S012, where builds between tags may not be distributed at all.

## O8 - operational - minor

The check only enforces that one label is set when the pull request is opened. Pull requests grow:
PR #26 brought, besides smooth scrolling, `VERSION 0.3.0` and a rework of S026 (debe80e, outside its
scope). A pull request labelled `version:patch` at the start can end with a new feature. Possible
remedy: the label is part of the review before the merge (one line in `CLAUDE.md` and in the
pull request template, if one is added), not only a check.

## Explicitly not objecting to

- The tag as the only source of the version, without bump commits: it keeps main free of release
  commits and fits merge commits.
- A workflow separate from `ci.yml`: `ci.yml` cancels in-progress runs per ref, which would lose
  versions.
- CMake scripts instead of a new language or a third-party action: CMake is already required,
  `string(JSON)` exists since CMake 3.19 (the project requires 3.25), and the rules become testable
  on all three platforms.
- `0.0.0+unknown` with a warning for builds without git, and `-DHITOKOTO_VERSION` for source archives.
- Major resting while the version is 0.x.
