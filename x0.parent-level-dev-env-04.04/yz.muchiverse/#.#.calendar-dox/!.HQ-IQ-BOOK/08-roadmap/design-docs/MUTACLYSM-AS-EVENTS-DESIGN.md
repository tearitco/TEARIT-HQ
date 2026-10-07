# Pulling mutaclysm back in, as events: design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner (2026-10-07): "we also have mutaclysm, we want to pull that project back in, just as events."
Related: `ENTITY-NEEDS-AND-CARE-DESIGN.md`, `DSR-SIMULATION-DESIGN.md`, `SURVIVAL-GATHERING-FARMING-MINING-WATER-HANDOFF.md`, NIGHT 20/26 ("as few hardcoded game mechanics as possible", "everything is an event").

## 1. What mutaclysm is (read, not assumed; `101.mutaclsym.../README.md`, `dox/00-HANDOFF.md`, `ops/`)
"A small CDDA-inspired survival game": a tile map, **hunger/thirst**, **monsters**, **crafting**, and survival; a hero with an inventory; an action bar (pickup, drop, eat, craft, examine, save, toggle emoji), an **interact mode** (free cursor to look/aim, throw the best weapon at range), a **voxel/3D** render path and a procedural map generator (`dox/02-procgen-design.txt`). It is a standalone prisc game loop: `pal/main_loop*.pal` driving compiled ops (`move_player`, `pickup`, `drop`, `eat`, `craft`, `examine`, `end_turn`, `tick_monsters`, `generate_map`, `save_game`, `muta_place_tile`, `muta_world_io`, `camera_control`, `call_event_op`, ...), state in files under `pieces/` (hero state, map, inventory, `pieces/registry/recipes/recipes.txt`). It already has `call_event_op` (the "Call Common Event" handler), and its handoff warns not to rebuild or write `pieces/` while a live session runs. Its README says it is also tied to a pet-import path (`pet_import`) and egg-pals.

## 2. What "just as events" means here
Mutaclysm stops being **a separate game** and becomes **a set of event commands and event data** any page can use (a DSR page, a gathering/farming page, a dungeon page), instead of C ops welded to one prisc main loop. Concretely:
| mutaclysm piece | becomes |
|---|---|
| `move_player`, `camera_control` | the existing Move/possession events (cursword/xelector/hero) |
| `pickup` / `drop` / `examine` | registered commands over the house inventory (an entity's `inventory/`); drop/pickup are already house actions |
| `eat` (and a drink verb) | `feed` / `drink` from the needs design (one implementation of hunger/thirst for the whole house, not a second one) |
| `craft` + `recipes.txt` | a registered `craft <recipe>` command + **recipe data** (`.pdl`), the same recipes used by gathering/farming and shops |
| `tick_monsters`, `end_turn` | a **day/phase tick** event page subscribed to the place clock; monsters are entities (or sprite rows) with an event page, not a hardcoded loop |
| `generate_map` | a procedural-generation **event** that writes a page/map (tiles) from a seed (reproducible) |
| `save_game` | the house save-slot op (game data only), not its own save format |
| throw/melee | combat events with real costs (shares the cost model of DSR violence/war later) |
| `call_event_op` | already an event primitive: reuse as is |
Rendering stays a viewer concern (its voxel/2D render paths are viewers; they are not events).

## 3. Rules for the pull-in
1. **No second copy of a mechanic.** Hunger/thirst/eating/inventory/crafting each end up in exactly one place (needs design, inventory, recipe data); mutaclysm's op becomes a thin command over it, or is retired.
2. **Data before code:** move recipes, item properties (nutrition, hydration, damage, warmth) and monster stats into `.pdl` data first; ops only read data.
3. **Never touch a live mutaclysm session** (check `ps` first; copy the project to a scratch/beta tree and work there; do not rebuild while it runs) per its own handoff.
4. Port **one command at a time**, each with a pal harness over the old behavior (same inputs, same results) before the old op is retired.
5. Keep the standalone game launchable as **a page** (its map and monsters as a game page) until the event version reproduces it.
6. This work happens in **alpha**, rehearsed in **beta**; it is the content side of the Grok gathering/farming lane, so coordinate through the handoff doc rather than both lanes editing the same ops.

## 4. Build order
1. Inventory the data it actually keeps (recipes, items, monsters, hero state) and write the item/recipe `.pdl` schema (read `craft.c`, `eat.c`, `pickup.c`, `drop.c` first; I have not read their internals yet).
2. `craft`, `eat`/`drink`, `pickup`/`drop` as registry commands over that data, with harness cases that replay the old ops' behavior.
3. Tick events: day/phase tick page with hunger/thirst and the monster step as data-driven rules.
4. `generate_map` as a seeded event; a scratch page produced and compared.
5. Retire the old ops one by one; keep the page runnable.

## 5. Open questions for the owner
1. Is the standalone game still wanted as its own game, or fully absorbed into pages?
2. Keep the CDDA-style turn-based feel (a turn = a tick) or run on the DSR day/phase clock?
3. Monsters as entities (windows/processes) or as board/sprite rows (recommended, same cost reasoning as citizens)?
4. Does the pet-import path (egg-pals/muchi-pals) stay part of this?
