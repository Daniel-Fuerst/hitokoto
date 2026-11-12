# Arch Linux packaging for hitokoto

This directory holds the recipe of the AUR package
[`hitokoto`](https://aur.archlinux.org/packages/hitokoto): `PKGBUILD` and `hitokoto.desktop`.
The AUR holds only the recipe; users build hitokoto themselves with `makepkg` or an AUR helper.

## Building and installing locally

On Arch Linux or a derivative (EndeavourOS, Manjaro, Garuda):

```sh
cd packaging/archlinux
makepkg -si
```

This builds `hitokoto` from the release archive of the version in `pkgver` and installs the
executable, the desktop entry, the scalable icon and the licence:

- Executable: `/usr/bin/hitokoto`
- Desktop entry: `/usr/share/applications/hitokoto.desktop` (MIME types `text/markdown`,
  `text/x-markdown`; one file per start, `%f`)
- Application icon: `/usr/share/icons/hicolor/scalable/apps/hitokoto.svg`
- Licence: `/usr/share/licenses/hitokoto/LICENSE`

In this directory the checksum of the archive is `SKIP`, so that a local build works for any
`pkgver`; the AUR always gets the real checksum.

## How the AUR is updated (S034)

The workflow `.github/workflows/packaging.yml` runs after every successful run of "Version" on
main. It compares the highest version tag `vMAJOR.MINOR.PATCH` with the `pkgver` in the AUR. If the
tag is higher, it

1. fills in this `PKGBUILD`: `pkgver` from the tag, `pkgrel=1` and the SHA-256 of GitHub's archive
   of the tag (`scripts/packaging/AurPackage.cmake`), and generates `.SRCINFO`;
2. builds and installs the package in `archlinux:base-devel` as an unprivileged user, validates the
   desktop entry and checks that `hitokoto --version` prints the version;
3. pushes `PKGBUILD`, `.SRCINFO` and `hitokoto.desktop` to the AUR with the message
   `Update to X.Y.Z`.

A merge without a new version (`version:none`) pushes nothing; a missed run is caught up by the
next one; the AUR is never downgraded, and the workflow never creates the package. A change to
this recipe alone reaches the AUR with the next version.

A pull request that changes this directory, `scripts/packaging` or the workflow runs everything up
to the push (dry run), and so does a manual run of "Packaging" with "publish" off. The job summary
shows the plan and the diff against the AUR.

When `hitokoto.desktop` changes, its checksum in `PKGBUILD` has to change with it; the test
`S034_TheDesktopEntryChecksumMatches` checks that.

## Setting up the push (once)

In the repository settings, an environment `aur` with the deployment branch `main`, holding:

- the secret `AUR_SSH_PRIVATE_KEY`: the private key of a key pair only for this workflow, without
  a passphrase, whose public key is in the AUR account that maintains the package;
- the variables `AUR_NAME` and `AUR_EMAIL`: the author of the AUR commits.
