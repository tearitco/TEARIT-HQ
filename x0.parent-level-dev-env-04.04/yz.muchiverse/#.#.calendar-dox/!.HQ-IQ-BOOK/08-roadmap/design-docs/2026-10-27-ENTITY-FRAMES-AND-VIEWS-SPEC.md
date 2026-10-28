# Entity frames and views (spec, 2026-10-27)

Status: SPEC (nothing in this file is built yet unless marked FACT). Owner direction 2026-10-27, in his words: most objects exist independently, not together on a map; some belong to a "great-entity" (ship, planet); they all just happen to have a relative position in the galaxy, each has its OWN 0,0,0 like a .mtl/fbx object, and can scale relative to the galaxy voxels. "It is a game engine with swappable parts; it should be as simple as it suggests at first principles."
Supersedes the direction of `2026-10-27-PIECES-INTO-THE-WORLD-GRID-DESIGN.md` (stamping pieces into the grid): that patched the wrong model and is kept only as a record of what was found there (section 2 of it is still FACT).

## 1. The model (four rules)
1. **An entity is a folder** `pieces/<name>/` with its model in LOCAL coordinates (origin 0,0,0 at its own corner or declared centre) and a transform that places it in a PARENT frame.
2. **The root frame is the galaxy** (voxel = 1 unit). Any entity may name a parent entity; its transform is then in the parent's local frame. A **great-entity** is not a special type: it is an entity that other entities name as `parent=` (the ship is the parent of bridge, hold, quarters, engine; a planet is the parent of its land chunks).
3. **Scale is size over dim:** a model of `dim 6 4 6` with `size_x 12` covers 12 parent voxels, scale 2. No separate unit system. Rotation is a later key (`rot=`), absent = 0.
4. **A view is a projection of the resolved list, never a second description of the world.** 3D and 2D (and any later view: ascii, minimap, a 2D arcade game) walk the same entities, resolve transforms to world, and draw the model their own way. A new model format (fbx/obj/mtl) is one loader that produces the local model; nothing else changes.

## 2. What already exists (FACT, read from the code, `bv_render_3d.c load_vehicle` and `RPG-SPACE-ENGINE-PIECES.md`)
- `pieces/<name>/state.txt`: `pos_x pos_y pos_z size_x size_y size_h selflit desk=`; `model.txt`: `dim X Y Z` + `x y z r g b`; since cd825a6cd `tile_model.txt` + `tiles.rgba` (tile-faced model). The engine resolves size/dim as the box and draws it. Hero, xelector, sun, moon, ship_01, land_01, veh_1..24 already follow it. So rules 1 and 3 are mostly done.
- NOT there: a `parent=` key (everything is world-relative), parts of a great-entity (ship and planet are baked page cells, `desk=` selects pages), a 2D projection of pieces (`bv_render_2d.c` has no piece code: observed with live dumps 2026-10-27, bar lists a skiff at d10 and asteroids at d17..d20, 3D draws them, 2D does not).

## 3. File contract (PROPOSAL; additive keys, old files keep working)
state.txt gains, all optional:
- `parent=<entity name>` (default: the galaxy root). `pos_*` is then in the parent's local voxels.
- `frame_origin=corner|centre` (default corner).
- `rot=0` (later).
- `kind=<free text>` (rock, ship, land, loot) for views that want a sprite/emoji; the 2D projection uses it only as a fallback.
A great-entity needs no extra file: its parts name it. The desk concept (`desk=`) becomes "the part whose local frame the camera is in": a part is visible from outside (the whole ship from space) and from inside (its frame fills the view).

## 4. The two projections
**3D** (exists): box + model per entity, resolved to world.
**2D (to build):** for each visible entity, resolve its footprint (x, y, width, depth) in the camera's frame; for each footprint cell take the top-most voxel of the model column (or the top tile of `tile_model.txt`, or the entity's `kind` emoji if it has none) and draw it at the 2D cell size; sort by height. The 2D camera is the same frame as the 3D camera: inside the Hold it shows the Hold's local page as today; zoomed out it shows the parent frame (ship, then galaxy) with every entity in it, using the existing `+`/`-` zoom (`view_2d_zoom`). One source, so a thing cannot be in 3D and missing from 2D.

## 5. Gates (every step needs both)
- **Parity gate** (harness, pal, no new .sh): for each kind of entity drawn in 3D, remove its folder and check the 2D frame changes at its footprint (and the reverse); NEGATIVE test: a harness case with an entity file the 2D projection ignores must FAIL the gate.
- **Look gate** for the 2D projection: a PNG pair (2D and 3D of the same desk) in the commit message evidence, per the human-IO evidence rule.
- Existing gates stay green: rpg_space_planet_map (101), bv_gpu_cut/voxel/tile tests.
- Cost line (house rule): measure frame time with a browser running, idle and active, before and after.

## 6. Migration and what gets retired
1. Spec (this file). 2. 2D projection of pieces + parity gate (additive; changes nothing in 3D). 3. `parent=` support in the loader and 2D; express the skiff and asteroids with it (they already are pieces). 4. Ship desks as parts of `ship_01` and land chunks as parts of the planet (the big step; the page-cell overlay is retired only after the parity gate passes on them). 5. Retire, in this order, each only after the gate proves its replacement: `desk=*` hack and `space_all_desks`, the `obj_draw` flat-tile fallback, the land overlay in `maker_load`, the piece cap names list (`veh_name`), the grid-stamping doc.
Not retired: tile-faced pieces (the 3D look of an entity), the cutaway (extend it to pieces separately), `bv_look`.

## 6b. Status log
- 2026-10-27, step 2 BUILT (bv_render_2d.c `p2d_load` / `p2d_draw`, bv_menu_input.c zoom -1): the 2D view reads the same `pieces/<name>/` files as the 3D engine (names ship_01 + veh_1..24, desk rule empty / * / equal) and draws each piece top-down over its footprint (highest voxel per column, or the top tile of `tile_model.txt`). Zoom level -1 (`-` key from fit-page) fits the page AND every piece. Gate `ops/bv_2d_piece_test.c`: 8 checks pass with the new renderer; with the renderer built before the change the three drawing checks FAIL (negative test). LIVE: window dumps of the rpg-space bridge at zoom -1 show the page, the tile skiff and the three asteroids at their world cells. LATER THE SAME DAY: (1) CORRECTION: the land is NOT a gap. The 3D land overlay (`bv_render_3d.c maker_load`) only fills the EMPTY cells of the shown desk page from the region page (pos_x/pos_y shift), so land never extends beyond the 16 x 16 page in 3D either; the same planet backdrop is painted into each desk's map.png by the daemon, so 2D and 3D show the same land. Observed: bridge map.png cells around the hull are the region cells at the same position, darkened (the 3D 'sunk' look). The earlier '48 x 48 land' worry was the map-limit box and the direct-flight page shift, not extra drawn land. (2) The gate is now a pal harness: `harness/pieces_2d_parity.pal` + `cases/pieces_2d_parity.pdl`, 22 checks: it compiles the renderer and `bv_2d_piece_test.c` from source into a scratch dir and runs a POSITIVE case plus a NEGATIVE case (the same renderer built with `-DBV_2D_NO_PIECES`, which must FAIL the drawing checks). It also covers the desk-picture branch (map.png, what rpg-space uses), a piece off the page invisible at fit-page and visible at zoom -1, and zoom -1 without pieces equal to fit-page. (3) BUG found by that last check and fixed: zoom -1 with no piece indexed `zpct[-1]` (negative-tested: the unfixed build fails the check). (4) A project with no drawable piece renders a byte-identical 2D frame to the build without the piece code (cmp, scratch). (5) COST: with the 18 real rpg-space pieces (6382 voxel rows) 8-9 ms per 2D frame without piece code, 9-10 ms with it (30 runs each, niced, scratch copy; one process start included). Frames are made on screen change, not in a loop. NOT DONE: `parent=` and parts (steps 3-4); the zoom -1 path exists only for the map.png branch (the plain glyph branch just draws the pieces that fall in view).

## 7. Open questions (owner)
- Origin convention: corner (as pieces are today) or centre (as most fbx exporters)? Default proposed: corner, with `frame_origin` to override.
- Should a part be able to be seen from outside AND inside at once (the ship hull from space while the camera is in the Hold)? Proposed: yes, the camera frame only decides what is drawn at full detail.
- 2D zoomed-out galaxy view: draw every entity or only those above a size threshold?
