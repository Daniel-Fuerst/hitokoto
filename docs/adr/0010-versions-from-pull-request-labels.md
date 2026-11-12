# ADR 0010: Versions from pull request labels, tagged by CI

- **Status:** Accepted (Armin and Daniel, 2026-11-09, with the confirmation of S030); proposed by
  Claude on 2026-11-08
- **Date:** 2026-11-08
- **Deciders:** project owner (Armin, Daniel)

## Context

Until now the version is set by hand in `project()` in `CMakeLists.txt` (0.1.0 since S000, 0.3.0
since debe80e) and was tagged once by hand (`v0.3.0-tulip`, a lightweight tag on a commit that still
says 0.1.0). Armin wants versions set automatically, every pull request merged into main being a new
version unless it changes nothing in the built program (2026-11-09, with Daniel), so that builds and
later packages can be told apart and updated (S030, `unrefined.md`). The project merges every pull
request into main with a merge commit after green CI; Daniel and Armin open and merge pull requests,
Claude sessions work on `agents/…` branches. Commits on the branches follow no common message
format: Daniel uses Conventional Commit prefixes (`feat:`, `refactor:`), Armin and the Claude
sessions do not.

## Options

1. **A tag by hand.** A maintainer tags main when a version is due; CMake reads the tag. Simple, no
   tooling, but not automatic and easily forgotten.
2. **One label per pull request** (`version:major`, `version:minor`, `version:patch`,
   `version:none`), enforced by a check; after the merge a workflow tags the merge commit.
   Automatic; the level is decided consciously when the pull request is opened or reviewed; commit
   messages stay free.
3. **The pull request title in Conventional Commit form** (`feat: …`, `fix: …`, `feat!: …`). As
   explicit as a label, but the title carries format and content at once and needs a format check.
4. **All commits in Conventional Commit form, with release-please or semantic-release.** Widely
   used; release-please also writes a changelog. With merge commits every commit of the branch
   counts, so one `feat:` anywhere makes a minor version; it needs discipline in every commit by
   everyone, and adds a third-party action (Node) to the release path.
5. **GitVersion** (maintained, 6.7.0 in March 2026). Computes the version from branches, tags and
   merge messages and supports models such as GitFlow; a .NET tool with its own configuration,
   more than a main-plus-short-branches flow needs.

## Decision

Option 2.

**Meaning of the levels**
- `version:none`: no functional change; only documentation (stories, notes, reviews, README,
  `docs/`) or infrastructure (CI, workflows, build scripts, tests, tooling), and the program behaves
  the same. Anything that ships in the program (`src/`, resources such as themes and icons) is at
  least `version:patch`, even without a visible change. The merge gets no tag; builds after it
  carry the distance to the last version.
- `version:patch`: fixes, follow-up work and refactoring of what ships, and build changes that
  change the built program; nothing new for users.
- `version:minor`: something new for users, typically a finished story, or changed behaviour that
  stays compatible.
- `version:major`: incompatible for users, such as settings or theme files that no longer load or
  removed behaviour, or deliberately the step to 1.0. While the version starts with 0, major is not
  used and incompatible changes are minor (SemVer item 4 lets anything change while the major
  version is 0).

**Mechanics**
- The annotated tag `vMAJOR.MINOR.PATCH` on the merge commit is the only source of the version;
  nothing in the repository is changed for a version. Only tags of exactly that form count.
- CMake derives the version at configure time with `git describe`: `X.Y.Z` on a tagged commit,
  otherwise `X.Y.Z+N.gHASH`, with `.dirty` for uncommitted changes. Without git:
  `-DHITOKOTO_VERSION`, else `0.0.0+unknown` with a warning. `project()` gets the numeric core,
  the full version goes to `QApplication::applicationVersion` and CPack.
- The rules (parsing, choosing the base tag, the next version) are CMake scripts run with
  `cmake -P`, tested by ctest on fixed inputs. No new language and no new dependency (brief,
  section 3); the workflow uses only `git`, `gh` and CMake, which the GitHub runners provide.
- A workflow `version.yml`, separate from `ci.yml`: a label check on pull requests into main and a
  tag job on push to main. The tag job walks main's first-parent line since the last version tag,
  oldest first, and tags each commit that is the `merge_commit_sha` of a pull request with base
  `main`, unless that pull request is labelled `version:none`; commits pushed directly and merges
  into other branches (reached through a second parent) get no version. A run that GitHub cancels
  while pending is caught up by the next. Versions follow merge order: at the first merge without
  exactly one version label the job stops and is red, and tags nothing after it until the label is
  set and the job re-run.
- The tag job checks out the full history with tags; without a reachable version tag it fails
  instead of starting over. It pushes with `GITHUB_TOKEN`; such a push starts no other workflow, so
  builds for a new version run in the same workflow after the tag job or through
  `workflow_dispatch`.

## Consequences

- Changes reach main only through pull requests (a ruleset for main, set by the owner); notes and
  status changes that used to be pushed directly take a pull request with `version:none`.
- Every merge into main makes a version unless labelled `version:none`, also small ones; version
  numbers rise quickly. A code change labelled `version:none` by mistake gets no version; the
  review before the merge has to catch it.
- A forgotten or wrong label shows as a red check before the merge (if required) or a red tag job
  after it; a wrong level cannot be undone once tagged (a tag is never moved), only the next pull
  request can correct the course.
- The label is checked for presence only; whether it fits the final content is part of the review
  before the merge.
- One merge without a label holds back the versions of all later merges until it is labelled.
- Commits pushed to main directly make no version; their builds carry the distance to the last one.
- Builds from a shallow checkout report `0.0.0+unknown`; jobs whose artefacts carry the version
  fetch the full history.
- The version is that of the last configure; an incremental build without a reconfigure keeps the
  old string.
- A version tag set by hand would become the base of all later versions; nobody sets one apart from
  the starting tag `v0.3.0`. Nothing enforces this; a tag rule in the repository settings could
  later.
- macOS bundle versions carry only the numeric core, so builds between two tags look the same
  there; only tagged builds are distributed (S012).
- No changelog is written; GitHub can generate release notes from pull request titles later.
- Daniel's habit of setting `VERSION` and tags by hand ends; `CLAUDE.md` and `README.md` say so.

## Reconsider if

- A changelog becomes important: release-please (option 4) writes one, at the price of Conventional
  Commits for every commit.
- More people contribute and labels are forgotten often: option 3 with a title check, or a default.
- Versions should come from something other than merges, such as release candidates or several
  supported release lines; then GitVersion or release branches.

## Amendment: the tag only after a green CI on main (S035, 2026-11-12)

Approved by Armin in conversation on 2026-11-12, after the tag job had tagged merges while their CI
on main was still running (and could have ended red).

- The tag job no longer runs on a push to main. `ci.yml` calls `version.yml` (`workflow_call`) as
  its last job "Version", which needs every other job of the CI, benchmarks included, and runs
  unless one of them failed or was cancelled. The tag job checks out the commit of that CI run and
  tags merges up to it. `version.yml` keeps the label check on pull requests; its manual trigger is
  gone.
- A red CI on main gives no tag until "Re-run failed jobs" turns the run green; the job "Version"
  then runs in that run. A merge whose CI stays red is tagged by the next green run (catch-up),
  whose commit contains it. A missing label still stops the tag job; after setting it, the CI run's
  failed jobs are re-run.
- CI runs on main are never cancelled (one concurrency group per push); pull requests still cancel
  their older runs.
- Builds for a new version follow the CI by `workflow_run` on "CI" (`packaging.yml`, S034), not the
  tag job: a run started by `workflow_run` belongs to the newest commit of main, so only one such
  step keeps the commit the CI passed (`github.event.workflow_run.head_sha`).
