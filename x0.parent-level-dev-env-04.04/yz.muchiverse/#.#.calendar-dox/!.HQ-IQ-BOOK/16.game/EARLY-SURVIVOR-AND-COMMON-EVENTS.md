# Early survivor loops, books, and common events

2026-11-04. Owner ask, written into the game chapter. The prose version is `XO/31.Nov/nov-4/6.grok-read-and-tear-it-co-2026-11-04.md`. This page is the technical needs. Status 2026-11-04: steps 1-6 are built in the editor's QUARANTINED book (see the checklist under "Order to build"); the slice is complete in the quarantined book (nothing outside it calls it). `GAME-MECHANICS-SPRINT.md` still lists weather and liquid flow under what waits. `S1-EVENT-LEDGER.md` still returns `todo:<verb>` for `walk`, `dig`, `build`, and `craft`.

One resolver. Three places a craft may run. New facts are files, not a second C game.

## What already exists (do not rebuild)

- Ledger: `state/events.txt` rows `EVENT|ts|actor|verb|target|at|queued`, `EVRES`, `REJECT`. Verbs `event`, `resolve`, `inventory`. Replay is the inventory. `&.widgits/_shared-lib/game_events.c`.
- CDDA recipe row already on disk: `RECIPE|id|output|count|tool|skill|minutes|inputs` in `@.apps/cdda-hq/recipes.pdl`. Example: `bandage` from `rag:1`. Skill is a number today, not a skill id plus a cap.
- MCL recipe row: `RECIPE|id|grid|output|count`, nine tokens, `.` empty. `@.apps/mcl-hq/recipes.pdl`.
- Registry commands that a page may call: `change_items`, `change_gold`, `take_gold`, `shop_processing`, `control_switch`, `control_variable`, `change_variable`, `craft` is not a registry command. The game verb is the ledger verb `craft`.
- Switches and variables: `switches.txt` and `variables.txt`, name and integer, written by `control_switch` and `control_variable`.
- Clock phases already drive dusk lamps. Rain is another phase on that clock, not a new clock.
- Item ids stay three (cdda, df, mcl). A shop maps them. Conservation runs per id.

## Files this slice adds

All plain text, one row per line, no `|` inside a field except the ledger, which already uses `|`.

| File | Row | Who reads it |
| --- | --- | --- |
| `<game>/furniture.pdl` | `FURNITURE\|id\|hp\|drops` drops are `item:count` commas, no spaces | CDDA smash. MCL and DF do not use this table for their start |
| `<game>/books.pdl` | `BOOK\|item_id\|skill\|cap\|minutes` | Read verb. Raises cap only |
| `<game>/state/skills.pdl` | `SKILL\|actor\|skill\|level\|cap\|xp` | Recipe visibility and practice |
| `<game>/state/fluids.pdl` | `FLUID\|room\|x\|y\|kind\|volume` kind is `water` or `lava` | Common water/lava step |
| `<game>/state/fire.pdl` | `FIRE\|room\|x\|y\|fuel` | Fire tick, rain, water step |
| `<game>/minerals.pdl` | `MINERAL\|id\|tile\|item` | `gen_map` writes the id into the column. Dig reveals it. No second roll |
| map chunk | existing `cells.txt` plus a furniture id on the cell | `gen_map` |

`drops` and recipe `inputs` share the `item:count` shape the CDDA recipe file already uses.

## Verbs this slice adds to the ledger

Same `EVENT` line. Bodies replace `todo:<verb>` only for these. A missing body stays `todo:`.

| Verb | Target | Result |
| --- | --- | --- |
| `smash` | furniture id at `at` | hp down by 1. At 0, append each drop via `give_item`, clear the furniture id, leave the floor |
| `read` | book item id | Spend `minutes` on the clock. Set `cap` on that actor's skill row. Level unchanged |
| `practice` | skill id | Called by a resolved `craft`, `smash`, `dig`, or `strike` of that skill. `xp` up. Level up only while `level < cap` |
| `wield` | item id | One held slot. The melee need is this slot, not a new item type |
| `eat` `drink` | item id | Need number down. Item count down. Refuse at count 0 |
| `light` | cell | Write a `FIRE` row if a fuel item was spent or the cell is fuel |
| `flow` | `water` or `lava` | One step, see below |
| `rain` | phase on or off | Common event. No target cell |
| `burst` | radius | Creeper only. Cells in the radius become air. No drops |

`give_item` and `take` stay the only inventory writers. Smash and craft call them. They do not edit the count file themselves.

## Craft places (one resolver)

`craft` checks, in order: skill `level` and `cap` from `skills.pdl`, tool, inputs on the actor or the place, then time.

- CDDA place: the actor's feet and the four neighbors, or a tool in hand. No bench required. First recipes paid by the smash pile: pipe spear, bandage, fire drill, rag bind.
- DF place: a workshop cell whose stockpile holds the inputs. The anvil is a tool row the first metal recipe names. Dig practice uses `practice` on mining. The wagon, not a smash, supplies food, drink, seeds, picks, and the anvil (`give_item` at embark).
- MCL place: 2×2 matches the inventory shape with no bench. 3×3 matches only when the actor stands on a crafting-bench cell. Furnace is `craft` plus a fuel item plus minutes. Bed is a placed cell id that `spawn_actor` reads on death. It is not the CDDA sleep verb.

A recipe with a skill the actor has not capped stays out of the list. The row remains in `recipes.pdl`.

## gen_map

One writer, three calls. Output is files, so a second run on the same seed matches byte for byte. That is the same determinism gate as the campus legend.

- CDDA: overmap cell `forest|field|road|house`. Stepping on `house` writes one chunk: walls, doors, furniture ids from `furniture.pdl`, loot as `give_item` rows at cells. Basement is the next z, not a second map format.
- DF: one site column. Soil, stone, mineral id from `minerals.pdl`. The player never walks onto a new chunk in this slice. Dig reveals the id already written.
- MCL: seed, biome, height, trees, ore in stone, water volume at sea level into `fluids.pdl`. New chunks only when the player walks off the loaded set.

## Common events

A common event is an event page with no owner cell. The clock runs it once per phase tick. One cell changes per call.

- `flow`: pick one `FLUID` row with `volume > 0` and an empty or lower neighbor. Move 1 volume. Refuse if the move would raise the sum of volumes. Lava on a fuel cell calls `light`. Water on a `FIRE` row or an open container calls extinguish or `give_item` of water.
- `rain`: while the phase is on, every open container gains 1 water, every exposed `FIRE` loses 1 fuel, every farm or dirt cell sets switch `wet` for that cell name. Off does not dry the cell in this slice.
- Fire tick: every `FIRE` row loses 1 fuel. If fuel remains, one neighboring fuel cell may gain a `FIRE` row. At 0, delete the row. Rain and water call the same delete.
- Minerals do not flow and do not roll at dig time.

Creepers, beds, and benches are not common events. Night spawn is the clock waking a mob page. The creeper page calls `burst` once. The bench page runs only when the actor's cell is the bench.

## Conservation

Per item id, and per fluid kind. A smash that lists `plank:2` must have those planks in the drop row before the call, and the furniture cell must be consumed. A flow step conserves volume. `burst` is the exception that is allowed to destroy blocks and drop nothing. The event writes that, so a replay does not invent drops. Coins are unchanged by this slice. The shop check stays a separate page.

## Order to build (status checklist, updated 2026-11-04)

The house-wide checklist (all features, done vs not done, spec links) is `../08-roadmap/GRAND-ROADMAP-CHECKLIST-2026-11-04.md`; this section is the detailed view of this slice.

Where it lives: the editor's own book `@.apps/rpg-edit-hq/books/ledger-lab/` (has a `QUARANTINE` file; every op refuses a folder without one). "Proven" means a scratch-house harness passes AND a mutation run fails; nothing below runs in cdda-hq, df-hq or mcl-hq yet.

- [x] **1. furniture + smash + needs + eat/drink/wield/light.** `ops/survivor_op.c`. Harness `survivor_slice.pal` 44 PASS. One real hand-read run in the lab book.
- [x] **2. books, skills, read, practice, craft.** Same op. `survivor_craft.pal` 37 PASS. One recipe hidden until the cap rises, then spends the smash pile.
- [x] **needs follow the game clock** (added, not in the original list). Core `clock_event` handles `common:needs_tick` via `survivor_clock.pdl`, shipped `ENABLED | 0`. `needs_clock.pal` 13 PASS. The schedule row is in `time.pdl` but NOT installed on the live clock.
- [x] **3. gen_map for one CDDA house chunk + one DF column.** `ops/gen_map_op.c` (splitmix64 over FNV-1a of the seed text, files only). `gen_map.pal` 21 PASS: same seed = byte-identical files, different seed differs, 4 mutations refused, generated furniture is smashable. Not done: MCL chunks, generation when the player steps onto a house, loot pickup verb, loading the chunk into the editor.
- [x] **4. fluids, one `flow` step, one `rain` phase, one fire tick.** `ops/world_common_op.c`. `world_common.pal` 38 PASS, mutation run fails as it must. As built (differs from the table above): one room only (no room column), fire stays in `state/fire.txt` (`x|y|fuel`, the file `light` already writes), fluids in `state/fluids.txt` (`x|y|water or lava|volume`), page bounds 0..63; flow gives 1 volume to the first N,E,S,W neighbour that is empty or at least 2 lower (so it cannot oscillate) and refuses a move that would change the volume sum; water and lava never mix; lava landing on furniture lights it; water landing on a fire puts it out; rain fills containers (cap), lowers exposed fire fuel (a `roofed.txt` cell is exempt), writes `wet.txt` rows for `dirt.txt` cells, and turning rain off does not dry them. Clock wiring (2026-11-04): core `clock_event` handles `common:world_flow` / `world_rain` / `world_fire` through `world_clock.pdl` (shipped `ENABLED | 0`), `world_clock.pal` 14 PASS; the `time.pdl` rows are not installed on the live clock. Not done: MCL water volume at sea level, creeper/night spawn.
- [x] **5. MCL bench, 3x3 craft, bed cell, one `burst`.** `ops/mcl_op.c` (`craft`, `smelt`, `place`, `respawn`, `burst`), data `mcl_recipes.pdl`, items `mcl.*` in the lab `items.pdl`. `mcl_bench.pal` 49 PASS, mutation run fails as it must. As built: a recipe whose cells fit one 2x2 corner needs no bench, a larger one needs a placed `bench` cell at the actor's x,y; refusals burn nothing; placing a bench, bed or furnace burns the matching item; `respawn` reads the actor's own bed cell else a fallback; `burst` (radius 1..4, dx*dx+dy*dy <= r*r) removes block rows and placed cells and writes NO item (the ledger is byte-identical before and after). Not done: nothing calls these from the clock or a player action, no night spawn page, the respawn position file is read by no real spawn yet, no port into `mcl-hq`.
- [x] **6. Shop slice with the three item ids mapped.** Spec `SHOP-SLICE-SPEC-2026-11-04.md`; `ops/shop_op.c` (`list`, `buy`, `sell`), data `shop.pdl`, coin item `coin.play` in the lab `items.pdl`. `shop.pal` 41 PASS, mutation run fails as it must. Decisions (owner 2026-11-04): coins on the ledger; ids stay three. As built: each sale is ONE atomic ledger `trade`; the shop never mints (stock and till are minted by the owner); every price row needs sell < buy or `shop.pdl` is refused at load; no verb swaps one id for another, a cross-id exchange is sell for coins then buy for coins; refusals change no balance (8 of them checked at once by comparing the whole ledger balance before and after). exchange-hq is NOT used: it is a paper currency order book with no game items. Not done: the editor Shop tab (older, separate coin counter), a real customer, restock, dynamic prices, the coin <-> exchange bridge.

Items the owner decided: three separate item ids (cdda / df / mcl); everything quarantined in the editor's own book until the owner lifts it (release criteria in the lab book README).

## Out of this slice

Vehicles, mutations, bionics, factions, military, moods, caverns, redstone, a second z world, and a full recipe import. Weather beyond the rain phase. Fluid faster than one cell per tick.
