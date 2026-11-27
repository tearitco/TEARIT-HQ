# City block generator in a new rpg-edit map, and a walk-through presentation (plan, 2026-11-27)

Labels: **SAID** owner 2026-11-27; **OBS** seen in the repo; **DOC** my reasoning, not run. Nothing here is built.
Companions: `HEAVY-SPRINT-PLAN-SHIPPABLE-TUTORIAL-AND-VEHICLES-2026-11-27.md` (the sprint this feeds), `RTA-BATTERY-CONVERSION-2026-11-21.md` (the existing city), `GTA-HQ-DESIGN.md`, `VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md` (gym maps), `!.PRESENTATIONS-THE-ONE-PLACE-STYLE-TOOLS-AND-HOW-TO-2026-11-18.md` (decks and video), decision D145.

## 1. What the owner asked (SAID, 2026-11-27)
Do the vehicle work "in a new rpg-edit map". "Do we have the RTH map? it's not as good compared to this." Make a **generator of a decent-looking small section of a city**: buildings single and multi story, then roads, parks, modern structures, and **a presentation of walking through one of them**. Doc it, and put it in the nov-27 XO folder with the sprint plan.

## 2. Do we have "the RTH map"? (OBS)
"RTH" is the car folder the reference came from (`!.🤖🏪️.RTH-X.com`), so this is **RTA, Robot Trap Auto** (`@.apps/rta/`): a hand-authored slice of the Charleston Battery. What it holds, observed 2026-11-27:
- `board_city.pdl` (2,902 bytes): `BOX`, `CELL` (10 m per cell), `KIND`, and `FEAT` rows (park, water, road, path, pier, building).
- `road.grid` (530 bytes): a **32 x 14** grid of single letters (W water, S street, K ...), so about 320 m by 140 m. **CORRECTION 2026-11-27:** that is only the small board slice; the city itself is larger. The Charleston Battery conversion ("Centon") is **168 x 254 cells at 10 m** (sources outside git in `#.NNEST_ASSETS/rta-battery/`: `battery.pdl`, `battery-with-buildings.pdl`), and the bay slice (Dogpatch to the Bay Bridge landing) is 163 x 216 at 20 m (`#.NNEST_ASSETS/rta-bay/`). Whether those footprints carry heights or stories is unchecked.
- The painter `ops/rta_city_op.c` (359 lines) turns features into cells by priority; its `building` kind is a **flat cell class: no height, no stories, no door, no roof**. That is why it reads as thin next to the voxel island.
- Verdict: RTA is a good *source of features and a real-place reference*, but it cannot carry multi-story buildings, doors or interiors. It is not what the generator should extend; it is something the generator can optionally read for a real layout later (RTA doc: "how a later agent finishes a city").
The rmmc island is the quality bar: a voxel world over a ledger (`@.apps/rmmc/vox/blocks.pdl`, `BLOCK | kind | hardness | tool_best | ... | solid | opaque | ...`), walkable, breakable, first person.

## 3. Where it lives: a new book, not the island
**D145 shapes.** (A) extend the RTA painter with height columns; (B) a new generator op that writes **voxel block rows** for a new rpg-edit book (working name `cityblock`), reusing rmmc's verbs, ledger, tutorial and first-person camera; (C) hand-draw the city. Test: (1) one authority per fact: in (B) the layout is a **seeded projection** (same seed, same bytes), the ledger holds only what players did; (A) keeps a second 2D truth. (2) modular: (B) is one op plus data rows; no engine or `game_core` change (D92). (3) scalable: a bigger district or another style is a profile and a seed, the 10th city costs a file; (C) is per city labour. (4) long term: the vehicles sprint needs roads, lots, doors and seats on a real map; walking, driving and the AI tool calls all use block rows. (5) house shapes: pdl config + C op + harness invariants, no Python. **Recommendation: B**; RTA stays as is (it is a Battery conversion with its own owner, grok's line), and may later feed (B) as a real-place profile. Labelled stopgap: none.

## 4. The generator (DOC)
Op **`city_gen_op <house> gen <book> <seed> <profile>`**, deterministic: fixed seed, integer maths only, no clock, no `rand()` (same discipline as the heightmap op).
1. **Profile** (`@.apps/cityblock/profile.pdl`, CONFIG): `SIZE | 96 | 96` cells; `ROAD | spacing 24 | width 5 | sidewalk 2`; `LOT | min 8 | max 18`; `STORIES | weights 1:4 2:3 3:2 5:1`; `PARK | share 12`; `STYLE | name | wall | roof | window_every`.
2. **Roads:** a jittered grid (arterials every `spacing`, a few curved/diagonal cuts), widths and sidewalk strips from the profile; every intersection gets a crosswalk row.
3. **Blocks to lots:** each block is split by recursive subdivision down to `LOT` sizes; every lot must **touch a sidewalk** (its door side).
4. **Land use per lot:** from the profile weights: house (1 to 2 stories), shop row (2 to 3, flat roof, wide windows), apartment (3 to 5), tower (6+, rare), **park** (grass, trees, a path, benches), plaza, parking.
5. **Building from a footprint:** walls by material row, `STORIES x 4` blocks high, floor slabs per story, **windows on a regular grid**, a **door on the sidewalk face** (a two-block opening, state closed), a roof (flat with a parapet, or gabled), optionally a stair core and one furnished room. A single-story house and a five-story block are the same op with different parameters.
6. **Street furniture:** lamp posts every N cells (light value in `blocks.pdl`), trees along sidewalks, a hydrant per block, signs.
7. **Output:** block rows into the new book's seed/heightmap format that rmmc already reads (OBS: `terrain.heightmap`, `legend.txt` SLOT rows), plus a `lots.pdl` (a PROJECTION: lot id, kind, door cell, stories) that other ops (vehicles, quests, the tutorial) read instead of rescanning blocks.
**New block kinds** (`blocks.pdl` rows, no engine change): `asphalt`, `sidewalk`, `concrete`, `brick`, `glass`, `roof_tile`, `door_closed`, `door_open`, `lamp`, `road_line`. Door state is the vehicles-plan door row, shared.

## 5. Proof the generator is right (what a harness can check without a human)
Case `city_gen`, scratch house, headless; each gets a mutation that must fail:
1. **Deterministic:** two runs with one seed give identical bytes; a different seed differs.
2. **Roads connected:** a flood fill from one road cell reaches every road cell and every lot's door cell.
3. **Every building has a reachable door:** door cell is on or adjacent to a sidewalk, two blocks high, the cell in front is walkable.
4. **No overlap, no floating blocks:** footprints disjoint; every non-ground block is supported from below.
5. **Stories as asked:** a lot with stories N has exactly N floor slabs, and windows only on wall faces.
6. **Walkable:** the existing rmmc walk rules can path from the spawn to a door and inside (a driven run through the real verbs).
7. **Budget:** block count and file size within a stated cap (runtime file standard); generation time measured, run niced, one job at a time.
Looks (is it "decent"?) stay a human call: the presentation below is how the owner sees it.

## 6. Roadmap (effort DOC, not measured)
| Step | Result | Effort |
|---|---|---|
| 1. Book skeleton `cityblock` cloned from rmmc config (new rpg-edit book, own ledger, no change to rmmc) | opens in the editor, flat ground | S |
| 2. Block rows + roads + sidewalks | a street grid you can walk | S to M |
| 3. Lots + one building type (house, 1 story, door, roof) | first recognisable street | M |
| 4. Stories, windows, shops, apartments, towers | the "multi story" look | M |
| 5. Parks, trees, lamps, crosswalks, furniture | city feel | S |
| 6. `lots.pdl` projection + case `city_gen` | provable | S |
| 7. Presentation (section 7) | something to watch | M |
| 8. Vehicles gym: roads become the test track, doors and seats from the sprint doc | the GTA return | follows sprint E |

## 7. The walk-through presentation (DOC; uses the existing pipeline)
- **What:** a 60 to 90 second first-person walk down one street into one building and up a floor, then out to a park; a second pass from the air (camera mode 3) shows the layout.
- **How (existing parts, OBS):** the rmmc shot scripts (`@.apps/rmmc/shots*.pdl`) and director drive a recorded run from the **ledger** (a scripted path = real verbs, so the video is also a test); frames are captured the same way as the QA video; narration through Kokoro on debil; deck and colours from the ONE presentations doc (no new style file); real frames only, never mock-ups.
- **Cost rules:** the recording and any ffmpeg run on **debil or the Mac** (`-threads 2`, niced, one job), never on this machine while the die is above about 75 C; the local machine only shows the finished file. A frame check opens the PNG before it counts.
- **Deliverable:** `XO/31.Nov/nov-27/` gets the docs now; the video lands there (media is allowed in XO) once recorded; the generator and book stay in the repo.

## 8. Open decisions for the owner
1. Book name and place: `cityblock` as a new rpg-edit book (recommended)? Or a district of the existing island?
2. Style: modern (glass and concrete, flat roofs), small-town (brick, gabled), or both profiles?
3. Real-place feed later: keep RTA's Charleston Battery as a source for a "real layout" profile, or leave RTA alone?
4. Interiors: shells only first (recommended), or one furnished room per building?
5. Scale: 96 x 96 cells (about a 15 minute walk end to end at tutorial speed) or smaller first?

## 9. External references
Repo `https://github.com/tearitco/TEARIT-HQ.git`, branch `claude`. `@.apps/rta/` (board_city.pdl, road.grid, ops/rta_city_op.c), `@.apps/rmmc/vox/` (blocks.pdl, terrain), `@.apps/gta-hq/`. Decision log D144 (vehicles), D145 (this). Presentations: `!.PRESENTATIONS-THE-ONE-PLACE-STYLE-TOOLS-AND-HOW-TO-2026-11-18.md`.
