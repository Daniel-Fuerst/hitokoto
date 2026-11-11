# Arch Linux Packaging for hitokoto

This directory contains the Arch Linux packaging files (`PKGBUILD`, `hitokoto.desktop`, and `.SRCINFO`) for [hitokoto](https://github.com/Daniel-Fuerst/hitokoto).

## Building and Installing Locally

To build and install the package on Arch Linux or derivatives (such as EndeavourOS, Manjaro, or Garuda):

```sh
cd packaging/archlinux
makepkg -si
```

This compiles `hitokoto`, installs the executable, desktop entry (`hitokoto.desktop`), scalable icon, and license, and registers the package with `pacman`.

## Desktop Integration

The package installs:
- Executable: `/usr/bin/hitokoto`
- Desktop entry: `/usr/share/applications/hitokoto.desktop` (MIME types: `text/markdown`, `text/x-markdown`)
- Application icon: `/usr/share/icons/hicolor/scalable/apps/hitokoto.svg`
- License: `/usr/share/licenses/hitokoto/LICENSE`

## Updating the Package

When a new release tag (e.g. `v0.5.0`) is published:

1. Update `pkgver` in `PKGBUILD`.
2. Update source checksums:
   ```sh
   updpkgsums
   ```
3. Regenerate `.SRCINFO`:
   ```sh
   makepkg --printsrcinfo > .SRCINFO
   ```
