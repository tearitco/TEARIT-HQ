# Early survivor loops, books, and common events

2026-11-04. Owner ask, written into the game chapter. The prose version is `XO/31.Nov/nov-4/6.grok-read-and-tear-it-co-2026-11-04.md`. This page is the technical needs. Nothing here is built. `GAME-MECHANICS-SPRINT.md` still lists weather and liquid flow under what waits. `S1-EVENT-LEDGER.md` still returns `todo:<verb>` for `walk`, `dig`, `build`, and `craft`.

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

## Order to build

1. `furniture.pdl` and `smash` for one bench: plank, rag, pipe, can. Needs tick already specified in the sprint. One fire, one drink, one eat, one `wield`.
2. `books.pdl`, `skills.pdl`, `read`, `practice`. One recipe hidden until the cap rises, then one craft that spends the smash pile.
3. `gen_map` for one CDDA house chunk and one DF column. Same seed twice, identical files. One mutation case must fail.
4. `fluids.pdl`, one `flow` step, one `rain` phase, one fire tick.
5. MCL bench, 3×3, bed cell, one `burst`.
6. Shop slice after this, with the three item ids mapped. Not before.

## Out of this slice

Vehicles, mutations, bionics, factions, military, moods, caverns, redstone, a second z world, and a full recipe import. Weather beyond the rain phase. Fluid faster than one cell per tick.
