# generator-hq: replace RPG Maker tiles one sheet at a time

2026-11-05. Owner: the first task is not a new game. Stop using RPG Maker assets. Replace them with our own tiles, one tileset at a time. Design an in-house generator with input options so later runs can make more variants, and wire that generator into palettes as generator-hq. This page is the plan. Nothing here is built.

Public wording stays "RPG Maker-style." The art does not stay theirs. Imported maps may still name the old slots. The pixels in those slots become ours.

## What is already there (READ)

- Palettes menu `&.widgits/palettes/pallets.pdl` has `CATEGORY | rmmv | RPG Maker Tiles | rmmv` and `CATEGORY | generate | Generate | generate`. A category with no chooser opens the stub window.
- `rmmv_options.txt` is the sheet list a person sees: tilesets Dungeon, Inside, Outside, SF_Inside, SF_Outside, World, plus characters, faces, sv_actors, sv_enemies, battlebacks, parallaxes, animations, system, titles. The active tileset row on that file is `SF_Outside`, category `b`.
- `tilesets/tileset_registry.pdl` names the RTP sheets (`Outside_A2.png`, `World_B.png`, `SF_Outside_B.png`, and the rest) under `rmmv-www-img/tilesets`.
- A palette cell the board can arm is a folder `sprites/<id>/.../sprite.csv`, not a raw PNG. CDDA, DF, and Doom already publish that way. The generator must write `sprite.csv` (and a PNG beside it for the eye). A PNG-only folder will not show up in the grid.
- DF, Doom, CDDA, Mineclonia, OHR, and Tiled are separate palettes. This plan does not replace those. It replaces the `rmmv` sheets only.

## Rule

One tileset per pass. A pass is done when every role in that sheet's map is drawn from `sprites/gen/<sheet>/`, the palettes row for that sheet no longer points at the RTP file, and the same seed run twice writes the same bytes. The next sheet does not start until that proof exists. Characters, faces, and battlebacks are later passes. They are not tilesets.

## Order of sheets

Match the names already in `rmmv_options.txt`, ground first:

1. `Outside` (the town ground: grass, dirt, path, water edge, wall top).
2. `SF_Outside` (the active sheet today; same roles, a second palette, not a second generator).
3. `Inside` (floor, wall, window, door).
4. `Dungeon`.
5. `World` (overworld, coarser).
6. Actors and faces only after the five ground sheets no longer load RTP.

`A1` autotiles (water, deep water, waterfall) are the second half of Outside, not a separate product. If the autotile edge is wrong, the sheet is not done.

## Role map, not a pixel clone

Each RTP slot we still use gets one role id. The generator fills the role. It does not trace the RTP PNG.

First Outside role list, and no others until these read:

| Role | What it has to do |
| --- | --- |
| `grass` | walkable ground, one flat color plus a 1 px noise |
| `dirt` | walkable, distinct from grass at 16 px |
| `path` | walkable, lighter than dirt |
| `water` | not walkable, reads as liquid |
| `water_edge` | the autotile neighbor of water and grass |
| `wall` | blocks, darker than grass, a top edge |
| `door` | a wall cell a passage can mark |
| `tree` | blocks, a canopy on a trunk |
| `roof` | sits above a wall, a third color |

A book that asked for `Outside_B` tile 12 now asks for role `wall`. The old index stays in a map file `roles.pdl` so existing maps do not have to be repainted by hand:

```
ROLE | sheet | old_slot | role
ROLE | Outside | B:12 | wall
```

`old_slot` is the RTP name we are retiring. New maps write the role only.

## generator-hq

App: `@.apps/generator-hq`. It is not a second palette program. It writes a folder the existing palettes publisher can list.

One run:

```
GEN | sheet | Outside
GEN | seed | 20261105
GEN | cell | 16
GEN | roles | grass,dirt,path,water,water_edge,wall,door,tree,roof
GEN | palette | #3a6b32,#6b4a2a,#c2b48a,#2a5a8a,#4a4a4a,#1e3a16,#8a3a2a
GEN | style | flat
GEN | outline | 1
```

`style` starts as `flat`. Later values (`dither`, `soft`, `night`) are new rows, not new programs. `cell` is 16 or 32. 48 waits until 16 has a proof.

Output, per role:

```
@.apps/generator-hq/out/<sheet>/<seed>/<role>/sprite.csv
@.apps/generator-hq/out/<sheet>/<seed>/<role>/tile.png
```

The op is C. The case is pal. No Python and no new shell script. Same seed and same GEN rows must produce the same `sprite.csv` bytes. One case flips the seed and must fail that compare. One case omits a role and must refuse to publish.

Options the window exposes, and no others in the first slice: sheet, seed, cell size, the role list, the palette (one hex per role), style, outline on or off. A freeform prompt is not an input. More options are new GEN keys after the first sheet ships.

## Wiring into palettes

Use the row that is already in `pallets.pdl`: `generate` / label `Generate`. Do not add a second category called generator-hq. The app name is generator-hq. The menu label can stay Generate.

The chooser follows the DF pattern: a window `palettes-generate.xhtpm`, a whitelist entry, `publish` that reads `generate_active.txt` and writes `palettes-generate_state.txt` with one row per role (`glyph`, `label`, `sprite_dir`). Arming writes `state/generate_brush.txt`. The brush the editor already places is that `sprite.csv`.

`rmmv` stays in the menu until the last sheet in the order above is swapped, so a half-finished map can still be opened. A sheet that has passed its proof is not selectable as RTP: `rmmv_options.txt` drops that TILESET line. The Generate row shows it instead.

## Proof for one sheet

1. Run the generator twice on seed `20261105`. Hash the nine `sprite.csv` files. They match.
2. Publish Generate. The grid shows nine cells and no RTP path in those sprite dirs.
3. Arm `grass` and place one cell on a scratch page. The page's tile id is the role, or `roles.pdl` maps it. The PNG on disk is the generated one.
4. Open the old Outside sheet in the rmmv chooser and confirm that TILESET line is gone.

That is the pass. Inside is the next pass, with its own role list, written down before the run.

## What this is not

Not a rewrite of the raymarch. Not a new atlas format. Not a replacement for CDDA, DF, Doom, or Mineclonia art. Not an image model in the house. Not a batch that regenerates every sheet in one night. Characters and faces stay on the RTP until the ground sheets are done, and they get their own role list when their pass starts.

## Rejected, 2026-11-05

The catalog that shipped (`generator_tile_op`) is flat fills, a little noise, and a few shapes. The owner looked at the sheet PNGs and rejected them. They are not a stand-in for an RPG Maker sheet. More roles drawn the same way will not fix it.

What an Outside grass cell actually has, and this painter does not: a light direction, two or three shades of the same green, an outline, blades or pebbles that repeat on a 16 px grid, and an edge that meets the next cell. Water is an autotile (corners and sides), not one blue square. A character is a shaded body with a face, not an oval.

The model does not draw the cell. A shape comes from the original sheet.

That is the rule for anything with an outline: a rock, a tree, a roof, a door, a character, a water edge, an autotile corner. Copy that cell. Do not invent a new silhouette in the painter, and do not ask a model to invent one. A flat fill is the only thing a generator may still color, and even grass already has a real cell, so the fill is not the art.

Grass and rock use the original RPG Maker Outside sheet. The sliced cells are already in `palettes/sprites/rmmv/Outside_a2` and `Outside_b`. `events/gen_outside_rtp/roles.pdl` names `Outside_a2/001` as grass and `Outside_b/165` as a rock. Those two were opened: a full grass cell, and a grey boulder cluster. `Outside_a2/008`, `Outside_b/157`, and `Outside_b/164` were opened and dropped. The first two are fragments. `164` is a stone ring around a yellow center. `fromcsv` writes a viewable PNG from that `sprite.csv`. A person paints a shape only when the original sheet has no cell for it.

A person makes the 16 px tile (or picks one we already have the right to use). The same `sprite.csv` and `sheet.png` path still publishes it. The model's job is classification only: given that picture and the role list, it writes the label (`grass`, `water_edge`, `wall`, which sheet, whether the cell is a side or a corner). A wrong label is the thing a person corrects. The word bank is the list of allowed labels, not a prompt for a picture.

The October diffusion trial and the grass draft of 2026-11-05 (`0.grass-draft-512.png`, a map with a grid and blobs, 117 s on debil) are examples of the model drawing a scene. They are not the art path. Do not run another draft to get a better grass cell.

The way to that detail is already written, and it is not a bigger C painter.

- `16.game/WORD-BANK-TO-IMAGE.md`. W0 is built: word rows and `img_prompt` turn a part into a prompt. W1 is a model that drafts a PNG off this machine. W2 is a small in-house model trained only on drafts a person accepted. tomom is that student, later. It is not the teacher, and it is not wired.
- `16.game/GAMES-DECISIONS-2026-10-13.md` section 8: Stable Diffusion, hand-tuned with word banks and concept banks, workers where we have them, a person as the judge. Nothing is generated while a game runs.
- The one trial that was run (2026-10-13, debil, CPU, tiny SD, about two minutes a picture) drew cars, not a door, and not pixel art. The bank words were the thing to change next. No larger model was tried. This machine has no GPU and, at that reading, no weights.

The next Outside shapes are more original cells, each opened before it is named. A missing shape is hand-painted. generator-hq publishes the cell. It does not become a neural-network runtime.

## Algorithmic diffusion, 2026-11-05

Owner: we can diffuse using the RPG Maker tilesets, algorithmically.

The sheet is the only source of pixels. A new cell is a rearrangement of a named cell, not a picture invented from words. The outline stays the outline of that cell. Colors in the new cell are colors that already occur in it. An edge is quilted from that cell's own edge, which is how an autotile grows without a new shape. Grass `Outside_a2/001` and the boulder `Outside_b/165` are the first two sources. The same rule covers a tree, a roof, a door, and a character.

What this word does not mean: the Stable Diffusion draft. That model drew a scene. It is not rerun. Classification still only writes the label. A person corrects a wrong label and paints a shape only when the sheet has no cell for it.

The proof, when this verb is built, is a harness case: every output pixel's color exists in the source `sprite.csv`, the same seed matches byte for byte, and a source with one color cannot grow a second color. Decision log D24.

## Good and bad stay in different directories

Owner, 2026-11-05: the bad versions and the good ones must not sit in one folder.

View copies, already split:

- Good: `XO/31.Nov/nov-5/1.generator-sheets/0.good/` holds `0.rmmv-grass.png` (`Outside_a2/001`) and `0.rmmv-rock.png` (`Outside_b/165`).
- Bad: `XO/31.Nov/nov-5/1.generator-sheets/0.bad/` holds the flat catalog sheets (`0.Outside.png` and the other thirteen) and the rejected grass draft (`0.grass-draft-512.png`, `0.grass-draft-16.png`).

A later run writes the same way. The painter and any draft land in `out/bad/<sheet>/<seed>/`. A cell moves to `out/good/<sheet>/<role>/` only after it is a named source cell, or an algorithmic rearrangement of one, and a person has kept it. `roles.pdl` names the good cells. It does not name anything under `out/bad`.

## Tiling-compliant sheets, 2026-11-05

Owner: the generator does make sheets that tile, and the way it runs is an op called from an event page.

A ground sheet is tiling-compliant when each placed cell is one of 48 shapes, and each shape is four quadrants taken from the source block. That table already exists. It is `FLOOR_AUTOTILE_TABLE` in `_.monads/_.livedesk-taskbar/ops/tile_autotile.c`, the real MV floor table, 48 rows, checked against a real `World_A2` sheet. Walls use the 16-row table in the same file. The generator does not grow a second copy of those numbers.

The op is still `generator_tile_op`. A new verb reads one named source block (the good cell's autotile block, not a flat fill) and writes the 48 shapes with that table. The event page is the editor's button, the same shape as `events/gen_characters/event.pal`: one `exec` of the op, then `halt`. There is no plugin list. A second program would be a second place that knows how a sheet tiles.

Output of that verb goes to `out/bad/` until the edge check passes and a person keeps it, then `out/good/`. The check is: a shape's quadrant pixels equal the source block at the table's coordinates. Decision log D25.

Built 2026-11-05: `generator_tile_op shapes` and `shapes-check`. The table is still only in `tile_autotile.c` (`tile_autotile_blits`). The first source is an in-memory proof block, 96×144, each 24×24 half a flat color, so a wrong quadrant cannot hide. `shapes-check` exits 0 only when all 48 shapes match. `shapes <outdir>` writes `s00`–`s47` plus `sheet.png`. The editor page is `events/gen_floor_shapes/event.pal`. Harness `generator_tiles.pal` verdict after that case: `VERDICT|PASS|passed=37|failed=0`. The written proof sheet is in `out/bad/floor-proof/` and the view copy is `XO/31.Nov/nov-5/1.generator-sheets/0.bad/0.floor-shapes-proof.png`. It is a color code, not grass, and it stays in bad.

Not done: reading a real 96×144 RPG Maker block (no `World_A2.png` under `palettes` on this pass), pointing one live map at the sheet, or a 3D frame of that map. The quality bar for calling a sheet Tear-it is `16.game/TEAR-IT-TILES-100-2026-11-05.md`.

## First build, when the owner says go

1. `roles.pdl` for Outside only, the nine roles above.
2. The C op that reads a GEN block and writes the nine folders. Determinism case.
3. Palettes publish for the existing `generate` category, pointing at that seed folder.
4. One scratch page, one placed grass cell, screenshot, then stop.
