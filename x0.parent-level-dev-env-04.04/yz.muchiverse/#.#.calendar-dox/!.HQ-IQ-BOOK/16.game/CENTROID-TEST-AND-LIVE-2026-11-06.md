# centroid-test and centroid-live (2026-11-06)

Status: **plan. Nothing here is built.** SAID = the owner on 2026-11-06. OBS = files opened for this stock-take.

Two runs of the same campus, named by the owner:

- **centroid-test** is a playthrough on a scratch house. Pets meet food and sleep, plant food, make beds, gather stone, and raise one room. The harness can fail, and a second attempt is another phase table.
- **centroid-live** is that same table on the board you watch. It adds one toggle: the RPG Maker skin or the generated skin. The columns do not change.

The campus shape stays `CENTROID-VISUAL-BOOK-2026-11-06.md` (three floating islands, voxel rooms, a swinging door). This note is only how the two runs are fed.

## What we already have

| Piece | Where | What it does today |
|---|---|---|
| Dig and haul | df-hq `dig`, `dig_area`, `undig`; playthrough `play_df_hq` (36 checks) | A dwarf marks rock, walks, and receives stone. Stocks tab shows the stone total. |
| Mine, craft, place | mcl-hq `mine`, `craft`, `place`; playthrough `play_mcl_hq` (43 checks) | Mine stone and logs, craft planks, place a block. A placed block stops a walker until it is mined. |
| Hunger | `needs_tunables.pdl`, harness `digipet_needs` | Hunger rises one step a day. Food lowers it. Starving is a later threshold. Default in the design is off until a world turns it on. |
| Sleep, as a design | `ENTITY-NEEDS-AND-CARE-DESIGN.md` | `sleep` restores `energy`. A bed is named as a place to sleep. The command is a design, not a df-hq verb. |
| Planting, as a design | the same note, "farming later" | No plant verb in df-hq or mcl-hq `verbs.pdl`. |
| DF menus | `DF-HQ-DIG-HOWTO.md` | Designations (mine), Build (wall, door, workshop are quiet "later" rows; no rule), Stocks, Units (labor and skills are "later"), Help, the book dropdown for which pet. |
| Shared verbs that are pages | df-hq `verbs.pdl` | `place` takes an item and a cell. `resolve` is the fortress job pass. `clock` and `day_tick` advance time. `view` is 2D / 3D / glyph, which is not the art skin. |
| The other skin | palette studio, D33–D35 | Sheets can be generated. Nothing in the campus reads a `VIEW` row yet. `out/good/` is empty until you keep a sheet. |
| The harness | `playthrough_op` | One phase table, a scratch house, a verdict, a weight in the behavior bank. |

## What centroid-test still needs

One phase table, `play/phases/centroid_test.phases.pdl`, on the verbs above. The missing rules, in order:

1. **Needs on.** Hunger and sleep start on for these pets only. A day tick with no food raises hunger. A night with no bed drops energy. The table fails if a pet finishes the room while starving or exhausted. Dying stays off for this run.
2. **Plant.** A farm cell spends a seed from the pile and, after a counted number of day ticks, gives food. The food is the only thing that lowers hunger. No craft invents a meal.
3. **Bed.** `place bed` spends planks (mined logs, the mcl craft). Sleep on that cell restores energy. Sleep on the ground does not.
4. **Room.** The stone pile from digging is the only stone. Place floor, walls, roof, and one door. The door proof stays the one in the campus note: shut blocks, open enters the room under the roof.
5. **Order.** A pet that is hungry plants or eats before it hauls. A pet that is tired sleeps before it places. That is the DF job pick, and it is the thing the table is for. Build-tab rows for wall, door, and workshop stop being "later" only for the rows this table uses.

A second attempt edits the phase table. It does not edit the pets by hand.

## What centroid-live adds

The same verbs, on the board, with one extra field: `VIEW | rmmv` or `VIEW | studio`. The key writes that field. It is not the existing `view` verb (2D / 3D / glyph). Both can be on at once: a 3D picture of the generated skin, or a 2D picture of the RPG Maker skin.

Until a studio sheet exists for stone, wood, plant, bed, and door, that face stays on the RPG Maker sheet and the corner says so. The live run is not started until centroid-test passes. The live window goes through `cpu_gate`. The test never opens it.

## Menus the live run shows

Designations and Build gain the rows the test proved: plant, bed, wall, door. Stocks gains food and planks beside stone. Units gains hunger and energy beside the name, because a need you cannot see is not on. The skin toggle is one row under View, labeled with the sheet set, not a new menu.

## Not in either run yet

The second bridge, the second island, the tower, shops, and the school. Labor skills. Death. A generator that draws the campus. Those wait on a passing centroid-test.
