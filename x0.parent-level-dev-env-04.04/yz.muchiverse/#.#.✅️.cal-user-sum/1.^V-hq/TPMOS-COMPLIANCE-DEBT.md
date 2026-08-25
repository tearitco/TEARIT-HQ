# TPMOS-COMPLIANCE-DEBT.md — real, confirmed architecture violations

**Severity: HIGH. Standing #1-tier priority, alongside `house-compaction.md`'s receipt
finding.** Written 2026-08-25, direct instruction: "this cant propagate forward into the
codebase by naive agents." This doc exists specifically so a future agent (naive or not)
reads this BEFORE copying any of the patterns below as a "working example."

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

**#2/#3 (palettes/bookmarks) are PARTIALLY addressed — don't mistake this for resolved.**
Both were migrated off the deprecated standalone `khtpm_hq_render.c` onto the merged
`khtpm_entity_menu_render.c` binary (that file has since been deleted outright — see
`khtpm-merge-how2.md`). That migration fixed the *decommission* concern (no more stale
duplicate binary to maintain) and, for bookmarks, added the `open:`/`exec:`/`input:`
generic dispatch + chtpm-live-reload it needed. **It did NOT touch the actual violation
this doc is about**: `palettes_menu.sh`/`bm_menu.sh` still `printf` raw `.chtpm` XML
directly in bash, with no manager process and no testable Op. That remediation (item 2/3
below) is still real, open work — the renderer-binary migration was a separate, narrower
fix that happened to land first because a live "keep the old window working" report
forced the priority order.

## Recommended remediation priority (stats-hq done; palettes/bookmarks still open)

1. **stats-hq first** — it's the only one that's actually broken (dead tabs), not just
   non-standard. Needs a real compliant redesign: either a genuine manager binary
   (heaviest, most correct) or, if stats-hq's own scope is judged too small to warrant a
   full manager process, at minimum a real compiled Op (§12-testable) replacing the
   inline bash regex-scrape, PLUS real tab-click wiring (either give stats-hq its own
   `TAB_LABELS`-equivalent live array, or generalize the tab-match logic to be
   data-driven instead of a fixed C array — check which approach the house's own
   "no hardcoded UIs" standing rule 7 / `!.HOUSE_STDS.md` §K would actually prefer before
   picking one).
2. **palettes and bookmarks** — lower urgency (currently functional), but same
   remediation shape once stats-hq's pattern is proven, so the fix isn't invented twice.
3. **The broader audit** (see "Not yet audited" above) should happen before or alongside
   #2, since it may surface more instances worth fixing in the same pass.

## Cross-references
- `house-compaction.md` — the separate (also real, also HIGH priority) receipt/frame-
  history compliance gap found earlier this session in `khtpm_hq_render.c`. Different
  finding, same session, same severity class — read both.
- `khtpm-merge-how2.md` — the merge status doc; note its own "kept live for stats-hq"
  language about `khtpm_hq_render.c` will need updating once stats-hq's real
  remediation (not just a launcher repoint) is designed.
