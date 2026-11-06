# Tear-it generator: state, rules, how to test, and what is next (handoff, 2026-11-05 night)
Labels: **SAID** owner, **OBS** observed this session, **DOC** my reasoning. Longer history: `HANDOFF-GENERATOR-GROK-WORK-AND-INTERIM-ASSETS-2026-11-05.md` (sections 1-19), `GENERATOR-WORKFLOW-AND-CHECKS-2026-11-05.md`, decisions D26 to D29 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.

## 1. The rules that matter most
1. **Existing levels and characters are never replaced** (SAID, last message of 2026-11-05). Tear-it art is for NEW levels. Never write into `#.NNEST_ASSETS/rmmv-www-img`. New levels read their own root (`generator_level_op` takes a root per level).
2. Nothing enters `good/` unless claude opened the picture and judged it (SAID: owner delegated approval). `good/APPROVED.md` lists what and why; `sha256sum -c good/APPROVED.sha256` and harness cases pin the hashes.
3. Lore-based looks are READ cues; every colour that is a guess is DOC, not canon. No outside services, no invented canon (soul-pen-lore skill).
4. No Python, no new `.sh`: tools are C ops, data is `.pdl`, checks are pal harness cases. Heavy runs go through `cpu_gate` (exit 3 = wait and retry). Commit by pathspec with `commit_scoped.+x` (paths are relative to the repo root `NNEST-12.00`, including the `x0.parent-level-dev-env-04.04/yz.muchiverse/` prefix).
5. A proof is not a picture: open the file before saying it is good. Say how many you opened.

## 2. The tools (all in `44.xyz.01.00/@.apps/generator-hq/`, harness in `&.widgits/_shared-lib/harness/`)
| Tool | What it does | Harness (passes) |
|---|---|---|
| `ops/generator_tile_op.c` | restyle cells, `sheet`/`sheet-png` (A2 sheets by block), `tone-run` (whole-sheet recolour, 99 sheets), `verify-png`, `verify-root` (same name and size as the originals) | generator_tiles 37, generator_restyle 44, generator_sheets 55 |
| `ops/generator_cast_op.c` | flat 2D cast from `events/gen_cast/cast.pdl` (126 characters), `slots` (RPG Maker people/face sheets by name) | generator_cast 44, generator_char 12 |
| `ops/generator_voxel_op.c` | voxel recipes (D29): `render`, `sheet`, `fromcast`, `walksheet`, `cast-sheets`, `icons`; data in `events/gen_voxel/{chars,items,buildings}.pdl` (+ generated `cast_vox.pdl`) | generator_voxel 24 |
| `ops/generator_level_op.c` | `level` draws a test level from any asset root (`events/gen_level/yard_*.pdl`), `raw-png` converts a game `scene.raw` | generator_level 7 |
Data: `events/gen_restyle/` (ramps `tearit-*.pdl`, `blocks.pdl`, `blocks-calm.pdl`, `sheets.pdl`), `events/gen_theme/` (8 world themes: venus, metalsun, hydro, eden, ffu, mars, centroid, planetx). Outputs: `#.NNEST_ASSETS/generator-hq/{good,interim}` (interim is mostly gitignored and rebuilt in seconds; `interim/try/` holds experiments).
Run one suite: `cd &.widgits/_shared-lib/harness; HOUSE=<abs house>; $HOUSE/&.widgits/_shared-lib/ops/+x/cpu_gate.+x $HOUSE run <label> -- $HOUSE/&.widgits/_shared-lib/system/+x/prisc+x.+x generator_<name>.pal`, verdict in `results/generator_<name>.txt.verdict.txt`. Use ABSOLUTE house paths (a relative one failed three times).

## 3. How to test art in a real game without touching the live folder (OBS, worked 2026-11-05)
1. Scratch house in `/tmp`: `#.NNEST_ASSETS/rmmv-www-img` as a symlink farm of the originals (`cp -rs`), an empty `&.widgits`, and a copy of `@.apps/rpg-pet` with its compiled `ops/+x/rpg_pet.+x`.
2. **Overlay a replacement by first `rm`-ing the symlink, then `cp`** (writing through a symlink would overwrite the original).
3. `cd <scratch>/@.apps/rpg-pet && ./ops/+x/rpg_pet.+x status` repaints `state/scene.raw` (660x430 RGBA); `generator_level_op raw-png scene.raw 660 430 out.png` makes the picture.
4. The unmodified binary loaded our sheets BY NAME. A run with only `good/` overlaid was byte-identical to the original (that room uses `Inside_A5`, `Inside_B` and `Actor1`, which are not in `good/`): a negative control that proves the overlay can matter.
Pictures: `interim/try/2026-11-05-real-game-scene/`, `2026-11-05-level-yard/`.

## 4. Findings the real test produced (fixed or open)
- FIXED: restyled grass was far noisier than the original (now `tearit-green-calm`, in `good/`).
- FIXED: floor sheets (`*_A5`) showed dark seam lines between tiles at 6 tone levels; they now use 14 levels and no outline (`sheets.pdl` rule, harness case `seamless-floors`). Verified in the real room (one picture opened); not yet applied to approved files because no A5 sheet is in `good/` yet.
- OPEN: voxel people are darker and smaller than the RPG Maker ones and have no hand-drawn faces; side views are thin; Time Twin echo is not drawn; the 3D board does not read voxel models.
- OPEN: Venus theme is nearly one yellow-green colour; Hydroverse battleback is flat blue (widen the ramps before approving a theme).
- OPEN: one flat white A2 block (Outside_A2 block 24) is a dull purple square.

## 5. Next steps (each can be a separate agent)
1. Approve more files by opening them: the A5 floors (retoned), Inside_B, the other people and face sheets, then pin them in `APPROVED.sha256`.
2. First NEW level built on Tear-it art only: a level file for `generator_level_op` per world theme, then a real book for a petslike game that reads a Tear-it root (needs a per-game asset-root setting; the core finds `#.NNEST_ASSETS/rmmv-www-img` by walking up from the app, so a new game needs its own root key: design-choice test first).
3. Voxel: attack and mine poses on the lore cast with real looks, left/right views as exact mirrors, an export to the 3D board piece format, building variants per world theme.
4. Content not done: side-view actors and enemies, big monsters, vehicle sheet, A4 walls (16-shape table) and A1 water tables, caps and a growth-standard row for `generator-hq/out/` and `interim/`, a `cpu_gate --wait` option, real looks for Harold and the pets (owner supplies).
5. Awaiting owner: which `0.good` pictures they liked; written approval of the pattern (for stricter `good/` use); canon for DOC colours.

## 6. Update: D30 step 1 done (2026-11-05, late)
The humanoid body is now data: `@.apps/generator-hq/events/gen_voxel/partsets.pdl` (`PS | set | part | x | y | z | w | d | h | colour | when`, `PP | set | pose | part | dx | dy | dz`). `generator_voxel_op fromcast <cast.pdl> <out.pdl> <partsets.pdl>` reads it; the old C shapes are gone (one authority). **OBS:** generated recipes are byte-identical to before (126 models, 3,521 lines); harness `generator_voxel` pins the sha256 and has negatives (edited row changes output, unknown variable or condition, empty or missing file refused): 33/0. To add a hairstyle, beard or armor piece: add `PS` rows with a `when` condition, no C. Still in C: the five creature kinds (D30 step 5). Next: step 2 seed and knobs plus `STYLE` rows; then equipment slots with `TRAIT` rows (RPGAtlas note section 5).

## 7. Update: D30 step 2 done (seed and knobs)
Make a character from a seed: add `CHAR | hero_a | 42 | preset=heroic skin=#8a5a3a` to a characters file and run `generator_voxel_op fromchars <file> <out.pdl> events/gen_voxel/partsets.pdl`; then `walksheet`, `render`, `icons` as before. Sample: `events/gen_voxel/characters.pdl` (4 STYLE presets, 16 characters); pictures in `interim/try/2026-11-05-seed-knobs/`. Same seed and knobs give the same bytes (harness pins it). Presets: chunky, slender, heroic, grim (DOC looks). To add a knob: add a variable to `ev()` and the knob table in `fromchars`, then use it in `PS` rows. **Harness lesson:** patterns with ` | ` are split by the harness; use `..` wildcards and `EXPECT_OUT_IS` (I found five weak checks this way and fixed them). Voxel 49/0, sheets 55/0, level 7/0. Next: step 3 equipment slots and `TRAIT` rows.

## 8. Update: D30 step 3 done (equipment)
Dress a character: `CHAR | knight | 501 | preset=heroic equip=iron_helm,plate_body,wooden_shield,bronze_sword`. Add an item: `EQ` rows in `events/gen_voxel/partsets.pdl` (slot, db item name or `-`, part boxes with body expressions, `follow` hand). `generator_voxel_op check-equip partsets.pdl <house>/&.widgits/db-hq/data` must say `missing=0`. Stats are NOT here: they live in db-hq (one authority). Pictures: `interim/try/2026-11-05-equipment/`. Voxel suite 66/0. **Lessons:** (1) rebuild, then re-check both older sha256 pins; a build warning about a wrong buffer size was a real overflow. (2) A zero-count check passes vacuously if the file it counts was never made: add an explicit success check for each generated file. Next: step 4 (2D projection with a pixel pass and the quality gate against the flat cast op), then step 5 (creatures as data: add serpent and ghost plans).

## 9. Update: D30 step 4 gate failed; D31 new art, not recolours (2026-11-05, night)
1. **2D gate:** the voxel projection does not match the flat cast sheets (boxy heads, thin side views, flat-cap hair), so the flat op stays the finisher for RPG Maker 2D sheets; both read the same spec. Pictures and verdict: `interim/try/2026-11-05-quality-gate/` (NOTE.md). New tools: `generator_level_op zoom` (nearest-neighbour enlarge, optional crop), `generator_tile_op compare-png` (changed share, mean colour shift, luminance correlation), `fromchars face=1`, trailing `face` and `pixel` flags.
2. **Owner correction (SAID): "we didn't want to just change the color, but the actual images art meant to be displayed. art style of same things is fine."** Measured (OBS): the six tile files in `good/` are repaints (Outside_B correlation 0.903 with the original, the item icons are the same shapes; Grassland 0.989; A2 sheets 0.67 to 0.81). They are now labelled interim placeholders in `good/APPROVED.md` and must be replaced. Decision D31 (proposed) in the decision log: role catalog per cell, objects and items drawn from voxel recipes, ground textures generated from scratch into the existing 48-shape autotile table, acceptance rule `compare-png` correlation below about 0.5 AND a person judging it readable. The character sheets in `good/` are new art (generated from part recipes) and stand.
3. **Next:** (a) role catalog for Outside_B (`CELL | sheet | col | row | thing`), (b) render the catalogued things from voxel recipes plus new prop recipes, (c) from-scratch ground textures for Outside_A2 grass, sand, stone, water, (d) gate with `compare-png`. Also open: flat op accepting the shared knobs; creatures as data (step 5).

## 10. Update: D31 first slice (new art of the same things)
Run it: `generator_voxel_op objsheet <out.png> events/gen_objects/outside_b.pdl events/gen_voxel/props.pdl` (writes a 768x768 sheet with 25 things in Outside_B's cell positions; other cells empty). Add a thing: a model in `props.pdl` (`CYL`, `ELL`, `PART` rows) and a `CELL` row in `outside_b.pdl`. Judge it: `generator_tile_op compare-png <original sheet> <ours>`: correlation must be low (target < 0.5) AND a person must read it. Pictures: `interim/try/2026-11-05-new-art-outside-b/` (original left, ours right, 2x). Output sheet: `interim/objects/tilesets/Outside_B.png` (gitignored, rebuilt in a second). Result: whole-slice correlation 0.141 (old recolour 0.903), readable but plainer than the original; polish pass is the next task. Role catalog covers only the lower-left props; the shop signs (rows 0-1), walls, windows, doors (rows 2-7), plants, snow (right half), tents, lamps and ladders still need `CELL` rows and recipes.
