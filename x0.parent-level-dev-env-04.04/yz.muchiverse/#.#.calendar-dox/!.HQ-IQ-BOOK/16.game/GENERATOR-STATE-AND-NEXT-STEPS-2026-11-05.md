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
