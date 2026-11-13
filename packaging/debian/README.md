# Debian package for trixie

Every version of hitokoto from 0.5.1 on gets a GitHub release with a Debian package for trixie
(amd64), built against Debian's own Qt (S036). This directory holds the recipe:

- `control`: the fields of the package; `@VERSION@`, `@DEPENDS@` and `@INSTALLED_SIZE@` are filled
  in by `scripts/packaging/DebianPackage.cmake`. `Depends` is what `dpkg-shlibdeps` finds, plus
  `qt6-qpa-plugins` and `qt6-svg-plugins`, which no library links.
- `copyright`: in the machine-readable format, referring to `/usr/share/common-licenses/GPL-3`.
- `versions.txt`: the first version that gets a package (`first`), the first version in the
  changelog (`changelog-from`), and versions skipped for good (`skip X.Y.Z <reason>`).
- `build-depends`: the Debian packages the build needs.
- `build-package.sh`: builds, tests and assembles the package of one version.
- `install-check.sh`: installs, starts and purges it in a fresh container.

The desktop entry is the one of the AUR recipe, `packaging/archlinux/hitokoto.desktop`, so that
both packages carry the same file.

The package installs:

- Executable: `/usr/bin/hitokoto`
- Desktop entry: `/usr/share/applications/hitokoto.desktop`
- Application icon: `/usr/share/icons/hicolor/scalable/apps/hitokoto.svg`
- `/usr/share/doc/hitokoto/copyright` and `changelog.Debian.gz`

## Installing

Download `hitokoto_X.Y.Z-1_amd64.deb` from the release and check it against `SHA256SUMS`:

```sh
sha256sum -c SHA256SUMS
sudo apt install ./hitokoto_X.Y.Z-1_amd64.deb
```

To fetch the newest package automatically (for example into a local apt repository), ask GitHub
for the latest release; no token is needed:

```sh
curl -fsSL https://api.github.com/repos/Daniel-Fuerst/hitokoto/releases/latest |
  jq -r '.assets[] | select(.name | endswith("_amd64.deb") or . == "SHA256SUMS") | .browser_download_url'
```

The tag name (`.tag_name`, `vX.Y.Z`) says whether there is something new. Only the highest
version is marked as latest; a version released later to catch up never is.

## Building locally

In a trixie container, from the root of a checkout with the tags (Docker or Podman):

```sh
v=0.5.1
mkdir -p debian-work/source debian-work/out
git archive "v$v" | tar -x -C debian-work/source
cmake -DMODE=changelog -DREPO_DIR=. -DVERSION=$v -DVERSIONS_FILE=packaging/debian/versions.txt \
      -DCONTROL_FILE=packaging/debian/control -DOUTPUT=debian-work/changelog \
      -DNOTES=debian-work/out/notes-$v.md -P scripts/packaging/DebianPackage.cmake
docker run --rm -e VERSION=$v -v "$PWD:/w" -w /w debian:trixie bash -euo pipefail -c '
  apt-get update -q
  grep -v "^#" packaging/debian/build-depends | xargs apt-get install -y -q --no-install-recommends
  bash packaging/debian/build-package.sh debian-work/source "$VERSION" debian-work/changelog debian-work/out'
docker run --rm -v "$PWD:/w" -w /w debian:trixie \
  bash packaging/debian/install-check.sh debian-work/out/hitokoto_${v}-1_amd64.deb $v
```

The build runs the tests of that version with platform `offscreen`; a failing test stops it.
`debian-work/` is not part of the repository.

## How the releases are made (S036)

The workflow `.github/workflows/packaging.yml` runs after every successful run of "CI" on main,
beside the AUR jobs and independent of them:

1. `debian-plan` compares the version tags (from `first` on, without the skipped ones) with the
   published releases. A version without a release is to be released; a `version:none` merge
   releases nothing, and a missed run is caught up by the next one.
2. `debian-package`, one job per version: generates the changelog (from 0.3.0 on, one entry per
   version tag, the merged pull requests with number and title) and the release notes from Git,
   builds and tests the source of the tag in `debian:trixie` with this recipe, assembles the
   package, runs `lintian` (no error allowed), then installs, starts and purges the package in a
   fresh `debian:trixie`. A failing version leaves the others alone.
3. `debian-release`, the only job that may write: for each checked package it deletes a draft of
   the tag left by a failed run, creates the release as a draft with the package and
   `SHA256SUMS`, and publishes it. A published release is never changed.

A pull request that changes this directory, `scripts/packaging` or the workflow, and a manual run
of "Packaging" with "publish" off, build and check the package of the highest version and release
nothing. A manual run with "publish" on main releases what is missing.

If a version can never be built or tested with Debian's Qt, add `skip X.Y.Z <reason>` to
`versions.txt`; the next run leaves it out and names it in the job summary. A change to this recipe
reaches the next released version; released packages stay as they are.
