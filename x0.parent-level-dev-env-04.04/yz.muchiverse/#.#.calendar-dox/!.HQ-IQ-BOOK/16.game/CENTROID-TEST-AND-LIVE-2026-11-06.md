# centroid-test and centroid-live (2026-11-06, reconciled 2026-11-10)

**I was told 2026-11-10.** The owner's words that day replace the two bullets below. The 2026-11-06 text stays in this file as the older run, now named **centroid-needs**, and it is not the definition of done.

- **centroid-test** is a harness in a scratch house. It runs one job list made from the skyland blueprint (`@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl`). The crew places columns with the existing build verbs. The run passes only when the placed columns equal that slice's projection, and it fails when a column is missing or wrong. The first slice is the chapel. Every later `BUILD` and island uses the same generator (`bp_op gen`), not a second planner.
- **centroid-live** is the same job list on page `live`, walked in stages until the whole island is built. The crew is df-style pets programmed to run those jobs. A motive pet or a trained policy is optional and is not how the island gets finished. The skin toggle from 2026-11-06 stays a display switch. It does not decide a column.
- **Whole island.** One sitting may prove only the chapel. The finish line is every island and every `BUILD` row. About 30,000 columns may be too heavy to draw at once. The runner still walks the plan in stages. Scale stays the blueprint's `SCALE` row.

The checker is `bp_op` (`@.apps/centroid-hq/ops/bp_op.c`): `check`, `list`, `gen`, `verify`, `break`. `JOB | place | x | y | z | material` is the crew list. df-hq's `place` verb is still `item x y` with no z.

**FSM lane, started 2026-11-10.** `crew_op` (`@.apps/centroid-hq/ops/crew_op.c`) is the first autonomous builder. One pet reads only those JOB rows. Each tick is the df dig loop's shape: idle, then CLAIM, then PLACE. A place that is not in the job list is refused. `mineplan` writes `DESIGNATE` rows for the stone floor (the chapel writes 9) so the existing df dig loop can mine them. It does not start a pet, and the live tick was not edited. Talking, trading, buying and house-building stay Teropoly's verbs in `@.apps/deskopoly/verbs.pdl` (`talk`, `offer`, `accept`, `buy`, `build`). This lane does not grow a second copy. Farming is a later plant row. Harness `centroid/crew.pal`, case `cases/centroid_crew.pdl`: `VERDICT|PASS|passed=115|failed=0` on 2026-11-10 (one stone; fountain 12; bridge 16, stone z=10 and roof z=11; `under:S6` 11; `under:S3` 38; `mech` bridge jobs=16 cells=8; stage jobs=32 cells=12, world 29,28, last roof `32,30,2`). Status: `XO/31.Nov/nov-10/grok/12.where-we-are.md`. The deck is `XO/!.PRESENTATIONS/0.skyland-two-slices/`, silent, three frames. The phase table is `play/phases/centroid_crew.phases.pdl`, pal `play_centroid_crew.pal`. Ran 2026-11-10: `VERDICT|PASS|passed=11|failed=0`. Step one shows the pet (yellow) one cell left of the stone (red). Step two stands on it. Pictures: `XO/31.Nov/nov-10/grok/8.pet-step1.png` and `8.pet-step2.png`. The life row is `EVENT|round|actor|verb|target|at|result`. Older `CLAIM` / `PLACE` / `CALL` rows are refused. A table act is handed off. The trade stays in Teropoly. `bp_op count` on the blueprint printed `LAND columns=1940 volume=25566 buildings=1859 scale_not_applied=4`. Decision D69: every route row has a kind, `world` or `table`. Talk, offer, buy and build are table and name deskopoly. Mine and place are world. Haul and farm are world and OPEN. A table verb owned by the crew is refused. A `CALL` row points at the owner. It does not play the trade.

Status of the 2026-11-06 stock-take below: **plan, except the court file and its 5-check harness.** SAID = the owner. OBS = files opened for that stock-take.

Older definition, kept so the eat-and-sleep work is not thrown away:

- **centroid-needs** (was called centroid-test on 2026-11-06) is a playthrough on a scratch house. The University is **already built** when the run starts. Pets meet food and sleep, plant food, and make beds on that campus. They do not raise the buildings in this run. The harness can fail, and a second attempt is another phase table.
- The 2026-11-06 **centroid-live** added one toggle: the RPG Maker skin or the generated skin. The columns do not change. That toggle remains. It is no longer the whole live run.
- Both pages live in one editor book, **centroid**. Page **test** is the harness campus. Page **live** is the one you open and watch. The book the launcher resolves is `@.apps/<id>/rooms.pdl`. Book **centroid** is `@.apps/centroid/rooms.pdl`, pages `test` and `live`. They are no longer rows of the Islands `rooms.pdl`. Harness `centroid_book.pal`, run 2026-11-06 after the map package: `VERDICT|PASS|passed=11|failed=0`. The map package is `centroid-pchq/pieces/system/maps/centroid/game.pdl` with desks test and live. The open editor was told `CONFIRM_START_MAP:centroid` and `CONFIRM_SET_DESK:test`. `board/state/ui.txt` then read `book_label=book:centroid` and `page_label=page:test`. **Sheet key (2026-11-06 02:36):** backslash (`92`), not slash. On `map_id=centroid` it swaps `cells-rmmv.rgba` and `cells-gen.rgba` onto `cells.rgba` and writes `skin.txt`. Observed `rmmv` → `gen` → `rmmv` on page test. The gen file is an R/B swap of the Islands RPG Maker atlas (110592 bytes, grass slot `1,0`) until a studio sheet of that size exists. **Drive 2026-11-06 02:30, stop.** Inbox desk changes reached the window (`page:live`, then `page:test`). Relay `112` dumped both. Both frames are a flat grass sheet and a dirt corner. The overlay says `ray miss` and the click line stays `02:16:22`. Step 2 of the drive loop (edit this page) cannot start until a click hits the floor. The court file is not what the pane draws. **Ray (2026-11-06 02:41):** the 6×6 page did not contain the xelector at 8,8, so the look ray left the map. Both desks are 16×16 grass. Canvas 443,360 hit `8,8,0`. Canvas 443,540 hit `8,13,0` with `inside=1`. Sky above the far edge (443,200) still misses. `possessed_id=hero_01`, so a hit does not move the xelector. CPU alert from 2026-11-05 22:39 was Chrome at 686% and 0 house processes; the alert file was removed after that read. Guard at 02:30 was busy 10, 44 C. **Driven 2026-11-06:** after a relaunch, `ui.txt` listed `test` and `live` (`n_room=8`). With chat mode `edit` and In on, `goto test` and `goto live` set `room_label` to those names and moved the pet `robot` (events.txt `DOOR|...|robot|menu|test` then `live`, then back to `start`). `board_ctl preflight` failed: renderer pid 3682305 had no X window of the expected size on `DISPLAY=:0`, so no frame was written. The book tab title is still the editor's. The pictures, when a window is mapped, are plain interiors, not the 6×5 court. Next phase waits on a mapped window and on drawing that court onto the page.
- **2026-11-10:** the manager job list is no longer "later". It is centroid-test. The prebuilt court remains the centroid-needs campus. Employees, or df-style pets on the live page, pass only when their placed columns match the blueprint slice.

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

1. **The prebuild.** One chunk file: the main island's court, one roof over one room, one swinging door, one bridge, one smaller island. Loading it twice gives the same columns. The pets' bags start empty of building stone. **The basic building is the file the harness loads.** `@.apps/centroid-hq/data/court.pdl` is 156 cells. The main block is 6×5, larger than the Islands 2×2 stamp. Two floor levels (z=0 and z=3). 24 places to stand, each a floor with two empty cells above it. The door at (3,0) opens onto that room. Three bridge cells have air under them. One farm sits on the far island. Harness `centroid_test.pal`, run 2026-11-06 after this file landed: `VERDICT|PASS|passed=5|failed=0`. A 2×2 stamp, a missing roof, and a filled bridge each exit 2. This is one room on two floors, not the clock-tower campus, and it still does not feed or sleep a pet. There is no board frame of it, so no customer video.
2. **Needs on.** Hunger and sleep start on for these pets only. A day tick with no food raises hunger. A night with no bed drops energy. Dying stays off.
3. **Plant.** A farm cell is already on the smaller island. A seed from the pile, after a counted number of day ticks, gives food. That food is the only thing that lowers hunger.
4. **Bed.** A bed is crafted from planks and placed in the room. Sleep on it restores energy. Sleep on the ground does not.
5. **The day.** A hungry pet plants or eats before anything else. A tired pet sleeps before anything else. The table fails if a pet ends the counted days starving or exhausted, or if any campus column changed.

A second attempt edits the phase table. It does not edit the pets by hand.

## The manager, later

After centroid-test passes, a second table may hand a manager the blueprint (the same chunk file, read-only) and a crew whose bags start empty. The manager's only output is jobs: dig, haul, craft, place. Employees run those jobs and no others. The run passes when their columns match the blueprint and fail when a column differs. centroid-test does not include this, and the manager is not taught by rewriting the prebuild.

## What centroid-live adds

The same verbs, on the board, with one extra field: `VIEW | rmmv` or `VIEW | studio`. The key writes that field. It is not the existing `view` verb (2D / 3D / glyph). Both can be on at once: a 3D picture of the generated skin, or a 2D picture of the RPG Maker skin.

Until a studio sheet exists for stone, wood, plant, bed, and door, that face stays on the RPG Maker sheet and the corner says so. The live run is not started until centroid-test passes. The live window goes through `cpu_gate`.

**2026-11-10, owner correction.** Page `test` can and should open the editor for the final proof: `dump_frame_png_op` of the edit window from different points of view, with `book:centroid` and `page:test` readable. Page `live` is the same blueprint and the same harness job list, being built on `page:live`. The scratch `plan.ppm` stays the picture that can be drawn again. A video-player clip of that plan is not either page. The sentence above that said the test never opens the window is withdrawn.

## Menus the live run shows

Designations and Build gain the rows the test proved: plant, bed, wall, door. Stocks gains food and planks beside stone. Units gains hunger and energy beside the name, because a need you cannot see is not on. The skin toggle is one row under View, labeled with the sheet set, not a new menu.

## Customer video

SAID 2026-11-06: harness proofs that go to potential customers are a YouTube video, made with the house presentation pipeline (`10-user-docs/PRESENTATION-VIDEO-PIPELINE.md`). One folder per proof: `snapshots/`, `manifest.txt` (`file | seconds | caption`, no pipe inside a caption), `REPRODUCE.md`, then the archive `make_presentation_video.py` (ffmpeg re-encode, pet voice, a `-yt-summary.txt` beside the mp4). The copy customers can be handed also lives under `XO/!.PRESENTATIONS/<prefix>.<name>/`.

A video is a picture of the run, not a reading of the verdict line. centroid-test step 1 has no board frame, so it has no mp4 yet. The first customer cut is the first real frame of the loaded court, with the harness verdict in the caption, and the reproduce steps naming `centroid_test.pal`.

## Not in either run yet

The second bridge's twin island details beyond the one prebuilt bridge, the tower, shops, and the school, unless they are in the loaded chunk. The manager and the employees. Labor skills. Death. A generator that draws the campus. The manager waits on a passing centroid-test.
