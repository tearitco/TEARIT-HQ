# rpg-space: the engine pieces, which ones draw, which never did (2026-10-25)

Written because the owner asked to document the engine pieces that are loaded but never drawn ("we may draw them in micro later, inside the ship's voxels, or give it its own room"). Everything below was read from `&.widgits/board-viewer/ops/bv_render_3d.c` and from the live instance folder `@.apps/rpg-space/rpg-space-pchq/pieces/`, not remembered. Companion: `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` (sections 4e, 4f, 7c, 7f).

## 1. What a "piece" is
A folder `pieces/<name>/` of the pc-hq instance. The engine (`bv_render_3d --daemon`) re-reads the pieces on every scene build (`load_entities`, `load_xelector`, `load_hero`, `free_vehicles(); load_vehicle()` all sit in the per-frame scene function), so moving a piece is a number in a text file, no book rewrite and no reload.

| piece | files | drawn? | what for |
|---|---|---|---|
| `hero_01`, `xelector_01`, `sun_01`, `moon_01`, `world_01` | `state.txt` | yes | the engine's own cast: the player's cursor, the sun and moon, `world_01/state.txt` holds the desk id on show |
| `land_01` (and the older `land_0..8`) | `state.txt` (`entity_type=land`, `pos_x/pos_y`, `alt`, `src=region`) | **yes**, as an overlay | the planet under the bridge in direct flight: the engine fills the EMPTY cells of the bridge desk from the planet page desk, shifted by the entity's position (the old textured / extruded look, `maker_load`). Moving it is the direct flight. |
| `ship_01` | `state.txt` (`entity_type=vehicle`, `desk=none`, `pos_x/y/z`, `size_x/y/h`), `model.txt` | **no, on purpose** | the ship as an entity with a world position (the truth the land pieces are placed against, `sk_ship_world_write`). `desk=none` never equals a desk id, so `load_vehicle` skips it: the real hull is the baked bridge desk. |
| `veh_1` .. `veh_9` | `state.txt`, `model.txt` | **loaded, never drawn** | tried for remnants (see 3). Not used any more. |
| `1` .. `5` | (not looked at) | | numbered folders in the instance, not examined for this doc |

## 2. The vehicle piece format (engine `load_vehicle`, `bv_render_3d.c` ~ line 968)
- The names are a FIXED list: `ship_01, veh_1 .. veh_9` (`g_veh_names`, `VEH_MAX 10`). No other folder name is ever read.
- `state.txt` keys: `pos_x`, `pos_y` (both required: the piece is skipped unless both are present), `pos_z` (height of the underside), `size_x`, `size_y`, `size_h` (the box in cells), `selflit`, `desk=` (if present it must equal `world_01`'s `desk_id`, else skipped).
- `model.txt`: first line `dim X Y Z` (voxels along grid x / height / grid y), then one row per voxel `lx ly lz r g b`, at most `VEH_VOX` (4096) voxels.
- Drawn as ONE box with a dense voxel model (`GPU_ADD_MODEL` then `ADDBOX(x, z, y, x+sx, z+sh, y+sy, 150,150,160, selflit)`), the same GPU path as the hero and the trees. World axes: X = grid x, Y = up, Z = grid y.

## 3. What was tried and what was seen (do not repeat blindly)
- Remnants as `veh_1..3` pieces (2026-10-25). A temporary debug line in the engine confirmed `loaded=3` with the right position and size and a box count far below the limit, yet NOTHING appeared in the rpg-space scene, not even a 4x4x4 test block. The engine was not at fault in any way that was visible in the log. Replaced by extra rows in the room's `events.txt` (the pets' own draw path), which draws and animates fine (design doc 7c).
- Open hypotheses, none tested: (a) the model slot the piece asks for (`GPU_ADD_MODEL` with `dim-1`) comes back valid but empty because the voxel coordinates in `model.txt` are outside `dim`; (b) the box is built but the rpg-space scene takes a GPU path that skips the vehicle loop (the loop sits before the `sc.maker` branch, but inside the scene function only reached after the desk is chosen); (c) `pos_z` is read in world height of the underside, and the bridge floor sits higher than the value used; (d) the box is hidden by the desk's own floor cells when `size_h` is small. A real test needs a piece with a big, bright, self-lit model high above the bridge and a debug log of the box it adds.
- The hero piece's `pos_z` is NOT the standing level (it resets after a reload: read 9, not 17): `tuning.pdl remnant_z` (17) carries the standing height instead.

## 4. What draws today for the new things (so nobody waits for the engine)
- remnants, resource nodes and built blocks: rows `x y r g b rem <idx> <dir> 0 *` appended to the room's `events.txt` plus 16x24 frames `rem_<idx>_<dir>_0.rgba` (the path the pets use). Whole cells only.
- asteroids and other ships on the galaxy grid: cells of the SPACE desk painted by `sk_build_space` (`sk_space_draw`): a grey rock cell per asteroid node and a one-cell teal hull per other ship, with the node block on top. No engine piece.

## 5. Plans (owner 2026-10-25, not built)
1. **Micro**: draw pieces INSIDE the ship's voxels at a small scale (the ship interior as a miniature scene, "dustopia" microverse), so things in the hold, crew inside another ship, or a mined chunk can be seen at their own size without the engine drawing them at room scale.
2. **Own room**: give a thing its own room (desk) with its own pieces (an asteroid's surface, another ship's bridge), entered the way doors and the helm switch desks today. This is the path that needs no new engine drawing.
3. Whichever comes first: if vehicle pieces are ever made to draw (section 3), `veh_*` is the cheapest way to get sub-cell, smoothly moving remnants and ships; the names list must then grow past 10 (it is a fixed array) before more than a few ships and asteroids can be pieces.
