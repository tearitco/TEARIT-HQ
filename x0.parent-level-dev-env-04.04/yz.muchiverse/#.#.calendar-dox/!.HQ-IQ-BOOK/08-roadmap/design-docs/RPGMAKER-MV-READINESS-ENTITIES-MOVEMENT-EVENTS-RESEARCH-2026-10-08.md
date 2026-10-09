# RPG Maker MV readiness: entities, movement, the placer and events (research)

Written 2026-10-08, claude. Research and write-up only, nothing built. It replaces the
starting point of `ENTITY-DRIVEN-GAMEPLAY-VIA-CONTEXT-MENUS-PLAN` (marked PROVISIONAL): that plan
was written without reading the move placer, the event system or the RPG Maker MV source
notes, and the owner said so ("you are misunderstanding what I mean by entities, movement:
look at move placer work, events etc."; "Grok is better at RPG Maker, we just have to get
everything set up and documented better").

Every "exists" below was read in a doc or code header on 2026-10-08. What I did **not**
do: build, run, or read the full body of the 1,225-line placer. Items marked **(inferred)**
are my reading.

## 1. The one-paragraph answer

In this house an **entity is a folder** (a desk pal) and also a window; its verbs are
`METHOD` rows in `meta.pdl`, its menu is `menu.chtpm`, and its place is a `DESK` row in a
**page file**. **Movement is not the entity walking by itself.** It is: the player (the
cursword, the xelector, or a possessed body) picks **Act → Move**, a **range matrix** of
`#`/`.` cells is drawn, the player clicks a cell, a path planner turns it into waypoints in
an append-only **animation queue**, and a ticker applies them (desk: `XMoveWindow` per tick;
pc-hq: one cell per engine step). The result is written back to the page row and a ledger
line. **Events** are RPG Maker style *pages* (`event_pkg/pages/page_N/` with a
`condition.pdl` trigger) whose commands come from a data registry of ~87 commands. What is
missing is the glue RPG Maker gets for free: a **map loop** that, after every move, checks
touch triggers, steps NPCs on their move routes, and runs autoruns. That glue is designed
(sections 6 and 8) and mostly not wired.

## 2. Entities (what an entity is, on the desk and in pc-hq)

| Fact | Where |
|---|---|
| Entity = folder: `meta.pdl` (METHOD rows), `menu.chtpm`, `desktop_pos.txt` (`x= y= z=`), `inventory/` (items are nested entities), `zz.phone`, events in `event_pkg/` | desk pals under the user's `pals/` |
| Act menu is **generated** from `entity-cli/skills.pdl` (`SKILL | Move | move`, Use, Attack) by `open_entity_act.sh` / `act_row.sh` / `act_menu_row.sh` | entity-cli |
| Page file = the shared store: `DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n` (px at or above 40 are cells times 80) | `PAGE-FILE.md` |
| pc-hq keeps private copies (`hero_01/state.txt`, `xelector_01/state.txt`, `animals.txt`...) and is being moved to read and write the page rows (steps 1-6 proved) | `PAGE-FILE.md`, `LEARNINGS.md` |
| Z is per entity (`z=`) plus `#.desktop/desktop_active_z.txt`; cursword `c`/`v` set the visible level | `MOVE-AND-TAKE.md` |
| xelector = pc-hq cursor entity with `possessed_id`; cursword = desk cursor, will be the possessor on both; "a window is an entity, and vice versa" | `XELECTOR-ENTITY.md`, `CURSWORD-POSSESSION-DESIGN.md` |
| Items are entities in `inventory/`; Take = `mv` into the taker's inventory (data layer built and tested on a throwaway house: `khtpm_inventory.c`, `inventory_op`), Place = pop the selected slot | `CURSWORD-POSSESSION-DESIGN.md` 5b |

The owner's model, as I now read it: **everything that appears is an entity row on a page;
every action on it is a menu verb; every verb fires an event page.** RPG Maker's *event* is
exactly this (a thing on a map with pages and commands). So the cleanest mapping is
**entity = RPG Maker event (or actor)**, **page file = the map's event list**, **book = the
project, page = the map**.

## 3. Movement: the placer pipeline, step by step

1. **Arm.** Act → Move runs `move_entity_on_desk.sh <house> <entity_dir>` (pure launcher by
   rule; kills a stray placer for the same entity, exports the entity's origin as
   `TP_ORIGIN_X/Y`, sets `FE_PLACE_CLICK=<entity>/move_click.txt`).
2. **Range.** `tp_gen_range_matrix.+x` writes a `#`/`.` matrix centred on the origin.
   `khtpm_move_range.c` (`mvr_*`, shared text-include) holds the pure part: the matrix, a
   **depth map** (how many z-levels deep a `#` reaches, for the 3D diamond), a **stepped
   path planner**, and the **waypoint queue** `animation_queue.txt` plus `.cursor`
   (append-only ledger and cursor, never rewritten).
3. **Pick.** `tp_arm_placer_rmmv.c` opens a full-screen InputOnly window (no grab; the
   Mutter/XWayland finding in its header: real clicks only reach X11 when they land on a
   mapped surface), draws the labelled 80 px grid (columns A.., rows 1..), and writes the
   clicked cell's reference px to `move_click.txt`. Keys can jump to a typed cell ref.
4. **Plan and animate (desk).** `move_entity_init.+x` snaps to the grid, clamps, plans
   waypoints, starts the pal loop; `move_entity.pal` is `exec ./ops/move_entity_tick.+x;
   sleep 90000; j loop` (prisc `sleep` is microseconds); the tick applies one waypoint
   with `XMoveWindow` and advances the cursor.
5. **Write back.** The entity's `desktop_pos.txt` and its page row; a ledger line
   `entity_move` goes to `data/master_ledger.txt`.
6. **pc-hq side.** `bv_move_range.c` / `bv_dispatch.c` consume the same matrix and queue
   at one cell per engine step; the 3D diamond is `|dx|+|dy|+|dz| <= 2` computed in C
   (not yet per-layer `#` files). Gap: hero Act → Move exits before the desk placer when
   the path contains `/pieces/`.

Two different "movers" exist and must not be confused: the **placer move** (player-chosen,
range-limited, tactics style, Fire Emblem) and the **autonomous mover** (chicken wander in
`pc_clock_daemon.c`, planned as a generic `move` command). RPG Maker has both too: *player
transfer by input* and *event Set Movement Route / autonomous move type*.

## 4. Events: what exists and how it runs

- **Shape.** `event_pkg/pages/page_N/{condition.pdl, event.ir.pdl, event.pal, cmd_N.sh}`.
  `condition.pdl` holds `COND | trigger | <value>`. `play_event.sh <pkg> [house] [trigger]`
  scans pages and runs the **highest-numbered page whose trigger matches** (RPG Maker's page
  rule). Switch/variable page conditions: not built as page conditions (name-based
  Control Switch/Variable commands exist).
- **Command registry.** `#.ref/menu/event_commands.registry.pdl` (1,023 lines, **87
  commands**): each `COMMAND ... TEMPLATE exec <op> args`. Three tiers
  (`EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`): pure data; generic exec templates (almost
  all commands); genuine compiler logic (Conditional Branch, loops) in C. Never hardcode a
  command in a renderer; add a registry block.
- **RPG Maker commands already in the registry** (as *state writes* via `mr_world.+x`, no
  camera yet): `transfer_player`, `scroll_map`, `set_move_route` (writes a route string, does
  not walk), `fadeout/in`, `tint`, `flash`, `shake`, `shop_processing`, `battle_processing`,
  `play_se`, `control_self_switch`, `control_timer`; plus show_text/choices, party and actor
  commands, `show_animation`, `erase_event`, `change_transparency`, `followers`, flow control
  (if/else/loop/label/jump/exit/comment), clock verbs, `eden_verb`, `use_skill`, `grade_report`.
  Each says in its header that it is state, not presentation, so **Movement, Screen and
  Scene commands are recorded, not yet shown.**
- **Triggers vs RPG Maker.** RMMV: 0 Action Button, 1 Player Touch, 2 Event Touch,
  3 Autorun, 4 Parallel (integer on the page). House: `on-click` ~ Action Button;
  `player-touch` built end to end for pc-hq (`touched_npc` ledger line from the `MOVE`
  handler in `pc_menu_input.c`, `pc_trigger_watcher.c` tails the ledger and calls
  `play_event.sh`; proven on `cdda_sample` x=6,y=5); Autorun/Parallel exist for **common
  events** (`common_events_manager.c`, ~1 s poll, name-based switch field); Event Touch and
  desk-side touch: not built.
- **Common events** are the same package shape rooted at a session (`common_events/`);
  `event-ez` edits any package via `EZ_PKG_DIR`; the editor is the db/event authoring UI.
- **Doctrine from the RMMV source read** (`RMMV-EVENT-ARCHITECTURE-LEARNINGS.md`): RMMV
  polls everything in one process every frame and interprets the live command list; the
  house **must** use files because windows are processes. Do not copy the per-frame poll;
  copy the **data model**: composite self-switch key `[map, event, letter]`, one global
  switch/variable table, page-by-condition. Persistent state is one append-only ledger
  replayed for truth (the `101.lpns+map+4` pattern), not a second keyed store.

## 5. Where the model and the house line up (the mapping Grok needs)

| RPG Maker MV | House |
|---|---|
| Project / Map | Book (session) / Page (desk file) |
| Event on a map | Entity row on the page + its `event_pkg` |
| Event page, conditions, trigger | `pages/page_N`, `condition.pdl` (trigger; switches by name) |
| Event commands (codes) | registry `COMMAND` blocks (87) |
| Common Event | `common_events/<name>/` (Autorun/Parallel by switch) |
| Self switch A-D | `control_self_switch` (`mr_world`), key map+event+letter |
| Switches / Variables | name-based Control Switch / Variable (numeric ids half built) |
| Party / Actors | cursword / possessed body; actor commands exist |
| Player Transfer | `transfer_player` -> `mr_transfer_desk.+x` (door entity has it) |
| Set Movement Route | `set_move_route` writes a route; **no executor** |
| Tile / autotile | `tile_autotile.c` (RMMV 48/16/4 tables ported), `tileset_registry.pdl` |
| Map scroll/camera | pc-hq camera entity row; desk has none |
| Game loop | **missing** (see section 6) |
| Save/Load | save slots, `game_snapshot_op`; load-while-running unsolved |

## 6. The gaps, ranked by what blocks RPG Maker style play

1. **Post-move hook** (the heart). After any move (placer or autonomous) something must run:
   touch trigger check, NPC step, ledger line, autorun check. Documented, **not wired on the
   desk**; built for pc-hq player-touch only. This is RMMV's `updateNonmoving`.
2. **Movement route executor.** `set_move_route` stores text. A route (move types: fixed,
   random, approach, custom with up/down/left/right/wait/turn/speed/frequency) needs a tiny
   interpreter that emits waypoints into `animation_queue.txt`, reusing the existing ticker.
   `PLAY-MODE-ENTITY-HARNESS-DESIGN.md` plans `move` as a registry command (wander,
   pathfind-to-entity); its 6.2 question (command vs built-in verb) is still open.
3. **Event Touch and desk-side touch.** Only player-touch in pc-hq exists.
4. **Page conditions beyond the trigger** (switch/variable/self-switch/item) and numeric ids.
5. **Presentation of state commands** (screen tint/shake, transfer with camera, show
   animation, chat bubbles): recorded, not drawn. Message box decision: continuous mode
   pauses via `livedesk:clock:pause`; turn-based folds it into the turn (decided in
   `CURSWORD-SOUL-VISION.md` 4).
6. **Chicken/NPC tick** is not started by the current board launcher; Eden has no
   positions or move rows.
7. **Single store.** Book:page in pc-hq and the desk are two stores until the owner picks
   (`LEARNINGS.md`); Synch partly copies.
8. **Play modes.** build / play / play-test and available maps are designed, not built.
9. Possession, hotbar, dock-stack, Take/Place wiring: designed, data layer only.

## 7. What I got wrong (so Grok does not repeat it)

- The earlier plan treated entities as things that "act on a clock tick" and invented
  menu-driven gameplay without the placer or the event pages. The real structure already
  has: placer for chosen moves, event pages for consequences, a ledger for audit.
- I said Eden `eden_game` was missing (it is under `home/livedesk`), and that the chicken
  ticks per player action (it is a clock daemon, not started by the launcher).
- Movement is **not** the same as the window drag. Drag-and-drop is a different mechanism;
  Move is the range-limited placer flow.

## 8. Proposed route to "RPG Maker MV ready" (small, each with an exit test)

R0. **Docs first (this doc, plus a one-page mapping table in the Grok handoff).** Exit: Grok
    can name, for any RMMV concept, the file that holds it.
R1. **Post-move hook on the desk**: after `move_entity_tick` finishes a path, append a
    `MOVE_DONE` ledger row with entity and cell; a watcher (same lifecycle as
    `pc_trigger_watcher`) looks up `player-touch` rows for that cell and calls
    `play_event.sh`. Harness: move onto a trigger cell -> event ran once; elsewhere ->
    none; mutant with the hook removed fails.
R2. **Route executor** `move_route_op`: parses a `MOVE_ROUTE` string into waypoints, appends
    to `animation_queue.txt`; same ticker. Harness: a 4-step route lands on the expected
    cell; a wall stops it; a mutant that ignores walls fails. Then registry `move` (wander,
    approach) built on it.
R3. **Event Touch**: after an autonomous step lands adjacent or on the player cell, ledger
    row `touched_by_event`, same watcher.
R4. **Page conditions**: extend `condition.pdl` with `COND | switch | name`, `variable`,
    `self_switch`, `item`; `play_event.sh` evaluates them (highest page whose conditions are
    all true). Numeric ids as aliases to names.
R5. **Autorun lock**: while an Autorun page runs, the placer and menus refuse input
    (RMMV's input block), via the existing pause mechanism.
R6. **Presentation**: draw what state commands already record (tint, shake, bubbles) in the
    renderer through generic layout, one command at a time.
R7. **One store**: owner decision (section 6.7), then Synch as a real page copy.
R8. **Eden as the worked map**: positions on `pre-design:eden-test`, acts emit move rows
    through R2, tick started by the board launcher, viewer shows it.

Order is chosen because R1 and R2 reuse proven pieces (ledger + watcher; ticker + queue)
and unlock R3, R5 and R8. Do **not** start with presentation or a new engine.

## 9. Questions for the co-lab session with Grok (the owner will open it)

1. Which RMMV features first: touch triggers, routes, or conditions? (I suggest R1, R2.)
2. `move` as a registry command or a built-in verb of the harness (6.2 above)?
3. Which of Grok's RMMV knowledge maps to our data (event codes -> registry blocks; move
   route codes -> a route string grammar)? Ask for the real route command list and have it
   check our `mr_world` verbs against MV's code numbers.
4. Is a map loop ever needed, or is "ledger line -> watcher" enough for every trigger?
5. Single store: book/page of the desk or the pc-hq map project?
6. How should self-switch keys be named across pc-hq and the desk (map id = page name)?

## 10. Honesty

Read (docs and headers): MOVE-AND-TAKE, INTENDED, XELECTOR-ENTITY, CURSWORD-POSSESSION,
PAGE-FILE, LEARNINGS, SYNCH, PLAY-MODES, PLAY-MODE-ENTITY-HARNESS, EVENT-TRIGGER-LAYER,
EVENTS_RUNTIME, EVENT_AI_VISION, EVENT-COMMAND-REGISTRY-ARCHITECTURE, RMMV-EVENT-ARCHITECTURE-
LEARNINGS, GAME-READINESS-GAP-ANALYSIS, MAPS-TILES-ZLEVELS, GAME-CONDUCTOR/EDEN, the
registry (command list and the world block), the headers of the placer, the range library
and `move_entity_on_desk.sh`. Not read: the bodies of the placer, `move_entity_init/tick`,
`mr_world.c`, `common_events_manager.c`. Nothing was built or run. Some source docs are older
than the code (EVENT_AI_VISION says play_event is page-1 only; that is corrected by its own
banner). The R-list is a proposal.
