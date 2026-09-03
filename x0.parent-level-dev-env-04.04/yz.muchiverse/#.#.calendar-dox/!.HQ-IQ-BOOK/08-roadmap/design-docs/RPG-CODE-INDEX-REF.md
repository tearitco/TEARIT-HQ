# RPG Maker MV code index — real findings from real engine source

**Source**: a real, deployed RPG Maker MV game ("SpaceShop388"), NOT the
community SDK download — real, readable, unminified JavaScript engine
source (`rpg_core.js`, `rpg_objects.js`, `rpg_managers.js`, `rpg_scenes.js`,
`rpg_sprites.js`, `rpg_windows.js`) plus real tile/sprite assets. Location
recorded in `RMMV-ASSET-SOURCE-LOCATION.pdl` (same directory) in case the
external drive/mount path changes. Read on 2026-08-27 specifically to
ground-truth-check `TILE-SYSTEM-DESIGN.md`'s autotile section — several
real corrections resulted, noted inline below and in that doc.

**Purpose of this doc**: an index of what's actually in this real engine
source, so a future session doesn't have to re-discover file locations
or re-read the whole engine to find something specific. Add to this
file as more of the engine gets read for other reasons — don't let it
go stale/incomplete silently, but also don't feel obligated to
document the WHOLE engine up front just because it's now accessible.

---

## File layout (`www/` directory of the deployed game)

- `js/rpg_core.js` — the real engine core: `Bitmap`, `Sprite`,
  `Tilemap` (the class documented below), `Graphics`, `Input`,
  low-level rendering primitives.
- `js/rpg_objects.js` — game data objects (`Game_Map`, `Game_Player`,
  `Game_CharacterBase`, etc. — not yet read in detail this pass).
- `js/rpg_managers.js` — singleton managers (`DataManager`,
  `SceneManager`, etc. — not yet read in detail this pass).
- `js/rpg_scenes.js` / `js/rpg_sprites.js` / `js/rpg_windows.js` — UI
  layer (not yet read in detail this pass).
- `js/plugins.js` + `js/plugins/` — this specific game's own plugin
  customizations (not read this pass — a real, game-specific
  customization layer on top of the stock engine, worth checking if a
  future question is "how did THIS game do X" rather than "how does
  stock RPG Maker MV do X").
- `img/tilesets/*.png` + matching `*.txt` — real tile atlas images.
  Real naming convention observed: `Dungeon_A1.png`, `Inside_A1.png`,
  etc. — the `A1`/`A2`/`A3`/`A4`/`A5` suffix matches the engine's own
  real `TILE_ID_A1..A5` category constants below, not arbitrary.

---

## The real `Tilemap` autotile mechanism (`rpg_core.js`)

### Real tile ID encoding (ground truth, exact values)

```js
Tilemap.TILE_ID_B      = 0;      // 0-255:    B tileset (ground, no autotile)
Tilemap.TILE_ID_C      = 256;    // 256-511:  C tileset
Tilemap.TILE_ID_D      = 512;    // 512-767:  D tileset
Tilemap.TILE_ID_E      = 768;    // 768-1535: E tileset
Tilemap.TILE_ID_A5     = 1536;   // 1536-2047: A5 (ground autotile, no water)
Tilemap.TILE_ID_A1     = 2048;   // 2048-2815: A1 (water/waterfall autotile)
Tilemap.TILE_ID_A2     = 2816;   // 2816-4351: A2 (ground autotile)
Tilemap.TILE_ID_A3     = 4352;   // 4352-5887: A3 (wall-top/roof autotile)
Tilemap.TILE_ID_A4     = 5888;   // 5888-8191: A4 (wall-side autotile)
Tilemap.TILE_ID_MAX    = 8192;
```

```js
Tilemap.isAutotile = tileId => tileId >= TILE_ID_A1;   // NOTE: A5 (1536-2047) is
                                                         // ALSO a real autotile
                                                         // category despite being
                                                         // numerically below A1 -
                                                         // isAutotile() as written
                                                         // only catches A1-A4; A5 is
                                                         // checked separately
                                                         // (isTileA5) - a real,
                                                         // easy-to-miss quirk.
Tilemap.getAutotileKind  = tileId => Math.floor((tileId - TILE_ID_A1) / 48);
Tilemap.getAutotileShape = tileId => (tileId - TILE_ID_A1) % 48;
Tilemap.makeAutotileId   = (kind, shape) => TILE_ID_A1 + kind * 48 + shape;
```

**Real correction to this house's own `TILE-SYSTEM-DESIGN.md`**: that
doc's first draft assumed a "47-variant" table based on common
tilemap-community folklore. The REAL, ground-truth number in this
actual engine source is **48** real shape slots per autotile kind
(confirmed by counting `FLOOR_AUTOTILE_TABLE`'s real entries directly:
48, not 47) — `TILE-SYSTEM-DESIGN.md` has been corrected to cite this
file rather than repeat the wrong folklore number.

### The REAL rendering algorithm — quadrant compositing, NOT one whole
tile per shape (major correction)

`TILE-SYSTEM-DESIGN.md`'s first draft assumed each of the 47(48)
"shapes" was ONE pre-drawn whole tile image, selected by a neighbor
bitmask. **This is wrong** — the real mechanism
(`Tilemap.prototype._drawAutotile`, `rpg_core.js` ~line 5040-5124) is:

1. Each shape's table ROW is 4 `[x,y]` pairs — one per QUADRANT
   (top-left, top-right, bottom-left, bottom-right) of the final tile,
   e.g. `Tilemap.FLOOR_AUTOTILE_TABLE[0] = [[2,4],[1,4],[2,3],[1,3]]`.
2. At draw time, the tile is built by blitting FOUR separate
   **half-tile-sized** source rectangles (`tileWidth/2 × tileHeight/2`
   each) from the atlas, one per quadrant, into the four quadrant
   positions of the destination tile (`bitmap.bltImage(...)` called 4×
   per autotile draw, `rpg_core.js` ~line 5103-5122).
3. This means the actual atlas only needs a SMALL library of real
   corner/edge quarter-tile pieces (not 48 whole pre-drawn tiles) — the
   48 "shapes" are really just 48 different ways of COMBINING a smaller
   set of quarter-pieces, which is why a real RPG Maker MV autotile
   graphic file is much smaller than "48 full tile images" would imply.

**Real, honest gap this doc does NOT close**: this engine source
computes shape→quadrant-pixel-coordinates (step 1-2 above) but does
**NOT contain any neighbor-bitmask→shape-index computation** — grepped
`rpg_core.js` for "neighbor"/"bitmask"/"computeShape": zero hits. That
logic lives in RPG Maker MV's closed-source, Windows-only map EDITOR
(not present in this deployed-game bundle) — the shape index is
computed ONCE at author-time and baked directly into the saved map
data's tile IDs; the runtime engine here only ever READS an
already-decided shape index, never derives one from live neighbor
state. **This house's own implementation genuinely needs to design its
own neighbor→shape-index formula** (since we don't have RPG Maker's
own editor source) — `TILE-SYSTEM-DESIGN.md`'s original edge+corner
bitmask reasoning is a reasonable, real approach to that missing piece,
just needs its own real shape-index numbering scheme (not assumed to
match RPG Maker's own internal shape-index order, since that ordering
is only meaningful relative to RPG Maker's own quadrant tables, which
THIS house's design can reuse verbatim as its OWN internal numbering
too, sidestepping the need to reverse the closed editor at all — see
`TILE-SYSTEM-DESIGN.md`'s updated §2 for the practical resolution).

### Three real tables, three real sizes (not one universal 48)

```js
Tilemap.FLOOR_AUTOTILE_TABLE     // 48 entries - ground/floor tiles (A1 water, A2 ground, A5)
Tilemap.WALL_AUTOTILE_TABLE      // 16 entries - wall-top/roof/wall-side tiles (A3, A4)
Tilemap.WATERFALL_AUTOTILE_TABLE //  4 entries - waterfall tiles specifically (a real
                                  //  subset of A1)
```
Real, confirmed by direct byte-count of each table's own source. A
future implementation needs to pick the right table based on which
real tile CATEGORY (A1/A2/A3/A4/A5) is being drawn — not one universal
48-entry table for everything, as the first design draft assumed.

### Real animation handling (water tiles)

`_drawAutotile`'s A1 (water) branch reads `this.animationFrame % 4` (or
`% 3` for waterfalls) to pick which of several real pre-drawn water
"surface" frames to composite — confirms this house's own
`TILE-SYSTEM-DESIGN.md` §3 (animated tiles as a pure function of
elapsed time) is the right shape, just note the REAL engine keys frame
selection off a shared `animationFrame` counter incremented once per
real engine tick, not off raw wall-clock time directly — either
approach is real and valid, just worth knowing which convention is
being matched if exact-frame-timing parity with real RPG Maker MV
output is ever a goal.

### Real tile-category semantics worth knowing

- `isWaterTile` — true for A1 tiles EXCEPT kind 96-192 (waterfalls
  live in that sub-range of A1).
- `isWaterfallTile` — A1, tileId in [+192, A2), AND kind is odd.
- `isGroundTile` — A1, A2, or A5.
- `isRoofTile` / `isWallTopTile` — A3/A4 respectively, kind%16 < 8.
- `isWallSideTile` — A3 or A4, kind%16 >= 8.
- `_isTableTile` (A2 only) — a real, additional rendering special-case
  for "table" (furniture-height) tiles that draws a shorter, offset
  strip instead of a full quadrant — not fully read this pass, flagged
  for whoever needs A2 "table" tile parity specifically.

---

## Not yet read this pass (real, honest scope boundary)

- `rpg_objects.js`'s `Game_Map`/`Game_CharacterBase` — the real player/
  NPC movement and collision logic. Directly relevant to
  `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`'s "no player/collision
  runtime loop exists" gap — worth reading if that gap gets picked up.
- `rpg_managers.js`'s `DataManager` — how RPG Maker MV actually loads/
  parses a `.json` map file (this house uses flat PDL instead, so this
  is reference-only, not something to port directly, but useful for
  understanding what fields a REAL map file needs to carry).
- The plugin layer (`js/plugins/`) — this specific game's own
  customizations, not stock engine behavior.

Add to this doc, don't replace it, when any of the above gets read for
a real reason later.
