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

## 3. The real, minimal design — REVISED 2026-09-14, real hook point found

**§4's own open question is now answered — and the answer changes this
plan for the better: the "master ledger" this needs already EXISTS and
is already live**, not something to build. Confirmed by direct code
read: `pc_menu_input.c` and `pc_clock_daemon.c` BOTH already call a
real `ledger_append(root, turn, actor, action_type, details)` that
appends to `data/master_ledger.txt` in exactly the
`timestamp|turn|actor|action_type|details` row format — the SAME
format, independently, as `101.lpns+map+4`'s own proven ledger
(`RMMV-EVENT-ARCHITECTURE-LEARNINGS.md` §7). It's already multi-writer
TODAY: the player's own `MOVE` action appends `player|move|tick:N`
(`pc_menu_input.c` line 1001) and each animal's own wander tick appends
`<entity_id>|wander|x:N,y:N` (`pc_clock_daemon.c` line 194) to the
exact same file. **`board_events.txt` as a separate new file is no
longer the right design** — superseded by this finding; use
`master_ledger.txt` directly instead, one new `action_type` value
(`touched_npc`, `picked_up`, etc.) alongside the `move`/`wander`/
`end_turn`/`jump` ones already there.

**Step 1 — one new check inside the existing `MOVE` handler.**
`pc_menu_input.c`'s `MOVE` case (~line 987-1001) already runs on every
real player movement, AFTER board-viewer's own `bv_menu_input.c` has
already written the player's new `pos_x`/`pos_y` to `xelector`'s state
file (confirmed by that handler's own header comment: "the xelector's
own pos_x/y/z were already written directly by board-viewer... before
this ever arrives here"). Add: read the player's now-current position,
compare against `events.pdl`'s registered `EVENT | x= y= |
trigger=player-touch` rows, and on a match call the SAME
`ledger_append()` already declared right there (line 304) with
`action_type="touched_npc"` (or similar) instead of `"move"`. No new
file, no new write mechanism — one new comparison and one new call to
a function that's already sitting in scope.

**Step 2 — the bridge script.** A new, small, standalone process
(`tail -f`-shaped, per §2's own real precedent) watching
`data/master_ledger.txt` for new lines (not `board_events.txt` — see
above), filtering for the new `touched_npc`/etc. `action_type` values
specifically. For each match:
1. Parse the trigger string (`touched_npc:NAME` etc.) out of the
   ledger row's `details` field.
2. Look up whether ANY registered Common Event's `condition.pdl` has a
   matching `COND | trigger | <value>` row for that exact string — via
   the existing page-scan logic `play_event.sh` already has (reused,
   not reimplemented).
3. If found, call `play_event.sh <pkg_dir> <house_root> <trigger_string>`
   exactly the way the editor's own Play button already does.

**Step 3 — the smallest provable proof, per NIGHT_05's own bar**: one
NPC, one trigger string, one Common Event, end to end. Walk the player
piece onto `x=6,y=5` in `cdda_sample` (a real, already-existing
`trigger=player-touch` row), confirm `data/master_ledger.txt` gets a
new `...|touched_npc|...` line appended, confirm the bridge script
fires `change_hp`/`change_state` on its own, confirm a real, checkable
state change (an hp value in `hero_01/state.txt`, or a screenshot) —
not "should work."

## 4. Open, real question before implementation starts

- ~~Exactly where in `pc_world_manager.c`/`pc_generate_chunk.c` does
  movement get resolved each tick~~ **ANSWERED (2026-09-14)**: it
  doesn't — `pc_world_manager.c` is world reset/init only, no movement
  code. The real per-tick position-write happens cross-project in
  board-viewer's own `bv_menu_input.c`; the real POST-move hook
  piececraft-hq owns is `pc_menu_input.c`'s `MOVE` handler, per §3
  Step 1 above.
- ~~Does the bridge script need to be a long-running daemon~~
  **ANSWERED (2026-09-14, confirmed with user)**: yes, a persistent
  forked companion process — it must stay alive for the whole session
  to catch new ledger lines as they happen, a one-shot op would miss
  everything after the moment it ran. Match `pc_clock_daemon.c`'s exact
  lifecycle convention (`launch_clock_daemon_if_needed()`'s PID-file +
  `kill -0` liveness check, forked once per world at
  `CONFIRM_START`/`CONFIRM_START_DEBUG`) — a second daemon launched the
  same way, alongside the clock daemon, not a new pattern.

## 5. Downstream

This is the direct dependency the "move" Common Event / entity harness
work (`PLAY-MODE-ENTITY-HARNESS-DESIGN.md`) needs — an NPC that can
wander/pathfind into a trigger tile only matters once something is
actually listening for it to arrive. Build this doc's Step 1-3 first;
that design doc's own §7 sequencing already says so.

## Next step

Not started (no code written yet). Real hook point found and
documented (§3/§4 above) — next actual task is implementing §3 Step 1
(the adjacency check inside `pc_menu_input.c`'s `MOVE` handler) and
Step 2 (the bridge watcher on `data/master_ledger.txt`), then proving
§3 Step 3 end to end.
