# hitokoto

**hitokoto** (一言, "a single word") is a fast, native Markdown editor for one file at a time,
with Obsidian-style live preview: the document is always plain Markdown, rendered as you type,
and the raw syntax shows only on the line where your cursor is.

![hitokoto, light theme](docs/assets/screenshot-light.png)

Version 0.1 (MVP): headings, bullet lists, bold, italic and strikethrough render inline; open, edit,
save and save as; light and dark themes plus your own theme file; native on Linux (Wayland and X11),
Windows and macOS.

Licence: GPL-3.0-or-later (see `LICENSE`). hitokoto uses Qt under the LGPLv3.

## Using it

```sh
hitokoto notes.md                 # open a file
hitokoto                          # start with an empty document
hitokoto --theme my-theme.toml    # use a specific theme file
```

Your own look goes into a theme file; see [docs/theming.md](docs/theming.md).

## Building

Requirements: a C++20 compiler, CMake 3.25 or newer, Qt 6.8 or newer (Core, Gui, Widgets; Test
for the tests). Tests and benchmarks fetch GoogleTest and Google Benchmark automatically if they
are not installed.

### Linux

Debian 13 (trixie) or newer:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-wayland
cmake --preset release -G Ninja
cmake --build --preset release
./build/release/src/app/hitokoto
```

Distributions with an older Qt (Ubuntu 24.04 ships 6.4): install Qt 6.8+ with the Qt Online
Installer and add `-DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/gcc_64`.

hitokoto runs natively on Wayland and on X11; Qt picks the right one. Force one with
`QT_QPA_PLATFORM=wayland` or `QT_QPA_PLATFORM=xcb`.

### Windows

Install Visual Studio 2022 (workload "Desktop development with C++") and Qt 6.12 for
"MSVC 2022 64-bit" with the Qt Online Installer. In an "x64 Native Tools Command Prompt for VS 2022":

```bat
cmake --preset release -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64
cmake --build --preset release
build\release\src\app\Release\hitokoto.exe
```

To start it from Explorer, either add `C:\Qt\6.12.0\msvc2022_64\bin` to `PATH` or create a
distributable folder (below).

### macOS

With Xcode Command Line Tools and Homebrew:

```sh
brew install cmake qt
cmake --preset release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build --preset release
open build/release/src/app/hitokoto.app
```

## Development

The working agreement is in [CLAUDE.md](CLAUDE.md), the brief in [docs/brief.md](docs/brief.md),
decisions in [docs/adr](docs/adr), the backlog in [docs/stories](docs/stories), the design in
[docs/architecture.md](docs/architecture.md).

```sh
cmake --preset dev            # Debug, tests, warnings as errors
cmake --build --preset dev
ctest --preset dev            # core and UI tests (UI tests run headless)

cmake --preset core-sanitize  # core only, ASan/UBSan, no Qt needed
cmake --preset fuzz           # libFuzzer targets for the core (Clang)
cmake --preset bench          # benchmarks, see docs/performance.md
```

Code is formatted with `clang-format` (`.clang-format`) and linted with `clang-tidy`
(`.clang-tidy`); CI enforces both. CI builds and tests on Linux (Qt 6.12 and 6.8), Windows and
macOS, runs sanitizers and fuzzers, and benchmarks on all three systems.

## Distributable builds

Packaging in CI is story S012 and not automated yet. Until then, by hand from a release build:

- **Windows:** `windeployqt --release --no-translations build\release\src\app\Release\hitokoto.exe`
  copies the Qt DLLs and plugins next to the executable; zip that folder.
- **macOS:** `macdeployqt build/release/src/app/hitokoto.app -dmg` creates a `.dmg`
  (unsigned; signing and notarisation need an Apple developer account).
- **Linux:** an AppImage with [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) and its Qt
  plugin: `cmake --install build/release --prefix AppDir/usr`, then
  `linuxdeploy --appdir AppDir --plugin qt --output appimage`. A Debian package is planned separately.

Every distribution must include `LICENSE` and the Qt LGPLv3 notice.
