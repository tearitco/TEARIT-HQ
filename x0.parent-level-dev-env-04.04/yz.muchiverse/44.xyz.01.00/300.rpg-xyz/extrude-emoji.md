# Fix report: players/items render flat & color-only in 3D mode while walls render extruded + emoji-textured

## Summary

In 3D mode (`render_mode == 1`), brick walls (and other non-walkable
terrain/furniture) are ray-marched as extruded unit-cube AABBs and
texture-sampled from pre-baked emoji voxel data. Players, monsters, and
ground items are not — they render as flat, solid-color quads at `y=0`,
indistinguishable from ordinary floor tiles. This is not a small
regression; the 3D renderer (`ops/compose_rgb_frame.c`) simply never
built an entity pass. Walls got upgraded to a real ray-marched/textured
pipeline on direct instruction; entities were never included in that
work and still fall through to the original flat-tile code path by
construction (a walkability check that defaults unknown glyphs to
walkable), not by an explicit "only walls get textures" conditional.

Renderer: this project is a custom CPU-side software rasterizer/ray
marcher (`ops/compose_rgb_frame.c`), not a WebGL/Three.js pipeline. It
writes into a CPU RGBA framebuffer that `system/gl_mirror.c` later
uploads to a single GL texture/quad — that mirror file has no scene
logic and is not implicated in this bug.

## Where the two pipelines live

### Wall pipeline (working) — `ops/compose_rgb_frame.c`

- `render_3d_view()` (`:1287`) calls three passes: floor (`:1438-1475`),
  walls (`:1481-1482`), and a hardcoded debug cube (`:1484-1492`).
  **There is no fourth pass for entities.**
- `raymarch_walls_3d()` (`:1066-1183`) is the wall pipeline:
  - `:1122` — resolves the glyph's walkability via `glyph_walkable_3d()`.
  - `:1128` — resolves an `asset_id` via `terrain_or_furniture_asset_id()`
    (`:343-347`ish, terrain/furniture registries only).
  - `:1136-1144` — **the extrusion gate**: `if (!m->walkable)` runs a
    ray/AABB slab test (`ray_aabb_hit_3d()`, `:924-965`) against the
    tile's full `(col,col+1) x (0,1) x (row,row+1)` unit cube.
  - `:1152-1179` — on a hit, computes hit-face UV and calls
    `sample_voxel8_pixel()` (`:1023-1038`, backed by the per-asset
    `voxels_8.csv` cache `get_voxel8_cached()` at `:991-1017`) to fetch
    the actual emoji texture color for that pixel.
- The comment at `:884-919` documents this was ported from
  `wraith_rgb_daemon.c`'s ray marcher on direct instruction ("u should
  use the same ray marching formula wraith-alpha piececraft-3d-wraith is
  using"), and `:1162-1166` notes "all of the current 3d blocks should be
  emoji" — explicitly scoped to blocks/walls.

### Player/item/monster pipeline (broken) — same file

There isn't a separate one; entities fall into Pass 1, the **floor**
loop:

- `render_3d_view()` Pass 1 (`:1438-1475`):
  - `:1450` — `if (!glyph_walkable_3d(...)) continue;` — only walkable
    glyphs proceed.
  - `:1452-1456` — resolves a **flat solid RGB color** via
    `glyph_rgb_top()` / `glyph_fallback_rgb()` — no texture lookup at
    all.
  - `:1472-1473` — draws a flat `y=0.0` quad (`draw_quad_3d`) for the
    tile. No AABB, no extrusion, no emoji sampling.
- `glyph_walkable_3d()` (`:860-882`), `:864`: `if (!f) return 1;` — any
  glyph not found in a registry file defaults to **walkable**.
  Hero (`@`), xlector (`X`), monster glyphs, and item glyphs are never
  entries in `pieces/registry/terrain/terrain_types.txt` or
  `furniture/furniture_types.txt`, so they always evaluate to
  `walkable=1` and therefore always land in Pass 1, never Pass 2.
  This is confirmed by the comment at `:290-296` (in `glyph_fallback_rgb`
  region): "Glyphs not covered by either registry (hero, ground items,
  monsters — none of which have an rgb_top field yet, a known v0
  limitation...) get an obvious fallback color."

### The asset-id data needed for texturing already exists — just not passed in

`main()` builds a parallel per-cell asset-id map, `cell_asset` (declared
`:1691`), specifically so the 2D top-down emoji renderer can texture
every tile type, entities included:

- `:1709` — terrain/furniture cells: `terrain_or_furniture_asset_id(...)`.
- `:1739` — ground items: `snprintf(cell_asset[iy][ix], ..., "%s", item_id)`.
- `:1763` — monsters: `snprintf(cell_asset[my][mx], ..., "%s", monster_type)`.
- `:1782` — hero: hardcoded `"hero"`.
- `:1801` — xlector cursor: hardcoded `"xlector"`.

`cell_asset` is only ever read by the 2D top-down path, at `:1884-1888`
(`load_emoji_voxels()` + `blit_emoji_tile()`, inside the
`render_mode == 0` branch). The call to `render_3d_view()` at
`:1858-1859` passes `grid` but **not** `cell_asset` — the 3D pipeline
has no way to look up an entity's emoji asset even if it wanted to.

## Root cause (precise)

1. `render_3d_view()`'s signature/body never receives `cell_asset`
   (`:1287-1291`, call site `:1858-1859`), so no 3D code path can resolve
   an emoji texture for hero/items/monsters even in principle.
2. `raymarch_walls_3d()` independently re-derives its own asset id via
   `terrain_or_furniture_asset_id()` (`:1128`) instead of reusing
   `cell_asset` — and that lookup function is structurally restricted to
   the terrain/furniture registries, so it could never resolve an entity
   glyph even if called for one.
3. `glyph_walkable_3d()` defaulting unknown glyphs to `walkable=1`
   (`:864`) means entity glyphs always take Pass 1 (flat floor quad,
   `:1450`/`:1472-1473`) and never Pass 2 (extrude + texture,
   `:1136-1144`/`:1152-1179`). This is the mechanism, not a single "if
   entity" branch — there's no per-entity special case to point to
   because entities were never added to the 3D renderer's model at all.
4. `hero_z` is read but explicitly marked unused at `:1533-1534` with the
   comment "used in 3D rendering once z-level visual support is added" —
   further evidence entity 3D rendering was left as known future work.

## Fix direction (not yet implemented)

Add a fourth pass to `render_3d_view()`, analogous to
`raymarch_walls_3d()`, that:

1. Receives `cell_asset` (needs threading through the
   `render_3d_view()` signature and its call site at `:1858-1859`).
2. Iterates hero/xlector/monster/item positions (the same data already
   used to populate `cell_asset` at `:1739`/`:1763`/`:1782`/`:1801`)
   rather than scanning `grid` for "unknown-but-walkable" glyphs.
3. Builds an AABB (or billboard quad, since entities are likely meant to
   look like standing sprites rather than solid blocks — a design call
   for whoever implements this) per entity, and samples the entity's own
   `voxels_8.csv` via `sample_voxel8_pixel()`/`get_voxel8_cached()`, the
   same machinery `raymarch_walls_3d()` already uses, instead of falling
   into the generic flat-floor-quad Pass 1.
4. Runs before Pass 3 (the debug cube) and should occlude/be-occluded
   correctly against Pass 2's wall hits (nearest-hit-wins, matching the
   existing ray-march ordering) if entities are implemented as ray-marched
   AABBs; a billboard/sprite approach would need its own depth test
   against the wall pass's hit distance instead.
