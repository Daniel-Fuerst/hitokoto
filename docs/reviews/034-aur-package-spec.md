---
mode: spec
spec: docs/stories/034-aur-package.md
reviewed: 2026-11-12
reviewer: Claude (Advocatus Diaboli, same session)
objections:
  - id: O1
    category: failure
    severity: major
    title: The first publication is not specified
    disposition: resolved
    disposition_rationale: The package exists and is Daniel's (Armin, 2026-11-12); decision 2 now
      makes a run without a readable pkgver red instead of creating a package; scenario "no
      package is created" and ctest cases added.
  - id: O2
    category: operational
    severity: minor
    title: Merging the story publishes, and the publish path is first run live
    disposition: documented
    disposition_rationale: Notes say that the environment must exist before the merge and that the
      first run after the merge publishes v0.5.0 and is to be watched.
  - id: O3
    category: threat
    severity: minor
    title: The AUR key and the package build share one job
    disposition: fixed
    disposition_rationale: Decisions 4 and 5 split a build job without the environment from a
      publish job that builds and runs nothing.
  - id: O4
    category: design
    severity: minor
    title: Decision 9 promises a manual route that decision 2 forbids
    disposition: fixed
    disposition_rationale: The manual route is dropped; a recipe change alone reaches the AUR with
      the next version (decisions 3 and 11).
  - id: O5
    category: failure
    severity: minor
    title: Two runs can race on the AUR push
    disposition: fixed
    disposition_rationale: Decision 5, concurrency group without cancelling and a second read of
      the AUR before the push; scenario "two runs in quick succession".
  - id: O6
    category: failure
    severity: minor
    title: The checksum of GitHub's tag archive is only stable by policy
    disposition: fixed
    disposition_rationale: Decision 9 names the dependency and adds a warning on every run when the
      AUR's checksum no longer matches the archive.
  - id: O7
    category: design
    severity: minor
    title: "\"The commit the Version run built\" is not what a workflow_run checkout gives"
    disposition: fixed
    disposition_rationale: Decision 1 checks out head_sha; a manual run uses its own commit.
  - id: O8
    category: failure
    severity: minor
    title: aarch64 is declared but never built
    disposition: fixed
    disposition_rationale: Decision 10, arch=('x86_64').
---

# S034 — The AUR package follows every version: objections (spec)

Story as first drafted on 2026-11-12 on `packaging/aur-update-on-new-version` (after the merge of
main bbc61ab), together with `packaging/archlinux/PKGBUILD`, `hitokoto.desktop` and
`.github/workflows/packaging.yml` at 877ef80. The decision numbers below are those of that draft;
the dispositions refer to the story as revised.

## O1 - failure - major

**The first publication is not specified.** Decision 2 compares the highest tag with "the `pkgver`
the AUR package has now (from its `.SRCINFO`)", and every scenario starts from an AUR package that
exists. Whether `hitokoto` is in the AUR today is not recorded anywhere in the repository (the
review could not check it: aur.archlinux.org refuses the review environment). If it is not, the
anonymous clone yields an empty repository without `.SRCINFO`, and the story does not say whether
that means "publish" or "fail". Pushing to `ssh://aur@aur.archlinux.org/hitokoto.git` creates the
package, so the first run decides who maintains the package base.
*Wanted:* a decision and a scenario for "the AUR has no package `hitokoto` yet", and a `ctest` case
for an empty `.SRCINFO`; the story should state whether the package exists and under whose account.

**Disposition: resolved.** The package exists and is maintained by Daniel (Armin, 2026-11-12).
Decision 2 now makes a run red when the AUR repository has no readable `.SRCINFO` with a `pkgver`,
so the job never creates a package; scenario "no package is created" and the `ctest` cases cover it.

## O2 - operational - minor

**Merging the story publishes, and the publish path is first run live.** `workflow_run` workflows
run from the default branch, so before the merge only the dry run (decision 6) can be exercised.
The pull request carrying this story changes no program code, so it is `version:none`; its merge
still starts "Version", then "Packaging", which then publishes `v0.5.0` at once if the AUR is
below it (decision 2). The ssh, push and environment steps thus run for the first time unwatched,
and if the environment `aur` does not exist yet the run is red.
*Wanted:* say in the story that the environment must exist before the merge, and that the first
publication is the run after the merge (or a manual run with "publish"), to be watched.

**Disposition: documented.** The notes of the story say both.

## O3 - threat - minor

**The AUR key and the package build share one job.** Decisions 4 and 7 put the build (`makepkg`
running CMake and the compiler over a downloaded archive, with packages fetched by `pacman`) and
the push with the SSH key into the same container. Writing the key after the build narrows but
does not close the window: anything the build left running or changed in the container (a
`known_hosts`, a git config, a wrapper on `PATH`) is still there when the key is used.
*Wanted:* two jobs: the build job without the environment `aur` hands `PKGBUILD`, `.SRCINFO` and
`hitokoto.desktop` on as an artifact; the publish job, the only one with the environment, does
nothing but clone, copy, commit and push.

**Disposition: fixed.** Decisions 4 and 5 of the revised story.

## O4 - design - minor

**Decision 9 promises a manual route that decision 2 forbids.** Decision 9 says a recipe-only
change reaches the AUR "by a manual run with 'publish' once the story's follow-up decides how
`pkgrel` is raised", but decision 2 publishes only a tag higher than the AUR's `pkgver`, also in a
manual run, and decision 3 fixes `pkgrel=1`.
*Wanted:* either drop the manual route from decision 9 (recipe changes wait for the next version)
or define it now (an input `pkgrel` that allows the same version with a higher `pkgrel`).

**Disposition: fixed.** The manual route is dropped; a recipe change alone reaches the AUR with
the next version (decisions 3 and 11 of the revised story).

## O5 - failure - minor

**Two runs can race on the AUR push.** Two merges in quick succession start two runs of "Version"
(the tag job serialises them with `concurrency: version-tag`), and so two runs of "Packaging".
Both may read the same AUR state and try to push; the second fails non-fast-forward and is red,
or pushes an older version after the newer if it read the state before the first pushed.
*Wanted:* a `concurrency` group for the publish job without cancelling, and the publish job reads
the AUR state again right before it pushes and publishes nothing if the AUR is no longer below.

**Disposition: fixed.** Decision 5 of the revised story and the scenario "two runs in quick
succession".

## O6 - failure - minor

**The checksum of GitHub's tag archive is only stable by policy.** Decision 3 pins the SHA-256 of
`archive/refs/tags/vX.Y.Z.tar.gz`. GitHub changed those archives once (January 2023) and reverted;
it now promises six months' notice before a change. If the archive changes, `makepkg` fails for
every user of the published version, and the job does not notice because it acts only on new
versions.
*Wanted:* name the dependency on GitHub's archive in the story (known limit), and let every run,
also without a new version, compare the AUR's checksum with the current archive and warn.

**Disposition: fixed.** Decision 9 of the revised story names the limit and adds the comparison
with a warning on every run.

## O7 - design - minor

**"The commit the Version run built" is not what a workflow_run checkout gives.** Decisions 2
and 3 take tags and the recipe from the commit the "Version" run built
(`github.event.workflow_run.head_sha`). `actions/checkout` in a `workflow_run` workflow checks out
the newest commit of the default branch unless `ref` is set; with merges in quick succession that
is a later commit, so the tag decision and the recipe can come from different commits.
*Wanted:* state that the checkout uses `head_sha` (and that a manual run uses the commit it was
started on).

**Disposition: fixed.** Decision 1 of the revised story.

## O8 - failure - minor

**aarch64 is declared but never built.** The `PKGBUILD` declares `arch=('x86_64' 'aarch64')`;
decision 4 builds only in `archlinux:base-devel` on an x86-64 runner. The AUR is an x86-64
repository (Arch Linux ARM is a separate project), so `aarch64` is a promise nobody checks.
*Wanted:* either `arch=('x86_64')` or a sentence that `aarch64` is untested.

**Disposition: fixed.** Decision 10 of the revised story, `arch=('x86_64')`.

## Explicitly not objecting to

- `workflow_run` after "Version" as the trigger: the tag job pushes with `GITHUB_TOKEN`, so tag
  pushes start no workflow (decision 1 and `unrefined.md`).
- The AUR as the state that is caught up (decision 2): it removes O4 of the code review without
  touching `version.yml`, and it makes missed runs harmless.
- The decision logic as a CMake script with `ctest` cases, as in S030; it keeps the workflow thin
  and testable without a runner.
- One recipe in the repository and `.SRCINFO` generated, not kept (decision 3).
- Pinned AUR host keys and the key in an environment restricted to main (decision 7).
- `Exec=hitokoto %f` (decision 8): `main.cpp` opens only the first positional argument.
- Size S: the build in the container and the script are small; most of the work is in the CI runs.
