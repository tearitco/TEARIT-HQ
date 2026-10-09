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

## The look is the whole MV window, including the font

Owner, 2026-10-09: the bars are not a tile skin dropped on the current
house type. When the setting is on, the taskbar, the x11-hq menus, and
the other screens that use those bars should look as if they were built
from RPG Maker MV assets. That includes the font.

Checked in `rpg_windows.js` (`Window_Base`, same MV tree as the core
file above):

| What | MV value |
| --- | --- |
| `standardFontFace` | `GameFont` (SimHei / Heiti TC when the system is Chinese, Dotum / AppleGothic when Korean) |
| `standardFontSize` | 28 |
| `lineHeight` | 36 |
| `standardPadding` | 18 |
| `textPadding` | 6 |
| `standardBackOpacity` | 192 |

`GameFont` is the face MV ships as `fonts/gamefont.css` pointing at
`GameFont.ttf` (the RTP mplus face). **Unsure:** that css and the ttf
were not opened in this pass. The numbers above were read from the JS.

28px is the MV base, not a fixed house size. Owner, same day: the
settings size control scales it up and down with the other windows.
Drawn size is `28 * font_scale` (the `hq_ui.pdl` `font_scale` the
size buttons already write). Line height scales the same way from 36.
Off still restores the house family and the house scale.

`gamefont.css` in the MV `www/fonts/` tree maps the family name
`GameFont` to `mplus-1m-regular.ttf`. The file is about 1.5 MB. It
was not copied into this repo.

The draw path does not open a TTF by path. `khtpm_draw_core.c` and
`khtpm_core_render.c` call `XftFontOpenName` with a fontconfig family
string plus `pixelsize`. `UI_FONT_FAMILY_NEXT` only cycles the eight
names in `g_font_family_choices` (DejaVu Sans through DSEG14 Classic).
There is no `stbtt_` and no `FcConfigAppFontAddFile` in those files.

Smallest change that can show this face: register the ttf with
fontconfig for the process (`FcConfigAppFontAddFile` on the existing
config, then `XftFontOpenName` of `GameFont`), and add that name to
the cycle only while `bar_skin` is on. Copying the file into git is
not part of that change.

Licence: no license file sits next to this `gamefont.css`. Upstream
M+ is distributed under the M+ FONT LICENSE, which allows
redistribution if the license text stays with the font. **Unsure**
until that license file is read from the M+ distribution itself. Do
not commit the ttf before that.

A 36px line fits a bar block drawn at the desk cell (80px, then UI
scale). It does not fit a raw 48px cell once 18px of window padding
is added on both sides. The bar uses the scaled desk cell, not the
raw 48. A long label adds middle blocks. It does not shrink the font
below the scaled 28.

When `bar_skin` is set, labels inside those bars use that face at the
scaled 28px,
with 6px of text padding inside the 18px window padding, and a line
height of 36. The block grid still decides the bar size. The font does
not stretch the tiles. A label that does not fit grows the bar by whole
middle blocks, or clips, once the owner picks which. It does not switch
to the house UI font to squeeze in.

When `bar_skin` is empty, the house font (`hq_ui.pdl` `font_family` and
`font_scale`) stays as it is. Item 22 is still the only switch.

This is a skin for bars and their labels. It is not a port of MV's
`Window_Menu` command list, gold window, or scene stack.

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

## Checked — 2026-10-09, after the co-lab reply

Opened:

- `Window.png` at `NNEST-12.00/#.NNEST_ASSETS/rmmv-www-img/system/Window.png`.
  Pillow reports **192×192 RGBA**.
- `rpg_core.js` from the local MV tree
  `rpg-maker-mv-og-mt/js/rpg_core.js`. Line numbers below are that file.

### Window frame and fill

`Window.prototype._refreshFrame` (line 6696): margin `m = 24`, `p = 96`.
The frame is the **top-right** 96×96, origin x=96, not the top-left.
The earlier primer had that rectangle on the wrong half.

Each `blt` stretches the source to the destination size:

- Top edge source `(120, 0)` size `48×24`, drawn to width `w-48`, height 24.
- Bottom edge source `(120, 72)` size `48×24`, drawn along the bottom.
- Side edges source width 24, height 48, drawn to height `h-48`.
- Four corners stay `24×24`: `(96,0)`, `(168,0)`, `(96,72)`, `(168,72)`.

`_refreshBack` (line 6669): the back is inset by `this._margin`.
First it **stretches** `(0,0,96,96)` across the whole back. Then it
**tiles** `(0,96,96,96)` in 96px steps over that. Then it applies
`_colorTone`.

Sampled pixels on this `Window.png`: the top-edge row at y=4 is black
`(0,0,0)` across the 48px width. y=12 is one white pixel then black.
The corner at `(96,0)` is transparent. This skin is a thin border, not
a filled bar.

### Can that frame be a 48px cap and middle?

No. The pieces are 24px corners and a 48×24 edge, and the engine
stretches the edge. A 48×48 block cut out of that region mixes the
border with empty black. Repeating it will not make a solid rectangle
bar. A plain B–E tile that is already a horizontal rail is the better
source. Which cell that is remains unpicked. I did not open a B–E sheet
in this pass.

### A1 frames and the column split

`Tilemap.prototype.update` (line 4703): `animationCount++`, then
`animationFrame = floor(animationCount / 30)`. One frame step per 30
tilemap updates.

`_drawAutotile` (line 5040), when `isTileA1`:

- Water uses `waterSurfaceIndex = [0,1,2,1][animationFrame % 4]`.
  That is **three pictures**, ping-pong, not four distinct frames.
  Kinds 0 and 1, and even kinds in the later block, step `bx` by
  `waterSurfaceIndex * 2`.
- Waterfall is the odd kind in that later block (`kind % 2 === 1`):
  table switches to `WATERFALL_AUTOTILE_TABLE`, and `by` adds
  `animationFrame % 3`. **Three frames.**
- `isWaterfallTile` (line 5340) is true only for tile ids in
  `[TILE_ID_A1+192, TILE_ID_A2)` whose autotile kind is odd.

A2 (line 5078): floor table, `bx = tx*2`, `by = (ty-2)*3`.
A3 (line 5083): **always** `WALL_AUTOTILE_TABLE`, block height 2 tiles
(`by = (ty-6)*2`).
A4 (line 5088): wall table only when `ty % 2 === 1`. Otherwise it
keeps the floor table. So "A3 and A4 both use the 16-row wall table"
was too coarse. Wall tops on A4 (`isWallTopTile`, kind `% 16 < 8`)
are floor-type. `isFloorTypeAutotile` (line 5373) says the same:
A1 that is not a waterfall, A2, and A4 wall tops.

The first floor-table row in `rpg_core.js` line 5389 matches the first
row of `FLOOR_AUTOTILE_TABLE` in `tile_autotile.c`. I compared that
one row only, not all 48.

### tile_autotile.c claims, now that the JS was open

- 48 / 16 / 4 table sizes: the JS names those three tables. I did not
  recount every JS row against every C row.
- Quadrant blit `(bx*2+qsx)*w1` and dest `(i%2)*w1`, `(i/2)*h1` match
  `_drawAutotile` lines 5101–5109.
- The C file still does not implement the A1 water index, the A1
  waterfall `by` shift, or the A4 even/odd table switch. Those stay
  outside what the C port claims to draw.
- `autotile_pick_quadrant` is still not in this JS. The runtime reads
  `getAutotileShape(tileId)`. The neighbor-to-shape gap stands.

## Rail candidates — 2026-10-09

Looked at the B and C sheets under
`#.NNEST_ASSETS/rmmv-www-img/tilesets` (768×768, 16×16 cells of 48px).
A contact sheet of the four pairs is `/tmp/rmmv-rail-candidates.png`.
It is not in git.

Columns are 0-based. Crop id, if the palette uses `row * 16 + col`, is
in parentheses. Those `Dungeon_b` files exist. I did not re-decode the
`sprite.csv` pixels, so the id match is the numbering assumption only.
`SF_Outside_B` is not under `&.widgits/palettes/sprites/rmmv/`.

| Pair | Left | Middle | Right | Why |
| --- | --- | --- | --- | --- |
| Wood rail, `Dungeon_B` | 0,14 (224) | 1,14 (225) | 4,14 (228) | A post closes each end. The middle is a plain plank that can repeat. |
| Iron rail, `Dungeon_B` | 11,14 (235) | 12,14 (236) | 15,14 (239) | Same shape in metal. Middle bars repeat. Ends close the run. |
| Neon, `SF_Outside_B` | 8,5 | 9,5 | mirror of 8,5 | A thin glowing tube. There is no separate right-cap cell. |
| White panel, `SF_Outside_B` | 8,8 | 9,8 | 8,8 | A low white rectangle. The end cell has an oval fixture, so it is the weakest bar. |

The wood rail is the one that already has a real left, a repeatable
middle, and a real right. The owner picks. Nothing is wired.

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
