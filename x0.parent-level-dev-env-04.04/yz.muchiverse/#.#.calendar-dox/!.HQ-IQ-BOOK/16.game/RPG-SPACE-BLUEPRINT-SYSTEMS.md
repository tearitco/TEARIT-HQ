# rpg-space systems as event blueprints, before any port to rpg-dust (plan, 2026-10-25)

Owner 2026-10-25: *"those should all be events based, blueprint viewable, before we do the port. Not shared libs, but reusable ops, events, pal etc."* Nothing below is built; every claim about what exists was read from files today.

## 1. Where we are (read)
- rpg-space has **11 event pages** (`@.apps/rpg-space/events/*`: collision, death, destroyed, environment, helm, newplanet, planet, respawn, start, zoom_in, zoom_out). Each page is ONE node, `type=rpg_space_verb | verb=<verb>|arg=...`, that calls the C skin (`rpg_space.+x <verb>`). Blueprintz can list the page but shows no logic: life, mining, building, pickup, air and collision all happen inside `rpg_space.c` (about 1000 lines; rpg-dust has none of them, 478 lines).
- `rpg_space_verb` is **not in the command registry** (`#.ref/menu/event_commands.registry.pdl`, 0 matches), unlike `rpg_pvp_hq_verb`, `df_*`, `chq_*`. Blueprintz's source list (`&.hq-apps/blueprintz/state/bp_sources.pdl`) is a flat `EVENT | name | path` list; rpg-space and rpg-dust are not in it.
- The house already has the pieces for what the owner asks: the **data-driven command registry** (a SIMPLE command is one registry entry, no recompile: `TEMPLATE exec <op> args` or `PAL` lines using `ecall` on state files), the `event_pkg/pages/page_N/{condition.pdl,event.ir.pdl,event.pal,cmd_N.sh}` chain, `mr_*.+x` ops in `&.widgits/events-hq/ops/`, and the verb-pages design (`EVENT-VERB-PAGES-DESIGN.md`: C verbs stay the rules, player-meaningful verbs get pages, a generator stamps `META | generated | verbgen`).
- Commands that already exist and fit: `change_hp`, `change_items`, `change_variable`, `variable_math`, `random_roll`, `if / else / end / loop`, `wait`, `call_common_event`, `control_switch`, `recover_all`, `revive_all`, `kill_event`, `erase_event`, `transfer_player`, `show_text`, `set_clock_rate`, plus game ones (`df_mine`, `df_build`). Whether they read the same state files the rpg-space skin writes (`state/life_<id>.txt`, `drops.txt`, `nodes.txt`) is NOT checked: `change_hp` uses `{STATE_DIR}/actor_<id>_stats.txt`.

## 2. The target: each system is a graph of nodes a person can read
A **page** = trigger + condition + a list of registered command nodes; the **rules numbers** stay data (`life.pdl`, `blocks.pdl`, `environment.pdl`, `collision.pdl`, `space.pdl`); anything that must run every tick stays a throttled loop (see 4). Sketches (node names are registry commands; `NEW` = a command to register):

| system | trigger | nodes (one line each) |
|---|---|---|
| hunger / starving | parallel, every `hunger_ms` | per character: `change_variable hunger +1` -> `if hunger >= 100` -> `change_hp -starve_hp` -> `if hp <= 0` -> `call_common_event death` |
| death | parallel (called) | `NEW drop_items pct` (inventory -> remnant rows) -> `control_switch dead on` -> `show_text` -> (mode) `call_common_event respawn` or stop |
| respawn | parallel (called) | `recover_all` -> `transfer_player respawn room/x/y` -> `control_switch dead off` |
| mine (key m) | action / key | `NEW node_at front` (-> variables kind, progress, hardness) -> `if found` -> `variable_math progress += tool_power` -> `if progress >= hardness` -> `call_common_event destroyed` else `show_text progress` |
| destroyed | parallel (called) | `random_roll damaged_pct` -> `NEW spawn_remnant item count damaged` -> `change_variable node amount -1` -> `if amount == 0` -> `erase_event node` -> log row |
| build (key b) | action / key | `NEW node_at front` -> `if free` -> `change_items -cost` -> `NEW place_node kind` -> log row |
| pickup | called by the pickup scan | `change_items +n` -> `erase_event remnant` -> log row (the slide animation stays in the daemon) |
| air / temperature | parallel, every `tick_ms` | per room: `variable_math o2/temp toward base or outside` -> per character `if o2 < ok or temp out of range` -> `change_hp -pm` |
| collision | called by the lift/move check | `variable_math damage = base + speed*k` -> per character `change_hp` -> `change_variable ship_hits +1` -> `if speed >= breach_speed` -> `NEW set_breach room` |
| hold (ship inventory) | verbs | `change_items`, `NEW entity_board / entity_leave` (relative cell) |

## 3. Reusable ops (widget-level, game-agnostic, not shared C libs)
The few `NEW` rows become small single-purpose binaries in a widget folder (proposal: `&.widgits/world-ops/ops/` beside `events-hq/ops/mr_*`), each: reads its game folder from an argument, reads/writes plain files, exits; each is ONE registry entry (`TEMPLATE exec ...`), so events-hq / event-ez / blueprintz offer it with no per-game code. Candidates (check first whether `variable_find`, `variable_sweep`, `row_append`, `tile_lookup`, `apply_range` already cover them): `node_at`, `place_node`, `spawn_remnant`, `drop_items`, `set_breach`, `entity_board`. Games differ only in their data files and their event pages.

## 4. Footprint rule (owner 2026-10-25) applies to blueprints too
A page is a process chain (`play_event.sh` -> `cmd_N.sh` -> op): fine for transitions, wrong for per-tick work. So: pages fire on **transitions** (key pressed, hp hits 0, hunger step, air step, destroyed), and the per-tick scans (remnant pickup range, slide animation, the air clock) stay in the one resident daemon as data-driven loops that CALL the page when something happens. Measure forks/s before and after each step (doc 7g method); no step may raise the idle fork rate.

## 5. Order (each step has a gate)
1. **B0, viewable, behaviour unchanged.** Register `rpg_space_verb` (and a `rpg_dust_verb`) in the registry; add rpg-space and rpg-dust events to `bp_sources.pdl`; open blueprintz and read one page. Gate: a screenshot of blueprintz showing the page.
2. **B1, life.** Rewrite hunger/death/respawn as the node graphs above, keep `life.pdl`; keep the C verbs as the fallback exactly like today's death/respawn pages. Gate: the per-game verb-script diff (same results as before) + ledger shows the page played + health bar unchanged + fork rate.
3. **B2, world:** mine / destroyed / build / pickup. **B3, air.** **B4, collision and hold** (rpg-space only; they need the ship).
4. **B5, the port to rpg-dust = copy data + events + key rows** (`life.pdl`, `blocks.pdl`, `environment.pdl`, `events/*`, `HOSTKEY` rows in its `pchq-overlay`). No C is copied: rpg-dust's C only needs the tick loop hooks (life tick, air tick, pickup scan) that call the same pages. Gate: the same verb script runs on both games.

## 6. Open questions for the owner
- New widget `&.widgits/world-ops/` for the reusable ops, or add them to `events-hq/ops/` next to the `mr_*` ops? (recommend the new widget, so events-hq stays an editor)
- Keep the daemon's per-tick scans in C (recommended, section 4) or make them a pal loop on the prisc VM too? (a pal loop is allowed; it is another process loop, measure first)
- Start with B0 and B1 (life)? (recommended: B0 is nearly free and shows the owner the blueprint before anything moves)
