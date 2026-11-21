# Robot Trap Auto: Battery conversion report

Started 2026-11-21. Research and the menu rows only. No board, no ledger, no raster of Charleston.

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

Proposed box, to confirm before the export, not a surveyed boundary: the peninsula tip from the Battery waterfront north through the College of Charleston, wide enough for East Bay and a few blocks inland, and not as far as the MUSC campus unless the owner wants the hospital inside the first slice. The HTML's hospital is a rectangle. The real hospital can wait for a second chunk.

Cell size is the other decision. The HTML is 80 by 60 cells with no meters. A Battery slice about 1.2 km by 0.9 km at 10 m a cell is about 120 by 90, which still fits one desk. At 4 m a cell it becomes a few hundred on a side and should be chunks, the way the editor already thinks in desks. Recommend 8–10 m for the first import so a building footprint is several cells and a road is one or two. Record the origin (south-west corner, lon/lat) and the meters per cell next to the file so the raster can be repeated.

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
- Do not start the raster until the box and the meters per cell are confirmed.

## External references

- HTML: the file named in `@.apps/rta/reference.path` (`rta-v3.html`) and `wussup-rta.txt` in the same folder. The folder is the car directory under Desktop `!.🤖🏪️.RTH-X.com`.
- OpenStreetMap features as GeoJSON: one Overpass export of the box (roads, water, piers, amenities). Tool list, not data: `https://github.com/tmcw/awesome-geojson`.
- City footprints that can answer GeoJSON: `https://gis.charleston-sc.gov/arcgis2/rest/services/External/EAM_ASSETS/MapServer/18` (Buildings, query format includes geoJSON). Checked as a service description on 2026-11-21. Not downloaded.
- Draw Battle precedent: `DRAW-BATTLE-CLOCK-AND-FIGHT-2026-11-21.md`. Ledger rule: `../02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`.
