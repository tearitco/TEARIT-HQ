# khtpm-merge-how2.md — merging the 5 khtpm_*_render.c copies into one shared parser

**Written:** 2026-08-15. **Compacted:** 2026-08-24 (doc-compaction pass) — the
step-by-step Stage 1-5d execution trail (2839 lines, 21 dated sub-sections of
"real findings/DONE" logs) has been moved verbatim to `archive/khtpm-merge-how2.ARCHIVE.md`
in this same directory. Everything below is current and complete for
understanding where this merge effort stands today; open the archive only if you
need the historical blow-by-blow of HOW a specific stage got done (exact diffs,
exact bugs hit, exact verification commands per stage).

---

## CURRENT REAL STATUS (2026-08-16, still accurate as of the 2026-08-24 compaction pass)

**Stage 5 (literal single-binary merge) is DONE for all 5 window apps.**
entity-menu, taskbar-settings (swatch-picker mode), db-hq, events-hq,
and chat-hai all now live in ONE compiled binary,
`*.livedesk-taskbar/ops/khtpm_entity_menu_render.c`, mode-selected via a
real `class=` attribute on each app's own `<window>` tag. Each app's own
distinct logic (db-hq's tag/id `activate_elem()` + tabs/sidebar/panel,
events-hq's live file-watch-poll + drag + modal overlay, chat-hai's
session sidebar + ledger-poll + composer) was ported as a documented
per-mode branch/exception, not forced into one shared abstraction — see
the ARCHIVE's §5d.6 through §5d.13 for the real, full blow-by-blow
(window-shape survey, the one-binary decision, each app's merge pass, 2 real
chat-hai bugs found+fixed post-merge, and the legacy-source archive).

Old standalone renderers for events-hq/chat-hai/taskbar-settings + their
build scripts + old binaries are archived (zipped, dereferenced) at
`_.ARCHIVED-pre-merge-legacy.zip` (house root). db-hq's own old
standalone renderer (`khtpm_hq_render.c`/`build_db_hq.sh`) was kept live
past the initial merge because `stats-hq` and `palettes` (and later
`bookmarks`) still independently launched it against their own `.chtpm`
files. That's now resolved: **2026-08-25, all three were migrated onto
the merged `khtpm_entity_menu_render.c` binary** (stats-hq got a full
TPMOS-compliant rebuild with a real manager; palettes got sprite/badge/
nav ports plus a font-cache and nav-double-assignment fix; bookmarks got
the `open:`/`exec:`/`input:` generic mechanisms, chtpm-live-reload, and
inline emoji-in-text rendering ported over). A `grep -r
"khtpm_hq_render"` across the tree confirmed zero remaining launch sites
(comment-only references remain in a few build scripts). `khtpm_hq_render.c`
and `build_db_hq.sh` have been deleted outright (not zip-archived like the
others — nothing referenced their binary artifacts, and full git history
already preserves the file if it's ever needed again).

**Still real, open work**: legacy GL migration — **current status now lives in
`legacy-shared-fix.md`, not here** (as of 2026-08-17: 3 of 16 projects converted
to the shared `x11_mirror.c` binary, 13 remain). This doc's own §5c sections in
the archive are the historical discovery/inventory for that thread, not its
live status — read `legacy-shared-fix.md` for what's actually still open there.
The taskbar's own `LayDoc`/`khtpm_strip_layout.h` architecture stays
intentionally separate from Elem/CSS (a confirmed, deliberate stop, not
unfinished work — see the archive's closing sections for why).

---

## HOUSE STANDARD (added 2026-08-16) — the real decision rule for "should this
## be shared, and how" — kept here in full, this is load-bearing, not historical

**Direct instruction, quoting exactly**: "i love using tpmos as std
across the house cause it makes choices easier and things stay in same
shape. pls communicate that in docs as well it was lack of
understanding of this that has caused the drift we are now fixing in
the first place."

**The rule, stated plainly**: `1.TPMOS_c_+rmmp.0103.0001` (root
`button.sh`, `pieces/chtpm/`, and any real project under `projects/`)
IS this house's real, load-bearing architecture standard — not a
distant inspiration, not one option among several, THE reference. When
a design question comes up in khtpm/livedesk work ("should this be
shared? how? one binary or many? where does this data live?"), the
right first move is always: go read the real, live TPMOS structure for
the equivalent case, then match its shape — not invent a locally
plausible-looking alternative and rationalize it afterward.

**The real, live example of what happens when this rule is skipped**
(2026-08-16 session): a real, confirmed finding that
`dump_frame_png()`/`poll_agent_relay()`/`launch_module()` were
independently copy-pasted, same-shaped, across 6 khtpm apps got
"fixed" by moving the duplicated TEXT into one shared `.c` file
(`khtpm_relay_utils.c`) and `#include`-ing it into every consumer —
the exact same "shared SOURCE, not shared BINARY" shape this document
had already identified elsewhere as the real, root cause of this house's
drift from TPMOS. It still compiled N duplicate copies into N binaries;
it only LOOKED deduplicated in the source tree. Direct correction: "u are
using include instead of launching the binary thru fork/exec/sys like
tpmos/wraith does. this is the standard for binary calls." Real fix
applied same session: reverted the `#include`-based version entirely,
built a real standalone `dump_frame_png_op.+x` (`&.widgits/_shared-
lib/ops/dump_frame_png_op.c`, matches the real, confirmed shape of
`1.TPMOS_c_+rmmp.0103.0001/projects/fuzz-op/ops/toggle_clock.c` —
own `main()`, own X connection, one discrete job, exits) and had
taskbar-settings invoke it via `system()` — verified live, byte-
identical PNG output, but now genuinely one compiled copy, invoked as
a real subprocess, matching TPMOS exactly.

**Concrete decision rule, going forward, for "should this be shared,
and how":**
- Is it a genuinely independent, possibly-long-running process with
  its own lifecycle (business logic, a persona loop, a module)? →
  real `fork()`+`execv()`, tracked pid, matches `launch_module()`
  (already correct, ported verbatim from `wraith_parser_alpha.c`).
- Is it a discrete, occasional, single-purpose action (a screenshot
  dump, a state-file mutation, a toggle) that can run to completion
  and exit? → a real, standalone op binary in `&.widgits/_shared-
  lib/ops/` (or an app-local `ops/` for something genuinely
  app-specific — see the real `projects/<id>/ops/*.c` vs `pieces/
  chtpm/ops/*.c` split TPMOS itself uses), invoked via
  `system()` or `fork()`+`execl()`, matching `toggle_clock.c`/
  `resolve_project_op.c`.
- Is it pure, stateless, per-frame-hot-path logic that must run
  in-process because it needs direct access to the caller's own live
  memory/callbacks (the Elem tree, `handle_key()`, an open X
  connection's `Display*`/`Pixmap`)? → THIS is the one real,
  legitimate case for `#include`-ing shared `.c` source
  (`khtpm_render_core.c`, `khtpm_css_parser.c`) — still N compiled
  copies, same as TPMOS's own per-app duplication tolerance for truly
  hot-path code, but honestly labeled as such, not mistaken for real
  binary-level reuse.
- When genuinely unsure which bucket something falls into: go read the
  real TPMOS example for the closest equivalent case FIRST, don't
  guess from what "feels" reusable.

---

## What's in the archive (`archive/khtpm-merge-how2.ARCHIVE.md`), section map

Read the archive only if you need the historical HOW, not just the current WHAT
(already covered above). Section numbers match the archive's own headers:

- **§0-2** — why this doc exists, the file inventory, target architecture (one
  shared core, thin per-app shims)
- **§3 (Stage 1)** — extracting proven utility functions (redraw/dump split, font
  caching, elem-pool dirty-flag rewind) — DONE on all 5 apps
- **§4-5 (Stage 2-3)** — shared Elem/parse_chtpm()/draw primitives, layout_pass()
  generalization — DONE for taskbar-settings + ai-cell (partial), see §5.1b/5.2b
  for the real per-app findings
- **§5b (Stage 4)** — wraith-alpha-style single-parser segregation — reversed an
  earlier "don't do this" call after real evidence; scope/risk notes only, this
  stage was never fully executed
- **§5d (Stage 5)** — the real one-shared-binary merge, DONE for all 5 apps;
  §5d.6-§5d.13 are the actual per-app merge logs (db-hq, events-hq, chat-hai)
  including 2 real post-merge bugs found+fixed
- **§5c** — legacy GL migration inventory/first-pilot — status now tracked live in
  `legacy-shared-fix.md` instead, this section is historical-only
- **§6-8** — the taskbar's own place in the merge (deliberately separate),
  testing convention, concrete order-of-operations
