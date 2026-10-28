# Pieces into the world grid (design, 2026-10-27, nothing built)

Owner question: the skiff and asteroids look like a different renderer from the ship and planet. Is there one path? Can they stay entities?

## 1. What is really different (checked in code, bv_raymarch.frag / bv_render_3d.c)
- Ship and planet are CELLS of the world grid (GL_R8UI 3D texture) with the maker tile atlas (u_cells + u_atlas). Vehicle pieces (skiff, asteroids, remnants) are BOXES with a small voxel model (u_bmdl/u_mdl) or, since cd825a6cd, a tile-faced model (u_ptex).
- Already SHARED by both: the final lighting (ambient, u_light), the 0.75 side factor (tile-faced pieces), fog, the sky. So "a different lighting scheme" is not the cause.
- NOT shared: the cutaway (is_ghost only tests grid cells; pieces never go transparent); plain voxel models use per-voxel face factors 0.82 / 0.66 / 0.5 where the grid uses one 0.75; asteroids are one colour per voxel (rock tile averaged per fine voxel), the ship shows a full 24x24 picture per face; pieces are not in the DDA grid, so no grid-based occlusion/shadow/column_top.

## 2. Why pieces cannot simply be stamped into the grid today
- The grid of a view is the desk's board: observed `grid=16x16x20` (cut_debug.txt). The skiff sits at x 11.5..17.5, y 15.5..21.5 and the asteroids at x 24 / -9.5 / -13.5, y -6.5..19.5: outside it.
- Code that assumes grid column = floor(x), floor(y) with origin at the desk corner: bv_gpu_column_top, the cutaway target, view cropping (vww/vwh), the maker cell map (u_cells indexed by c.x, c.z), cut_target.txt.

## 3. Target design (keeps entities, owner rule 2026-10-24: entity = authority, no book rewrite)
- A piece keeps state.txt (pos, size, desk=*) and a CELL model (tile_model.txt: x y z top side, already written for the skiff). Nothing in the game moves cells.
- The engine STAMPS each piece's cells into the grid (and u_cells slots) at load, at the piece's position, with a world origin offset so the grid covers space. One renderer, one look: cutaway, fog, tile pictures, occlusion all apply.
- Stamping is a projection of the entity (one authority per fact). A moved piece re-stamps only its own old and new cells (sub-image upload), not the whole grid.
- Limits to decide: fractional position and rotation snap to cells (the spinning loot bag stays a piece, or snaps); asteroids far from the board may stay pieces beyond a distance.

## 4. Steps and gates
1. Grid origin offset: add (ox, oz) to the grid/cells/cut code; gate: ship and cutaway unchanged (bv_gpu_cut_test 12/0, gate 101/0, pictures identical on the bridge).
2. Enlarge the view grid to cover the space bounding box (about 40x30x20 = 24 k cells, memory trivial). MEASURE: frame time with a browser running, idle and active, before and after (rule: say what a feature costs). Rays that miss walk further: this is the real risk on the weak CPU/GPU.
3. Stamp the skiff only. Gate: a harness picture of the skiff next to the ship floor tile with the cutaway applied; cost line in the commit.
4. Asteroids, then remnants/nodes decision (cells vs pieces).
5. Retire the piece-model path only when nothing uses it.

## 5. Open / unmeasured
Grid-size ray cost; stamp cost per moving piece; whether u_cells (RG16UI per column, floor+wall slot) can express a free-standing 3D object (it is per COLUMN, two slots) - a column with a skiff roof and a hollow room needs more than one cell per column, so a 3D tile layer (or the legend/terr path) may be needed. This may be the real blocker and is the first thing to prototype.
