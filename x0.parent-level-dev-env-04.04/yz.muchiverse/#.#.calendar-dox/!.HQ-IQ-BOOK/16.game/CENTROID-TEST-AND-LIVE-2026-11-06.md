# centroid-test and centroid-live (2026-11-06)

Status: **plan. Nothing here is built.** SAID = the owner on 2026-11-06. OBS = files opened for this stock-take.

Two runs of the same campus, named by the owner:

- **centroid-test** is a playthrough on a scratch house. The University is **already built** when the run starts. Pets meet food and sleep, plant food, and make beds on that campus. They do not raise the buildings in this run. The harness can fail, and a second attempt is another phase table.
- **centroid-live** is that same table on the board you watch. It adds one toggle: the RPG Maker skin or the generated skin. The columns do not change.
- **Later, not this test:** a manager is shown the master blueprint of that prebuilt campus and asked to break it into jobs for employees. The prebuilt volume is the answer. The manager passes only if the employees' placed columns match it. That run is not part of centroid-test.

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

The campus volume is data loaded at the start of the scratch house, not a thing the pets place during the test. One phase table, `play/phases/centroid_test.phases.pdl`. The missing rules, in order:

1. **The prebuild.** One chunk file: the main island's court, one roof over one room, one swinging door, one bridge, one smaller island. Loading it twice gives the same columns. The pets' bags start empty of building stone. **Step 1 is in the tree:** `@.apps/centroid-hq/data/court.pdl` (31 cells, two roof-over-floor columns, two door voxels, three bridge cells with air under them, one farm). `centroid_prebuild_op check` refuses a missing roof and a bridge with ground under it. Harness `centroid_test.pal`, run 2026-11-06: `VERDICT|PASS|passed=4|failed=0`. This case does not feed or sleep a pet.
2. **Needs on.** Hunger and sleep start on for these pets only. A day tick with no food raises hunger. A night with no bed drops energy. Dying stays off.
3. **Plant.** A farm cell is already on the smaller island. A seed from the pile, after a counted number of day ticks, gives food. That food is the only thing that lowers hunger.
4. **Bed.** A bed is crafted from planks and placed in the room. Sleep on it restores energy. Sleep on the ground does not.
5. **The day.** A hungry pet plants or eats before anything else. A tired pet sleeps before anything else. The table fails if a pet ends the counted days starving or exhausted, or if any campus column changed.

A second attempt edits the phase table. It does not edit the pets by hand.

## The manager, later

After centroid-test passes, a second table may hand a manager the blueprint (the same chunk file, read-only) and a crew whose bags start empty. The manager's only output is jobs: dig, haul, craft, place. Employees run those jobs and no others. The run passes when their columns match the blueprint and fail when a column differs. centroid-test does not include this, and the manager is not taught by rewriting the prebuild.

## What centroid-live adds

The same verbs, on the board, with one extra field: `VIEW | rmmv` or `VIEW | studio`. The key writes that field. It is not the existing `view` verb (2D / 3D / glyph). Both can be on at once: a 3D picture of the generated skin, or a 2D picture of the RPG Maker skin.

Until a studio sheet exists for stone, wood, plant, bed, and door, that face stays on the RPG Maker sheet and the corner says so. The live run is not started until centroid-test passes. The live window goes through `cpu_gate`. The test never opens it.

## Menus the live run shows

Designations and Build gain the rows the test proved: plant, bed, wall, door. Stocks gains food and planks beside stone. Units gains hunger and energy beside the name, because a need you cannot see is not on. The skin toggle is one row under View, labeled with the sheet set, not a new menu.

## Not in either run yet

The second bridge's twin island details beyond the one prebuilt bridge, the tower, shops, and the school, unless they are in the loaded chunk. The manager and the employees. Labor skills. Death. A generator that draws the campus. The manager waits on a passing centroid-test.
