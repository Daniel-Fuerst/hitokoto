# S016: Application icon

- **Status:** In progress
- **Basis:** User request on 2026-10-25; use `icons/hitokoto.svg` as the app icon.

## Story

As a user, I want the hitokoto icon to appear for the running app, so I can identify it in the
taskbar or dock.

## Scenarios

### Application and window expose the icon

Given the application starts
When its main window is created
Then both the application and main window have a non-empty icon rendered from the hitokoto SVG.

### Native bundles use the icon

Given a Windows or macOS application bundle is built
When the executable or bundle is displayed by the operating system
Then its native application icon is derived from the same hitokoto SVG.

## Dependency: Qt SVG (brief, section 3)

Qt SVG is part of the Qt Project and released with Qt Core, Gui and Widgets (same maintainers,
release cycle, documentation and compatibility promise as the Qt accepted in ADR 0001), licensed
under LGPLv3, GPLv2 or commercially like the rest of Qt, and part of the Qt installer's main
package. Footprint on Linux: libQt6Svg 0.8 MB plus two plugins of 0.1 MB (Qt Widgets: 10 MB);
cold start unchanged within measurement noise (median 35.6 vs 37.5 ms, 3 x 10 runs, offscreen).
It is used only behind `ui::applicationIcon()`.

Alternative considered: PNG renditions of the SVG in several sizes need no new module, but add a
second set of generated assets to keep in sync and blur at sizes not provided. Qt SVG will also be
needed for SVG images in Markdown (brief, section 1, planned later).

Reconsider if the icon stays its only use and the module causes trouble in distributable builds
(S012).
