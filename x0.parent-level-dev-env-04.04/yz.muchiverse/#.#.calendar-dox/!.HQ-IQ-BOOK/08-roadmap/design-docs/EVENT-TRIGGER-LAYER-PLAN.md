The Event Trigger Layer — Design Doc
=====================================
Promoted from `1-1.HARNECIENT.SMOL/NIGHT_05_THE_TRIGGER_LAYER.txt` and
`12.calendar/2026-09-13/sept-13-events-trigger-buildout-tracking.md`
(whose §2 legacy-events-hq state check is now fully CLOSED — this doc
is the real "next step" that closure pointed at). Reference doc,
2026-09-14. VISION PHASE — nothing in this doc is built yet, but every
claim below is checked against real, current code, not the narrative's
own paraphrase of it.

## 1. The actual gap

Today, exactly one thing can start a Common Event: a human opens the
events-hq editor and presses **▶ Play** (`dashboard.chtpm`'s
`<button id="play-test">` → `khtpm_events_hq_manager.c`'s `play` action
→ `play_event.sh`). The `trigger=on-click`/`trigger=player-touch`
labels already present in real event data
(`@.apps/piececraft-hq/pieces/system/maps/cdda_sample/events.pdl`:
`EVENT | x=6 y=5 glyph=t | trigger=player-touch cmds=change_hp,
change_state`) are inert filter tags `play_event.sh` reads only after
a human has already pressed Play — confirmed no code path anywhere
compares them against a real player action. This doc's job: give
events a second, REAL, automatic way to start — walking a piece onto
`x=6,y=5` fires `change_hp`/`change_state` on its own, no editor, no
Play button.

## 2. Real, existing groundwork — checked today, not assumed

- **Per-tick player position already exists as a real file**:
  `pc_compose_frame.c` (~line 327-331) reads
  `pieces/hero_01/state.txt`'s `pos_x`/`pos_y`/`pos_z` keys every frame
  it composes — this is the real, current, already-written per-tick
  position state NIGHT_05 asked "does the engine already write this
  anywhere?" about. It does. (board-viewer's own separate
  `scene_receipt.pdl` — `hero_x`/`hero_y`/`selector_x`/`selector_y` —
  is a DERIVED snapshot one layer up, written by `bv_render_3d`, not
  the engine's own source of truth; `hero_01/state.txt` is the real
  root.)
- **The `board_events.txt` NIGHT_05 proposed does NOT exist yet** —
  confirmed: `player-touch`/`touched_npc`/`board_events` appear
  nowhere in the codebase except as inert data inside `events.pdl`
  itself. This is real, net-new work, not partially built.
- **A real watcher-process precedent already exists in this exact
  app**: `piececraft-hq/ops/launch_hq_info_window.sh` (line 66)
  already does `tail -f "$STATE_FILE"` as a live status viewer — a
  genuinely separate process, no linking, matching this house's
  standing "separate file-watcher, not a linked module" rule. The
  trigger-layer watcher should be built the same shape (its own small
  script/binary, not new code inside `pc_compose_frame.c` or
  `khtpm_events_hq_manager.c`).
- **`play_event.sh`'s trigger-string lookup is a page-scan by exact
  match, not a registry** (confirmed reading the script, lines 69-91):
  `awk` pulls the `condition.pdl`'s `COND | trigger | <value>` row for
  each page under `event_pkg/pages/page_*` and keeps the
  highest-numbered page whose value equals the filter argument
  (default `"on-click"`). **This part is real and reusable as-is** —
  the bridge script doesn't need its own lookup table, it can just
  call `play_event.sh <pkg_dir> <house_root> <trigger_string>` with
  the real trigger string it read off the board-events line. Confirms
  NIGHT_05's own claim: "the bridge script doesn't need to reimplement
  what an event DOES, only recognize WHEN one should run."
- **The effect side (db-hq reads/writes, Show Text, Change Gold) is
  proven, live-verified this week** — not touched by this doc at all.

## 3. The real, minimal design

**Step 1 — engine writes `board_events.txt`.** A small, real addition
to whatever piececraft-hq code already detects tile-adjacency/pickup
(needs its own follow-up code search into `pc_world_manager.c`/
`pc_generate_chunk.c`'s movement-resolution path before implementation
— not done in this doc, flagged as the real first implementation
task). Append-only, one line per event, same convention every marker
file in this house already uses: `touched_npc:NAME`,
`picked_up:ITEM_ID`, `entered_tile:X,Y`. Lives alongside
`pieces/hero_01/state.txt`, e.g. `pieces/display/board_events.txt`.

**Step 2 — the bridge script.** A new, small, standalone process
(`tail -f`-shaped, per §2's own real precedent) watching
`board_events.txt`. For each new line:
1. Parse the trigger string (`touched_npc:NAME` etc.).
2. Look up whether ANY registered Common Event's `condition.pdl` has a
   matching `COND | trigger | <value>` row for that exact string — via
   the existing page-scan logic `play_event.sh` already has (reused,
   not reimplemented).
3. If found, call `play_event.sh <pkg_dir> <house_root> <trigger_string>`
   exactly the way the editor's own Play button already does.

**Step 3 — the smallest provable proof, per NIGHT_05's own bar**: one
NPC, one trigger string, one Common Event, end to end. Walk the player
piece onto `x=6,y=5` in `cdda_sample` (a real, already-existing
`trigger=player-touch` row), confirm `board_events.txt` gets
`touched_npc:t` (or whatever the real glyph/name resolves to)
appended, confirm the bridge script fires `change_hp`/`change_state`
on its own, confirm a real, checkable state change (an hp value in
`hero_01/state.txt`, or a screenshot) — not "should work."

## 4. Open, real question before implementation starts

- Exactly where in `pc_world_manager.c`/`pc_generate_chunk.c` does
  movement get resolved each tick, and is there already an
  adjacency/collision check close enough to append the trigger line
  from, or does this need new tile-lookup code? **Not yet researched —
  first real task when this doc moves from plan to build.**
- Does the bridge script need to be a long-running daemon (like
  `pc_clock_daemon.c`) or can it be spawned/torn down alongside the
  piececraft-hq session itself? Match whichever existing lifecycle
  convention `pc_clock_daemon.c` already uses — don't invent a new one.

## 5. Downstream

This is the direct dependency the "move" Common Event / entity harness
work (`PLAY-MODE-ENTITY-HARNESS-DESIGN.md`) needs — an NPC that can
wander/pathfind into a trigger tile only matters once something is
actually listening for it to arrive. Build this doc's Step 1-3 first;
that design doc's own §7 sequencing already says so.

## Next step

Not started. First real task: trace `pc_world_manager.c`'s movement-
resolution path to find (or confirm the absence of) an adjacency check
Step 1 can hook into.
