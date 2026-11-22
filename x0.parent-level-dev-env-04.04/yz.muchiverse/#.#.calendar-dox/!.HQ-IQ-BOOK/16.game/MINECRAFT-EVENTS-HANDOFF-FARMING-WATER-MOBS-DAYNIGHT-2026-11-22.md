# Minecraft in the house, built with EVENTS: primitives, NPCs, farming, water, mobs, day/night, hunger, beds, and leaf decay (handoff, 2026-11-22)

Written by Claude (branch `claude`) at the owner's request: "going into incredible depth how to use events etc to create minecraft primitives, npcs, farming, water, mobs, day/night, hunger, beds, etc, including improvements to minecraft like tree destruction leftover leaves." Twin of `TEARIO-HANDOFF-WHERE-WE-ARE-AND-WHAT-IS-NEXT-2026-11-22.md`.

**Labels.** READ = I opened the file this session. MINE = my design proposal, nothing built, free to change. NOT FOUND = I searched and did not find a mechanic (a word match in a catalog is not a mechanic; I did not read each hit). OWNER = said by the owner. Do not quote a MINE row as if it exists.

**Hard order from the owner's side (grok's answers file, `XO/31.Nov/nov-22/2.answers-for-claude.md`, READ): "Minecraft after the replay is green: digging is a new verb on a voxel. Do not lift the flat city row into 3D height. Do not start it in the same stretch as ENTER/EXIT."** So this doc is the plan to pick up AFTER the car/ENTER/EXIT replay is green. Do not start building from it before that, and ask the owner.

---

## 1. The one rule that shapes everything (OWNER, D92, 2026-11-18)

**Every game rule is an OP, called by EVENT PAGES. No native rules, no exceptions.** Read in this order before writing anything:
1. `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`
2. `02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`
3. `02-architecture/PAL-VS-EVENTS.md`
4. Decision log D92 and the five design-choice questions (`02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`). Answer them in the log BEFORE you build; add a row.
5. `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`: a game fact lives in ONE append-only ledger row written through a verb. Everything else is a projection (delete-safe), config, or preference.

### 1.1 The shape of one mechanic (use this for every section below)

| Piece | What it is | Where (READ for mcl-hq) |
|---|---|---|
| Numbers | `RULE`/`TUNE`/`LIFE` rows, re-read by the daemon without a rebuild | `@.apps/mcl-hq/tuning.pdl`, `life.pdl`, `time.pdl`, `recipes.pdl` |
| Kinds | a data table of what things exist | `@.apps/rpg-space/blocks.pdl` is the working model (`BLOCK | kind | hardness | drops | count | damaged_pct | ...`, `TOOL | name | power`) |
| Op | a pure module + CLI that changes the ledger and prints one line | e.g. `rpg_space.+x life hurt <id> <n>`; new ops in `&.widgits/_shared-lib/ops/` |
| Verb row | which verbs are event pages | `@.apps/mcl-hq/verbs.pdl` (`VERB | mine | yes | ui | x y | mine a block`; checked by `verbgen check .`) |
| Event page | trigger + condition + calls to ops | `@.apps/mcl-hq/events/<name>/event_pkg/pages/page_1/{condition.pdl,event.ir.pdl}` (READ for existence of `mine`, `place`, `craft`, `phase_*`, `day_tick`, `greet_*`, `reply_*`, `chest`, `buy`) |
| Clock | timed events | `time.pdl`: `AT | phase_night | 19:00 | every:1day`; `TURN | advance | 5m` for roguelike turns |
| Ledger | the only authority | `state/events.txt` (rows `DEATH`, `RESPAWN`, `TAKE`, `DAY`...); grok's new city ops use `ENTER/EXIT/DRIVE/EAT/PART` rows in the same style |
| Harness case | a case that can FAIL | pal + `.pdl` case under `&.widgits/_shared-lib/harness/cases/`; playthrough tables under `harness/play/phases/*.phases.pdl` |

**Gate for every mechanic below:** (a) green case, (b) the same case with ONE number deliberately wrong goes RED (negative control), (c) replay the ledger twice and get identical rows, (d) a PNG of the thing on screen that you opened and described (a ledger-only pass with an empty screen is a failed test).

### 1.2 What actually exists (READ; the honest inventory)

- `@.apps/mcl-hq` is a **renamed copy of dsr-rpg** (its README says "Nothing here is MCL-specific yet"). It has: verbs buy/control/craft/delpal/goto/hold/load/mine/newgame/newpal/newroom/place/remove/save/use; events for those plus `phase_dawn/day/dusk/night`, `day_tick`, six NPC greeters and repliers (`greet_fenn`, `reply_fenn`, gus, maren, orin, pip, rex); `recipes.pdl` with ONE recipe (`planks | log........ | planks | 4`) and a stated gap: "3x3 versus 2x2 and a crafting table are NOT checked yet".
- `life.pdl` (READ): `LIFE | enabled | 0` (default OFF), `hunger_ms 60000`, `starve_hp 1`, `drop_pct 50`, `death_mode respawn|permadeath|session`, respawn room/cell, `take_range 2`; `ACTOR` rows name who the world-ops steps act on.
- `time.pdl` (READ): dawn 06:00, day 08:00, dusk 17:00, night 19:00, `day_tick` 00:00; clock speeds; `TURN | advance | 5m`. The clock is the house `lc_clock` with its own mini root.
- `@.apps/rpg-space` is the **working 3D mechanics reference** (READ docs `RPG-SPACE-LIFE-DEATH-RESPAWN.md`, `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md`): life/death/respawn/drops built 2026-10-25 (hunger rises, starve damage, `life hurt/feed/give/set/take`, `events/death`, `events/respawn`, health bars); mining with per-cell damage (`state/damage.txt`), remnants (`state/drops.txt`) drawn in the scene and auto picked up, `blocks.pdl`, `collision.pdl`, `foods.pdl`, `environment.pdl`, `reactions.pdl`, `lights.pdl`. Owner-decided but NOT built: 3 block damage stages with numbers-based sound and animation; blood overlay by hp fraction; graphic level per game.
- `@.apps/df-hq` (READ via the 2026-11-10 handoff): `dig`, `dig_area`, `undig`, `place`, `resolve`, `clock`, `day_tick`; rules table `16.game/RULES-df-hq.md`; hook `GE_RULE_HOOK` in `ops/df_hq.c`.
- NOT FOUND as built mechanics (grep hits in catalogs and `.c` files exist but I did not read them as mechanics, so verify before saying "none exist"): **farming/crops, fluid flow, mob spawning and AI, beds/sleep, leaf decay or saplings.** `piececraft-hq` docs mention planting/hunger in design text only.
- Grok's handoff of 2026-11-10 (READ) asked for generators `mech_new_op` (mechanic skeleton from args: kind dig|mine|craft|place|plant|sleep|build-job) and `vis_new_op`. `vis_new_op` is NOT built; check whether `mech_new_op` exists before writing one (`ls &.widgits/_shared-lib/harness/ops | grep mech`; I did not).

**Where to build:** a new voxel skin, not mcl-hq's dsr-rpg rooms. Start from `game_new_op` (guide: `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`) into a SCRATCH house, never the live editor. Reuse `petslike_core.c` only if the voxel needs its tick loop; otherwise `game_core` (data + events, no per-game C).

---

## 2. Primitives: the voxel, the block, place, break, drop

### 2.1 Data (MINE, shaped like rpg-space `blocks.pdl`)

```
# BLOCK | kind | hardness | tool_best | drops | count | solid | opaque | light | gravity | flammable | breaks_into
BLOCK | dirt   | 2  | shovel | dirt     | 1 | 1 | 1 | 0 | 0 | 0 | -
BLOCK | stone  | 6  | pick   | cobble   | 1 | 1 | 1 | 0 | 0 | 0 | -
BLOCK | log    | 4  | axe    | log      | 1 | 1 | 1 | 0 | 0 | 1 | -
BLOCK | leaves | 1  | shears | sapling  | 1 | 1 | 0 | 0 | 0 | 1 | -
BLOCK | sand   | 2  | shovel | sand     | 1 | 1 | 1 | 0 | 1 | 0 | -
BLOCK | water  | -  | -      | -        | 0 | 0 | 0 | 0 | 0 | 0 | -
```
Keep unknown columns (the rpg-space design rule 4c): nothing reads them until a rule does.

### 2.2 Ops and ledger rows
- `vox_op set <x y z> <kind>` writes `VOX | tick | x y z | kind | cause` (cause = place, break, flow, grow, decay). **The world is the fold of VOX rows over the seed terrain**, never a mutable grid file (ghost-data rule). A snapshot is a projection regenerated from the ledger.
- `mine <x y z>` (event page `mine`): condition (actor within reach, block solid, tool power known) -> `vox_op damage` adds `tool_power` to `damage.txt`-style progress -> at `progress >= hardness` -> `vox_op set air` + drops via the SAME mechanism as death (`drops.txt`, `life take`). Reach and tool power are RULE rows.
- `place <item> <x y z>`: refused if the target is solid, if the item is not in inventory (conservation: placing removes one), or if no neighbour supports it (RULE `place_needs_support`).
- 3 damage stages (OWNER 2026-10-26, intact / cracked / crumbling): stage = `floor(3 * progress / hardness)`; frames and a tone generated from numbers, no asset files.
- Gravity blocks (sand/gravel): a `gravity_tick` op moves a gravity block into air below it, one VOX row per fall; it is an event page on a timer, not engine code.
- Gate: break a block with each tool, assert remnants and VOX rows; break with insufficient tool power must NOT break it (negative control); replay twice, same rows.

### 2.3 Collision and vision order (READ design, RPG Maker `canPass` order)
Map limit, then through/ghost flag, then solid cells, then other entities. `collision.pdl` rows `COLL | typeA | typeB | block|overlap|ignore` run `events/collision` or `events/overlap`.

---

## 3. NPCs

READ: mcl-hq has six NPCs with a `greet_X` page and a `reply_X` page (calls the events-hq greeting page at most once per `greet_page_ms` because each page run costs about 150 ms of CPU: **never run a page per tick**).

### 3.1 Roles as data (MINE)
```
NPC | id | role | home_bed | work_block | schedule_set
ROLE | farmer | wants=crop | job=harvest,replant | trades=wheat:bread
```
- An NPC is an entity row plus a `life_<id>` file like any actor (`ACTOR` row), so hunger, death and drops need no special code.
- **Schedule = `time.pdl` AT rows per role**, not a loop in code: `AT | npc_wake | 06:00`, `npc_work | 08:00`, `npc_home | 17:00`, `npc_sleep | 19:00`. Each fires one event page that sets each NPC's GOAL row (`GOAL | id | go_to | x y z`).
- Movement uses the existing roguelike turn (`TURN | advance 5m`): one verb per step, so the harness can replay.
- Same verbs as the player (OWNER via grok's answers: "eat and sleep are the same verbs as a job step; not a second simulation"). An NPC farming is the `plant` and `harvest` verbs called by a job page.
- Trading = `trade <npc> <give> <get>` as an op with conservation (items leave one inventory and enter the other; a case asserts the sum before and after is equal, which also catches minting).
- Gate: schedule replay (NPC is at its bed at 20:00, at its work block at 09:00) and a deliberately wrong time that fails.

---

## 4. Day and night

READ: `time.pdl` already fires `phase_dawn/day/dusk/night` and `day_tick`; the phase pages write `state/daylight.txt` (e.g. `phase=dawn`).
- Make **daylight a number, not four words** (MINE): `DAYLIGHT | minute | level` rows (0..15) so light interpolates and rules can use it (`light >= 9` crops grow, mobs spawn below 7).
- **Per the owner's answer: the world clock is OFF by default and time advances by manual TURN or a stated sleep skip.** Do not start an autotick. A sleep writes `SLEEP | actor | bed | skipped_turns=N` and the clock advance is that row.
- The sky and light projections read the ledger's current minute; they never store time.
- Gate: advance 144 turns of 5m = one day, assert DAY row once, assert level at 06:00 and 19:00 equal the table; flip one table value, case goes red.

---

## 5. Hunger and food

READ: hunger exists in the life layer (`hunger_ms`, rises 1 per step 0..100, at 100 costs `starve_hp`), **but `enabled` is 0 by default**. rpg-space also has `foods.pdl`.
- **Change from wall-clock to turns** (MINE): `hunger_ms` is real time; with the clock off, hunger must be driven by turns/ticks (`HUNGER | per_turns | 12`). Otherwise it contradicts the "manual turn" ruling above. Decide with the owner.
- Add **saturation** (MINE, Minecraft-like): `FOOD | bread | hunger=-25 | sat=+6`; hunger only drops when saturation is empty. Data rows, one op `eat <actor> <item>` (`EAT` row, removes one item: conservation).
- Actions cost: `ACTION | sprint | hunger_cost 1/20 turns` as rows read by an event page `exertion`.
- Regeneration: `LIFE | regen_if_hunger_below | 40`: an event page on turn heals 1 hp if hunger is low enough.
- Gate: starve to 100 -> hp falls by `starve_hp` per step; `eat` with an empty bag must refuse; replay identical.

---

## 6. Beds, sleep, respawn

NOT FOUND as built. Pieces that exist: death/respawn with `respawn_room/x/y` (READ).
- `place bed` makes a `BED | id | x y z | owner` row (it is a block with a rule row, not a special case).
- `sleep <actor>` op, event page `sleep` with conditions (MINE): it is night (daylight < 4), a bed is within 3 cells, no hostile mob within 8, hunger below the starve threshold. Effects: write `SLEEP` row with skipped turns, advance the clock by that, heal, **set the respawn point to the bed** (`SPAWNPOINT | actor | x y z` row; `respawn` reads the newest such row before falling back to `life.pdl`).
- Multi-player (later): sleep only when a RULE percent of players are in beds; skip is one ledger row.
- A bed destroyed: an event page `destroyed` clears the spawnpoint row (append a clear row, never edit).
- Gate: sleep by day refuses; sleep with a mob near refuses; sleep sets spawnpoint, die, respawn lands at the bed (the end-to-end check that proves two rules talk through the ledger).

---

## 7. Farming

NOT FOUND. Design (MINE), all ledger + data + event pages:
```
CROP | wheat | seed=wheat_seed | stages=8 | light_min=9 | water_within=4 | ticks_per_stage=240 | drops=wheat:1,wheat_seed:0-3
SOIL | farmland | from=dirt | tool=hoe
```
- `till <x y z>` (hoe on dirt/grass -> farmland), `plant <seed> <x y z>` (needs farmland, air above, seed in bag: conserved), `harvest <x y z>` (stage == last: drops per the CROP row, replant seed row; earlier stage drops only the seed).
- Growth = an **event page on a random-tick op** `crop_tick_op`: per turn, for each crop pick cells by a **seeded deterministic RNG** (`seed = hash(ledger_position, x, y, z)`), and advance one stage if light >= `light_min`, farmland is wet (water source within `water_within`), and the roll passes. Deterministic so replay matches.
- Farmland dries and reverts to dirt without water; trampling (`jump onto farmland` while moving fast) is an `overlap` collision response, optional.
- Bonemeal/fertilizer later: a data row that adds N stages.
- Gate: a wheat plant in light + water reaches stage 8 in N turns under the seeded RNG; the same with no water never grows (negative control); harvest at stage 3 returns only the seed.

---

## 8. Water and fluids

NOT FOUND. Design (MINE), the simplest correct flow:
- Each cell has `fluid_level` 0..7 (7 = source). Ledger rows `FLOW | tick | x y z | level` only when the level CHANGES; the grid is the fold.
- `fluid_tick_op` is called by an event page once per turn, processing cells **in a fixed order (sorted by y descending, then x, then z)** so two runs give the same result. Rules, each a RULE row: water falls straight down at level 7 if the cell below is air; otherwise it spreads horizontally decreasing by 1 per cell (`spread_decay 1`); a flowing cell with no higher neighbour above or beside decays by 1 per turn.
- **Infinite source** (two sources beside an empty cell make a source) is one RULE row `infinite_source 1`. Turn it OFF in a "conserved water" game mode (water volume is then an item count: no minting; this is the same no-minting check grok wants for fuel).
- Interactions as event pages: water + lava -> `reactions.pdl` rows (rpg-space has `reactions.pdl`: reuse), water next to farmland keeps it wet (section 7), water extinguishes fire, swimming = entity-in-water flag via an `overlap` page (slow move, air counter, drowning damage through `life hurt`).
- Bucket: `scoop`/`pour` ops, conserved.
- Perf: only cells with changed neighbours are in a work list (CPU rule: weak machine, idle cost near zero); cap per turn as a RULE row.
- Gate: a source on flat ground spreads exactly 7 cells and stops; a hole lets it fall; two runs of the same seed give identical FLOW rows; a deliberately wrong `spread_decay` fails the expected-extent assertion.

---

## 9. Mobs

NOT FOUND as built. Design (MINE): a mob is an entity + `life_<id>` + a FSM, exactly like NPCs; the spawner and the AI are event pages.
```
MOB | zombie | hp=20 | speed=3 | hostile=1 | sun=burn | drops=rotten_flesh:0-2 | light_max=7
SPAWN | cap_per_area=8 | min_dist_player=24 | max_dist=48 | per_turns=10
```
- **Spawn:** event page `mob_spawn` on a turn timer: choose a candidate cell by seeded RNG, require solid ground, enough air, `light <= light_max` (day/night section), distance from every player >= `min_dist_player`, area cap not exceeded. Each success appends `SPAWN | mob | x y z`.
- **Despawn:** beyond `max_dist` or by daylight burn (`sun=burn`: take `life hurt` each turn in direct light).
- **AI as an FSM in events:** states idle -> chase (target within `sight`) -> attack (adjacent) -> flee (hp low, optional); each transition is an event page condition, each step a verb (`step <mob> <dir>`) so the replay is exact. Pathing: the same route op the car work needs (grok's trap list: junction tie-break; here, pick the neighbour with the lowest path cost, ties by fixed direction order).
- **Damage:** `attack` page calls `life hurt @target n` with cooldown; knockback is a position verb. Death uses the existing `death` world-op page and drops use `drops.txt`.
- Passive mobs (cow, sheep): same spawner with `hostile=0`; breeding as `breed <a> <b>` needing food items (a conserved op).
- Gate: spawn refused in light; spawn refused too close to the player; a zombie reaches and damages a player in the expected number of turns; kill it, assert drops; sun burn kills one in the open and does not in shade (control).

---

## 10. Trees, leaf decay, and the improvement the owner asked for: destruction that leaves leaves

NOT FOUND (no leaf mechanic located). **Owner wish: "tree destruction leftover leaves".** In Minecraft, breaking a trunk leaves the canopy floating until it decays. In this house we can make that a visible, data-driven mechanic rather than instant deletion.

### 10.1 Design (MINE)
```
LEAF | oak_leaves | persistent=0 | max_dist=4 | decay_ticks=60-240 | drops=sapling:5%,stick:2%,apple:0.5%
```
1. **Distance field.** A leaf block belongs to a tree if a path of leaf/log cells of length <= `max_dist` reaches a log. Store this as a derived projection `leafdist.txt` (never the authority; rebuild from the VOX ledger with `leaf_dist_op rebuild`).
2. **Trigger.** Whenever a log is removed (the `mine` page raises a `destroyed` page, existing world-ops shape), `leaf_dist_op update <x y z>` recomputes distances locally (bounded flood fill, radius `max_dist + 1`). Leaves whose distance is now > `max_dist` get an append-only row `DECAY | leaf x y z | due_tick` with `due_tick = now + seeded random in [decay_ticks]`. **That is the "leftover leaves": they stay and are drawn for a while.**
3. **Decay.** An event page `leaf_decay` (timer) removes due leaves one at a time: `vox_op set air cause=decay`, rolls drops (sapling/stick/apple, deterministic RNG), places `remnants` mini blocks as for any destroyed block. If a NEW log connects the leaf again before `due_tick` (a sapling grew, or the player placed a log), a `DECAY_CANCEL` row clears it.
4. **Persistent leaves** (player placed): `persistent=1` never decays.
5. **Felling polish (optional, same shape):** `fell_tree` option: breaking the lowest log with an axe breaks the connected trunk upward with a per-log delay row, each dropping logs; then leaves enter step 2. It is an event page calling the existing `mine` op repeatedly, not a special engine feature.
6. **Visual:** semi-destroyed stage reuse: leaves in `DECAY` show the "crumbling" stage frame (owner decided 3 stages generated from numbers, so no new art), and a few dust points when they go.
7. **Saplings:** `plant sapling` -> `crop_tick_op` style stage growth (section 7) to a tree by a `TREE` template row (trunk height range, canopy radius), placed as VOX rows with `cause=grow`.

### 10.2 Gate
- Chop the only log of a small tree: leaves remain at tick+1 (assert N leaf cells still present: this is the leftover leaves behaviour), then all gone by `due_tick` max (assert none left and drops within the table).
- Two trees touching: chop one trunk; shared leaves stay (distance to the other trunk is within `max_dist`); control.
- `persistent=1` leaves never decay.
- Replay: identical DECAY rows twice; wrong `max_dist` makes the expected count fail.
- PNG: open it and confirm leaves are visibly floating after the chop and gone after the wait.

---

## 11. Other improvements worth doing (MINE, in value order, each one event page + op + case)

1. **Block damage stages and crack frames** (owner decided, nothing built): stage from `progress/hardness`, numbers-based frames and tones.
2. **Remnants everywhere:** one drop mechanism for mining, death, destroyed (OWNER 4g); remnants drawn and auto-picked up (built in rpg-space 7c: reuse, do not rebuild).
3. **Crafting with 2x2/3x3 and a table** (stated gap in `recipes.pdl`): add `RECIPE | id | grid | output | count | needs_table` and refuse 3x3 without a placed table block within reach.
4. **Fire:** `flammable` column + a `fire_tick_op` spreading by seeded RNG to flammable neighbours, extinguished by water; gives leaves and logs a reason to burn.
5. **Chests/containers** (events `chest` exists in mcl-hq): conserved inventories as rows.
6. **Tools wear:** durability as a row decremented on use; breaking removes it.
7. **Light:** `light` column + `lights.pdl` (rpg-space has it) so torches matter for mob spawn.
8. **Weather** as another timed phase row (rain waters crops, extinguishes fire).

---

## 12. Build order and acceptance (do these in order; one at a time)

0. Owner go-ahead, and the car replay green first.
1. Decision log: answer the five questions, add a row (cite this doc).
2. Scratch voxel app via `game_new_op`; `vox_op` ledger + `blocks.pdl`; place/mine/drops (section 2). Gate.
3. Daylight as numbers + manual turns + sleep skip (section 4, 6).
4. Hunger on turns + eat (section 5).
5. Farming (section 7) then water (section 8); water must exist before crops need it.
6. Trees + leaf decay (section 10).
7. NPC schedules (section 3), then mobs (section 9).
8. Only then: fire, weather, tool wear.

Each step ends with: a green case, its negative control red, the replay twice, an opened PNG, a commit through `commit_scoped.+x`, and one row each in the decision log, this doc (add a "BUILT" subsection with the case name and the numbers you observed), and the `16.game/00-INDEX.md`.

## 13. Traps (from this house, READ in AGENTS.md and the pitfalls)

- No Python, no new `.sh`. Harness = pal + case + C op.
- CPU: one heavy job at a time through `cpu_gate`, niced; a page run is a sh+bash+prisc launch (about 150 ms), never per tick; work lists, not whole-world scans.
- Runtime files need a cap (`RUNTIME-FILE-GROWTH-STANDARD.md`); petslike `events.txt` silently drops over 4 MiB (the live rpg-space log froze this way), so plan ledger rotation before a world that appends every turn.
- Do not copy third-party art or game text; Tear-it art is new levels only; palettes/tiles come from the palette studio.
- A scratch copy of an app can call the LIVE one by absolute path in event pages (df-hq-test flipped the live INT switch): rewrite paths before running.
- Do not edit `game_core.c` for a rule (D92). If a generic option is needed, add it as a generic engine option with its own case, not a per-mechanic branch.

## 14. Questions for the owner before building

1. Is the voxel skin a new app, or evolve `mcl-hq`?
2. Hunger on manual turns (my proposal) or keep wall-clock?
3. Infinite water sources allowed (Minecraft) or conserved water?
4. Leaf decay delay: fixed or random per leaf (proposal: random, seeded)?
5. Graphic level default for damage/blood (per game, OWNER decided that it is per game; value not chosen).

## 15. Preflight check, same day (RAN: grep and find, nothing built)

- `vox_op` is NOT FOUND: a search of every `.c`, `.pdl` and `.md` under `44.xyz.01.00` (minus `xyzfs`) for the name returned no file. Section 2's "vox_op ledger" is therefore a MINE design, not a primitive. Step 2 of section 12 has to build it (or reuse the `rpg-space`/`rpg-dust` `blocks.pdl` rows plus the pieces ledger; read them first).
- Found and usable: `blocks.pdl` in `rpg-space`, `rpg-dust`; `life.pdl` in `rpg-pet`, `rpg-space`, `cdda-hq` (hunger_ms, off by default); `mech_new_op` (`_shared-lib/ops/+x`, case `mech_new`, 43 checks); `rta_life_op` (turn-based hunger, `TURN` rows) as a model for hunger on turns.
- `@.apps/rta/` has `ops/` only: no `events/` folder yet, so nothing calls `rta_life_op` from an event page.
- Confidence ranking and the owner questions in section 14 are unchanged; Q1 (new app or evolve `mcl-hq`) now decides step 2.
