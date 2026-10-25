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
