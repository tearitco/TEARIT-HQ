# centroid-test and centroid-live (2026-11-06, reconciled 2026-11-10)

**I was told 2026-11-10.** The owner's words that day replace the two bullets below. The 2026-11-06 text stays in this file as the older run, now named **centroid-needs**, and it is not the definition of done.

- **centroid-test** is a harness in a scratch house. It runs one job list made from the skyland blueprint (`@.apps/centroid-hq/data/skyland/skyland.blueprint.pdl`). The crew places columns with the existing build verbs. The run passes only when the placed columns equal that slice's projection, and it fails when a column is missing or wrong. The first slice is the chapel. Every later `BUILD` and island uses the same generator (`bp_op gen`), not a second planner.
- **centroid-live** is the same job list on page `live`, walked in stages until the whole island is built. The crew is df-style pets programmed to run those jobs. A motive pet or a trained policy is optional and is not how the island gets finished. The skin toggle from 2026-11-06 stays a display switch. It does not decide a column.
- **Whole island.** One sitting may prove only the chapel. The finish line is every island and every `BUILD` row. About 30,000 columns may be too heavy to draw at once. The runner still walks the plan in stages. Scale stays the blueprint's `SCALE` row.

The checker is `bp_op` (`@.apps/centroid-hq/ops/bp_op.c`): `check`, `list`, `gen`, `verify`, `break`. `JOB | place | x | y | z | material` is the crew list. df-hq's `place` verb is still `item x y` with no z.

**FSM lane, started 2026-11-10.** `crew_op` (`@.apps/centroid-hq/ops/crew_op.c`) is the first autonomous builder. One pet reads only those JOB rows. Each tick is the df dig loop's shape: idle, then CLAIM, then PLACE. A place that is not in the job list is refused. `mineplan` writes `DESIGNATE` rows for the stone floor (the chapel writes 9) so the existing df dig loop can mine them. It does not start a pet, and the live tick was not edited. Talking, trading, buying and house-building stay Teropoly's verbs in `@.apps/deskopoly/verbs.pdl` (`talk`, `offer`, `accept`, `buy`, `build`). This lane does not grow a second copy. Farming is a later plant row. Harness `centroid/crew.pal`, case `cases/centroid_crew.pdl`: `VERDICT|PASS|passed=146|failed=0` on 2026-11-10. Every `BUILD` id raises through one factory: chapel, fountain, bridge, stage, hut, mill (62), houses_B2 (76), houses_B1 (132), east_stair (110), cathedral (752), cathedral_spire_main (121), cathedral_spires (432). `under:S6` and `under:S3` are the two undersides that were generated. Scale 4 is not applied. Until 2026-11-11 the editor desk was 16 by 16 and each slice was parked at page 8,6, so one dump could not show the island. That night `page:live` was rewritten to `GRID` 72 by 52 (1940 land cells, 216 wall cells) and `page:test` was cropped from 33 rows to 24 so the minimap is one island. Dumps: `XO/31.Nov/nov-10/grok/28.live-land.png` and `27.test-one-minimap.png`. The lobes on that page are blueprint ellipses, not a crew earth walk.

**Finish line, owner 2026-11-11, looking at `page:live`.** The grass lobes and dirt edges are progress. They are not the buildings. By the end a building uses RPG Maker tiles, reads in the 3D view and in the 2D top-down of the same `cells.txt`, and can be walked into. Stairs work. A walker is one entity. The owner's camera in that shot stays where they put it (`pos 8,8,1` on the HUD, the look ray hit `49,44,0`). Do not move it back.

The sheet is one authority. `cells.txt` is `floor,wall,depth`. Floor slot and wall slot are atlas tiles (page `test` already paints wall slot 36). The 2D top-down and the 3D maker both read that file. A second picture of the building is a bug.

```
enter(actor, door):
  door is door.txt: bit 0 shut, bit 1 open. leaf rows are the two cells of the opening.
  shut: those cells block. open: they are floor.
  open is one bit for 2D and 3D. it does not write a room id and it does not change the page.
  Islands DOOR rows teleport. centroid doors do not.
  drive: walk the hero onto the open leaf. the hero cell is inside. dump the 3D frame and the 2D sheet.

stair(actor, next):
  a stair is a run of cells whose depth changes by 1 each step.
  if next.depth == here.depth: walk.
  if next.depth == here.depth + 1 and next is on the stair run: walk, entity z = next.depth.
  if the drop or the rise is more than 1: refuse.
  the same rule paints the 2D sheet: the higher cell is the next tile, not a new page.

walk(actor, next):
  one fact, one ledger row: EVENT|round|actor|walk|ground|x,y,z|ok
  refuse when the cell has a wall slot and the door on that cell is shut.
  refuse when the cell is sky (0,0,0).
  the entity position is that row. a side file that remembers the cell is ghost data.
```

First proof, 2026-11-11: one 5 by 5 house on `page:live` at 46,40 (door cells 48,44 and 49,44, stair at 48,42 depth 7). Harness `enter_house`: `VERDICT|PASS|passed=7|failed=0`. A shut door refuses, an open door reaches the stair, a wall refuses. The camera was not moved. The dump `XO/31.Nov/nov-11/grok/1.enter-house.png` still shows the lobes from the existing camera; the house is on the sheet and is not yet the test-page building filling the frame. The live hero was not walked. Status: `XO/31.Nov/nov-10/grok/12.where-we-are.md`. The deck is `XO/!.PRESENTATIONS/0.skyland-two-slices/`, silent, three frames. The phase table is `play/phases/centroid_crew.phases.pdl`, pal `play_centroid_crew.pal`. Ran 2026-11-10: `VERDICT|PASS|passed=11|failed=0`. Step one shows the pet (yellow) one cell left of the stone (red). Step two stands on it. Pictures: `XO/31.Nov/nov-10/grok/8.pet-step1.png` and `8.pet-step2.png`. The life row is `EVENT|round|actor|verb|target|at|result`. Older `CLAIM` / `PLACE` / `CALL` rows are refused. A table act is handed off. The trade stays in Teropoly. `bp_op count` on the blueprint printed `LAND columns=1940 volume=25566 buildings=1859 scale_not_applied=4`. Decision D69: every route row has a kind, `world` or `table`. Talk, offer, buy and build are table and name deskopoly. Mine and place are world. Haul and farm are world and OPEN. A table verb owned by the crew is refused. A `CALL` row points at the owner. It does not play the trade.

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

## Castle scale and the RPG Maker sheet (2026-11-11)

The owner showed a palace: many floors, towers above the roof, pale stone, dark roofs, colored windows, a grand door. The 5 by 5 orange house on page `small` is one bay and one floor. It is not that building. Page `small` keeps that island. The castle is built on `live`.

The atlas on the page was opened (`cells.rgba`, 384 by 72, 16 columns, 24 px, 43 tiles). It is the RPG Maker outside sheet.

- Slot 35 is a flat orange fill. A wall of it is the solid orange slab. It is not used for the castle.
- Slots 12 through 15, the last four tiles of the first row, are the castle drawings: gray stone, red roofs, a dark gate. Each one is a whole small castle, so repeating one of them on every cell stamps a tiny castle on every face. They are the tower caps and the roof cells, not the wall fill.
- The tiles that do repeat as a wall or a floor are on the bottom row: 34 the wooden door, 36 the cobble, 38 the blue window panes, 41 the white plaster, 37 the planks, 39 the wood floor, 40 the red carpet.

A castle from this sheet is those tiles across many cells. Walls are cobble or white plaster. Window cells are slot 38. The entrance is slot 34. Tower and roof cells are slots 12 through 15, so the red roofs sit on top. The reference picture's stained glass, columns, and lamps are not on this sheet. The color we can match is gray stone, white trim, red roofs, blue windows, and a wood door.

One floor of that building is about four cells tall, and one window bay is about two cells wide. A first castle that reads as a castle is about 20 cells wide, 12 deep, and two floors, with two towers one floor taller. That fills most of the south lobe on the 72 by 52 page. The full palace in the picture, with wings, is wider than that lobe. It waits for the blueprint scale of 4, which turns the page into 288 by 208. `small` is not scaled and not restamped.

The owner wants buildings made from a blueprint of reusable modules (2026-11-11). Five modules, named once: `wall` (slot 36 or 41), `window` (slot 38), `door` (slot 34, two cells), `roof` (slots 12–15 on the top only), `tower` (a taller `wall` with a `roof` cap). A `BUILD` row says how many of each and on which face. `centroid.parts.pdl` is where those modules live. `bp_op` remains the one stamper. A new generator per building is the wrong shape.

**Land generator, same day.** `land_op` (`@.apps/centroid-hq/ops/land_op.c`) starts from a blank page. `data/skyland/land.seed.pdl` holds `SEED | land` and `SCALE | land`. The page harness row `land` on `live` runs it (an ACTION, so the size is an argument). A CASE row cannot take a size. The run writes biomes (grass, tan fields, pale grove, cobble plaza, dirt pond), height from the seed, a cobble road along the waypoints, and a flat cobble frame around each building with the pad left empty. The same seed matches. A different seed does not. A scale that makes a side past 512 is refused. Harness `land_gen`: `VERDICT|PASS|passed=8|failed=0`. The live page was generated at seed 11, scale 7, 504 by 364. Page `small` was not.

The first generation used slot 33 as grass. Slot 33 is a sword on a shop counter, so the whole top of the island was that tile. Terrain is the first atlas row: 1 grass, 2 dirt path, 3 water, 4 sand, 5 pale sand. Regenerated from the same seed. Floor slot 33 count after that run was 0. Pitfall 29.

**Kits, same day (D73).** Roads, biomes, snow, levels, and trees are separate `.pdl` files in `data/skyland/kits/`. A creator ships the file. The generator is the one reader. Depth does not raise the top. Levels are EXTRUDE `a` 1, `b` 4, `c` 8, and the road and the building pad stay on `a`. Snow is RTP Outside_A2.png at 0,480, painted into slot 6. The legend is `&.widgits/palettes/tilesets/legend.pdl`. Mineclonia `default_snow.png` is the same role and is not the painted tile. Trees use wall slot 20 on grass only.

**Page ark-age, 2026-11-11.** A copy of `live` (504 by 364, same cells). `game.pdl` desk 4. `pc_generate_chunk` `map:centroid:ark-age` set `desk_id=ark-age`, then `map:centroid:live` set it back to `live`. Both `cells.txt` files stayed 504 by 364. `small` was not copied.

**Why a corner shot looks like a shelf (2026-11-11).** HUD `pos 8,8,15` is off the land: row 9 of `live/map.txt` is empty until column 345. House fog is `view_fog_start=18` `view_fog_end=48` in `#.desktop/hq_ui.pdl`, and `live` has no `view.pdl`. The board holds 256 cells (`MAX_BOARD_DIM`, `BV_VIEW_DIM` in `bv_render_3d.c`), so a 504-wide page is a window around the eye. **Dwarves and the day, 2026-11-11.** The renderer day is `compute_sun_light_level` on `game_time_epoch_sec`. The open world has `autotick_enabled=0`, so the HUD stays at noon. Teropoly is the player bar that swaps the side panels (`side_left`, `n_party`). Seven new dwarves are named in `data/skyland/dwarves.pdl`. They are not on the page. Writeup: `XO/31.Nov/nov-11/grok/0.dwarves-and-day.md`.

**Clock, seats, and the later rows, same day, not turned on.** `]` toggles the clock. A desk switch used to force noon and `autotick_enabled=0`. The source now keeps those two keys when the world file already has them. Seating the seven is a ledger row and a party line on the Teropoly bar, then `crew_op`. After that, still not built: animals as seats that eat, NPC backstories as ledger rows, a cave as `DESIGNATE` under the island, and a night threat that moves only in the dark half. Writeup: `XO/31.Nov/nov-11/grok/0.clock-seats-and-later.md`.

**Hard requirement, same day.** Clock, seats, animals, backstories, caves, and night threats are each a reusable harness plus a generator that reads a kit file. A one-off C edit does not count. Terrain kits now do: `land_op` reads `kits/` beside the blueprint, and `land_gen` fails if the kit slot is missing or if a kit names slot 33. Art weights are the Concept Bank (`NODE` / `SLOT` / `WEIGHT`, D32). The seven dwarf rows name spokes `seat_pick`, `seat_quarry`, `seat_malt`, `seat_kiln`, `seat_loom`, `seat_wick`, and `seat_helm`. Nothing reads those weights yet. Playthrough weights in `GAME-PLAYTHROUGH-HARNESSES.md` section 6 are a score of a run, not a look knob. The clock keep is a change inside `pc_generate_chunk`, not its own kit harness. Seats, animals, backstories, caves, and night threats still have no kit and no harness.

**Minimap on a page past 256, 2026-11-11.** The corner map used `MAX_BOARD_DIM` (256), so live (504 by 364) pinned the yellow eye on the edge. It now scales the whole page into `minimap_max_px`. Harness `minimap_fit`. Pitfall 30. The camera was not moved.

**Page map copy, 2026-11-11.** The live glyph map is `maps/centroid/live/map.txt`. `map_json_op` writes a JSON copy, a `.js` the page can read, and an HTML file that shows the max size. Owner note: `23.tools/maps/MAP-TO-PAGE.md`. The live copy is `XO/31.Nov/nov-11/grok/0.live-map.html`.

**Foundation pass, 2026-11-11, before seating anyone.** Harness `land_gen` verdict PASS, passed=12, failed=0 (seed match, scale past 512 refused, snow kit slot 9 present as floor `9,0,6`, slot 33 exit 2). The live page was not regenerated. `pc_generate_chunk` was compiled and copied onto session `1794377773-1326893` (34480 bytes). The open world was not switched. `autotick_enabled` is still 0. The seven are not ledger seats. Writeup: `XO/31.Nov/nov-11/grok/0.progress-kits-and-clock.md`.

Done the same day. `BV_VIEW_DIM` is 512, so this page is drawn whole. `live/view.pdl` and `ark-age/view.pdl` set fog 48..160. The page file wins over the house 18..48. The CPU walk still stops at 96. The GPU walks to 160. `MAX_BOARD_DIM` stays 256 for the letter grid. Writeup: `XO/31.Nov/nov-11/grok/0.view-limit.md`.
