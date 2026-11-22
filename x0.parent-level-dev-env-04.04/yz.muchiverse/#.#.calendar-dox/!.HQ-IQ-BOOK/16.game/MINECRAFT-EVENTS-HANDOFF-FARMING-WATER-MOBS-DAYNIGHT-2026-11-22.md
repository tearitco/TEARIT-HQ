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

## 16. BUILT 2026-11-22: steps 1 to 3 of section 12 (scratch-first; NO window, NO picture yet)

Decision **D105** (`02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`). Defaults the lead set for section 14: the voxel skin is a NEW app (not `mcl-hq`), hunger will run on manual TURNS (step 4, not built), leaf decay random per leaf but seeded (not built), the water source rule is decided later.

**What exists (all under `44.xyz.01.00`):**
- `@.apps/vox/ops/vox_op.c` (one op, `--describe`; built with `gcc -std=gnu11 -Wall -Wextra -O2`, no warnings): verbs `give move mine place wait sleep` (write) and `status cell inv verify replay` (read). The ONLY state is the append-only ledger `#.desktop/vox_ledger.txt` (git-ignored): rows `GIVE MOVE DAMAGE MINE DROP PLACE TURN DAY SLEEP SPAWNPOINT`. The world is the fold of those rows over `@.apps/vox/terrain.pdl` (a CONFIG). A refusal exits nonzero and writes NOTHING (the verb builds all its rows in memory and appends once). Exits: 10 unknown actor, 11 out of bounds, 12 out of reach, 13 target invalid, 14 ledger not reproducible, 15 no support, 16 item missing or not placeable, 17 tool power below `min_power`, 18 move blocked, 19 not night, 20 no bed near, 21 bag full, 9 missing rule or data file.
- Numbers: `@.apps/vox/rules.pdl` (`RULE` world size, reach 4, stack_max 64, move_max, turns per verb, `minutes_per_turn` 10, `day_minutes` 1440, `start_minute` 420, sleep rules; `TOOL` powers; `DAYLIGHT | minute | level` rows, linear between rows, integer division toward zero) and `@.apps/vox/blocks.pdl` (BLOCK rows shaped like rpg-space's: hardness, tool_best, min_power, drops, count, solid; the other columns are kept unread). The C file holds none (a rules file without `reach` makes `mine` exit 9). Note: the handoff's "144 turns of 5m = one day" is 720 minutes; with `minutes_per_turn` 10 it is 144 turns = 1440 minutes = one day.
- Mine: a swing adds the tool's power (a tool that is not the block's `tool_best` swings at the hand's power) to a `DAMAGE` row's progress; at `progress >= hardness` the same write adds `MINE` and `DROP` (the drop goes to the bag); stage = `floor(3 * progress / hardness)`. Place removes one item from the bag and needs a solid neighbour (`place_needs_support`). Each accepted verb adds a `TURN` row (`turns_*` rows); `wait n` is the manual turn; time of day is derived from the turn count and never stored; `DAY | d` is written once per midnight crossed. Sleep (night = level <= `sleep_max_light`, a `bed` block within `sleep_bed_reach`) writes `SLEEP | actor | bed | skipped_turns=N`, `SPAWNPOINT` and the `TURN` row that is the clock advance. NOT checked yet: a hostile mob near (no mobs exist); the spawnpoint is written but nothing reads it (no death yet).
- Hand-edit refusal: every writing verb first replays all intent rows (`GIVE MOVE DAMAGE PLACE SLEEP`, `TURN | n | wait`) into an anonymous in-memory file and compares bytes with the ledger (derived rows `MINE DROP DAY SPAWNPOINT` and non-wait `TURN` are regenerated, never replayed); a difference exits 14 and the verb adds nothing. Limit: a hand-added intent row that is itself legal (for example an extra `GIVE`) replays fine, because the ledger is the authority; only rows the rules would not have produced are caught. `vox_op replay <data> <ledger> <new file>` does the same into a new file (refuses an existing destination). Cost: O(n) folds per intent row, so verify is O(n^2); fine at scratch size, measure before a live window with thousands of rows.
- Event pages (D92, rta precedent D103): registry `COMMAND vox_place`, `vox_mine`, `vox_wait`, `vox_sleep` in `#.ref/menu/event_commands.registry.pdl` (the registry allows 4 params, so a page passes `cell` as one word `x,y,z`), four pages under `@.apps/vox/events` seeded by `bp_edit_op` (never hand-written), `BOOK | vox | events | @.apps/vox/events` in Blueprintz `books.pdl`.
- `mech_new_op`: its kinds `mine`, `place`, `sleep` count resources only (no cell, reach, support, time), so they cannot express these refusals. Not used here and not duplicated; resource accounting could later move onto `mech_step_op`.

**Harness `vox`** (`&.widgits/_shared-lib/harness/vox.pal`, `cases/vox.pdl`; run `cd harness && ops/+x/harness_case_op.+x cases/vox.pdl`, about 2 minutes because it builds 25 mutants; started through `cpu_gate`). OBSERVED: `VERDICT|PASS|passed=266|failed=0`. It covers: the daylight table (start 420 = level 15, 19:00 = 8, 06:00 next day = 8, midpoint 390 = 11, 144 turns = one day = exactly one `DAY` row); place/mine/drops with HAND-computed stages and 16 refusals each checked with `SUMTREE` before and after (nothing written); replay into a new file twice gives identical bytes (`cmp`); four hand-edits (a `DAMAGE` progress edited, the `DROP` rows deleted, a forged `MINE` row appended, a row of no known kind) each exit 14 on `verify` and on a writing verb with the ledger unchanged after; sleep by day refused, at night with a bed `skipped_turns=66` (HAND: 1200 to 420 is 660 minutes), clock lands on 07:00 next day, `SLEEP`/`SPAWNPOINT`/`TURN`/`DAY` rows, no bed near refused; registry rows (one each, a sabotaged copy has 0, exact templates), page IR and trigger, the page wrappers print the SAME lines and write the SAME bytes as the op alone; the ledger is git-ignored (and a neighbour name is not). **25 sabotages**: 22 code clauses broken with `sed` on a copy of the source (reach, min_power, tool_best, hardness, place on solid, support, empty-bag place, non-block item, consume, verify skipped, verify compare removed, drop past stack_max, give past stack_max, sleep by day, no bed, skip arithmetic, `DAY` rows, move into solid, long jump, sleep clock advance, unknown actor, bounds) and 3 data edits (one `DAYLIGHT` value, the `reach` RULE, dirt hardness). Each must change the output; each has a vacuity guard (source really differs, compiles, runs) and a control (the same scenario on the real op passes the inverted check, 25 controls). The first run found one bad mutant of mine (a `.` in a sed replacement); fixed.

**What is next (not done):** step 4 hunger on turns plus `eat` (model: `rta_life_op`), step 5 farming then water (the water source decision is still open), then trees and leaf decay (seeded random delay), NPCs, mobs. Before those, the LIVE step: make the app shell with `game_new_op` (a skin that reads `vox_op cell` per cell; the op has no layer dump verb yet, add `vox_op layer <y>` for the 2D/3D projection), photograph a mined and a placed block and a sky level from the `DAYLIGHT` rows, and measure the per-verb cost with a few thousand ledger rows. Nothing here was run in a window or with a picture.

## 17. BUILT 2026-11-22: step 4, hunger on manual turns and EAT (decision D110; NO window, NO picture)

**What exists (all under `44.xyz.01.00`):**
- `@.apps/vox/ops/vox_op.c`: new verb `eat <actor> <item>` and read verb `hunger <actor>` (prints `hunger=N max=M starved=0|1`). New ledger row `EAT | actor | item | value`. Hunger is NOT stored: the fold adds `hunger_per_turn` per turn to every spawned actor on each `TURN` row (cap `hunger_max`) and subtracts the `EAT` value (floor 0). The eat verb costs `turns_eat` turn(s), so its printed `hunger=` is after that turn.
- Numbers in `@.apps/vox/rules.pdl`: `hunger_per_turn` 1, `hunger_max` 40, `starve_at` 30, `turns_eat` 1, `food_apple` 4, `food_bread` 12. A food is one `RULE | food_<item> | n` row.
- Refusals (each writes NOTHING): 10 unknown actor, 16 not food (no `food_<item>` row) or none in the bag, 23 hunger 0. Starvation consequence: at `hunger >= starve_at` `mine` and `place` refuse with 22; `eat`, `wait`, `sleep`, `move`, `give` still work. No death rule.
- Event page: registry `COMMAND vox_eat` (`PARAMS actor,item`), page `@.apps/vox/events/vox_eat` seeded by `bp_edit_op` (`append vox_eat 'actor=joe|item=bread'`, `trigger ui`); the runtime `inventory/` and `.hq_manager/` folders it leaves were removed before the commit.
- `state.pdl` names `eat` among the ledger's writer verbs. Replay: `EAT` is an intent row, its `TURN | n | eat` row is derived.

**Harness `vox`** now `361` (was 266; +95): cases `hunger`, `hunger-data-and-page`, `hunger-sabotage`. Hand-computed numbers: spawn move = hunger 1, wait 5 = 6, eat apple = 6-4+1 = 3, eat bread = 0+1 = 1, wait 100 = capped 40, starved eat apple = 37, eat bread = 26. 11 new sabotages (26 hunger does not rise, 27 no cap, 28 eat does not lower, 29 eat keeps the item, 30 starved can mine, 31 starved can place, 32 eat at hunger 0 allowed, 33 non-food eaten, 34 eat costs no turn, 35 replay ignores EAT, 36 DATA food_apple 4 -> 9), each with a vacuity guard and a control on the real op. Regression observed the same day: `mech_new` 43, `rta_life` 62, `rta_pages` 81, all PASS.

**Cost of verify (OBSERVED once each, scratch ledgers of `wait` turns, niced, no browser measurement):** 1007 rows: `verify` 0.42 s, one `wait 1` 0.40 s; 5035 rows: `verify` 9.9 s, `wait 1` 11.0 s (5x rows = about 25x time: the verify is O(n^2), because every intent row re-folds the ledger). `hunger` (read, no verify) is 0.004 s at 1000 and 0.012 s at 5000. A long game needs a ledger checkpoint or a hash chain before step 5; not built.

**Not done:** death or damage from hunger, hunger effects on sleep or speed, food from the world (farming is step 5), a window or picture, showing `vox_eat` live in Blueprintz (see the presentation's Blueprintz section for the steps).

## 18. BUILT 2026-11-22: the PICTURE (decision D112; offline proof, NO window opened)

Owner complaint: the voxel app had no picture. Built: the ledger world as a pc-hq desk in 2D and 3D, a board shell, and an offline render you can open as a PNG. Nothing here opened a window.

**What exists (all in `@.apps/vox`, op `ops/vox_op.c`, harness case `vox_desk`, 69 rows).**
- `vox_op desk <data> <ledger> <desk_dir> [png=<raw_to_png path>]`: writes `cells.txt`, `cells.marker`, `cells.rgba`, `map.txt`, `extrusion.pdl`, `skin.txt`, `events.txt` and (with `png=`) `map.png`. Top-down: each column shows its highest block; the engine height path (`map.txt` glyph `2`..`G` = height 2..16, `1` = 1, `.` = empty, plus the `EXTRUDE` rows of `extrusion.pdl`) makes the 3D column as tall as the world. Axes: desk column = x, desk row = z, voxel y = height. `F,Wl` per cell: floor slot = the TOP block, wall slot = the block under it (height 1 has wall 0, because a wall slot on a height-1 cell makes the engine raise it to 3). No depth token is written, so a column is exactly its height tall.
- `vox_op layer <data> <ledger> <desk_dir> <y> [png=...]`: one y level as a flat floor (`F,0` of the block at that y); y above `world_y_max` refuses (2).
- Colours are DATA: `PICTURE | kind | slot | r | g | b` rows and one `PLAYER | r | g | b` row in `blocks.pdl`; `cells.rgba` is built from them (16 columns x 24 px tiles, 3 rows = 110592 bytes, as Teario's). The player is an `events.txt` row `x z r g b People1 0 2 1` (Teario's People1 row shape); mobs later are more rows.
- `vox_op feed <data> <ledger> <actor> feed=<file> desk=<dir> png=<op> self=<vox_op>`: the game bar feed (`gm_on gm_title gmt_i_text gmb_i_label gmb_i_act gm_nt gm_nb`), one row of turn/day/light, one of position and hunger, one of the bag, eleven buttons (Mine Place Eat Wait Sleep Left Right Back Forward Up Down).
- `vox_op act <data> <ledger> <actor> <action> desk=.. feed=.. png=..`: what a button runs. mine = the highest solid block under the player's feet (the tool is the block's best tool if it is in the bag, else the hand); place = the first placeable item in the bag on top of that column; eat = the first food; wait = 1 turn; sleep; x- x+ z- z+ up down = one `move`. It appends the verb's rows, then rewrites the desk and the feed; a refusal changes nothing (not even the marker). `act` verifies the ledger first, so a hand-edited ledger refuses (14).
- Offline render `&.widgits/board-viewer/ops/bv_gpu_desk_render.c`: rebuilds the engine's maker scene from a desk folder and draws it with the same GPU raymarch (headless EGL, no window, no live engine): `bv_gpu_desk_render <desk_dir> <out.raw> [yaw pitch dist]` then `raw_to_png <raw> <png> --size 640x400`. Build: `gcc -O2 -I. -o x bv_gpu_desk_render.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2`, run with `BV_SHADER_FILE=<ops>/bv_raymarch.frag`. Row 0 of the desk is the bottom of this picture (the camera stands on the row-0 side). Observed in the picture: the top face shows the floor slot, the side faces the wall slot.
- Board shell: `@.apps/vox/board/{pchq-board.xhtpm,pchq-board.css,side_style.pdl,open_board.sh}`, `@.apps/vox/button.sh`, package `@.apps/vox/vox-pchq` (only the book's `game.pdl` is tracked; the desk is made at launch). No game driver: `open_board.sh` writes the desk and feed from the LIVE ledger (read only), opens the shared pc-hq board with the voxel template, starts the book `vox` on page `play`.

**How a later job opens the window (one window, only when no other board is recording).**
1. Build the op into the app: `cpu_gate <house> run vox-build --wait 120 -- nice -n 15 gcc -std=gnu11 -O2 -o @.apps/vox/ops/+x/vox_op.+x @.apps/vox/ops/vox_op.c`.
2. The live ledger `#.desktop/vox_ledger.txt` needs a player: `vox_op move @.apps/vox <ledger> player 8 3 8` (spawns on the surface; y 3 is air above the y 2 dirt) and `vox_op give ... player cobble 8 <why>` (a GIVE row states its reason; this is a state change of the live game, so say it). Or point the launcher at a scratch ledger first.
3. `cpu_gate <house> run vox-window --wait 120 -- sh @.apps/vox/button.sh run` (it is `nice`d by the gate; the launcher itself checks the gate again; never wrap `reset`).
4. UNVERIFIED parts a window job must check: that the book `vox` is listed and the page `play` opens (the instance init, `CONFIRM_START_MAP`), that the 2D renderer shows `map.png` and the player figure from `events.txt` at the right cell, that the engine reloads when `cells.marker` grows after a button press (the 3D camera may need key 6, desk reload, as Make does), that the engine's default camera frames a 16x16 desk, and that the button actions (`gmb_i_act`) run from the popup.
5. Photograph, open the PNGs: (a) the 3D view before any press; (b) after Mine x2 (a pit one level deep at the player's column); (c) after Up, Place, Up, Place (a cobble tower, 4 levels); (d) the 2D view at (a) and (c) (only the cobble tower top shows as a different tile colour; a pit shows in 2D only once it is dug down to stone, otherwise use `layer` slices); (e) the popup text (turn, hunger, bag).

**Evidence of this section (run 2026-11-22, offline).** Opened `b.png` of the scratch run: a 16x16 dirt slab with a 4-level cobble tower, a pit beside it and the yellow player box. Harness `vox_desk`: expected cell rows are hand-computed; marker sizes 1, 2, 3 after desk, first swing, second swing; a refused act leaves every desk file byte-identical (the SUMTREE of the desk); replay twice is byte-identical and the desk projected from the replayed ledger equals the live one; a deleted desk regenerates the same bytes (`map.png` included) and renders the same raw; the render after a mine differs from the one before and the same desk rendered twice is identical; four sabotages (height off by one, marker never grows, act does not redraw, no player figure) each fail, and the real source is cmp'd unchanged. Not built: the window itself, mobs as `events.txt` rows, per-layer 3D (the heightmap shows a column's top block only), water and leaf transparency, event pages for `act`, a 2D height cue.
