---
title: "Objection record: S024 front matter shown as properties (spec)"
mode: spec
spec: docs/stories/024-front-matter.md
branch: agents/s024-front-matter
commits_examined: "uncommitted working tree on 3a17d89 (S024 refined, ADR 0007 and S025 new, S009 and backlog changed)"
reviewed_on: 2026-10-30
dispositioned: 2026-10-30 (Armin decided O1, O2, O6 and O9; the other objections and the capped concerns as proposed by Claude, before the red tests were committed)
reviewer: Advocatus Diaboli (independent agent)
environment: "Linux container, no build; js-yaml 4.3.2 (Node 22.23.4), ruamel.yaml 0.19.1 and PyYAML 6.0.3 outside the repo; Obsidian help \"Properties\" read 2026-10-30"
objections:
  - id: O1
    category: premise
    severity: major
    title: "The owner's example, a review record, becomes a wall of JSON that is harder to read than its YAML"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: unsupported values stay JSON as in Obsidian, but show at most three wrapped lines ending in an ellipsis; the full JSON is the row's tooltip and accessible name. Story decision 1, rules 'Types' and 'JSON for unsupported values', first scenario. Tests: UI tst_s024_front_matter::aReviewRecordShowsAsProperties (height bound, full JSON in the accessible name); core S024_FrontMatter.ReviewRecordShowsItsProperties."
  - id: O2
    category: premise
    severity: minor
    title: "S024 replaces S023's text view for every user; Obsidian lets the user keep the YAML (Source) or hide it"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: feature toggle ui::Options::frontMatterAsProperties, on by default; off gives the S023 behaviour (Obsidian's Source). Hidden is not offered and is listed among the differences. Story decision 9, rule 'Toggle', scenario 'the toggle gives the S023 behaviour'. Test: tst_s024_front_matter::theToggleGivesTheS023Behaviour."
  - id: O3
    category: premise
    severity: minor
    title: "Tags is a type of its own in Obsidian; the story shows `tags` as a generic list and does not record the difference"
    disposition: fixed
    disposition_rationale: "Tags is a type of its own with a tag icon (story decision 1, 'Types'); a scalar tags value is one chip, recorded among the differences as possibly not what Obsidian does (not verified). Tests: S024_FrontMatter.ListsAndTags, tst_s024_front_matter::eachTypeHasItsLook."
  - id: O4
    category: design
    severity: major
    title: "Embedding like a table collides with TableEmbedder, which unhides every hidden or reserved line that is not a table"
    disposition: fixed
    disposition_rationale: "Verified: TableEmbedder::sweep (table_embedder.cpp:264-296) unhides every hidden line and clears every reserved height it does not own. The story now requires an owner for hidden and reserved state (per block in BlockLayout or one embedder interface shared by tables and properties); this changes TableEmbedder once, after which a further embedded element needs no change to existing ones. Scenario 'front matter directly followed by a table'. Test: tst_s024_front_matter::frontMatterAboveATableStaysConcealed."
  - id: O5
    category: design
    severity: minor
    title: "The badge has no tokens of its own, and chips and checkboxes miss visible properties"
    disposition: fixed
    disposition_rationale: "Tokens added: properties.badge-text (fallback unsupported-text), badge-font-family, badge-font-scale, checkbox-background, chip-border, chip-border-width, chip-padding. Tests: S024_FrontMatter.ThemeTokensAreParsed, ThemeTokensFallBack, TokensAreDocumented."
  - id: O6
    category: failure
    severity: major
    title: "A selection reveals the block and changes its height, so a mouse drag across it can flip the layout on every move"
    disposition: fixed
    disposition_rationale: "Armin, 2026-10-30: the YAML shows only while the caret and the anchor of the selection both lie in the block; a selection reaching beyond it keeps the properties block (copying copies the YAML lines, as with tables, ADR 0005); while a mouse button is held the block keeps its form. Story decision 10, 'When the YAML shows', scenario 'a selection across the block keeps the properties'. Test: tst_s024_front_matter::aSelectionAcrossTheBlockKeepsTheProperties (Shift+click, select all, drag across, press on a row)."
  - id: O7
    category: failure
    severity: minor
    title: "Reading the YAML in idle slices moves the lines below without the caret entering or leaving, against the S009 scenario"
    disposition: documented
    disposition_rationale: "Story 'Long front matter': reading and building take at most 4 ms on a key and continue in idle slices; when they finish, the block switches once without the caret moving, an exception to decision 8 stated in the story. Front matter that fits the budget switches before the first screen. Test: tst_s024_front_matter::longFrontMatterIsReadAndBuilt (no state in between)."
  - id: O8
    category: failure
    severity: minor
    title: "Only reading is deferred; laying out every row and an unbounded JSON line happens on the key that leaves the block"
    disposition: fixed
    disposition_rationale: "Building and laying out follow the same budget as reading; rows on screen first; the JSON shows at most three lines (O1). Benchmark: 5,000 top-level keys and one nested value of 5,000 lines (story, requirements). Test: tst_s024_front_matter::longFrontMatterIsReadAndBuilt with both shapes."
  - id: O9
    category: failure
    severity: minor
    title: "A CommonMark document that starts with a break can now have its body concealed, with ATX headings dropped as YAML comments"
    disposition: documented
    disposition_rationale: "Armin, 2026-10-30: recognition stays as in Obsidian; the concealment of YAML-looking body text is recorded as a known consequence (story decision 7, 'Recognition', special-case scenario). Test: S024_FrontMatter.YamlLookingBodyTextBecomesProperties."
  - id: O10
    category: failure
    severity: minor
    title: "Valid YAML outside the subset is labelled Invalid, and no badge says what is wrong"
    disposition: fixed
    disposition_rationale: "The badge names kind and reason: 'Invalid YAML (line n): duplicate key' or 'Unsupported YAML (line n): anchor or alias', also as its accessible name; reasons in a table in the story, an enum in the core (ADR 0007). Tests: S024_FrontMatter.InvalidYamlNamesLineAndReason, YamlOutsideTheSubsetIsUnsupported; tst_s024_front_matter::invalidYamlStaysTextWithABadge."
  - id: O11
    category: operational
    severity: minor
    title: "The first acceptance scenario depends on a file that is in no branch of the repository"
    disposition: fixed
    disposition_rationale: "The tests carry the YAML of Armin's example as a fixture of the same shape (core kReviewRecord, UI kRecord); the story describes its content instead of referring to a file of Daniel's branch."
  - id: O12
    category: operational
    severity: minor
    title: "fuzz_render will rarely get deep into the parser in 60 s without a corpus, so the claim that it covers the parser is weak"
    disposition: fixed
    disposition_rationale: "Story requirement and ADR 0007: fuzz_render also feeds the whole decoded input, split into lines, to the parser, besides the YAML of each closed front matter; ci.yml stays unchanged. A seed corpus or dictionary would need ci.yml and is not asked for now."
---

# Objection record: S024 front matter shown as properties (spec)

Scope: the uncommitted changes on `agents/s024-front-matter` (base `3a17d89`):
`docs/stories/024-front-matter.md`, the new `docs/adr/0007-reading-yaml-front-matter.md` (Proposed)
and `docs/stories/025-edit-properties.md`, and the changes to
`docs/stories/009-concealment-quality-gate.md` and `docs/stories/README.md`. I read them against `CLAUDE.md`, `docs/brief.md` (sections 3, 4, 5 and
8), ADR 0002 to 0006, stories 009, 010, 018, 021 and 023, `docs/theming.md`, `docs/architecture.md`,
`src/core/src/elements/front_matter.cpp`, `markdown.hpp`, `block_styler.cpp` (front matter search),
`table_embedder.hpp/.cpp`, `editor_widget.hpp/.cpp`, `tst_s009_concealment.cpp`,
`test_s023_thematic_breaks.cpp`, `fuzz/fuzz_render.cpp`, `.github/workflows/ci.yml` and the S023
review records. I built nothing. YAML claims were checked with js-yaml 4.3.2, ruamel.yaml 0.19.1 and
PyYAML 6.0.3 outside the repository.

Armin decided eight points on 2026-10-30. O1 challenges decision 1 for nested values. O9 challenges
the consequences of decision 7 ("recognition unchanged") now that the lines are concealed. The other
objections take the decisions as given.

## O1 - premise - major

**Claim.** The first scenario takes a review record as its example. Review records keep their
content in a nested `objections` list, and decision 1 shows that list as one wrapped line of JSON.
For these files the block is neither compact, as the story promises, nor easier to read than the
YAML that S023 shows today.

**Evidence.**
- Story line 17: "shown as a compact block of properties". Lines 24–26 and 82: a nested value is
  "unsupported" and "one line of JSON (wrapping)", in `properties.unsupported-text`, which falls back
  to `colors.accent` (line 196). Line 127 expects exactly this for `objections`.
- I parsed the front matter of existing records with js-yaml 4.3.2:
  - `022-inline-links-code-recheck.md` (S022 branch, 46 front-matter lines): the JSON of
    `objections` is 2,241 characters.
  - `docs/reviews/023-thematic-breaks-code.md` (73 front-matter lines): `objections` is 6,067
    characters and `evidence_tools` 836.
- The value column is at most about 600 px wide (`layout.editor-max-width` 760 minus
  `properties.key-width` 160, minus padding). By my estimate, at roughly 75 to 85 characters per
  visual line, 6,067 characters fill 70 to 80 lines: about as many lines as the YAML, but with no
  break between objections, every `"` inside a rationale written as `\"`, and all of it in the
  accent colour.
- The record format the project uses for every story review (this file included) requires nested
  objections in the front matter. Every review record in `docs/reviews/` will look like this.

**Ask.** Before confirming, let Armin look at a mock-up of one real record. Then decide how an
unsupported value is shown, for example:
- one line, truncated with an ellipsis, with the full JSON in the accessible name or a tooltip;
- the value's YAML as indented, read-only text;
- or the JSON as specified, with this consequence recorded.

Pin the decision with the example scenario.

## O2 - premise - minor

**Claim.** S024 replaces S023's accepted text view for every user and every file. Obsidian, which is
the reference for the whole story, lets the user choose how front matter appears. The story neither
offers that choice nor records its absence as a difference.

**Evidence.**
- Obsidian help, "Properties": "Properties in document" can be set to Visible (the default), Hidden
  ("can still be displayed in the sidebar"), or Source ("displays properties in plain text YAML
  format").
- Story lines 7–9: S024 "changes accepted behaviour of S023". Lines 232–234 list the differences
  from Obsidian, and this setting is not among them.
- There is a precedent: ADR 0002 decision 6 adds behaviour as "a feature toggle, off until an options
  dialog offers it".

**Ask.** Choose one:
- add a toggle with Visible as the default and Source as the S023 behaviour, which a later options
  dialog can expose;
- or add "no Source or Hidden setting" to the list of differences.

## O3 - premise - minor

**Claim.** Decision 1 says "types as in Obsidian". Obsidian has seven types, but the story implements
six and shows `tags` as a generic list with the list icon. The list of differences does not mention
this.

**Evidence.**
- Obsidian help, "Properties": the types are Text, List, Number, Checkbox, Date, Date & time and
  Tags. "Tags properties are a special property type used exclusively by the `tags` property."
- Story line 25 lists text, number, checkbox, date, date and time, and list. Line 81 maps `tags`,
  `aliases` and `cssclasses` with a scalar to "list … one chip … bulleted list".
- The story also says that a scalar `tags: a, b` is one chip (line 81). A user report on the
  Obsidian forum ("Tags in front matter, dataview and search", 2022, before properties existed)
  found that `tags: tag1, tag2` gives two tags. This is weak evidence for today's Obsidian, but the
  story's rule rests on no evidence at all.

**Ask.** Choose one:
- give `tags` its own type and icon;
- or add "tags shown as a list" to the differences.

Check how the current Obsidian splits a scalar `tags` value before you pin "one chip" in a test.

## O4 - design - major

**Claim.** "Embedded like a table in S018" (line 218) assumes that `BlockLayout` hiding and reserving
can be shared. It cannot. `TableEmbedder` treats every hidden or reserved block in the range it
refreshes as its own. When that block belongs to no table, it unhides the block or drops its
reserved height. A properties block at the top of the document would be undone whenever the screen
is within 200 lines of the top, which covers most notes.

**Evidence.**
- `src/ui/table_embedder.cpp:264-296` (`sweep`): blocks that are not visible and not covered by a
  table get `layout_->setHidden(block, false)`. Blocks with a reserved height that is not a table
  header get `setReservedHeight(block, 0)`.
- `refresh()` calls `sweep(from, to)` (line 372) over the tables "on and near the screen (±200
  lines)" (`docs/architecture.md:46`). A refresh is scheduled whenever the styler completes
  (`editor_widget.cpp:63`) and after layout changes (lines 221–229).
- `TableEmbedder::caretMoved` moves a caret that lands on hidden lines (`table_embedder.hpp`).
  The front matter rule is different: the caret on a hidden line reveals the YAML.
- `BlockLayout` records no owner for a hidden or reserved block (`block_layout.hpp:24-30`).
- Brief section 5: "Adding a new element must not require editing existing elements or the
  editor's core loop." A second embedder has to change `TableEmbedder::sweep` and the wiring in
  `EditorWidget` (`editor_widget.cpp:46-65, 241, 273`). Neither the story nor ADR 0007 says how.

**Ask.** Decide, in the story or in ADR 0007, who owns the hidden and reserved state of a block.
For example, use a small embedder interface that claims line ranges and is shared by tables and
properties, or record an owner per block in `BlockLayout`. Add a UI test with front matter followed
directly by a table, with the table edited and refreshed while the properties block is concealed.

## O5 - design - minor

**Claim.** The theming baseline asks for tokens for every visible property of each element. The
badge for invalid YAML is a separate element, but it borrows the colour of the JSON text and has no
font tokens. Chips and checkboxes show properties that no token covers.

**Evidence.**
- Story line 196: `properties.unsupported-text` stands for "JSON of unsupported values; the badge
  of invalid YAML".
- The only other badge in the editor has three tokens of its own: `code-block.badge-text`,
  `badge-font-family` and `badge-font-scale` (`docs/theming.md:125-127`).
- Brief section 8: "Every styleable element is themeable on its own … for every property it
  visibly has … padding". `CLAUDE.md`: "each element a story adds or changes has its own documented
  theme tokens for every visible property".
- Chips have tokens for background, text and radius, but none for padding or border
  (lines 199–201). One colour, `properties.checkbox`, covers both the tick and the border of a
  checkbox, and there is no fill colour.

**Ask.** Add the following tokens, or state why each property is fixed:
- `properties.badge-text`, falling back to `properties.unsupported-text`;
- badge font family and font scale, as in S021;
- chip padding;
- chip border and border width.

## O6 - failure - major

**Claim.** The block reveals its YAML for a selection as well as for the caret (lines 93 and 102),
and revealing changes its height (lines 113–116). A mouse drag that crosses the closing line
therefore changes the layout under the pointer.

When the revealed YAML is shorter than the properties block, the pointer ends up below the YAML.
The selection then no longer touches the front matter, so the block conceals again and grows back
under the pointer. The selection and the layout can flip on every mouse move. Select All moves
every line of the document.

**Evidence.**
- Until now no reveal changed geometry. In the S009 scenarios for code fences and for breaks, "no
  line moves or changes height". S021 reveals "without geometry change"
  (`021-code-block-fences-in-panel.md:76`).
- Tables avoided this problem: a selection does not switch a table to its source. ADR 0005: "A
  selection in the editor that spans a table copies its Markdown lines"; only typing or
  Ctrl+Shift+M shows the source.
- Under the default theme the properties block is often the taller form. It has two margins of 3,
  two paddings of `layout.spacing` 8, a title row, row spacing of 2 and the rows themselves, while
  `---` / `tags: [a, b]` / `---` is three text lines. A long text value also wraps more in the
  value column (text width minus `key-width` 160) than it does as a YAML line.
- The S009 scenario (lines 176–185) and `tst_s009_concealment` walk with keys only. No slot uses
  the mouse. S010 covers drags that start on a concealed line, but only horizontal shifts.

**Ask.** Decide whether a selection reveals the block at all, as tables do not. Alternatively,
reveal only for a collapsed caret, or freeze the block's height while a mouse button is held. Add a
UI test that drags from the body upward across the block and one that Shift+clicks into it, both
with a block that is taller concealed than revealed.

## O7 - failure - minor

**Claim.** Long front matter is read in idle slices, and the YAML shows until reading finishes
(lines 162–164 and 220–221). The switch to properties then moves the lines below while the caret
is elsewhere. Decision 8 and the S009 scenario rule this out: "the lines below move only when the
caret enters or leaves the block" (lines 39–40 and 181–182).

**Evidence.**
- The story names no threshold for "long". The benchmark opens a 10 MB document "with front matter
  of 200 properties in front (first screen)" (line 225), but the story never says what that first
  screen shows.
- Confirming the closing line already runs in idle slices after a synchronous 4 ms
  (`block_styler.cpp:972-1016`, `kFenceSearchNanoseconds`). Until it finishes, line 0 renders as a
  thematic break. After opening, a long block can therefore show a break, then YAML, then
  properties.
- `EditorWidget` keeps the text still when blocks above the viewport change height
  (`architecture.md:45`). That does not help on opening, when the block is on screen.

**Ask.** Choose one:
- state the exception in S024 and in the S009 scenario;
- or keep the YAML until the caret next leaves the block.

Name the threshold and what the benchmark's first screen shows.

## O8 - failure - minor

**Claim.** The height of the concealed block is reserved on its first line (lines 113–115), so every
row has to be laid out to know that height: wrapped key, wrapped value and chips. The block is built
"when the caret leaves the block" (line 220), on that key. The story defers only reading the YAML,
not building or laying out the rows, and a nested value turns into one JSON string of unbounded
length.

**Evidence.**
- Line 184 asks for one accessible element per row. With 5,000 properties that is 5,000 objects,
  created on one key.
- The benchmark "leaving a block of 5,000 lines" (lines 226–227) does not say what is in the block:
  5,000 top-level keys, or one nested value of 5,000 lines.
- At the ratio of `023-thematic-breaks-code.md` (6,067 characters of JSON for 73 lines), a nested
  value of 5,000 lines gives a single JSON paragraph of about 400,000 characters. It is laid out
  again on every width change.
- Brief section 4: no key above one frame for typing; the story's own budget is "no key above 50 ms"
  (line 163).

**Ask.** Specify the benchmark contents: one case with 5,000 top-level keys and one with a single
nested value of 5,000 lines. Allow building and laying out the block in slices, as reading already
is, or lay out only the rows on screen. Cap how much JSON is displayed.

## O9 - failure - minor

**Claim.** Recognition has no limit. In S023 this was accepted because everything up to the next
`---` stayed visible as text. In S024 those lines are concealed whenever the text happens to be a
valid YAML mapping, and comment lines are "never shown" (line 59). In a CommonMark document that
starts with a thematic break and later has `---` under text, the body disappears, and ATX headings
`# …` drop out as comments.

**Evidence.**
- js-yaml 4.3.2 accepts `Status: draft` / blank / `# Chapter one` / blank /
  `Note: the plan is simple` / blank / `# Chapter two` as `{"Status":"draft","Note":"the plan is
  simple"}`. If such text lies between a first-line break and a `---` setext underline, S024 hides
  all of it and shows two rows. S023 showed it as plain text.
- S023 code review O5 (`023-thematic-breaks-code.md`) was dispositioned "documented", "as in
  Obsidian", and left the question open for S024. The story keeps the rule (lines 44–46) but does
  not say that concealment is the new consequence.

**Ask.** Choose one:
- record the consequence and pin it with a test;
- or keep as text, with the badge, any front matter in which a comment line looks like an ATX
  heading (`# ` and text) or a blank line separates two keys.

This challenges the consequences of decision 7, not the recognition rule itself.

## O10 - failure - minor

**Claim.** The badge says `Invalid properties (line n)` (line 110) for two different situations:
YAML that is wrong, and YAML that is valid but outside the subset. It gives no reason in either
case. A writer whose file is accepted by every YAML parser is told the file is invalid and is given
only a line number.

**Evidence.**
- Story lines 62–67 classify as invalid several constructs that are valid YAML. js-yaml 4.3.2
  accepts all of them:
  - `a: &x 1` / `b: *x` gives `{"a":1,"b":1}`;
  - `a: !!str 1` gives `{"a":"1"}`;
  - `? a` / `: b` gives `{"a":"b"}`;
  - `a: |2` / `   x` gives `{"a":" x\n"}`.
- For errors that really are errors, js-yaml names the cause: "duplicated mapping key (2:1)", "tab
  characters must not be used in indentation (2:1)", "unexpected end of the stream within a flow
  collection".
- ADR 0007 already says the parser reports "the first error with its line". Adding a reason costs
  an enum.

**Ask.** Use two badge texts, for example `Unsupported YAML (line n): anchor` and
`Invalid YAML (line n): duplicate key`. Make the reason the accessible name, too. Add one test per
error kind.

## O11 - operational - minor

**Claim.** The first scenario, "front matter shows as properties", uses
`022-inline-links-code-recheck3.md` (line 121). That file is not in the repository on any branch,
so the test cannot be written from the story.

**Evidence.**
- `git ls-tree -r` over every remote branch after `git fetch` finds no
  `022-inline-links-code-recheck3.md`. Daniel's `feature/links_references` (`60263e3`) has only
  `022-inline-links-code.md` and `022-inline-links-code-recheck.md`.
- The scenario expects exact rows, `O19` and "lines 1 to 27" (lines 121–127). None of these can be
  checked against a file that nobody can see.
- The file belongs to Daniel's open PR #21 and may change or be renamed before that merge.

**Ask.** Copy the exact bytes into `tests/` as a fixture, with the owner's consent, since it comes
from Daniel's branch. Alternatively, write a fixture of the same shape into the story.

## O12 - operational - minor

**Claim.** The story says `fuzz_render` "also feeds the YAML of each closed front matter to the
parser, so the existing CI job covers it" (lines 223–224). Under the current setup, the fuzzer will
rarely get past the first lines of YAML in its 60 seconds.

**Evidence.**
- `fuzz/fuzz_render.cpp:16-19` decodes the input as pairs of UTF-16 bytes. To reach the parser,
  the input has to start with `-\0-\0-\0\n\0` and also contain a closing delimiter line. Quotes,
  flow collections, block scalars and nesting to the depth limit of 64 come only after that.
- The CI job runs each target for 60 s with no corpus and no dictionary (`ci.yml:50-54`).
  `fuzz/` holds only the three sources and `CMakeLists.txt`.
- The parser processes untrusted files, and ADR 0007 promises "never throws, … linear". Linear time
  is checked only by the core test.

**Ask.** Inside `fuzz_render`, so that `ci.yml` stays unchanged, also pass the whole decoded input
to `parseProperties` as YAML lines, not only the YAML of a confirmed front matter. A seed corpus or a
dictionary would need a change to `ci.yml`; ask Armin about that separately.

## Capped concerns

- Cost: ADR 0007 estimates "about 600 lines". js-yaml's `loader.js` is 1,803 lines and also covers
  anchors, tags and directives. The subset still needs a scanner for flow collections, quoted
  escapes, folding of multi-line plain scalars and block scalars with chomping, plus a JSON writer,
  ECMAScript number formatting, calendar validation and line mapping.
- JSON numbers: "shortest decimal form" (line 89) contradicts "as JavaScript's `JSON.stringify`
  writes it". For `1e20`, Node gives `100000000000000000000` and shortest round trip (`repr`,
  `std::to_chars`) gives `1e+20`. For `1e-6`, Node gives `0.000001` and shortest round trip gives
  `1e-06`. The story's own example `1e3` → `1000` is not the shortest form either.
- Unlisted reference differences (line 241 names only dates, anchors and invalid calendar dates):
  `0b101` is 5 in js-yaml 4.3.2 and ruamel.yaml 0.19.1 but text under the core schema; `1_000` is
  1000 in ruamel.yaml.
- ADR 0007 has no "Reconsider if" section (brief section 5); the only trigger appears under
  Consequences.
- The depth limit (ADR 0007: 64 levels, deeper is an error) is missing from the story's list of
  invalid input.
- The story does not say whether `a` and `"a"` count as the same key (js-yaml: "duplicated mapping
  key"), or whether keys in nested mappings must be unique.
- Line numbers are mixed: "lines 0 to the closing line" is 0-based (line 93), "the start of line 1"
  (line 105) is ambiguous, and the badge (line 111) and the scenario "lines 1 to 27" are 1-based.
- Ctrl+Home and PageUp to the top put the caret on the YAML. A writer who jumps to the top of a note
  therefore always sees YAML, never the properties.
- Screen readers: the concealed YAML lines remain in the `QTextEdit`'s accessible text. The story
  does not say whether they are read in addition to the rows of the block (S009: "consistently").

## Explicitly not objecting to

- A parser of our own instead of rapidyaml or yaml-cpp (ADR 0007). The use is read-only, the core
  stays standard library only, the parser can be replaced behind `parseProperties`, and leaving out
  anchors and aliases removes alias-expansion bombs.
- Types inferred from values instead of a vault-wide list. hitokoto has no vault, and the story
  records this as a difference.
- Editing as text, with nothing written from the block. Byte-exact saving (ADR 0002) holds by
  construction, and editing through the block is a separate story (S025).
- The date rules. js-yaml 4.3.2 and ruamel.yaml 0.19.1 both give a string for `2026-10-30T16:28` and
  reject or stringify `2026-02-30`. Accepting `THH:MM` as date and time on purpose, and treating a
  space instead of `T` as text, is consistent with how Obsidian stores dates.
- The core-schema booleans and null (`True`/`TRUE`, `~`, an empty value) and `yes` as text. These
  match js-yaml and ruamel.yaml (YAML 1.2); PyYAML (YAML 1.1) differs, as expected.
- `...` not closing the block, and the BOM handling. Both are consistent with S023 and ADR 0002.
- Checking expectations against js-yaml and ruamel.yaml outside the repository, as is done with
  commonmark.js. Brief section 2 is not touched, because no Node.js enters the build, the tests or
  CI.
- Moving front matter out of the S023 caret walk into its own S009 scenario.

## Disposition of the capped concerns

- Cost of the parser: ADR 0007 now expects roughly 800 to 1,200 lines and names what the subset
  needs.
- JSON numbers: the story now says "as ECMAScript's Number::toString writes them" with the examples
  `1e20`, `1e21`, `1e-6` and `1e-7` (checked with Node 22); test `JsonAsJsonStringifyWritesIt`.
- `0b101` and `1_000` are text and listed as differences from the reference parsers; test
  `NumbersOfTheCoreSchema`.
- ADR 0007 has a section "Reconsider if".
- The depth limit is in the table of invalid input ("too deep"); test
  `InvalidYamlNamesLineAndReason`.
- `a` and `"a"` are the same key, and keys must be unique in nested mappings too; tests in
  `InvalidYamlNamesLineAndReason`.
- Line numbers: the story says that its rules count lines from 0 and the badge and the scenarios
  from 1.
- Ctrl+Home and PageUp to the top show the YAML: recorded in "When the YAML shows" as intended (the
  caret is in the block).
- Screen readers and the concealed YAML lines: whether the editor's accessible text still reads
  them is checked by hand on each platform with the S009 gate; the block's own rows are specified.
