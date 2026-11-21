# Robot Trap Auto: Battery conversion report

Started 2026-11-21. The menu rows were first. The rasters, the stood-up frame, and the ledger ops came later the same day. Read the section "Where the two cities stand" before the older sentences. Several lines below still say the download has not been run. Those lines are the plan as it was written. The later section is what ran.

The agent brief, same shape as Draw Battle's `grok-coach.md`, sits beside the example: `grok-coach.md` in the car folder with `rta-v3.html`. This book file is the map audit. That coach file is the build order.

The HTML is the example, the same way Draw Battle's page was. The product is an events board on the shared engine: a cell grid, a pure op for a rule, numbers in rows, the ledger as the only game-state authority. This file is the audit of what the page is and how the map gets into the house. The port itself is the next pass.

## What the page actually is

Read from `rta-v3.html` (title "Robot Trap Auto – Charleston Battery", 783 lines) in the Desktop RTH-X car folder. `wussup-rta.txt` beside it is the brief: a top-down emoji crime sandbox, 2D or extruded, a slice of the Charleston Battery (college, churches, tourists, ghost walks, piers, a residential block, a hospital).

Observed in the script, not inferred:

- The world is a typed grid, `80 x 60 x 6` (`WORLD_W`, `WORLD_H`, `WORLD_Z`). Cells are a `Uint8Array`. There is no lat/lon, no GeoJSON, and no Google call. The word "charleston" is the title and the name of `buildCharlestonBattery()`.
- Twenty-eight cell ids (`CELL` / `CELL_DEFS`): grass, dirt, sand, road, sidewalk, water, pier, building, roof, door, window, tree, bush, bench, lamp, car, fence, stairs, bridge, church, hospital, gun shop, police, college, park, path, interior. Each has an emoji, a color, and a `solid` flag.
- The district is drawn by hand: water on the south edge, a sand band, a park, three horizontal roads and two vertical roads, three piers, then `building()` rectangles. Named types are two churches, two college blocks, one hospital, one police station, two gun shops, and plain buildings. Roofs and one stair sit on `z = 1`. Cars are both cells and `Vehicle` objects.
- Play state lives in memory: player position, weapon, ammo, cash, wanted 0–5, hp, NPCs (wander / flee / chase), projectiles. A door to the gun shop shows an `alert`. `INTERIORS` has a gun shop and a church that are not the walking grid.

That grid is a sketch of a waterfront. It is not a copy of East Battery, Murray Boulevard, or the College of Charleston.

## Menu rows (done this pass)

Toys is a live scan for `toy.pdl`. Both rows are files, so the next open of the toys cell lists them. The cell was not clicked.

- Draw Battle: `@.apps/draw-battle/toy.pdl` (title Draw Battle) and `button.sh`. The script sets `PCHQ_PKG` and `PCHQ_BOARD_TPL` and runs `piececraft-hq/open_pchq_board.sh`. The rpg-edit File menu already lists the book because `maps/draw-battle/game.pdl` exists.
- Robot Trap Auto: `@.apps/rta/toy.pdl` (title Robot Trap Auto) and `button.sh`. The script opens the path in `reference.path`, which is `rta-v3.html`. There is no `game.pdl` map folder, so it is not a book. A book row waits until a desk exists.

## How the map is copied

Do not take the picture from Google Maps. A Google tile, a Static Maps image, or a screenshot is a photograph. It is not a walkable grid, the terms do not allow packing those tiles into the game, and the house would be storing a picture it cannot regenerate. Looking at Google while a person checks a street name is fine. The file the game reads is not a Google file.

The copy is a one-time GeoJSON export of a small box, saved in the repo as CONFIG. GeoJSON is the JSON shape: a `FeatureCollection` of points, lines, and polygons, each with a `properties` object (`highway`, `building`, `amenity`, `name`, `natural`). Two open sources cover this district. Neither needs a new Python tool in the house.

1. **OpenStreetMap, via one Overpass query saved as GeoJSON.** This is the road, water, pier, park, and landmark layer. Tags that match the HTML cell ids: `highway` to road, `natural=water` or `waterway` to water, `man_made=pier` to pier, `leisure=park` to park, `amenity=place_of_worship` to church, `amenity=hospital` or `healthcare=hospital` to hospital, `amenity=police` to police, `amenity=university` or `amenity=college` to college. A gun shop is sparse in OSM (`shop=weapons` or `shop=hunting`). Where the tag is missing, the cell stays a plain building and a later row names it. Do not invent a shop to fill the HTML's two gun shops.
2. **City building footprints.** Charleston publishes building outlines as a GIS layer that answers `geoJSON` (`gis.charleston-sc.gov`, Buildings layer on the external EAM map service, checked 2026-11-21). That is the polygon fill for `BUILDING`. OSM buildings are the fallback where the city layer is empty. A South Carolina `.pbf` from a mirror is the whole state (hundreds of thousands of roads and buildings). Do not import that. The game wants one neighborhood.

A GitHub repo is useful only when it already holds that small extract (a FeatureCollection someone committed). `awesome-geojson` is a list of tools, not a Charleston map. Do not add a converter script from that list. The artifact is the `.geojson` file. If a later refresh is needed, a person re-exports the same box and replaces the file. The game does not call Overpass, the city server, or Google while it runs.

Box, confirmed by the owner 2026-11-21: the peninsula tip from the Battery waterfront north through the College of Charleston, wide enough for East Bay and a few blocks inland. MUSC stays out of the first slice. The HTML's hospital is a rectangle. The real hospital waits for a second chunk. This is a chosen slice, not a surveyed boundary.

Cell size, confirmed the same day: 10 m. The HTML is 80 by 60 cells with no meters. A Battery slice about 1.2 km by 0.9 km at 10 m a cell is about 120 by 90, which still fits one desk. A building footprint is several cells and a road is one or two. Record the origin (south-west corner, lon/lat) and `10` meters per cell next to the file so the raster can be repeated. 8 m and 4 m (chunked) were the other choices and were not picked.

Raster, later, as one C op, not in this pass:

- Input: the GeoJSON (CONFIG) plus the origin and the cell size.
- Output: a cell grid in the same ids as `CELL`, written as the desk's map data. Water and roads first, then building polygons, then points (church, police, college) overwrite the building cell they sit on. `solid` follows `CELL_DEFS`.
- Height: OSM `building:levels` and the city footprint have no reliable floor stack. `z = 0` is the street. A roof on `z = 1` is a rule from levels, capped, not a measured scan. Stairs and interiors stay authored, as they are in the HTML.
- Names from the GeoJSON `name` property are labels on the cell. They are Charleston names, not Soul Pen names. Do not rename them into the cast.

The grid is CONFIG (an input the game does not write). Player position, a driven car, cash, wanted, and a fired shot are ledger rows when those rules exist. A picture of the grid (`map.png`, `events.txt`) is a projection. The HTML's in-memory `Player` and `NPC` objects are the ghost-data version of those facts. The port does not keep them only in memory.

## What converts, in the same order as Draw Battle

1. Cell catalog as data (`CELL_DEFS`: id, emoji, color, solid). No numbers copied into `game_core.c`.
2. One import op: GeoJSON plus origin plus cell size, write the grid. A harness case on a tiny fixture GeoJSON, not on a live download.
3. One desk on the shared board, flat and perspective, top-down. The axis stays top-down. Do not reuse a side-view stepper.
4. Walk, then a vehicle, then a door, each as an op called from an event page. Wanted, shops, and police are later rules, each with the five design-choice questions before the code.
5. A presentation that opens the flat frame and the same moment in perspective, after the grid is on the desk.

## What not to do

- Do not scrape Google tiles or check a Maps API key into the repo.
- Do not commit a state-wide PBF or a Python downloader.
- Do not call the map a ledger.
- Do not add a `game_core` mode named rta.
- The box and the cell size are confirmed (2026-11-21): Battery through the College, 10 m. The owner said go on 2026-11-21, then said to do it in small chunks and to check in before a huge pass. No Overpass download and no full-desk paint have been run.

## Tiles and voxels (how the look is managed)

Top-down only. The flat picture is `map.png`. The perspective picture is the same cells stood up as voxels from `cells.txt` plus `cells.rgba` (the maker atlas the board already reads). Do not set `axis=side`. Do not invent a second camera.

RPG Maker sheets already in the house, named by `tileset_registry.pdl`, not by a hardcoded path: `World_A1` is water, `World_A2` is grass and sand, roads and sidewalks come from the same outside set, buildings from `World_B`. A water or road edge is the existing floor autotile: eight neighbours of the same kind, same quadrant rule as `gta_hq.c` `draw_auto` and `tile_autotile.c`. Paint order is grass, then water (so the shore is the water autotile against land), then sand, then road, then sidewalk, then pier, then park, then a building footprint with its roof one step up. Charleston names stay labels. No Soul Pen names.

The reusable pieces are the ones that already exist: `game_new_op` for the editor app, `tile_autotile.c` for the 48 floor shapes, a harness case on a tiny fixture GeoJSON. A new C op may rasterize and call that autotile. No new Python, and no new shell script for the harness.

## Chunks (one at a time)

1. Tiny fixture GeoJSON plus a harness case that only checks the file parses. No network.
2. One raster op: fixture to a cell grid. Harness asserts water and a road cell. Still no network.
3. Paint that small grid to `map.png` and `cells.txt` / `cells.rgba` with the RPG Maker autotile. Open the PNG.
4. Check in, then one Overpass export of the confirmed box, saved as CONFIG.
5. Same op on that file, one desk. Open flat and perspective.
6. `game_new_op` editor app and the board window. Walk is a later rule, not part of the map chunk.

## External references

- HTML: the file named in `@.apps/rta/reference.path` (`rta-v3.html`) and `wussup-rta.txt` in the same folder. The folder is the car directory under Desktop `!.🤖🏪️.RTH-X.com`.
- OpenStreetMap features as GeoJSON: one Overpass export of the box (roads, water, piers, amenities). Tool list, not data: `https://github.com/tmcw/awesome-geojson`.
- City footprints that can answer GeoJSON: `https://gis.charleston-sc.gov/arcgis2/rest/services/External/EAM_ASSETS/MapServer/18` (Buildings, query format includes geoJSON). Checked as a service description on 2026-11-21. Not downloaded.
- Draw Battle precedent: `DRAW-BATTLE-CLOCK-AND-FIGHT-2026-11-21.md`. Ledger rule: `../02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`.

## Where the two cities stand (2026-11-21, observed)

The owner named the Battery slice **Centon**, capital of the Centroid. Charleston is the real city the slice was copied from. Do not call the game Charleston. The bay slice has no Centroid name yet. Call it the bay slice until the owner names it.

Neither city opens in an editor. Toys still launches `rta-v3.html`. There is no `game.pdl`, so rpg-edit has no book. Do not borrow the Draw Battle window to "open" a city. A proof frame is the painted PNG, or the board overlay raw with the engine text cropped off. `dump_frame_png_op` captures the whole X window, including the fight popup.

| City | Box | Cell | Grid | Painter output | Assets (not in git) |
|---|---|---|---|---|---|
| Centon | south 32.767, west -79.942, north 32.790, east -79.924 | 10 m | 168 by 254 | grass 6216, park 561, path 3852, road 11911, beach 101, water 8618, pier 249, building 11164, png 2688 by 4064 | `#.NNEST_ASSETS/rta-battery/` (`battery-with-buildings.pdl`, `battery-city.png`, `battery-city.grid`) |
| Bay slice | south 37.756, west -122.419, north 37.795, east -122.382 | 20 m | 163 by 216 | grass 7087, road 3661, water 12026, building 12434, park 0, pier 0, png 2608 by 3456 | `#.NNEST_ASSETS/rta-bay/` (`bay.pdl`, `bay.png`, `bay.grid`) |

The bay box is Dogpatch through the Bay Bridge landing, with Salesforce Tower at the north end (the footprint nearest lon -122.397, lat 37.790, named in the pdl). It is not the whole city. Overpass returned 502 that hour, so the bay pull is DataSF streets (`3psu-pn9h`) and building footprints (`ynuv-fyni`), paged with `within_box`. The one-off convert stayed in `/tmp`. Do not add that Python to the repo.

Ops on `grok`, local then pushed with this note:

- `rta_shore_op`, `rta_block_op`, `rta_city_op` paint a `.pdl` of `BOX`, `CELL`, and `FEAT` rows. Glyphs are `.KPRSWiB`. Harnesses `rta_shore`, `rta_block`, `rta_city`.
- `rta_stand_op` writes a desk from a city grid. Building glyph `B` extrudes to 3. Harness `rta_stand`.
- `rta_car_op`, `rta_walk_op`, `rta_cash_op`, `rta_wanted_op` append ledger rows and, for the car, rewrite `events.txt`. Harnesses `rta_car` and `rta_play`. Walk stops at water. Cash adds. Wanted stops at 5.
- Commits before this doc: `5ade0ebe0`, `08c1e20b3`, `fb3e095d0`, `261de11ed`, `734c59606`.

Decks, outside git: `XO/31.Nov/nov-21/12.rta-battery-city/`, `13.rta-stood-up/`, `14.centon-tour/`, `15.bay-slice/`. Narrator face is Actor1, second face, top row, in the caption strip. English voice, Japanese subtitle. The whole picture stays in frame.

## How a later agent finishes a city

1. Keep the map as CONFIG (the `.pdl` and the grid). Player position, the car, cash, and wanted are ledger rows. `events.txt` and `map.png` are projections.
2. Do not add a mode to `game_core.c`. A new rule is a new op, a harness case that can fail, and a row in the decision log before the code. Tick triggers are still not built, so the CLI op is the call.
3. To stand a grid up, run `rta_stand_op` and aim the existing selector at a south road. Do not change camera equations. Roof cubes come from extrusion height 3. The sprite path caps at 96 boxes, so leave `building_draw` unset and use cubes.
4. A car is a colored event box (`r g b` on the events line). No vehicle sheet was in the RPG Maker pack. Put the car one cell off the green cursor or the cursor hides it.
5. An editor book is `game_new_op` on a scratch house, then a desk copied from the stand op. Do that only when the owner asks. The live editor is not the scratch house.
6. The next paint gaps, both cities: autotile edges, piers on the bay slice (this pull had none), parks on the bay slice, and drawing the walker, the cash number, and the stars on the board. Those draws are projections of the ledger, not new state files.
7. A third city uses the same painter. Pick a box that fits under 220 by 300 cells (`rta_city_op` caps). 20 m cells are the way to keep a wider neighborhood in one desk. Name it only after the owner names it.

## Next grok session is the Minecraft handoff

Centon and the bay slice stop here unless the owner asks for the editor book or the draw gaps above.

Start the next session at `13.agent-coms/GROK/HANDOFF-2026-11-10-DF-MINECRAFT-EVENTS-META-HARNESSES-AND-CENTROID.md`, then the owner notes in `XO/31.Nov/nov-19/0.minecraft-handoff.md`. Read those before any Minecraft code. The rulings that are still in force:

- Do not start a Minecraft mode in `game_core.c`.
- Do not turn the live clock or autotick on.
- A kit is a plug (`PLUG`, `PARAM`, `COMMAND`), the same shape as day-night. The 11-19 KIT dialect is withdrawn.
- When a log breaks, its leaves only clear. They do not regrow.
- Two clocks stay two clocks. Minecraft is real-time. CDDA is turn-based.
- Mechanics are ops, RULE numbers, and ledger rows.
- Do not copy Minecraft, CDDA, DF, or Doom text, tiles, or code.

The Teario look question (flat 2D is the platformer, ordinary 3D is a board, do not remap a 2D row onto 3D height) is still open. Ask before changing it. Co-lab room N2UX stays closed.
