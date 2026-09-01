# TPMOS-COMPLIANCE-DEBT.md — real, confirmed architecture violations

**Severity: HIGH. Standing #1-tier priority, alongside `house-compaction.md`'s receipt
finding.** Written 2026-08-25, direct instruction: "this cant propagate forward into the
codebase by naive agents." This doc exists specifically so a future agent (naive or not)
reads this BEFORE copying any of the patterns below as a "working example."

**STANDING RULE, added 2026-08-25 after a real, confirmed process failure — read this
before touching ANY app listed as debt below:** if this doc is open in the same session
where you're about to migrate/patch/fix one of the affected apps (palettes, bookmarks,
or any future find), and the compliant manager+`<module>` pattern has ALREADY been built
for a sibling app in that same session (stats-hq's `stats_hq_manager.c` is the reference
example) — **stop and ask whether to build the real manager now instead of an interim
patch, before writing any workaround code.** Don't silently do the narrower literal
thing just because that's what was asked. Concretely, this bit tonight: bookmarks got
migrated onto the merged renderer WITHOUT a manager, which required inventing a
chtpm-live-reload mechanism in the renderer's own main loop just to let New+ show up
live — a real ~25-line piece of plumbing that becomes dead code the moment a real
manager (writing a state file the renderer polls) gets built, which was already the
proven, working pattern one file over. The fix wasn't hard to see; the doc already said
it. The miss was building around the gap instead of naming it and asking.

---

## Why this is severe, not cosmetic

TPMOS's core standard (`!.TPMOS_ONBORD_BIBLE_10.md` §1, §11, §12 — this house's own
declared "absolute" reference, not optional inspiration, per direct instruction recorded
in `khtpm-merge-how2.md`'s own HOUSE STANDARD section) requires:
- **Manager owns projection.** A real, running process reads sovereign state and
  publishes a structured projection (`gui_state.txt`) that the renderer substitutes —
  never a one-shot pre-launch script.
- **Ops are real, compiled, independently testable** (§12: "Every Op MUST be
  independently testable via CLI"). Business logic (parsing a stats file, formatting a
  bookmark row) belongs in a real, standalone, testable binary — never inline shell
  string-building.
- **"If it's not in a file it's a lie."** No hidden, one-shot, memory-only generation
  that can't be re-derived or live-updated.

**The specific anti-pattern found this session**, present in 3 confirmed files: a shell
launcher script directly `printf`s raw `.chtpm` XML tags (`<button onClick=...>`,
`<tab label=...>`) built from regex-scraped or hand-looped data, writes the result to a
static `.chtpm` file ONCE at launch, then execs the renderer against that frozen
snapshot. **This is dangerous specifically because it visually works** — the window
renders, looks identical to a compliant app, and a naive agent (or a rushed human) has no
visual signal that anything is wrong. The real damage is invisible until someone tries to
interact with it (see stats-hq below) or tries to extend it (the next dev copies the
`printf`-XML pattern because it's the nearest example in the tree).

---

## Confirmed affected files (real inventory, not exhaustive — see "Not yet audited" below)

### 1. `&.hq-apps/stats-hq/open_stats_hq.sh` — WORST, genuinely broken, not just non-standard
- No manager process at all. No `<module>` tag. Zero live state.
- Regex-scrapes stats `.txt` files (`grep -oE 'Total Turns:\s*[0-9]+'`) directly in the
  launcher, `printf`s `<tab>`/`<title>`/`<text>` fragments into temp files, splices them
  into `dashboard.template.chtpm` via a bash `while IFS= read -r line` token-replace loop
  (`__SESSION_TABS__`/`__SESSION_CONTENT__`), writes the frozen result, launches once.
- **Confirmed real, user-visible breakage from this**: the generated `<tab>` labels are
  session-date strings (e.g. `"2026-08-13 22:53:37"`). The renderer's own tab-click
  handler (`khtpm_entity_menu_render.c`'s `dbhq_*` family, ported from
  `khtpm_hq_render.c`) matches tab clicks against a FIXED `TAB_LABELS[]` C array specific
  to db-hq's own Common Events tabs. Stats-hq's date-string tabs were never wired to any
  matching logic — **clicking a stats-hq tab does nothing**. This isn't a style
  violation, it's a real, present, dead feature the UI itself advertises as clickable.
- Switching sessions requires relaunching the entire process with a different
  `SESSION_ID` argv — there is no live in-window tab switching at all.

### 2. `&.widgits/palettes/palettes_menu.sh` — same anti-pattern
- `compose_window()`/`compose_emojis()`/`compose_elements()` `printf` raw `<button>`/
  `<row>`/`<text>` XML directly into the `.chtpm` file, reading from CSVs
  (`chemistry_tiles_expanded🏆.csv`) and sprite-cache directories in bash.
- No manager, no live-update path. Currently functional (nav/click DO work here, unlike
  stats-hq) only because the content itself has no interactive state beyond
  onClick-dispatch strings the generic renderer already knows how to run — it got lucky,
  not because it's compliant.

### 3. `&.widgits/bookmarks/bm_menu.sh` — same anti-pattern
- `printf '<window class="database-window">...'` directly, same shape as the other two.

### The compliant reference pattern (what these should look like)
- `khtpm_hq_manager.c` (db-hq) — real, separate compiled binary, launched by the
  renderer's own `<module src="..."/>` tag via `fork()+execv()` (`launch_module()`,
  ported verbatim from `wraith_parser_alpha.c`), owns `common_events/` scanning and the
  "open in editor" action, talks to the renderer only through
  `#.desktop/db_hq_common_events.state.txt` / `db_hq_action.txt`.
- `khtpm_events_hq_manager.c`, `khtpm_open_hai_manager.c` — same real pattern, confirmed
  present and used by events-hq/open-hai.
- These three are the ONLY real manager binaries found in a `find -iname "*_manager.c"`
  sweep of `&.hq-apps/` + `&.widgits/` this session.

---

## Not yet audited — real gap in this document, not a claim of completeness

This session found these 3 by grepping for the `__TOKEN__`-splice signature and manually
checking a handful of `open_*.sh`/`*_menu.sh` launchers. **Not checked**: every other
taskbar-launched window/menu in the house. A real, thorough audit should grep every
`open_*.sh` and `*_menu.sh` under `*.monads/`, `&.hq-apps/`, `&.widgits/` for:
- Direct `printf`/`echo`/heredoc of `<` XML tags into a `.chtpm` file (the tell-tale sign)
- Absence of a corresponding real manager binary or `<module>` tag
- One-shot generation with no live-update mechanism for content that implies interactivity

---

## Status update, 2026-08-25

**#1 (stats-hq) is RESOLVED.** Got the full compliant rebuild this session: a real,
separate `stats_hq_manager.c` (matching `khtpm_hq_manager.c`'s own shape — init, poll
loop, publishes a structured `stats_hq_common_events.state.txt` projection), launched via
the same `<module src="..."/>` mechanism db-hq's own dashboard uses, with real sidebar-
item click switching (no more dead tabs). `dashboard.chtpm` is now static
sidebar+panel markup, not bash-regenerated per launch.

**#2/#3 (palettes/bookmarks) are now RESOLVED too, 2026-08-25 (same session, later
pass).** Real managers built, matching stats-hq's own proven shape exactly:
- `bookmarks_manager.c` — reads `<pal>/bookmarks.pdl`, publishes `<pal>/bookmarks_state.txt`
  (`name<TAB>path` rows). `bookmarks.chtpm`/`.css` are now static, provisioned once per pal
  from `bookmarks.template.chtpm`/`.css` (a plain token substitution + copy, not a
  per-launch regeneration). The renderer's own `dbhq_inject_bookmark_items()` builds real
  `<button>` rows from the published state at runtime. The chtpm-live-reload workaround
  mentioned above is deleted — it's exactly the dead code this doc's own standing rule
  predicted.
- `palettes_manager.c` — one binary serves both real categories (`emojis`/`elements`),
  told which one via `<module args="<category>"/>` (a small, generic renderer extension:
  `<module>` tags can now carry static extra argv, not palettes-specific). Reads the emoji
  pallet list or chemistry CSV (real quote-aware CSV parsing, not a naive `IFS=,` split -
  the old bash version silently mis-split quoted fields with embedded commas), pre-generates
  sprite tiles, publishes `palettes-<category>_state.txt`. The renderer's own
  `dbhq_inject_palette_tiles()` builds real `<row>`/`<button>` grids at runtime. Stub
  categories (rmmv/cdda/paint/...) stay fully static (title+hint+doc-link button, no
  manager needed — nothing to manage).
- Both `palettes_menu.sh`/`bm_menu.sh` are now thin launchers only: file ops (place a tile,
  add a bookmark row, provision a static template once) — zero XML generation.
- Real bugs found and fixed along the way (not architecture, but worth recording since they
  were hiding behind the old bash-compose path): a `sed` substitution silently corrupted on
  this house's own literal `&` in `&.widgits` (unescaped `&` in a sed replacement string);
  the SAME `&`-in-path issue broke a `sh -c` postcmd (unquoted `&` parsed as background-job
  operator) - fixed by single-quoting substituted paths, matching `do_add()`'s pre-existing
  "no single quotes in bookmark name/path" rule; `Elem.onclick` (512 bytes) silently
  truncated a real 913-byte postcmd (bumped to 1536, second time this exact buffer has hit
  this — see its own header comment history); a pre-existing double nav-assignment bug in
  `dbhq_assign_nav_indices()` (bookmarks' buttons got numbered twice, desyncing focus from
  the rendered highlight); `.pal-wide`'s `min-width`/`width:auto` were never supported by
  `css_layout_pass()` (only real `width:` is) — every wide element silently got `w=0`,
  invisible background, and crowded overlapping labels.

## Broader audit — DONE, 2026-08-25 (later pass). Zero additional instances found.

Full sweep of every `.sh` file under the house root for the anti-pattern's real signature
(not just the earlier targeted grep): `printf`/`echo` of a `<window`/`<button`/`<panel`/
`<item`/`<tab`/`<row`/`<text`/`<title` tag, any heredoc (`<< EOF`) whose body contains one
of those tags, and any script that writes (`>`/`>>`) to a `*.chtpm` path at all regardless
of composition style. Confirmed clean:
- The printf/echo-tag grep: zero matches anywhere (stats-hq/palettes/bookmarks were the
  only three, both now fixed).
- The heredoc sweep found ~20 files with a `.chtpm` string near a heredoc, but every one
  checked was either an unrelated state-file write (`cat > pieces/system/ez_state.txt`) or
  a comment/log line mentioning a `.chtpm` path, never actual markup composition.
- The `> *.chtpm` write-sweep found only `bm_menu.sh`/`palettes_menu.sh`'s own new
  one-time template-provisioning lines (real, intentional, not per-launch regeneration)
  plus two unrelated false-positive matches (`k3_flow.sh`, a mutaclysm test scenario) that
  don't actually write `.chtpm` content.

Conclusion: the printf-XML anti-pattern this doc exists to flag was fully contained to the
three apps already fixed. Other window-generating systems in this house (piececraft,
mutaclysm, tile-picker, event-ez, the `045.muchi-pal-agent` family, etc.) use their own
real compiled parsers/engines, not bash-composed `.chtpm` — genuinely different
architecture, not the same debt under a different name.

## 4. NEW real finding, 2026-08-31 — `dbhq_load_actors()` in `khtpm_entity_menu_render.c`

**A different, milder-but-real category from the printf-XML pattern above (the 2026-08-25
audit's own "fully contained to 3 apps" conclusion still stands for THAT specific
pattern — this is a genuinely separate violation, not a fourth instance of it).**

Found while investigating why reusing `class="db-hq"` for an unrelated new window showed
real Actor/RPG-database content instead. `dbhq_load_actors()` (in the shared, "hard
boundary" `khtpm_entity_menu_render.c` itself) reads a real PDL file
(`&.widgits/db-hq/data/actors.pdl`) and injects real rows directly — the CONTENT is real
file-based data, not hardcoded strings (confirmed: the displayed text does not appear
anywhere in the C source). **The violation is architectural, not data-fakery**: this is a
real "Manager owns projection" violation (TPMOS §11/§12, cited above) — the shared
renderer parses its own mode's sovereign data source inline, instead of a real, separate
manager publishing a structured projection the renderer only ever reads. Exactly the same
class of coupling risk this whole doc exists to name: every new mode tempted to add its
own inline loader to the SAME shared file compounds a real, load-bearing file nobody owns
end-to-end.

**This should not have happened** — the compliant pattern (`khtpm_hq_manager.c` for
Common Events) already existed in the SAME house, built the SAME week, for the SAME
window (db-hq), and Actors/Classes/Skills/etc. simply never got the same treatment.
**Condemned here explicitly, per direct instruction ("that being hardcoded... should be
condemned... it should have never happened")** — this is not a judgment call, it's a real,
avoidable miss.

**Real fix, not yet done**: a real `dbhq_actors_manager.c` (or extend an existing
manager's own scope) publishing a structured `db_hq_actors.state.txt` the shared renderer
reads generically via `reusable_slot()` — same real shape as Common Events, Stats-hq,
bookmarks, palettes already prove. See `au-31/00-todo.md`/`01-manager-design.md` for the
real, dated todo this produced and the direct instruction to fix this class of thing
BEFORE adding any new window mode to the shared file, so new work doesn't compound onto
non-compliant precedent. **Real follow-up, not yet done**: audit whether Classes/Skills/
Items/Weapons/etc. (db-hq's other tabs) have the same inline-loader shape — not checked
yet, real gap, same as this doc's own "Not yet audited" section above.

## 5. NEW real finding, 2026-08-31 — `khtpm_choice_picker.c` hand-builds its Elem tree, never parses a real `.chtpm`

Found while building the network-browser-hq centroid proof app: I (the assistant)
initially cited `khtpm_choice_picker.c` as real house precedent for a small,
wholly data-driven khtpm app skipping `.chtpm` authoring/parsing entirely and
constructing its `Elem` tree by hand in C. **Direct correction: "we still want
to use chtpm+layout module, we always should no matter what"** — there is no
size/complexity exception. See `CENTROID_GOLD_STD.md` §3 rule 1 (corrected same
day) for the now-standing rule this produces, and the
`khtpm-always-parse-chtpm-layout` memory for the full reasoning.

**Real, confirmed callers of `khtpm_choice_picker.c` today** (grep-confirmed,
not assumed) — this is live, load-bearing infrastructure, not dead code:
- `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c` — the desktop-tile
  context-menu system execs this binary directly for real, live "Show Choices"
  popups.
- `*.monads/*.book-stack/pieces/reader/event_pkg/pages/page_1/dispatch.sh` —
  book-stack's own real dialogue/menu-choice flow generates a real
  `choices_file` and execs this binary to display it.
- `&.widgits/tile-picker/ops/khtpm_show_choices.c` — the real, documented
  caller/design predecessor (its own header comment: "this binary IS the
  picker... simpler and lower-risk... than to also touch tp_desktop_window_
  rgb.c's own live SHOW_PAGE relay handler").

**Real fix, not yet done**: refactor `khtpm_choice_picker.c` to author a real
`.chtpm`+`.css` for its own chrome (title bar, row list, Cancel row) and parse
it through the real shared pipeline, injecting the caller-supplied
`choices_file` rows into the parsed tree via `reusable_slot()`/
`elem_inject_loop()` instead of hand-populating `Elem` structs directly. Not
started — flagged here so it isn't silently left as the "acceptable exception"
example it was wrongly treated as during this session.

## Cross-references
- `house-compaction.md` — the separate (also real, also HIGH priority) receipt/frame-
  history compliance gap found earlier this session in `khtpm_hq_render.c`. Different
  finding, same session, same severity class — read both.
- `khtpm-merge-how2.md` — the merge status doc; note its own "kept live for stats-hq"
  language about `khtpm_hq_render.c` will need updating once stats-hq's real
  remediation (not just a launcher repoint) is designed.
