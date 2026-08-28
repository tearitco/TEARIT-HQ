# Tile System Design — autotiling + animated tiles — 2026-08-27

**Status: DESIGN ONLY, nothing built yet.** Per the "Confirmed
next-steps order" in `COMMON-EVENTS-MANAGER-HANDOFF.md` (item 2) and
the direct instruction to plan autotiling/animation before any
tile-placement implementation starts. Goal, stated directly: "a living
interactive, event scripted desktop" — tiles that move and react,
driven by the real common-events system already built.

## 0. Honest premise check (done before writing this doc, not assumed)

Verified there is genuinely **no existing 2D tile-grid/map-placement
canvas anywhere in this codebase** — grepped for
`tile_grid`/`TileMap`/`map_grid`/`tile_placement`/`MapGrid`/`autotile`
across `&.widgits/` and `*.monads/`: zero hits. Real, adjacent-but-
different things that exist and are namechecked below where relevant,
but are NOT this system:
- `&.widgits/palettes/`'s `rmmv` category — a tile-picker SOURCE (pick
  one sprite, place it as a standalone desktop glyph window), not a
  grid canvas.
- `&.widgits/event-editor/gl_mock/RMMV_EVENT_EDITOR_GUIDE.md` — UI
  chrome for the event COMMAND list editor, unrelated to a map canvas.
- `101.mutaclsym🧟‍♂️️19.00/`'s voxel world (`convert_og_map_to_voxels.c`)
  — a real 3D voxel system, a genuinely different data model than a 2D
  RPG-Maker-style tile grid. Not reused here; noted so nobody conflates
  the two later.

This is real, from-scratch design work — say so plainly rather than
implying a shortcut exists that doesn't.

## 0a. MAJOR REVISION (2026-08-27, direct clarification) — tiles ARE
entities, not a separate canvas

Direct correction after the first draft of this doc proposed a new,
dedicated map-canvas renderer: **"each tile will be treated just like
adding a new entity to screen... they are still entity class in
mutaclysm 3d."** This changes §4 below entirely — there is no separate
canvas to build. A placed tile IS a real entity, spawned via the SAME
`tp_desktop_window_rgb.c` mechanism every desk-pal already uses.

**Real, confirmed grounding for this** (checked directly, not assumed):
`tp_desktop_window_rgb.c` (this house's own real per-entity desktop
window binary) already defines `#define GRID_CELL_PX 80` (line 215) —
the exact same 80px grid constant ported from a sibling project's
`egg_window.c`, confirmed via `/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/
#.♻️.EzYQL/2.muchi-verse-0.2/2.zoo_0000/dox/desk-grid-prompt.txt` (the
real doc explaining the grid's origin/math). Every existing desk-pal
already snaps to this real 80x80px grid, origin (0,0), column/row count
derived from the real, live screen resolution (`DisplayWidth`/
`DisplayHeight`) — not assumed or hardcoded per-machine.

**Practical effect on this design**:
- Placing a tile = spawning a real `tp_desktop_window_rgb.c` entity at
  a grid-aligned position, same as placing any pal. No new rendering
  binary needed for individual tile placement.
- §4's "new dedicated map-canvas binary" option is WITHDRAWN. §4's
  "extend tp_desktop_window_rgb.c" option was actually closer to
  right, but even that undersold it — a tile isn't a special mode of
  the entity window, it just IS one, with a tile-flavored `meta.pdl`
  (autotile group / animation frames per §2/§3 below) instead of a
  pal-flavored one.
- **Real, deliberately-deferred optimization idea (user's own words,
  not committing to it now)**: "there CAN also be 'mini map style
  entities' with multiple tiles especially for things like redundant
  grass" — i.e., one entity window could later represent a whole
  contiguous patch of identical tiles instead of one entity per single
  tile, if per-tile-entity performance ever becomes a real problem.
  **Not designed further here** — the user's own reasoning for
  deferring is sound: the desktop is a finite, real, already-measured
  grid (31x20 cells at this machine's actual resolution per the grid
  doc above), so one-entity-per-tile is unlikely to be "that bad" in
  practice. Revisit only if a real performance problem shows up.
- `GRID_CELL_PX` should become a real, configurable value read from a
  `.pdl` (direct instruction: "this size should be set/read from a
  .pdl which can be changed"), not a hardcoded `#define` — real,
  scoped follow-up: add a `desk_grid.pdl` (or similar, matching this
  house's own `SECTION | KEY | VALUE` convention) with a
  `GRID | cell_px | 80` row, read once at `tp_desktop_window_rgb.c`
  startup, replacing the compile-time constant. Small, mechanical,
  low-risk change.

## 1. Data model

### 1.1 Tileset

A tileset is a real, checked-in image atlas (matching the existing
`rmmv` palette category's own asset convention — reuse those source
images, don't re-source new ones) sliced into a fixed grid of square
tiles. **Real tile asset size, corrected (direct confirmation):
RPG Maker MV's own real tile size is 48x48px**, not 32px — the earlier
draft of this doc had this wrong. Each tile in the atlas has a real
integer ID (`row * cols + col`), same addressing shape
`palettes_manager.c` already uses for its own tile grids.

**Real, open sizing question this doc does NOT resolve**: the desktop's
own real entity grid (`GRID_CELL_PX`, see §0a) is 80px, not 48px. Since
a tile is now a real desktop entity (§0a), each tile ENTITY's own
window is grid-cell-sized (80px, or whatever `desk_grid.pdl` says once
that's built) — the 48x48 SOURCE ASSET gets drawn into that entity's
own window, scaled or padded to fit. **RESOLVED (2026-08-27)**: scale
the 48px source UP to fill the full current cell size (80px default, or
whatever `desk_grid.pdl` says) — matching real RPG Maker rendering
(tiles connect edge-to-edge with no visible gap, which autotiling's
whole premise depends on). Native-size-with-padding was rejected
specifically because it would break autotile edges from visually
touching. Changing `desk_grid.pdl`'s default to 48 instead of 80 was
also rejected — that's the SAME shared grid every existing pet/entity
already uses, so shrinking it would resize every non-tile entity's
spacing too.

Two tile KINDS, both real, need to coexist in the same tileset:
- **Plain tiles** — one atlas cell = one placeable tile. No autotiling,
  no animation. The common case (walls, furniture, decorations).
- **Autotile tiles** — a GROUP of 47 atlas cells (RPG Maker MV's own
  real convention — see §2) that render as ONE logical tile type
  (grass, water, cliff, etc.), with the actual cell drawn per-placement
  chosen automatically from neighbor state.

A tileset manifest (flat, human-readable, matching this house's own
`SECTION | KEY | VALUE` PDL convention used everywhere else) declares
which atlas regions are autotile groups vs. plain tiles:

```
SECTION      | KEY                | VALUE
----------------------------------------
AUTOTILE     | grass              | atlas_x=0,atlas_y=0
AUTOTILE     | water              | atlas_x=6,atlas_y=0
PLAIN        | wall_brick         | atlas_x=0,atlas_y=8
ANIMATED     | water_deep         | atlas_x=12,atlas_y=0|frames=4|fps=4
```

(`atlas_x`/`atlas_y` given in TILE units, not pixels — e.g. `atlas_x=6`
means "the autotile group's 47-cell block starts at tile-column 6.")

### 1.2 Map storage

One map = one real, flat text file, `map_<id>.pdl` — a real grid,
row-major, matching this house's own preference for flat/inspectable
formats over binary:

```
SECTION | KEY  | VALUE
-----------------------
MAP     | w    | 20
MAP     | h    | 15
ROW     | 0    | grass,grass,grass,wall_brick,grass,...
ROW     | 1    | grass,water,water,water,grass,...
...
```

Each cell holds the tile's real NAME (`grass`, `water`,
`wall_brick`) — matching the tileset manifest's own KEY column — not a
raw numeric atlas index, so a map file stays human-readable and
independent of atlas layout changes (an atlas can be re-packed without
invalidating every map that uses it, same principle as this house's
switch/variable NAME-based addressing over raw numeric IDs).

## 2. Autotiling — REVISED (2026-08-27) after reading real RPG Maker MV
engine source, not folklore

**Real correction, sourced from an actual deployed game's real,
readable `rpg_core.js`** (see `RPG-CODE-INDEX-REF.md` for the full
citation and code excerpts) — the earlier draft of this section got two
things wrong:
1. The real count is **48 shape slots per autotile kind**, not 47 (the
   "47" figure was community folklore, not verified against real
   source — confirmed by directly counting `FLOOR_AUTOTILE_TABLE`'s
   real entries: 48).
2. **Each shape is NOT one whole pre-drawn tile image.** The real
   engine composites each tile from **4 quadrant pieces** (top-left/
   top-right/bottom-left/bottom-right, each a real half-tile-sized
   rectangle blitted from the atlas independently) — a shape's table
   row is 4 `[x,y]` quadrant-source-coordinates, not one tile
   coordinate. This is materially more storage-efficient (a small
   library of real corner/edge quarter-pieces, not 48 full tile images)
   and is what this house's own implementation should do too, not a
   simplification original to this design.

Also real and newly confirmed: there isn't ONE universal table — floor/
ground tiles use a 48-entry table, wall tiles use a SEPARATE 16-entry
table, waterfall tiles a separate 4-entry table (see
`RPG-CODE-INDEX-REF.md` for all three, verbatim, with real source line
citations).

### 2.1 The bitmask — still the right approach, with one honest caveat

For a placed autotile cell, look at its **4 edge-adjacent neighbors**
(N/E/S/W) AND its **4 corner-adjacent neighbors** (NE/SE/SW/NW) — 8
total. For each of the 8, set a bit if that neighbor is the SAME
autotile group; clear it if not (map edge / different tile / empty
counts as "not same"). This gives an 8-bit neighbor mask (0-255).

A CORNER bit only matters if BOTH adjacent EDGE bits on that corner are
also set — an isolated diagonal neighbor with no matching edge on
either side can't form a visible connected shape, which is why 48 real
shape slots suffice instead of a full 256-way table:
```
corner_NE effective = corner_NE_bit AND edge_N_bit AND edge_E_bit
corner_SE effective = corner_SE_bit AND edge_S_bit AND edge_E_bit
corner_SW effective = corner_SW_bit AND edge_S_bit AND edge_W_bit
corner_NW effective = corner_NW_bit AND edge_N_bit AND edge_W_bit
```

**Honest caveat, confirmed by directly grepping the real engine source
for "neighbor"/"bitmask"/"computeShape" (zero hits)**: the neighbor→
shape-index COMPUTATION is NOT in this runtime engine at all — RPG
Maker MV computes it once, at author-time, in its own closed-source
Windows map editor, and simply bakes the resulting shape index into the
saved map's tile IDs. The runtime engine only ever reads an
already-decided shape index; it never derives one live. **This house's
own implementation must design its own neighbor→shape-index numbering
from scratch** — the real, practical resolution: reuse RPG Maker's own
REAL quadrant tables (`FLOOR_AUTOTILE_TABLE`/`WALL_AUTOTILE_TABLE`/
`WATERFALL_AUTOTILE_TABLE`, ported verbatim per `RPG-CODE-INDEX-REF.md`)
as this house's OWN internal shape numbering too — there's no need to
reverse-engineer the closed editor's own bitmask-to-index formula,
since this house is free to define its OWN edge+corner-bit → index
mapping (the 4-edge + 4-effective-corner bitmask above, hashed/ordered
however is convenient) as long as it consistently points at the same
48 real quadrant-coordinate rows. The VISUAL result is what has to
match RPG Maker's real assets, not the internal index numbering scheme.

### 2.2 Recomputation trigger

An autotile cell's DISPLAYED variant depends on its neighbors, so it
must be **recomputed whenever any of its 8 neighbors changes** — not
just at placement time. Real, minimal implementation: on any single
cell edit (place/remove/change), recompute that cell's own variant AND
all 8 of its neighbors' variants (since the edit changed THEIR
neighbor-state too). This is a cheap, bounded (9-cell) recompute per
edit — no need for a full-map rescan.

## 3. Animated tiles — separate system, real frame cycling

Independent of autotiling (a tile can be BOTH — e.g., an autotile
water variant that also cycles frames, though the simplest real MVP
scopes animation to PLAIN tiles only, expand to animated-autotile
combinations later if actually needed).

Real mechanism: each `ANIMATED` tileset entry (see §1.1 manifest shape)
declares `frames=N|fps=F`. At render time (Play mode only — a static
editor view shows frame 0, matching the flagged requirement "not shown
as animated in a static editor view"), the currently-displayed atlas
cell for that tile = `base_atlas_x + (real_elapsed_ms / (1000/F)) % N`.
This is a pure function of wall-clock time, no per-tile animation
state needs to be stored or ticked separately — every animated tile
instance of the same type is trivially in sync with every other
instance for free (matches real RPG Maker behavior: all water tiles of
the same type animate in lockstep).

## 4. Rendering integration point — RESOLVED (2026-08-27, see §0a)

**No longer an open fork.** Per §0a's direct correction: a tile IS a
real `tp_desktop_window_rgb.c` entity, grid-aligned via the real
`GRID_CELL_PX` mechanism (soon config-driven, see §0a). No new
rendering binary is needed for placing/displaying individual tiles.
What each tile entity needs, concretely, beyond a normal pal:
- A `meta.pdl` (or equivalent) identifying it as a tile: which
  tileset + tile NAME it represents (matching §1.2's map-file
  addressing-by-name convention).
- If it's an autotile-group member: its currently-computed variant
  (§2), recomputed on the entity's own redraw whenever a neighbor
  entity's occupancy changes (see §2.2 — "neighbor" now means "the
  entity sitting at the adjacent grid cell," a real, checkable thing
  since every entity's grid position is already real, live state).
- If it's animated (§3): the same pure wall-clock-time function,
  evaluated in the entity's own redraw.
- **RESOLVED (2026-08-27) — a real, HYBRID model, not a binary choice.**
  Direct instruction: "if a 'desk' has no 'map-print' file just show the
  entities/tiles, but if it does, use that. and if it has both, use
  both." Real rules:
  1. A desk with NO `map_<id>.pdl` — purely whatever live tile-entities
     currently exist, no map file involved at all. This is the
     real, low-friction "just place some stuff and play with it" mode
     — explicitly named as being for **"convenient debugging/
     prototyping and fun,"** not the serious/shareable path.
  2. A desk WITH a `map_<id>.pdl` — that file's tiles get expanded into
     live entities on load (the save/blueprint model from the earlier
     draft, confirmed correct).
  3. A desk with BOTH a map-print file AND additional live entities —
     **use both, union them** (the map-print's own tiles PLUS whatever
     extra live entities are also present) — not exclusive-or, the
     saved map doesn't get replaced/ignored just because live entities
     also exist.
  - **The real reason this matters, stated directly**: the desk itself
    is for prototyping/fun; **"boardview" (piececraft-xyz's own
    board-viewer widget, §4a) is for hardcore play and game sharing"**
    — an entire saved map-print file should be loadable directly into
    boardview for real camera-driven 2D/3D interaction (see §4a), which
    is "really the entire point" of having a real, portable map-print
    file format at all, not just a debugging convenience for the desk.
    This directly connects §1.2's map file format to §4a's 2D→3D bridge
    — the SAME map-print file is both "what a desk can optionally load"
    AND "what boardview loads for real play," not two separate formats.

## 4a. 2D→3D bridge (direct instruction, 2026-08-27) — a real, additional
requirement, not yet designed in detail

Direct clarification: "the 2d desktop tiles will be drag and dropped
into mutaclysm/piececraft, where they are expected to be 3d if 3d mode
is on." This means the tile system above is NOT a standalone 2D-only
canvas — a placed 2D tile needs a real path into becoming its 3D
equivalent when dragged into either 3D world. Real complication, not
yet resolved: **Mutaclysm and piececraft-xyz have genuinely DIFFERENT
3D data models**:
- **Mutaclysm** is voxel-based (`101.mutaclsym🧟‍♂️️19.00/ops/
  convert_og_map_to_voxels.c` — a real, existing 2D-map-to-voxel
  converter, though for its OWN map format, not this new tile system's
  `map_<id>.pdl` shape — worth reading directly before designing the
  bridge, it may already solve half of this).
- **piececraft-xyz** is chunk/block-based (`@.apps/piececraft-xyz/ops/
  pc_generate_chunk.c`).

**Real implication**: a single 2D tile type (e.g. `grass`) needs a
real, explicit mapping to (a) a Mutaclysm voxel type AND (b) a
piececraft block type — two separate conversion tables, not one,
since the two 3D systems don't share a block/voxel vocabulary today
(confirmed by their separate, unrelated data models — not assumed).
This is real, additional, NOT-yet-designed work — flagged here rather
than silently folded into the 2D design above, since it changes the
tileset manifest's own shape (§1.1 will likely need a `mutaclysm_voxel=`
and `piececraft_block=` field per tile TYPE, not per placement, so the
mapping is defined once per tile type and reused everywhere that type
is placed).

**Real, existing drag-drop precedent found (2026-08-27, direct
pointer)**: `#.DOX/drag-drop-how2.md` documents a REAL, already-working
X11 XDND drag-drop mechanism between a desktop pet window
(`egg_window.c`) and Mutaclysm's own GL mirror window (`gl_mirror.c`)
— real `XdndEnter`/`XdndPosition`/`XdndDrop`/`XdndFinished` protocol
messages, a real visual highlight on valid drop targets, and a real
`pet_import` trigger on successful drop. This is genuine, working
precedent for "drag an entity from the desktop into Mutaclysm" — a
tile (now a real entity per §0a) dragged into Mutaclysm should extend
this SAME real XDND mechanism, not invent a new one.

**Real, honest limitations of that existing mechanism, confirmed by
reading its own doc**: (1) it's currently ONE-DIRECTIONAL (desktop →
Mutaclysm only) — the user has directly asked for the REVERSE too
("dropping it from desk but also dropping from 'boardview' to desk"),
which is NOT built; (2) its own "Current Limitations" section admits
actual pet DATA transfer via `XdndSelection` isn't complete yet (the
drop is detected, but reading the real pet data back out is still
real, incomplete work per that doc); (3) it's pet-specific
(`pet_import`) — extending it to tiles means a real, new
`tile_import`/`tile_export` pair, not literally reusing the pet
function, though the X11 protocol plumbing (XDND event handling) is
the same and reusable.

**Phase timing, direct instruction**: "scope hook at least" — meaning
don't fully defer this to an unscoped future pass, but also don't
build the full two-target (Mutaclysm voxel + piececraft block)
conversion tables yet. Real, concrete Phase-1.5 scope: read
`drag-drop-how2.md`'s real source files (`egg_window.c`'s Xdnd-source
side, `gl_mirror.c`'s Xdnd-target side) and `convert_og_map_to_voxels.c`
in full, then write a real HOOK POINT design (where in
`tp_desktop_window_rgb.c` a tile-entity's own drag would send
`XdndEnter`/`Drop`, and where in Mutaclysm's/piececraft's own render
loop a matching drop-target handler would live, and what the
board-view→desk REVERSE direction's own hook point looks like) —
without committing to the full voxel/block conversion-table content
yet. That real conversion-table work stays Phase 2, scoped only once
this hook-point design exists and both 3D systems' own formats are
read in full.

## 4b. Tile-placement AUTHORING UI — REAL SCOPE ADDED (2026-08-27, direct
instruction + a real mockup provided)

**Direct correction: this was wrongly left as "not yet covered" (§5
below) — the user pointed out the real, already-reserved integration
point was sitting unused and provided a real HTML mockup
(`/home/no/Desktop/🤖️🪤️🏠️/xmp/rmmv-tiles+mo.0.txt.html`) of exactly what's
wanted.** Scoped properly now.

### 4b.1 The real integration point — already exists, currently a stub

**Confirmed, not guessed**: `&.widgits/palettes/pallets.pdl` already
has a real, reserved category — `CATEGORY | rmmv | RPG Maker Tiles |
rmmv` — the 4th category row (matching the user's own real navigation:
"6. palettes; 4. rpg maker tiles"). `khtpm_entity_menu_render.c`'s own
comment (~line 6558) confirms this category is currently "fully static
(title + [content])" — a real, acknowledged stub, exactly the slot this
whole tile system should plug into. **No new top-level window/binary is
needed** — this becomes a real `dbhq_load_palette_state()`/
`dbhq_inject_palette_tiles()`-driven picker, the SAME real mechanism
already proven this session for the chemistry/emoji palette categories
(`MARKETABLE-FEATURES.md`'s own palettes section) — not a new pattern.

### 4b.2 Real UI shape, from the provided mockup

The mockup shows two real, distinct pieces, both needed:
1. **Category tabs (A/B/C/D/E)** — matching the REAL tile-ID category
   split already sourced in `RPG-CODE-INDEX-REF.md`
   (`TILE_ID_B/C/D/E` = non-autotile "normal" tile strips; A1-A5 =
   the real autotile categories, §2 above). Clicking a tab shows that
   category's real tile grid (8 columns wide, 48px tiles, matching the
   mockup's own real CSS grid — `repeat(8, 48px)`).
2. **A tile grid** — real, individual, clickable tiles within the
   active category. Click = select (sets a "current brush," see 4b.3).
   The mockup's own info bar (`Selected: A1 (0,0)`) is real, useful UX
   — show the real category+coordinate of whatever's currently
   selected, not just a highlight with no label.
3. **A REAL, additional piece the mockup explicitly calls out as new**:
   a **tileset chooser** — a dropdown/list of real, named, complete
   tilesets (the mockup's own example: "001: Outside," "002: Inside,"
   "003: Dungeon," "004: World Map," "005: Ship Interior"). This maps
   directly onto real RPG Maker MV's own concept of a "tileset" as a
   named BUNDLE of A1-A5/B/C/D/E images together (confirmed: the real
   engine source has separate `Outside_A1.png`/`Inside_A1.png`/etc. per
   bundle, matching this exact naming). **This means §1.1's tileset
   manifest needs to become a real, multi-tileset REGISTRY** (a new
   `tileset_registry.pdl` listing each real named tileset bundle and
   where its real A1-A5/B/C/D/E source images live), not a single
   hardcoded manifest as the original draft assumed — switching
   tilesets in the picker re-points the SAME category-tab UI at a
   different bundle's images.

### 4b.3 How tile placement actually works (the real missing link,
answering "how are we supposed to place tiles")

1. User opens palettes → RPG Maker Tiles (the real, now-live category).
2. User picks a tileset from the chooser (4b.2 item 3), picks a
   category tab, clicks a tile in the grid — this sets a real
   "current brush" state (which tileset + category + tile coordinate
   is currently selected), written to a small real state file (same
   pattern as every other real picker state this session already
   built, e.g. `g_evhq_picker_type`/`g_evhq_field1` for the command
   picker).
3. **The actual PLACEMENT action is a real desktop click** — while a
   brush is armed, clicking an empty (or occupied, for replace) grid
   cell on the real desktop spawns/updates a real tile-entity there
   (§0a) with that brush's tileset+category+tile identity written into
   its own `meta.pdl`. This is the same real "click to act" pattern
   `dbhq_activate_elem()`/entity click-dispatch already uses everywhere
   else in this house — not a new interaction paradigm.
4. Placing/removing/changing a tile re-triggers §2.2's real 9-cell
   autotile recompute for autotile-category tiles.

**Real, still-open UI detail, not yet resolved**: does "armed brush,
click desktop to place" stay armed across multiple placements (paint
mode, matching RPG Maker's own real UX) until explicitly cancelled, or
does it disarm after one placement? **Recommendation: stays armed**
(matches RPG Maker MV's own real behavior directly, and makes placing a
grass field practical) — flag for confirmation before building, not
assumed silently.

## 5. What this design does NOT yet cover (real, honest scope boundary)

- ~~Tile-placement AUTHORING UI~~ — **RESOLVED, see §4b above.**
- `event_run` wiring (making a placed tile actually trigger a common
  event on interaction) — depends on the rendering decision in §4 and
  on the existing common-events system's own trigger mechanism, real
  follow-up work once a canvas exists to interact with.
- Collision/walkability per tile — not modeled yet; a real `PLAIN`/
  `AUTOTILE` manifest entry will likely need a `walkable=0|1` field
  added once the player/collision runtime loop (a separate, larger gap
  per `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`) is scoped.
- **The 2D→3D drag-into-Mutaclysm/piececraft bridge (§4a)** — real,
  confirmed requirement, explicitly scoped as Phase 2, not designed in
  detail yet (needs `convert_og_map_to_voxels.c` and piececraft's chunk
  format read first).

## 6. Real next implementation steps, in order (revised per §0a/§4)

1. **DONE (2026-08-27)** — `GRID_CELL_PX` is now a real runtime
   variable in `tp_desktop_window_rgb.c`, read from optional
   `#.desktop/desk_grid.pdl` (`GRID | cell_px | N`), default 80
   preserved. Compiled clean; parsing logic independently verified
   standalone (no file → 80, real file with `cell_px=96` → 96).
2. **DONE (2026-08-27)** — both real decisions confirmed: 48px tile
   assets scale UP to fill the current cell size (§1.1); the map model
   is a real hybrid (no map-print file = live entities only; map-print
   file = expanded on load; both = union, not exclusive) with the SAME
   map-print file format shared between a desk's own optional load and
   boardview's real play/sharing load (§4).
3. **DONE (2026-08-27)** — real `tile_autotile.c` built at
   `*.monads/*.livedesk-taskbar/ops/tile_autotile.c`: all three real
   tables ported verbatim (48/16/4 rows, confirmed by a real
   `sizeof`-based structural check, not eyeballed), the real
   quadrant-compositing blit-rect math ported from `_drawAutotile`
   (verified: 4 quadrants exactly tile a 48px tile edge-to-edge, no
   gaps/overlaps), and the edge+corner AND-reduction from §2.1
   (verified: an isolated corner bit is correctly zeroed, a
   both-edges-backed corner correctly survives). Compiled clean
   (`-Wall -Wextra`, zero warnings), standalone test passes.
   **Blit-math visually verified against real assets (2026-08-27,
   same day)**: copied a real tileset (`World_A2.png`, a real grass/
   dirt tileset) from the RMMV source into
   `&.widgits/palettes/tilesets/rmmv/`, rendered real shape rows
   (0/15/23/31/39/47) through the ported table+blit math, and visually
   inspected the output — clean, correctly-aligned grass/dirt edges, no
   garbling, and shape 47 (the table's own last row) correctly rendered
   as a small isolated dirt patch fully surrounded by grass, matching
   its expected "no real neighbor connections" semantic exactly. This
   is real, positive evidence the ported table + coordinate math are
   correct against real pixel data.
   **Still explicitly NOT done, flagged in the file's own header**: the
   shapes above were hand-picked for the visual spot-check, not derived
   from a real 8-neighbor bitmask via `autotile_pick_quadrant()` — the
   neighbor→shape-index mapping itself is the one real gap remaining
   before this can drive actual gameplay tile placement. Wiring this
   into `tp_desktop_window_rgb.c`'s real render path is the next real
   step after that mapping exists.
4. **DONE (2026-08-27)** — real multi-tileset registry built:
   `&.widgits/palettes/tilesets/tileset_registry.pdl` (2 real entries
   today — "World Map" keyed `outside`, "Inside" keyed `inside`, each
   with a real `a2` category path to the actual copied assets; other
   categories legitimately absent, not fabricated) + a real loader,
   `*.monads/*.livedesk-taskbar/ops/tile_registry.c` (groups
   `TILESET | <key>.<field> | <value>` rows into per-tileset entries,
   same real SECTION\|KEY\|VALUE convention as every other house PDL
   reader). Compiled clean, standalone test independently verified:
   loads exactly 2 entries, correct names/paths, and explicitly
   confirms `outside.a1` is empty rather than silently defaulting to
   something fabricated.
5. **IN PROGRESS (2026-08-27)** — real manager-side `rmmv` category
   handling built in `palettes_manager.c` (`publish_rmmv()`), following
   the exact real compliant shape the user directly required: the
   manager owns all real logic, the renderer stays fully generic (zero
   new tile-specific code in `khtpm_entity_menu_render.c` beyond what
   chemistry/emoji already proved). Real chain verified: house-standard
   PDL-driven category dispatch confirmed end to end (taskbar menu rows
   → `livedesk:open-palette:<category>` → manager launch, all data-
   driven, zero hardcoded category lists anywhere) - see the real
   hardcode AUDIT below, done the same session per direct instruction.

   **A real bug found and fixed along the way, via Grok's own review**
   (`/home/no/Desktop/🤖️🪤️🏠️/xmp/grok-question.txt`/`grok-quest.png`,
   2026-08-27): the first version of `publish_rmmv()` cropped the
   atlas into a flat grid of every raw 48px cell (192 for the real
   `World_A2.png` asset) and published ALL of them as independently
   selectable tiles. **This is wrong for autotile categories (a1/a2/a3/
   a4/a5)** - confirmed by Grok, matching real RPG Maker MV asset-
   authoring standards: each real "kind" occupies a fixed BLOCK of raw
   cells (2 cols × 3 rows = 96×144px for floor-type a1/a2/a5, matching
   the real `bx=tx*2/by=(ty-2)*3` addressing already sourced in
   `RPG-CODE-INDEX-REF.md`), and the picker should show exactly ONE
   real, artist-drawn representative thumbnail per kind - the block's
   own TOP-LEFT cell - never all 6 raw cells (the other 5 are real
   compositing FRAGMENTS, consumed only by `tile_autotile.c`'s own
   quadrant math at actual placement time, never shown directly to a
   user). Real, honest root cause: I had already built and visually
   verified the correct compositing math earlier the same session, but
   skipped using it when writing the picker's own tile-cropping code -
   caught before it shipped, not after.

   **Fixed and re-verified visually**: `publish_rmmv()` now iterates
   real KIND blocks (not raw cells) - 32 real kinds for the sourced
   `World_A2.png` asset (8 kind-columns × 4 kind-rows), each published
   as one real representative tile. Reassembled the 32 real thumbnails
   into a grid and visually confirmed: 32 distinct, sensible, complete
   tile types (grass patch, dirt path, cliff face, tree, mountain,
   water lily pad, rock formations, etc.) - not fragments. Scoped
   honestly to `a2` only in the code's own comments - `a1`/`a3`/`a4`/
   `a5` have different real block dimensions (a3/a4 are wall-type) and
   must NOT reuse this exact 2×3 math once their own real assets are
   sourced.

   **Still not done**: the actual renderer-visible picker UI (category
   tabs A-E + tileset chooser) - what's built so far is real, correct
   backend data (verified via direct manager invocation + PNG
   reassembly), not yet wired into `palettes-rmmv.chtpm`'s own Elem
   tree or a live, in-window test. Sets a real "current brush" state on
   tile click is also still pending (§4b.3).

### Real hardcode AUDIT (2026-08-27, direct instruction: "flag hardcoded
things in parser, they are supposed to use generic .pdl read functions
whenever possible")

Full sweep of `khtpm_entity_menu_render.c` for hardcoded category-name
checks, done directly (grepped for every known category string, not
just the one already noticed) - **exactly one real hit found**, fixed:
`int wide = (strcmp(g_pal_category, "elements") == 0);` (chemistry's
special wide-tile layout). Pre-existing, not introduced this session,
but a real, same-class violation - fixed the same way: `pallets.pdl`
gained a real `WIDE` column (`SECTION|KEY|LABEL|PICKER|WIDE`),
`palettes_manager.c`'s new `publish_layout_flag()` reads this category's
own real WIDE value once at startup and publishes it to a small sibling
file (`palettes-<category>_layout.txt`), and the renderer now reads
that generically - zero hardcoded category names left in the renderer
for this decision. Verified live: `elements` → `wide=1`, `rmmv` →
`wide=0`, both sourced from the real PDL column, not a string compare.

**A second, more clear-cut hardcode found in the same audit** (not in
the renderer, but the same real violation): `palettes_menu.sh`'s own
`title_for()` was a hardcoded bash `case` statement duplicating every
category's real LABEL verbatim from `pallets.pdl` - meaning a NEW
category added to the PDL would silently get no real title. Fixed to
read the LABEL column directly (same real `IFS='|'` parse shape
`list_cats()` in the same script already used) - verified all 4 tested
categories (`emojis`/`elements`/`rmmv`/`user-pallet`) produce identical
output to the old hardcoded version, and an unknown key now correctly
fails rather than silently guessing.

**Confirmed clean (verified, not assumed)**: the full real dispatch
chain - taskbar menu row generation (`livedesk_build_palettes_menu()`),
the `livedesk:open-palette:<category>` action parse, and the manager
launch itself - is genuinely PDL-driven end to end, zero hardcoded
category lists anywhere in that chain. This was the real, pre-existing,
already-correct precedent the new `rmmv` work was built to match.
6. Wire the armed-brush "click desktop to place" interaction (§4b.3) —
   spawns/updates a real `tp_desktop_window_rgb.c` tile-entity at the
   clicked grid cell, per §0a.
7. Build the map-file loader (§1.2) as a plain C parsing function,
   matching every other flat-PDL loader already in this codebase
   (`read_key_value()`-style, not a new parsing paradigm) — expanding a
   loaded map into real spawned tile-entities (§4), not a separate
   renderer.
8. Wire the autotile recompute-on-neighbor-change logic (§2.2, now
   "neighbor" = the real entity at the adjacent grid cell) and the
   animated-tile time function (§3) into each tile-entity's own real
   redraw path.
9. Read `#.DOX/drag-drop-how2.md`'s real source files AND
   `convert_og_map_to_voxels.c` in full; write the real Phase-1.5 hook-
   point design (§4a) — where tile-entity XDND drag/drop handlers would
   live, both directions (desk→3D and 3D→desk), without yet building
   the full voxel/block conversion tables (stays real Phase 2).
10. Real verification per this house's own standard: hand-build a small
    test map with a few autotile transitions (grass/water border) and a
    couple of plain tiles, spawn the real tile-entities, PNG-dump them,
    visually confirm the border entities show correct edge/corner
    variants — not just "it compiled." Do this on the dedicated
    "tile experiments" desk (user's own stated preference), not the
    live/real desktop.
