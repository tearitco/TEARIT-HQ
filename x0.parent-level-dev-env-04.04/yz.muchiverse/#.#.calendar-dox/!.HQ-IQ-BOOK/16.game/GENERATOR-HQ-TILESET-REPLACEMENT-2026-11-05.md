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

## First build, when the owner says go

1. `roles.pdl` for Outside only, the nine roles above.
2. The C op that reads a GEN block and writes the nine folders. Determinism case.
3. Palettes publish for the existing `generate` category, pointing at that seed folder.
4. One scratch page, one placed grass cell, screenshot, then stop.
