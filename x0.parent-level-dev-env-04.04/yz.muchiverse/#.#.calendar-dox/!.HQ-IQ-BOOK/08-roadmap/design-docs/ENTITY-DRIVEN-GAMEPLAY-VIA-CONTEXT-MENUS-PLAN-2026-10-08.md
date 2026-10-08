# Entity-driven gameplay: play through entities, their context menus and the taskbar relay (plan, 2026-10-08)

Status: **PLAN, nothing of this design is built.** Owner (2026-10-08): "drive all gameplay thru entities and tb relay nav; use the entities' context menus to drive their NPC/AI-like behavior; scores, FSM etc.; use entities on the desk or synced pc-hq"; "we want farming animations, visual chat bubbles". This is the main push for Grok. The owner's words are marked; the rest is my reading, to be confirmed.

## 1. The vision, as I read it

An entity is the character. Everything it does in a game is an **action row in its own context menu**. A human, an FSM/GOAP brain, or an agent driving the relay all play by pressing the *same* numbered rows through the taskbar relay, so a played game is replayable and watchable, and an agent never "just knows" the state: it opens its menu, reads its inventory, presses rows. The same entity lives on the **desk** (livedesk tile) and in **pc-hq** (a piece), kept in sync. Behavior (NPC and AI-like) is a state machine plus goals choosing rows. Scores are the existing grades, levels and MP, moved by harness verdicts and exams. Animations and chat bubbles are views of the rows that were pressed.

## 2. What exists (read 2026-10-08; the pc-hq window was opened live and driven; the NPC loop below was read, not run)

- **Desk entities (Asa, Ava):** a folder with `meta.pdl` (METHOD rows: Chat, Events, Play, Stop, Ledger, Dir, Inventory, Close), `event_pkg`, `inventory`, `phone.txt`, stats, `history.txt`, and `desktop_pos.txt` (`x=560 y=1520 z=0`; nothing changes it). None of their menu rows is a game action.
- **Eden today:** the conductor's day tick runs farming from outside the entities (actions seen live: eat, dig, plant, collect, water, talk, trade, repair; TALK rows carry real text). No positions, no move actions, no goals.
- **pc-hq hero (`pieces/hero_01/state.txt`):** `entity_type=hero hp=20 pos_x/y/z owner_id=player`; its context menu is built by `ops/pc_entity_ctx.sh`; **Move** is a menu row, with a range diamond shown while armed (a 2026-10-05 survey says the range is always on; not rechecked).
- **pc-hq NPC chicken (the template, `human-dev.md` section 11):** a position line in `pieces/world_01/animals.txt` (`name,x,y,z`); `tick_animals()` in `ops/pc_menu_input.c` takes a random -1/0/+1 step per axis, clamps to the map, rewrites the file, and logs `chicken|wander|x:..,y:..` to `data/master_ledger.txt` with the animal's own entity id; it runs once per player action (turn based); the 3D renderer reads `animals.txt` generically. The guide notes it is visible in 3D only.
- **Entity move action on the desk:** `move_entity_init/tick` moves a live window (XMoveWindow + marker). Not connected to Eden.
- **Sync between desk and pc-hq:** the pc-hq menu has a Synch row, and `#.desktop/pc_synch_request.txt` exists. I have not read how it works.
- **Live check on the owner's screen:** the pc-hq board opens with `open_pchq_board.sh`; its book dropdown lists default, File Explorer, mineclonia_sample, cdda_sample, test_walls, test_terraces, default-legacy (opened and cancelled with Escape; nothing was chosen). **There is no `eden-test` book or page yet.**
- **Not yet read:** `ENTITY-NEEDS-AND-CARE-DESIGN.md`, `PLAY-MODE-ENTITY-HARNESS-DESIGN.md`, `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md`, `ROBOT-CHAT-BLUEPRINT.md`, `PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md` (read these first; this plan must be reconciled with them, not replace them).

## 3. The design (proposal)

1. **Action rows.** Each Eden participant's `meta.pdl` gains METHOD rows per action: `Walk to...`, `Plant`, `Water`, `Dig`, `Collect`, `Eat`, `Talk to...`, `Trade...`, `Repair`. A row calls the existing conductor event page for that action with the entity as actor, so the effect and the history row are unchanged; what changes is *who presses it*. Rows with a target (a plot, a person) use the existing numbered second-level menu.
2. **Positions.** Every Eden thing gets x,y,z in data (plots, house, chickens, people). `Walk to` appends `MOVE|day|who|from|to` and updates the entity's position the same way the chicken's tick does (clamp, rewrite, ledger row with the entity as actor). The desk tile follows through `desktop_pos.txt` and the move action; the pc-hq piece follows through the same position file.
3. **FSM + goals choose rows.** A small FSM per entity (idle, going, working, talking, resting) with a goals list (the shape proposed for the goals board) picks the next row by need (hunger, shelter) and goal, then presses it through the relay path, never by writing the state directly. One brain, same code, for desk and pc-hq. Start with the chicken's random wander replaced by "walk toward the target of the current goal".
4. **Scores.** Grades, level and MP change only through deterministic checks (harness verdicts, exams), as the entity school design says; an action row never edits a score.
5. **Relay agent.** An agent plays by: opening the entity's menu, reading the frame dump for the real nav numbers, pressing a row, reading its inventory through the menu. It never reads engine files to decide. Evidence of a played game = the relay file, the history rows, and frame dumps.
6. **Views.** Desk tile and pc-hq piece show the position; the Eden viewer shows the farm map. **Animations** are keyed to history rows at the tile where they happen (seedling on `plant`, drops on `water`, ripe crop on `RIPE`, harvest pop on `collect`); a paused game freezes the picture. **Chat bubbles** are keyed to `TALK` rows: a bubble over the speaker with the row's text, fading after a few seconds; the text comes from the phrase path (later tomom).
7. **Sync.** The desk is the functional parent (house rule): pc-hq mirrors the entity's position and rows; if they disagree the desk wins. How Synch works today must be read before this step.

## 4. Build order, each step with an exit proof (scratch house first; relay driven; frame dump looked at)

1. **Read the five docs above and the Synch path; write the reconciliation as a short addendum.** (me)
2. **Prove the pc-hq pattern live:** drive the hero's Move and one End-turn tick through the relay and show the chicken step with a ledger row. *Exit:* before/after frame dumps and the ledger rows.
3. **Positions and `Walk to`:** x,y,z for Asa, Ava and the plots in a scratch game; the Walk row moves the tile on the desk. *Exit:* the tile's `desktop_pos.txt` and screen position change; a harness checks clamp, the ledger row and no write outside the entity.
4. **Action rows on Asa and Ava** (Plant, Water, Collect, Eat, Talk) calling the existing event pages. *Exit:* pressing the rows through the relay produces the same history rows as the day tick did.
5. **FSM v0:** need-driven choice of rows, one entity, deterministic seed. *Exit:* a harness replays N ticks and gets the same row sequence; a mutant that skips the need check fails.
6. **Chat bubbles** (cheapest visual: data exists), then **farming animations**.
7. **pc-hq mirror** of the same two entities; desk wins on conflict.
8. **Eden test page/book** in pc-hq (`eden-test`, the owner's request): copy-based like `dsrtest_*` so the live Eden is untouched; this is what gets presented.

## 5. Open questions for the owner

Which pc-hq book should hold the Eden page (a new `eden-test` book, or a page in an existing one); whether turn-based ticks (like the chicken) or the real-time clock drive the FSM in v0; what the position unit is on the desk (pixels, as `desktop_pos.txt` has now) versus pc-hq cells.

## 6. Presentation (owner request, when ready)

A presentation of Eden gameplay from this setup, built with the house's `.py` presentation converter once steps 3-6 produce footage or frame dumps. I have not yet located that converter.
