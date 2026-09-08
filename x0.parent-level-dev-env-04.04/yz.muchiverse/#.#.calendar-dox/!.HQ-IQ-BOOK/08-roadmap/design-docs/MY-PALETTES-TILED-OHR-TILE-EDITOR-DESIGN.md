# My Palettes + Tiled + OHRRPGCE + tile-editor — design

**Status: DESIGN ONLY, 2026-09-08.** No loader/editor code in this
pass. Catalog sibling: `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md`.

Owner ask: add Tiled-style and OHRRPGCE-style tiles to palettes; restore
a generic **My Palettes** library (loader/saver, file-explorer, personal
library); add a Tiled-like **tile-editor** category. Research first.

---

## 0. What already exists (do not reinvent)

| Piece | Fact |
|---|---|
| Chooser-grid pickers | `rmmv` / `piececraft` / `cdda` / `emojis` — DIR + TILESET + swatch, `palettes_manager.c` + projector. **Reuse this shape.** |
| `user-pallet` row | `pallets.pdl` currently `LABEL Canvas-Craft` `PICKER canvascraft` — Canvas-Craft **ate** My Palettes (`CANVAS-CRAFT-DESIGN.md` Q1). Do **not** steal that row back; add a **new** `my-palettes` category. |
| `write_png_thumb_csv` | Already crops any PNG to 48px sprite.csv. Tiled/OHR grids are the same crop with different cell size. |
| File Explorer | `&.widgits/file-explorer/` LOAD publishes `result=` in `file_explorer_ui.txt`. pdl-read already polls it. |
| GLUT viewers | `#.potential-assets/#.hampster-tiles…/tileD_sprout_a0…/tile_viewer.c` and `OHRRPGCE-TILES&CODE/wiki-tiletools]20x20]/tile_viewer_20x20]c4.c`. **Reference only** — new HQ path is khtpm, not GLUT. |
| Event guides | `#.ref/menu/event-guides/` — new styles get sheets later, not in v1. |

**Renderer rule:** no new `g_is_*` in `khtpm_core_render.c`. Tile-editor
v1 is `class="palettes-pal database-window"` + `<canvas>` **or** a
swatch grid + stamp into a map `.pdl`. Prefer the piececraft board
stamp path (`pchq_place_cell.sh`) over a new layout branch.

---

## 1. Research — Tiled (“tile-d”)

Tiled (mapeditor.org) stores:

- **Tileset** `.tsx` / `.tsj`: `<tileset tilewidth tileheight spacing margin columns>` + `<image source="sheet.png"/>` **or** a collection of per-tile images.
- **Map** `.tmx` / `.tmj`: layers of global tile IDs (`firstgid` + local id). Encoding CSV or base64+zlib. Orthogonal is v1; hex/iso later.
- Classic public example: `https://github.com/mapeditor/tiled/tree/master/examples` (`desert.tsx` + `tmw_desert_spacing.png`).

**Already on disk (no download required for v1 demo):**

Sprout Lands Basic pack (Cup Nooble, itch.io) at
`/home/no/Desktop/github/work/#.potential-assets/#.hampster-tiles…/tileD_sprout_a0…/Sprout Lands - Sprites - Basic pack/`

- `Tilesets/Grass.png` is **176×112 PNG** (not 320×200). These are
  **atlas PNGs**, not `.tsx`. A Tiled-style loader must accept:
  1. `.tsx`/`.tsj` with image + tilewidth/height/spacing/margin
  2. **bare PNG atlas** + a sidecar `sheet.pdl` (`tile_w`, `tile_h`,
     `spacing`, `margin`) when XML is missing
- Also `Characters/`, `Objects/` — DIR tabs like RMMV non-tileset dirs.

House copy target (same outside-zip rule):  
`NNEST-12.00/x0.parent-level-dev-env-04.04/#.NNEST_ASSETS/tiled-sprout-lands/`  
plus optional later clone of Tiled `examples/` as `tiled-official-examples/`.

PDL: `1.^V-hq/TILED-ASSET-SOURCE-LOCATION.pdl`.

---

## 2. Research — OHRRPGCE

Wiki (`https://rpg.hamsterrepublic.com/ohrrpgce/TIL`,
`Part_1_-_Maptiles`):

- Map tiles are **always 20×20**.
- A tileset page is **320×200** = **16 columns × 10 rows = 160 tiles**.
- Binary lump: `TIL` (same pixel layout as Mode-X `MXS`). Import/export
  also as BMP/PNG of that size.
- Passability is a **separate** wallmap (per-side walls, one-way), not
  in the PNG. Animation: **two patterns per tileset** (`TAP` lump).
- Walkabouts 20×20; heroes 32×40 — different loaders, later.

**Already on disk:**

`…/OHRRPGCE-TILES&CODE/wiki-tiletools]20x20]d6…/wiki-tilemaps-1gb/`
— many `320px-*.bmp.png` (wiki thumbs of real 320×200 sets: Vikings,
TS01–TS25, Taoki). Good enough to slice 16×10 at 20px **if** the wiki
thumb is still 320 wide; **verify pixel size on import** (thumbs may be
scaled — if not 320×200, skip or scale with a warning).

Full engine tree: `wiki.ohrrpgce-wip]ngn/` (Custom/Game). v1 does **not**
parse `.rpg` lumps. v1 = PNG/BMP 320×200 (or exact multiple) → 160 crops.

Owner note in `wiki-tiled]a1.txt`: passability + animation authored by
hand is desirable **before** an RMMV-autotile module. Tile-editor v1
should expose a **passability overlay** (4-bit NESW) as a sidecar
`.pass.pdl`, not baked into pixels.

PDL: `1.^V-hq/OHRRPGCE-ASSET-SOURCE-LOCATION.pdl`.

---

## 3. My Palettes (generic loader / personal library)

Restore the old “user’s own saved palette” idea (`pallette-design.txt`
`user-pallet`) **without** undoing Canvas-Craft.

### 3.1 New `pallets.pdl` rows

```
CATEGORY | my-palettes | My Palettes | mypal | 0
CATEGORY | tiled       | Tiled Tiles | tiled | 0
CATEGORY | ohrrpgce    | OHR Tiles   | ohr   | 0
CATEGORY | tile-editor | Tile Editor | tedit | 0
```

`user-pallet` / Canvas-Craft **stays**. `tiled` / `ohrrpgce` are
**stock demos** (like rmmv/cdda). `my-palettes` is the **library**.
`tile-editor` is the **authoring** window.

### 3.2 Library on disk

Per-user, outside generated `sprites/`:

`xyzfs/users/<uid>/home/livedesk/my-palettes/<entry_id>/`

```
meta.pdl          style=tiled|rmmv|cdda|piececraft|ohr|other
                  tile_w= tile_h= spacing= margin=
                  source_path=   (original import)
                  label=
sheet.png         or atlas copy
pass.pdl          optional NESW flags per tile index
event_guide.pdl   optional pointer into event-guides/
```

Index: `xyzfs/users/<uid>/home/livedesk/my-palettes/index.pdl`
`ENTRY | id=sprout-grass | style=tiled | label=Sprout Grass`

House-wide demos still use `1.^V-hq/*-ASSET-SOURCE-LOCATION.pdl`.
Import **copies** into the user library (do not alias `#.potential-assets`).

### 3.3 Loader styles (the dropdown)

| style | How to slice |
|---|---|
| `tiled` | TSX/TSJ if present; else PNG + `tile_w/h` from meta or guess (16, 32, 48) |
| `rmmv` | existing `publish_rmmv_asset_dir` geometry (A1–E / characters / faces) |
| `cdda` | one PNG or folder of PNGs as now |
| `piececraft` | folder of 16px block faces |
| `ohr` | 320×200 → 20×20 grid (160 cells); reject other sizes unless `tile_w/h` override |
| `other` | user types `tile_w`, `tile_h`, `spacing`, `margin` in a `<cli_io>` row, then slice |

### 3.4 My Palettes window (v1)

Same chooser families as rmmv:

- DIR = library entries (or “Stock: Tiled”, “Stock: OHR”, “Personal”)
- TILESET = sheets inside an entry
- Grid = crops via `write_png_thumb_csv`
- Toolbar: **File** → `file-explorer` LOAD; on `result=` copy into
  library + prompt style (default `other` with guessed 16/32/20)
- Arm/place: same `palettes_menu.sh` brush files (`arm-mypal`)

No XML writer in v1 except writing `meta.pdl`. Export TSX is v2.

---

## 4. Tile-editor (Tiled-like, house-shaped)

**Not** a GLUT port of `tile_viewer.c`. **Not** a full Tiled clone
(object layers, terrains, wang sets, infinite maps = later).

v1 product:

1. Left: swatch grid of the **active My Palettes / tiled / ohr** sheet
   (reuse projector).
2. Center: **map canvas** — a 2D grid of tile indices, stored as
   `map.pdl` / `map.csv` (Sprout `map.csv` already exists in the GLUT
   folder as a hint). Render: either `<canvas>` blit of a composed
   `.raw` (manager writes it, like piececraft overlay) **or** a
   `<repeat>` of swatches (var-cap: same KH_MAX_VARS cap as rmmv —
   keep maps small, e.g. 16×16, or compose to one canvas).
   **Decision: compose to one `<canvas>` sprite** so 50×50 maps do not
   blow the var table. Manager stamps pixels; renderer only blits.
3. Tools: pencil, fill, eyedropper, eraser. Passability toggle (OHR
   F3 analog) draws arrows on the canvas overlay.
4. Save: `xyzfs/…/my-palettes/<id>/maps/<name>.csv` + `pass.pdl`.
5. File menu: file-explorer for open/save-as.

v2: layers, Tiled TMX export, piececraft chunk export, event-guide
stamp on a cell.

---

## 5. Manager / files (implementation sketch, not this PR)

- `palettes_manager.c`: `publish_tiled()`, `publish_ohr()`,
  `publish_mypal()` — copy of `publish_cdda` with grid math.
- `button-pal.sh`: add `tiled|ohrrpgce|my-palettes|tile-editor`.
- Templates: `palettes-tiled.xhtpm` etc., CSS copy of piececraft.
- Tile-editor: `@.apps/tile-editor-hq/` **or**
  `&.widgits/palettes/palettes-tile-editor.xhtpm` + `tile_editor_manager.c`.
  Prefer **widgit next to palettes** so File/place share `palettes_menu.sh`.
- Zero new renderer C if canvas compose works; if a 2D grid element is
  required, `khtpm_draw_core.c` already has a `<grid>` draw path
  (2026-09-05) — **check that before adding a layout branch.**

---

## 6. Key decisions

1. **New `my-palettes` row**, do not reclaim Canvas-Craft’s `user-pallet`.
2. **Stock Tiled demo = Sprout Lands already on disk**; official Tiled
   `examples/` is optional second demo, not blocking.
3. **OHR v1 = 320×200 PNG/BMP only**, not `.rpg` / `TIL` lumps.
4. **Personal library copies files** into xyzfs; never gitadd PNGs.
5. **Tile-editor paints a composed canvas**, not per-cell xhtpm vars.
6. **Passability is a sidecar**, matching OHR’s wallmap split.
7. **No GLUT in the HQ path.**

---

## 7. Open questions (need owner)

1. Default Tiled cell size when PNG has no TSX — guess 16 (Sprout) vs
   prompt every import?
2. Tile-editor lives under palettes vs `@.apps/tile-editor-hq`?
3. Import wiki 320px thumbs that are **not** exactly 320×200 — scale
   or skip?
4. Should My Palettes also list **read-only stock** rmmv/cdda/piececraft
   entries, or only user imports + tiled/ohr demos?

---

## 8. PR plan

| PR | What | Depends |
|---|---|---|
| **0** | This doc + PDL pointers + copy Sprout Lands + verified OHR 320×200 sheets into `#.NNEST_ASSETS/{tiled-sprout-lands,ohrrpgce-tiles}` (no house-zip). | — |
| **1** | `publish_tiled` + `palettes-tiled.xhtpm`; DIR=Tilesets/Objects/Characters, TILESET=PNG stem, grid=slice from `sheet.pdl` or 16px guess. | 0 |
| **2** | `publish_ohr` + 20×20 slice of 320×200; skip bad sizes. | 0 |
| **3** | `my-palettes`: index.pdl, file-explorer import, style dropdown, copy into xyzfs, chooser of personal entries. | 1–2 |
| **4** | `tile-editor`: canvas compose + pencil/fill + save csv; read active my-palettes sheet. | 3 |
| **5** | Passability overlay + event-guide pointer per cell. | 4 |
| **6** | Optional: clone Tiled official `examples/`; TMX/TSX parse; TMX export. | 1 |

---

## 9. What this is not

- Not RMMV A1 autotile math (separate, `TILE-SYSTEM-DESIGN.md`).
- Not piececraft 3D drop (still unwired).
- Not parsing OHR `.rpg` games.
- Not shipping GLUT `tile_viewer` as the HQ editor.
