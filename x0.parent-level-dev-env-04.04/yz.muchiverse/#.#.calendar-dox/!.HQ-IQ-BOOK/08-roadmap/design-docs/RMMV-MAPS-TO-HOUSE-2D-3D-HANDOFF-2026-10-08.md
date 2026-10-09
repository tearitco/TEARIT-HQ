# RPG Maker MV maps -> house maps (2D + 3D, desk + pc-hq) - handoff to Grok

2026-10-08. Written by claude for Grok. Owner's request: load "The Sword of the Spirit" (TSOTS) maps as pages that show in
2D and 3D in pc-hq, and can also run on the desktop; the JSON must first be converted to house format; start KISS.
**Human-facing rule (owner, 2026-10-08): every step ends with something the owner can see or click (a before/after PNG of the
real window plus the click path). No docs-only rounds.**

## 1. Is there already a converter? No.

Searched all file types in the house (excluding sessions/xyzfs): no script reads `Map###.json` / `MapInfos.json` / `tilesetId`.
Only two docs mention the idea:
- `#.Zarchive-2-trash/00-compact/json-2-house-prompt.txt` (2026-09-08): a **spec** for `rmmv_json_house.py import|export|roundtrip`
  (never written). Its Map section says: target `@.apps/piececraft-hq/pieces/system/maps/<id>/map.txt + events.pdl`, "pick (a) for
  RM-like maps first". Event-list mapping (MV code -> house NODE type) is described there. Reuse it; do not restart it.
- `00-compact/page-manager-architecture.md`: the RPG Maker manager idea ($dataMap/$gameMap) mapped onto page managers.
Already built and reusable: `_.monads/_.livedesk-taskbar/ops/tile_autotile.c` (MV autotile 48/16/4 tables, pixel-verified),
`tile_registry.c` + `&.widgits/palettes/tilesets/tileset_registry.pdl`, `TILE-SYSTEM-DESIGN.md` section 6 (map-file loader is its
own listed next step).

## 2. The source data (verified today, read-only)

Game folder (path contains an emoji and a newline, use `find -exec`):
`/home/no/Desktop/<robot-emoji house>/xv.<astronaut> RMMV+sec]linux\n<penguin>]0002/RMMV_TSOTS]LINUX=elf?.../__.Tearrmv SpaceShop388.m/www/data/`
- `System.json` gameTitle = "The Sword of the Spirit". `MapInfos.json`: 94 named maps (id, name, parentId). **98 `Map###.json`
  files** on disk (4 have no MapInfos row - check which). 1305 events in total. Tileset ids used: 1-8
  (1 Overworld, 2 Outside, 3 Inside, 4 Dungeon ...; `Tilesets.json`).
- Map size: up to **120 x 110** tiles; 85 maps are <= 64x64, **13 are bigger**. Note: another copy of MV exists under
  `GITS/rpg-atlas-stuff/rpg-maker-mv-og-mt/data/` (not TSOTS); do not mix them.
- `Map###.json` keys: `width,height,tilesetId,data[],events[],note,displayName,bgm,bgs,parallax*,encounterList,scrollType`.
  `data` is flat: index = `(z*height + y)*width + x`, **6 planes**: z0-z3 tile layers, z4 shadow bits, z5 region id
  (Map001: 38*42*6 = 9576, checked). Events: `{id,name,x,y,pages[{conditions,image,list,moveType,trigger,...}]}`; index 0 is null.
- MV tile id ranges (rpg_core `Tilemap`): B=0, C=256, D=512, E=768, A5=1536, A1=2048-2815 (animated), A2=2816-4351,
  A3=4352-5887, A4=5888-8191. Autotile kind = `(id-2048)/48`, shape = `(id-2048)%48`. Passability lives in
  `Tilesets.json` `flags[tileId]` (bit 0x0F = blocked per direction, 0x10 = star/above-character, 0x20..0x80 ladder/bush/counter/damage).
  Not re-verified in this session: confirm against `rpg_objects.js` before relying on exact flag bits.
- **Visual copy** (owner has made one): `#.NNEST_ASSETS/sp-rmmv.map.png]99=24x/Map001.png ...` (99 files, 24 px per tile).
  Checked: 70 of the PNGs I tested have size exactly `width*24 x height*24` of their JSON (0 mismatches among those present).

## 3. House formats that already exist

- **pc-hq map project** (the one the JSON spec points at): `@.apps/piececraft-hq/pieces/system/maps/<map_id>/game.pdl` (GAME rows:
  type, icon, label, n_chunks, chunk_i_x/y, n_desks, desk_i_id) + `<map_id>/<desk_id>/map.txt` (ASCII glyph rows) +
  `extrusion.pdl` (`EXTRUDE | <glyph> | <height delta>`; META rows). Loader: `pc_generate_chunk.c` `load_map_surface()` - **it reads a
  16x16 chunk (`CHUNK_DIM 16`)**, so a bigger map today means several chunks or a different loader.
- **Desk page** (what pc-hq follows when Synch'd): `xyzfs/users/<u>/home/livedesk/sessions/<s>/desks/<page>.pdl`, rows
  `DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n`; pc-hq follows it through `open_book_page.txt`
  (house-relative `pdl=`). Since 2026-10-08 the 3D/2D floor is sized from the page's largest cell (`pgr_extent`), still capped by
  `MAX_BOARD_DIM 64` in `bv_render_3d.c` / `MAX_DIM 256` in `bv_render_2d.c`.
- Events: package under the entity (`event_pkg/pages/page_N/`), trigger rules from `EVENT-TRIGGER-LAYER-PLAN.md`; registry of 87 command
  blocks. Entities = folders; a page row places them.

## 4. Proposed conversion (KISS, in this order; each step has a picture)

**Step 1 - PNG practice (owner's KISS idea; no meta).** Show `MapNNN.png` as the 2D page background in pc-hq/desktop so the owner
can see "TSOTS map 4 loads". Needs: a way for the 2D view to draw an image per page (check whether `bv_render_2d.c` has any sprite/
background path before inventing one). Evidence: PNG of pc-hq showing Map004 and its name in the page tab.
**Step 2 - tile converter, tiles only.** `rmmv_json_house.py import --www <data> --out <house>` (Python + PIL, UTF-8-sig, skip null
index 0, never writes into the MV project). For each map: `maps/tsots_<id>/desk1/map.txt` = one glyph per tile cell from the top
non-empty layer, plus `layers/z0..z3.txt` raw tile ids (lossless), `region.txt`, `meta.pdl` (name, tileset, size, bgm, note).
Glyph table (`tsots_glyphs.pdl`): walls/solid -> `W`, A1 water -> `~`, floor -> `.`, etc., decided from tile id range + `flags`;
`extrusion.pdl` gives walls height for 3D. Big maps (>64) first as split chunks or skipped with a clear note - decide with the owner.
Evidence: 2D + 3D pc-hq screenshots of 3 maps (small, medium, 120x110) side by side with the PNG.
**Step 3 - events as entities.** Each `events[]` row -> a page row (`DESK` row with cell x,y, glyph) + a stub entity folder carrying the
pages (trigger, conditions, `list` -> NODE rows per the 09-08 spec table). Only the 5-6 most common command codes first
(101 text, 102 choices, 201 transfer, 121/122 switches/variables, 355/356 script = unmapped sidecar).
**Step 4 - Synch.** The page appears in the livedesk book; desk and pc-hq show the same page file (the `SYNCH.md` / `PAGE-FILE.md`
work: one page file, both views). Click path: Menu -> Book -> TSOTS -> Map N.
**Step 5 - real tile art** (autotile via `tile_autotile.c`, tileset PNGs from `www/img/tilesets`) on the 2D side only after 1-4.

## 5. Questions for the owner (do not guess)
1. Maps larger than 64x64 (13 of them): chunk, crop, or later?
2. Glyph look in 2D first (letters) or go straight to real tile art for the 2D page?
3. One book "TSOTS" with 94 pages, or build/load only the maps named in MapInfos?

## 6. Traps already known
- MV arrays have a null at index 0 (maps, events, tilesets). `json.load(open(f, encoding='utf-8-sig'))`.
- The game's path has newline + emoji: no shell variables, use `find -exec` or Python `os.walk`.
- Do not commit PNG clones or the game's `www` into git; read the asset source location from a pdl, never hardcode it in C.
- Camera/mirroring: desk +x is screen-right in every pc-hq view (fixed 2026-10-08, `95df603cd`); entities on a desk page stand at z=1
  on a single floor layer. If a converted map's 3D picture is mirrored, check there first. Do not re-apply camera reverts.
- Only claim "done" with a fresh build, fresh run and a PNG the owner can look at (AGENTS.md).
