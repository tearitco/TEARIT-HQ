# RMMV tiling and window-skin primer — 2026-10-09

Written for the co-lab ask in session `1791508660`. Branch `grok`
only. No renderer change. The earlier note
`TILED-BARS-FROM-RMMV-SETTINGS.md` (same folder, commit `3cef03fe6`)
is corrected below where it said to stretch one tile.

Sources read this session: `TILE-SYSTEM-DESIGN.md` §§1–3 and §6,
the header and tables of
`_.monads/_.livedesk-taskbar/ops/tile_autotile.c`.
`Window.png` and `rpg_core.js` were not opened here. Anything that
comes only from the usual MV layout is marked **unsure**.

## Reply, in short

The owner wants the taskbar and the x11-hq text bars to be able to
look like RPG Maker rectangle bars, chosen and turned off again from
HQ settings, strip item 22. Off must leave today's solid bars alone.

Sonnet's reading of the follow-up is the one to build: a bar is a
row of whole blocks. Left cap, repeated middle, right cap. Width
changes by adding or removing a middle block. The art is not
stretched to whatever pixel width the label happens to be. Text sits
in the inner blocks.

A single `sprite.csv` stretched to the element, which is what
`TILED-BARS-FROM-RMMV-SETTINGS.md` specified, is the wrong paint.
The settings switch (empty means off, edited only from item 22) can
stay. The value should name a small skin (cap, middle, and for a
taller window the top, bottom, and fill), not one crop.

Which exact cells look like a bar is **unsure** until someone looks
at a sheet. Do not treat `SF_Outside_c/210` as a cap. That path was
only an example of where crops live. Ground autotiles (A2 grass)
are the wrong art for a UI bar. The right source is either
`Window.png`'s frame region, or a plain B–E tile that is already
drawn as a horizontal rail. This house's palettes crops today are
48px cells from tileset sheets. A `Window.png` was not found in the
search this session.

## (a) Tile size and sheets

MV's tile is **48×48**. This house's desk cell is **80** reference
px (`GRID | cell_px`, default 80, `TILE-SYSTEM-DESIGN.md` §1.1 and
§6 step 1). The design already decided to scale the 48px source up
to the cell so edges meet. It rejected changing the desk grid to 48,
because that grid is also the entity grid.

**Checked in `tile_autotile.c`:** half of 48 is 24. `autotile_compute_blits`
places four 24×24 quads at (0,0), (24,0), (0,24), (24,24). The
file's own test asserts that. I did not re-run the test.

Sheet roles, from `TILE-SYSTEM-DESIGN.md` §2 and §6, plus the file
header:

| Sheet | Role in this house's notes | Table |
| --- | --- | --- |
| A1 | water / waterfall, animated | floor 48, and the 4-row waterfall table |
| A2 | ground | floor 48 |
| A3 | roof / wall-top | wall 16 |
| A4 | wall side | wall 16 |
| A5 | floor-like, no animation | floor 48 |
| B–E | plain tiles, one cell one picture | none |

A floor autotile block is **2 columns × 3 rows** of 48px cells
(96×144). The picker must show one thumbnail per block, not all six
raw cells. The other five are fragments. That is §6 step 5, and it
matches how the tables index quarter-pieces. **Unsure:** the exact
pixel origin of each block on a full 768-wide sheet was not
re-measured here. The design doc's §1 still says "47-cell block" in
the manifest example. §2 corrects that count to 48 shapes. The
48 is shapes, not 48 atlas cells. The atlas block is 6 cells.

## (b) How a shape is chosen

One drawn autotile is four quadrant blits, not one pre-drawn 48×48
variant. `FLOOR_AUTOTILE_TABLE` has 48 rows, `WALL_AUTOTILE_TABLE`
16, `WATERFALL_AUTOTILE_TABLE` 4. I counted the rows in the C file.
The file says those arrays were copied from `rpg_core.js` and that
rows 0, 15, 23, 31, 39, and 47 were drawn against `World_A2.png`
and looked right. I did not open that PNG.

Neighbor bits (`autotile_effective_mask`): N E S W, then NE SE SW NW.
A corner bit counts only when both of its edges are set. That
function is in the file and matches §2.1.

`autotile_pick_quadrant` maps one corner to five states: isolated,
edge A, edge B, inner corner, flat. **The file header says this
mapping is not visually verified, and it does not yet turn those
five states into a row index in the 48-table.** §6 step 3 says the
same gap. MV's runtime does not compute the shape from neighbors.
The editor bakes a shape id into the tile id (`TILE-SYSTEM-DESIGN.md`
§2.1). So this house still has to choose its own bit-to-row map.
I did not check that claim against `rpg_core.js` this session. I
am taking the design doc's word for it.

A2 ground, A3 wall tops, and A4 wall sides are different tables.
Using the floor table on a wall block will pick the wrong quarters.
**Unsure:** which A1 columns use the waterfall table of 4 versus the
floor table of 48. The file header says both, and does not list the
column split.

## (c) Animated tiles

§3: an `ANIMATED` entry has `frames` and `fps`. In play, the cell is
`base + (elapsed_ms / (1000/fps)) % frames`. The editor shows frame 0.
A tile can be both autotile and animated. The smallest scope in that
doc is animation on plain tiles only.

**Standard MV, not re-read here:** A1 autotiles carry three frames
side by side, and the engine steps them on its own timer. Waterfalls
use the 4-shape table and also animate. I did not confirm the frame
count against a sheet in this tree.

## (d) Window.png, and why it is not the same as a block bar

**Standard MV, unsure until `rpg_windows` / `rpg_core.js` Window is
re-read.** The usual `Window.png` is 192×192:

- The top-left 96×96 is the frame.
- Frame margin is 24. Four 24×24 corners stay 24×24.
- The top and bottom edge pieces are 48×24 and are **stretched** to
  the window width. The side pieces are stretched in height.
- The background pattern is a 96×96 region (commonly at y=96) and is
  **repeated** in 96px steps, not stretched.
- A cursor graphic and a pause graphic live in the other half.

So MV's own window frame does stretch the edges. It does not snap
the window to a tile grid. That is the closest MV widget to a
taskbar, and it is **not** what the owner asked for once the
follow-up is included.

What to build instead, still using MV-sized art:

- One block is one 48px cell, drawn at the desk cell size (80, or
  `desk_grid.pdl`) times the same UI scale the bars already use.
- A one-row bar of N blocks is N copies, edge to edge, no scale-to-fit
  on the individual cell.
- Left cap is block 0. Right cap is block N-1. Every block between
  them is the middle tile, repeated.
- A taller window is three rows of that: top caps and top edge,
  side tiles plus a fill tile, bottom caps and bottom edge. The fill
  repeats. It is not the MV stretch.

Text is drawn after the tiles, inset by one cap so the glyphs sit in
the middle blocks. If the label needs more room, add middle blocks.
Do not widen a block by a few pixels.

## (e) What snaps

Snaps to the block grid: bar width, bar height, and the position of
each tile inside the bar.

Does not snap: the label string, the font, the existing window's
outer pixel position on the X11 desktop, opacity, and the solid-fill
path used when the setting is off.

Menu structure, relay codes, and which rows exist do not change.

## 3 blocks and 7 blocks

Block width `B` is the drawn cell (48 scaled up to the desk cell,
then by UI scale). Height of a taskbar or menu row is `B` for a
one-row skin.

- 3 blocks: `[left cap][middle][right cap]`. Inner text width is one
  block. A short label ("hq", "menu") fits. A long label does not.
  Add blocks rather than shrinking the font into the caps.
- 7 blocks: `[left cap][middle ×5][right cap]`. Inner text width is
  five blocks. Same cap art, five copies of the same middle cell.

The two widths differ by `4 * B` and by nothing else. There is no
half block.

## Settings

Still only HQ item 22 (`livedesk:open-settings`). Proposed key in
`#.desktop/hq_ui.pdl`, because that file already live-reloads:

```
bar_skin=
```

Empty is off and is the rollback. A non-empty value names a skin
file that lists the cap, middle, and optional edge and fill paths
under `&.widgits/palettes/sprites/rmmv/`. Picking the skin is a
later step, from that same window, and only after a real sheet has
been looked at.

## What I did not check

- I did not re-run `test_tile_autotile`.
- I did not open `World_A2.png` or any crop PNG.
- I did not re-read `rpg_core.js`, so the "verbatim port" claim and
  the Window.png rectangle numbers stay second-hand.
- I did not read `DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md` line by line.
  The dock's current pixel layout is out of this primer. The bar
  skin has to sit in whatever rectangle that layout already assigned,
  quantized to whole blocks, or stay on the solid fill if the
  rectangle is not a whole number of blocks. That last rule is a
  proposal, not something the dock doc says.
