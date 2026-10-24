# Project: hitokoto — a native, single-file hybrid Markdown editor

"hitokoto" (一言, "a single word") is a desktop Markdown editor built around one file at a time. You are building it from scratch. Before writing any code, read this whole brief, then produce the architecture decision record (ADR) described in "Phase 0". Do not start implementing until I have confirmed the ADR.

## 1. Product vision

A single-file Markdown editor with an Obsidian-style hybrid live-preview editing model: the document is always plain Markdown text, rendered inline while you edit, with the raw syntax revealed only where the cursor or selection is.

Initial behaviours (MVP):
- `# Title` (levels 1-6) renders as a large heading. The `#` markers are hidden unless the cursor is on that line or the heading is selected, in which case the raw text is shown.
- List items (`-`, `*`, `+`) render as small bullet dots. When the cursor is on that line, the raw marker is shown.
- Single-file editing: open, edit, save, save-as, one document at a time. No vault, no tabs, no sidebar in the MVP.

The source of truth is always the plain Markdown text. Never keep a separate rich-text document model that can drift from the Markdown. Rendering is a view over the text.

Planned later (do not build now, but do not architect it out): bold/italic/inline code, links, task lists, numbered lists, blockquotes, code blocks, tables, images, search, spellcheck, export, `[[wiki links]]`, multiple documents, plugins, keybinding schemes, localisation, accessibility.

## 2. Hard technology constraints

Non-negotiable:

1. **No web technology and no JavaScript tooling stack.**
   - No HTML, web-style CSS, browser engines, webviews, Electron, or Tauri.
   - No NPM, pnpm, yarn, bun, deno, Node.js, or any JavaScript package manager or bundler, whether in the application, the build, the tests, or the dev workflow. No TypeScript.
   - JavaScript embedded inside an otherwise native toolkit (e.g. a declarative UI language that evaluates small JS expressions) is tolerated, but not preferred. If a candidate relies on it, flag it in the ADR, keep the JS surface minimal, and keep all real logic outside it. No third-party JS packages, ever.
2. **One codebase for Linux, Windows, and macOS.** No per-OS source trees or forks of application logic. Tiny, isolated platform shims are acceptable only if the chosen toolkit and standard libraries cannot cover something, and each must be justified in the ADR.
3. **Native Wayland support on Linux** (not only XWayland). X11 must work too.
4. **You choose the language and toolkit.** I am deliberately not prescribing them. Decide based on what is best for: (a) maintainability, (b) extensibility, (c) practicality, (d) performance (see section 4), (e) the features above, and (f) the features we will likely want later.

## 3. Dependency and stack policy (important)

I care more about the project staying healthy for years than about any single technology:

- **Every library, framework, and toolkit you pick must be well supported and actively maintained.** For each dependency, check and report: recent release and commit activity, number and responsiveness of maintainers, size of the user base, documentation quality, license, and how it handles breaking changes. Abandoned, single-maintainer, or experimental dependencies on the critical path are not acceptable unless you make an explicit, justified exception in the ADR.
- **No glue stacks.** I do not want a project that is, for example, a Ruby app driving Python which drives GTK widgets. Prefer one language for the whole project. If you use two languages (e.g. a core library plus a UI layer), the boundary must use an officially supported, maintained binding mechanism, and the ADR must justify why a single language would not do.
- **No clashing technologies.** Do not combine technologies whose design philosophies, build systems, threading models, or styling systems fight each other (for example, a native style engine plus a second custom theming layer pulling the opposite way). Pick a coherent stack where the parts are designed to work together.
- **Minimise the dependency footprint.** Every dependency is a long-term liability. Add one only when it clearly beats writing the code ourselves, and wrap third-party libraries behind our own small interfaces so they can be replaced.
- Evaluate honestly the hardest part: a custom text editing surface with inline rendering, hidden/revealed syntax markers, cursor-aware rendering, IME support, accessibility, and good performance on large files. A toolkit with an immature text-editing story is a serious risk, however pleasant the rest of it is.

## 4. Performance: hitokoto must feel instant

Speed is a core feature, not a polish item. A one-file editor gets opened constantly, often just to jot something down, so waiting is unacceptable. Treat these as budgets, measured on modest, typical hardware (not a high-end dev machine):

| Scenario | Target |
|---|---|
| Cold start to a usable, focused editor window | under 1000 ms (aim for ~500 ms); never more than 1.5 s |
| Opening a typical file (up to ~1 MB) | under 500 ms |
| Saving | imperceptible; never blocks typing or the UI |
| Keystroke to screen update (typing latency) | within one frame (about 16 ms), including re-rendering the affected line(s) |
| Scrolling and cursor movement | smooth 60 fps, including cursor-aware reveal/hide of markers |
| Large files (10 MB+) | still usable: open in under 2-3 s, no typing lag |
| Idle resource use | no CPU use when idle; modest memory footprint |

How to get there:
- Make performance a criterion in the ADR (section 5). Report each candidate's startup time, binary size, memory footprint, and rendering model, and treat a heavy runtime or slow startup as a real cost.
- **Never block the UI thread** with file I/O, parsing of large documents, or saving. Do heavy work off-thread or incrementally.
- **Incremental work.** Re-parse and re-render only what changed (the edited lines/blocks and the lines affected by cursor movement), not the whole document, on every keystroke.
- **Fast, safe saves.** Write atomically (temp file in the same directory, then rename), and keep the UI responsive while it happens. Never corrupt the user's file for the sake of speed.
- Avoid unnecessary work at startup: lazy-load anything not needed for the first frame (themes beyond the active one, rarely used features, etc.).
- **Measure, do not guess.** Add benchmarks to the repo (startup, open, save, typing latency, large-file scenarios), run them in CI, and fail or flag the build on significant regressions. Performance stories are written and tested just like feature stories.

## 5. Maintainability principles (design for these from day one)

The goal: adding a feature should be a small, local change, never a large refactor.

- **Strict layering.** A UI-independent `core` (Markdown model, parsing, cursor-aware render mapping, theme engine, file I/O, settings) and a thin `ui` layer. The core must be fully testable without any UI and must not import toolkit types.
- **Extension by addition.** Each Markdown syntax element (heading, bullet, later bold, links, tables, ...) is an isolated unit registered with the engine: one parse rule, one render rule, its own tests. Adding a new element must not require editing existing elements or the editor's core loop. Prove this early: implement headings and bullets through the same mechanism that future elements will use.
- **Stable internal interfaces.** Define small, documented interfaces between parser, render mapping, theme engine, and UI. Changing one side should not ripple into the others.
- **Document decisions.** Keep ADRs for every significant decision, keep `docs/architecture.md` current, and record in each ADR what would make us reconsider.
- **Refactor continuously in small steps** (the "refactor" in red-green-refactor), so no big-bang rewrites accumulate.

## 6. Phase 0: Architecture decision record (do this first)

Write `docs/adr/0001-technology-stack.md` containing:
- The candidates you seriously considered (at least three) that satisfy section 2, with a dependency-health assessment for each (see section 3).
- For each: how the hybrid live-preview editor would be implemented, Wayland status, Windows/macOS quality, theming capabilities, text editing/IME/accessibility maturity, performance profile (see section 4), build and packaging story, testing story (including headless UI tests), and risks.
- Your recommendation, the trade-offs you accept, and what would make you reconsider.
- A small throwaway spike of the hardest part: a text area where `# Heading` renders large with the `#` hidden unless the cursor is on that line. Measure and report cold start time, typing latency, and behaviour on a large file, along with code size and hackiness.

Stop after the ADR and spike and wait for my confirmation.

## 7. Development methodology

**Test-Driven Development.** No production code without a failing test first (red, green, refactor). Logic lives in the UI-independent core with fast unit tests: parsing, the mapping from text and cursor position to which markers are visible, theme parsing/validation, and file I/O. Use property-based or fuzz tests where useful (arbitrary text and cursor positions must never panic or corrupt the text).

**Story-Driven Development.** Work in small user stories written as Given/When/Then acceptance scenarios, stored in the repo (`docs/stories/NNN-title.md`). Each story:
1. is written before implementation, with explicit acceptance criteria,
2. maps to executable tests, either directly executable scenarios or clearly named tests referencing the story ID,
3. is implemented as the smallest vertical slice that satisfies it,
4. is done only when its tests pass and nothing regressed, including the performance budgets in section 4.

Example first stories:
- S001: Opening a Markdown file shows its content in the editor.
- S002: A line starting with `# ` renders as a large heading with the `#` hidden; moving the cursor onto it reveals the raw `# `.
- S003: A line starting with `- ` renders a bullet dot; the cursor on that line shows the raw `-`.
- S004: Saving writes exactly the Markdown text, byte for byte, with no rendering artifacts.
- S005: A custom theme file changes the UI's look.
- S006: The app reaches a usable editor window within the cold-start budget.
- S007: Typing in a 10 MB file stays within the latency budget.

Include UI-level tests where the toolkit allows (headless rendering, snapshot tests). Set up CI that builds, tests, and benchmarks on Linux, Windows, and macOS.

## 8. Look, feel, and theming

**Default look:** modern and flat. Reference points: Obsidian, Signal Desktop, Element (Matrix client), IntelliJ IDEA. Add a little colour: a tasteful accent colour, not a grey monochrome app. Ship light and dark variants.

**Theming is a core feature.** The user must be able to customise the app deeply, using either:
- a CSS-like (or otherwise declarative) theming language with documented properties, or
- an extensive theme system exposing at least: primary/secondary/accent colours, background and surface colours, text colours, editor font family and sizes, heading scale, border radius, button styles, spacing/density, and syntax-marker styling.

A hybrid is fine. Either way:
- Themes are plain text files that users can edit, share, and version-control, with no recompiling.
- All UI colours, radii, and fonts come from theme tokens. Hard-coded styling values in UI code are bugs.
- Themes are validated with clear error messages, and a broken theme never prevents startup (fall back to the default).
- Theme loading must not noticeably slow startup.
- Document the theme format in `docs/theming.md` with an example theme.
- The theming approach must work *with* the chosen toolkit, not against it. Explain this in the ADR.

## 9. Engineering standards

- Idiomatic code for the chosen language; formatter and linter enforced in CI.
- Small, focused commits with meaningful messages; one story per branch/PR where practical.
- `README.md` with build/run instructions for all three operating systems; document how to produce distributable builds for each.
- Ask me before making decisions that are expensive to reverse or that conflict with this brief. Never silently work around a hard constraint; if one is impossible, say why and propose options.

## 10. How to work with me

1. Phase 0 (ADR and spike), then wait.
2. Propose the MVP story backlog, then wait.
3. Implement story by story with TDD. After each story, summarise what changed, which tests were added, and what is next.
