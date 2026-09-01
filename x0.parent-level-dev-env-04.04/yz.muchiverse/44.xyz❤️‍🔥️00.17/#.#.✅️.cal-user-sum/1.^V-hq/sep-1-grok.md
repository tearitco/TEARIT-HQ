# sep-1-grok.md — making the rest of db-hq's RPG Maker menus functionally accurate

Purpose: explain, concretely and from real code (not guesswork), how
this house's own events/pal-script/session/desk systems actually work
today, so the remaining db-hq RPG Maker-style tabs (Actors, Classes,
Skills, Items, Weapons, Armors, Enemies, Troops, States, Animations,
Tilesets, System, Types, Terms — see `DB_HQ_TAB_LABELS[]`,
`khtpm_core_render.c:1097`) can be brought up to the SAME real standard
"Common Events" already meets, instead of staying flat name-lists.

## Where "events" already live (the reference implementation)

db-hq's tab bar has 15 tabs; index 11, `DB_HQ_COMMON_EVENTS_TAB`, is
"Common Events" — this IS the real RPG Maker MV/MZ-style event system,
already functionally accurate, and it's the template every other tab
should be measured against.

**The chain, end to end:**

1. `khtpm_core_render.c`'s db-hq mode shows a flat sidebar list of
   event names, read from `#.desktop/db_hq_common_events.state.txt`
   (`dbhq_load_common_events()`, line ~1226) — one name per line.
2. Selecting one calls `dbhq_ce_open(ce_name)` (line ~4113), which
   points `g_evhq_pkg_dir` at
   `<house_root>/common_events/<ce_name>/event_pkg` and launches a
   REAL, separate manager process, `khtpm_events_hq_manager.+x`,
   scoped to that one directory — embedded into the SAME db-hq window
   (RPG Maker's own "one dialog, sidebar + editor together" shape,
   direct instruction from 2026-08-26, not a spawned separate window).
3. That manager owns the actual RPG Maker Event Editor semantics:
   **pages** (each with its own trigger — Autorun/Action
   Button/Parallel/etc, stored in `condition.pdl` per page) and, within
   a page, an ordered **command list** (show text, change gold, show
   choices, conditional branches, etc — the same command vocabulary
   RPG Maker itself uses).
4. On every save, the manager compiles the page's command list into
   THREE real, inspectable artifacts on disk (not a binary blob):
   - `event.ir.pdl` — the intermediate representation, human-readable
     `SECTION | KEY | VALUE` rows (`NODE | id=N type=<cmd> |
     <params>`) — this is the actual source of truth for "what does
     this page do", editable/diffable/greppable.
   - `event.pal` — compiled from the IR, real shell opcodes (`exec
     cmd_N.sh`, `halt`) — "pal" = this house's own tiny bytecode-ish
     script format (prisc+x opcodes), NOT related to `muchi-pal-agent`
     (a separate, unrelated AI-persona app elsewhere in the tree —
     don't conflate the two "pal"s).
   - `cmd_N.sh` — one real shell script per command, each one a thin
     wrapper that locates the house root (via walking up for an
     `xyzfs` marker dir) and execs the real op binary for that command
     kind (e.g. `cmd_1.sh` in the `greet_player` example execs
     `*.monads/*.muchi-pet/ops/+x/mr_change_gold.+x` with the event's
     own package dir + amount).
5. Running the event is just executing `event.pal` top to bottom —
   each `exec cmd_N.sh` really runs, in order, and `halt` ends the
   page. No custom interpreter beyond `sh` itself; the whole event
   *is* a tiny real shell program.
6. State that events read/write while running lives as flat files
   right next to the event package: `common_events/<name>/history.txt`,
   `messages.txt`, `inventory.txt`, `choice_result.txt`,
   `master_ledger.txt` — same "everything is a plain, greppable file"
   convention as `#.desktop/` uses for the renderer/manager split.

**The standard to hold every other tab to:** a real backing directory
per row (not just a name string), a real manager process that knows
that tab's own domain vocabulary, and IR + compiled + per-step-script
artifacts that are all genuinely inspectable and independently
runnable — not a flat editable list masquerading as a database.

## What "functionally accurate" should mean for the OTHER tabs

None of Actors/Classes/Skills/Items/Weapons/Armors/Enemies/Troops/
States/Animations/Tilesets/System/Types/Terms have been audited this
session for how close they already are to this standard — that's the
real, concrete first task: for each tab, check whether it already has
a `dbhq_load_<tab>()`-style state reader (grep
`khtpm_core_render.c` for `DB_HQ_<TAB>_TAB` to find its current
handling) and whether selecting a row does anything beyond showing a
label, or whether it's still a flat list with no real backing package.
Do NOT assume symmetry — "Common Events" got real depth because it's
literally the thing being executed; something like "Tilesets" may
correctly need a much simpler real structure (an actual tileset image
+ passability data file, not an event-style command list) — the
RPG-Maker-accurate shape differs per tab, this doc isn't asking for
event_pkg/pal-script clones everywhere, it's asking for whatever THAT
tab's own real RPG Maker equivalent actually stores and does.

## User sessions / files / desks

These are three genuinely separate concepts in this house — don't
conflate them:

- **"Sessions"** (as in open-hai's own sidebar) = one saved AI chat
  transcript, one directory per session under open-hai's own state
  dir, tracked in a flat ledger the manager publishes from. Nothing to
  do with desktop window sessions.
- **"Files"** = the plain-file convention used throughout: almost all
  cross-process state in this house (renderer ↔ manager, action
  requests, published lists) is a flat text file under `#.desktop/` or
  a package's own state dir, read/written with simple mtime-gated
  polling — no database, no sockets, no shared memory. This is
  deliberate and load-bearing; don't introduce a different IPC
  mechanism for a new tab without a real reason.
- **"Desks"** = the livedesk taskbar's own saved-layout concept —
  `khtpm_taskbar_manager.c` reads/writes `<state_dir>/desks/<name>.pdl`
  (see its own `desks`/`.pdl` handling around line 2721) — a desk is a
  named snapshot of what's laid out on the desktop (tile positions
  etc), separate from any one app's own session/file state. If the
  RPG-Maker-menu work needs desk-level persistence (e.g. remembering
  which db-hq tab was open), this is the real existing mechanism to
  hook into, not a new one.

## Concrete next step

Before writing any new tab-specific code: for each of the 13
non-Common-Events db-hq tabs, grep `khtpm_core_render.c` for its own
`DB_HQ_<NAME>_TAB` constant, read what `dbhq_layout_pass`/whatever
handles selection for it currently does, and report back (or add to
this doc) which tabs are already real vs. which are still flat
name-lists needing a real backing structure — that audit hasn't been
done yet and should come before design work on any specific tab.
