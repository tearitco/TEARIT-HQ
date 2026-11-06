# good-new/: approved new art with no RPG Maker original (2026-11-05)

Separate from `../good/` on purpose: `generator_tile_op verify-root <rmmv-www-img> good` walks every file under `good/` and fails on a file with no same-name original (OBS: 8 problems when these were placed under `good/new/`). `good/` stays the drop-in replacement set; this folder is art for NEW levels only.

**Approval.** Owner 2026-11-05: "u can approve to good folder if u think they are acceptable" and "we can use our new stuff for the new levels". claude opened each picture (4 opened: buildings.png, items.png, CastSheet_00.png, FaceSheet_00.png) and judged them readable at 1x with no obvious defect. **Not opened individually:** CastSheet_01/02 and FaceSheet_01/02 (same generator, same layout as the opened ones, approved on that basis; reopen if one looks wrong).

| File | What it is | Note |
|---|---|---|
| voxel/buildings.png | 9 voxel buildings (houses, tower, gate, shop) | opened, good silhouettes |
| voxel/items.png | 16 voxel items (potion, key, chest, sword, pickaxe, axe, coin, cloth, torch, barrel, scroll, book, bread, gem...) | opened, readable |
| cast/CastSheet_00..02.png | the generated cast walk sheets | opened _00: readable but samey (many dark-clothed figures) |
| cast/FaceSheet_00..02.png | the matching faces | opened _00: readable |

**Not here and why:** `interim/objects/tilesets/Outside_B.png` (our new art) covers only 60 of its cells, so as a same-name drop-in it would blank the other objects in old levels; use it in new levels through the `x:` extra root of the level preview. Voxel characters, A5 floors, themes and the ~90 recolour sheets are not approved. Hashes: `APPROVED.sha256` (run `sha256sum -c APPROVED.sha256` here).
