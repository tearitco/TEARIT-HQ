# EDIT-ISLANDS-DESIGN — the floating-island town for rpg-edit-hq (paper design, 2026-10-21)

**Status: design only. Nothing of this is built.** The owner's picture ("centroid", a screenshot, no file on disk: no book TON / page centroid-u exists
anywhere in the house, searched 2026-10-21) shows floating grass islands with dirt cliffs, pines, a pond, wooden bridges and a cabin. This is an
approximation drawn from that picture, one island at a time. Owner rule for the buildings: **each island has exactly one building, and the five
buildings are the five teleport-door buildings the usual pet boards have** (mcl-hq `rooms.pdl`: castle/Cabin, bank, store, school, hotel).

## 1. The grid (24 x 17, x right, y down)

Legend: `G` grass (floor height 1) · `p` sand path · `T` pine (Outside_B, blocks) · `B` building 2x2 (World_B, blocks) · `D` its door (teleport, the trigger
is this floor cell) · `~` pond water (blocks) · `b` wooden bridge (walkable, floor height 1, no cliff under it) · `s` wooden ramp (decor) ·
`F` campfire (decor) · `S` sign (decor) · `r` floating rubble (decor, not walkable, floats at height 1) · `.` sky (no floor at all; the 3D view shows the
sky, never the grey void of the picture).

```
   012345678901234567890123
 0 ........................
 1 ..GBBSGGG...GBBGGGTT....
 2 TGGBBGGGGbbbGBBGGGGT.GBB
 3 ..GTDGGTG...GGDGGGGGbGBB
 4 ..GGGGGGG....GGGGGb..GGD
 5 .........BBTT.....bb....
 6 .........BB~~......b....
 7 .........GDGG......b....
 8 ...........b.......b....
 9 ..r.r...GBBGGG..GGGGG...
10 ...r....GBBFGG..GGGTG...
11 .....r..GTDGGGGGGGGGG...
12 ...r....GGpppppGGTGGs...
13 ........TGGGGGppppGGG...
14 ........GGGGGTGGGpGGG...
15 ........GGGGGGGGGGGGG...
16 ........................
```

## 2. The islands (from the picture, back to front)

| id | where | picture says | building (door cell) | notes |
|---|---|---|---|---|
| A | top left, x2-8 y1-4 | grass, pines, a sign and wooden steps, tiny islet next to it | **bank** at (3,1), door (4,3) | islet at (0-1,2) holds one pine, not reachable |
| C | top right, x12-19 y1-4 (cols 13-15 of row 4 added so the path goes around the store) | long curving strip, big trees, joins A and the far right | **store** at (13,1), door (14,3) | A-C bridge y2 x9-11; C-E bridge (20,3) |
| D | pond island, x9-12 y5-7 | small, a pond and a tree | **school** at (9,5), door (10,7) | floats alone; reached by the bridge (11,8) from F |
| E | far right, x21-23 y2-4 | one pine, a bridge from C | **hotel** at (22,2), door (23,4) | smallest walkable island |
| F | main, x8-20 y9-15 | big U/L shape in front, dirt path winding to a ramp, cabin top left, campfire, pines | **cabin** at (9,9), door (10,11) | the start island: pets spawn on its path; C-F bridge runs diagonal (18,4),(18,5),(19,5),(19,6),(19,7),(19,8) down to F |
| rubble | left, x2-5 y9-12 | loose sand chunks | none | decoration only |

Walkable network: A - C - E along the top, C - F by the diagonal bridge, F - D by one bridge. Everything is reachable from F.

## 3. Buildings and interiors

Outdoor pictures are the same World_B cells the other pet boards use for the same room, so a player who knows mcl-hq recognizes them:
cabin = World_B (8,0) 2x2, bank = (6,11), store = (0,7), school = (4,7), hotel = (4,11) (the `SCENERY` rows of `mcl-hq/rooms.pdl`). Pines
= Outside_B. Each door is a pair like mcl-hq: `DOOR | island | x | y | room | rx | ry | dir` outside, and one door inside that returns to the cell in
front of the outside door. Interiors reuse the cabin page's room shape per building (one small room each, same as `rooms.pdl` castle/bank/store/school/hotel).
The existing `start` + `cabin` pages of rpg_edit.c become `start` (the islands) + five room pages.

## 4. How it would be built (not started)

1. `rpg_edit.c` already paints a page from stamped RPG Maker tiles and writes `map.txt`, `cells.*`, `extrusion.pdl`, `events.txt`; change COLS x ROWS from
   16 x 12 to 24 x 17 (arrays and `write_atlas` follow the constants) and paint the grid above instead of the grass rectangle. **To verify first:** that the board
   viewer accepts 24 x 17 (mcl-hq's city is 20 x 13).
2. Sky: a cell with no floor tile and height 0 must show sky. Check how `extrusion.pdl` default 0 renders on a page with a `parallax.rgba` (mcl-hq paints the 3D sky from
   `parallax.rgba`); reuse that, do not invent a second sky path.
3. Cliffs: the picture's dirt cliffs come from extruding grass cells (floor f=1 above the sky). A taller cliff = a higher `EXTRUDE` value; try f=2 for the main island and f=1 for
   the small ones to get the picture's depth difference.
4. Doors, rooms and teleports reuse `rooms.pdl` row shapes (`ROOM`, `DOOR`, `SCENERY`); the walking daemon already handles the cabin door for Ava.
5. Pets (the eight terumon sprites) spawn on island F's path and wander only on walkable cells; the bridges are the only way between islands, so the wander
   is confined by the map, not by code.
6. Proof: relay-driven screenshots of the real window, one per island, plus Ava (or any pet) crossing a bridge and using a door.

## 5. Open points for the owner

- The picture is a screenshot, so heights, exact island outlines and the cabin position are by eye. If the real page ever turns up, replace this grid with it.
- Is the cabin on the main island the **castle** of the other boards (it is the owner's "cabin"), or should the main island get its own sixth building? Drawn here as the castle/cabin.
- One building per island is kept; the far-left islet has none (a single pine).

## 6. Checked on paper (script over the grid above, 2026-10-21)

- Walkable cells (`G p b D s F S`): 143; reached by a 4-neighbour walk from island F's path: 143 (all). All five doors are reachable: [(4, 3), (14, 3), (23, 4), (10, 7), (10, 11)].
- Negative test, one bridge cell removed at a time: without (19,8) the bank, store and hotel doors are cut off; without (11,8) the school door; without (10,2) the
  bank door; without (20,3) the hotel door. So every bridge is needed and the islands do not touch each other.
- Not checked: the real 3D look (heights, cliffs, sky), that the viewer accepts 24 x 17, interiors. Those need the build.

## 7. Built (2026-10-21)

The island page is live in `@.apps/rpg-edit-hq/ops/rpg_edit.c` (24 x 17, grid above, five buildings, five doors, the eight terumon walking; Ava went through a door and the board followed her to the cabin page). Doors all lead to one shared interior and return beside the door she used; per-building interiors are still open.

Renderer change that made it possible (owner: "z levels in the negative give the float depth, and yes we can have holes"), in `board-viewer/ops/bv_render_3d.c` + `bv_gpu_raymarch.{c,h}`, maker maps only, no effect when the keys are absent:

- No hole keyword (owner: "we dont need fake holes, we have negative z levels"). Once a map uses `DEPTH`, a cell with no floor tile and no wall tile simply has no block, so the sky shows. Maps without `DEPTH` are untouched.
- `extrusion.pdl`: `DEPTH | <glyph> | n` (1..16) = n blocks hung below the floor (negative z levels). The deepest DEPTH lifts the whole grid, the camera eye and every box (sprites, selector) by that many levels so floor level stays the same for sprites.
- Rock: `cells.txt` takes `rock_slot=<n>`, an atlas slot painted on every hung block (rpg_edit uses the Outside_A4 rocky-earth cliff face, row 13 of the sheet). Without it the hung blocks use the cell floor tile tinted brown.
- View limit: the window that is drawn whole grew from 128 to 256 cells (= MAX_BOARD_DIM), and `pieces/system/view.pdl` may set `view_fog_start` / `view_fog_end` for one map (the house default in hq_ui.pdl fades everything past 48 cells to sky; this map asks 90..250).
- Left panel: the edit board had no `board/pchq-board.css`, so the renderer gave the empty rail its default 220 px; the stylesheet (copy of piececraft's) sets it to 2 px.
- Used here: G/p/B/T/D/S/F depth 3, rubble 2, bridges/ramp/pond 1.

Not done: the camera default still frames part of the 24 x 17 map (press back/up keys); islands are the same thickness everywhere (no tapered undersides yet); no 2D `map.png`.

## 8. Tapered undersides and the far island (2026-10-21)

- Taper: `cells.txt` tokens may carry a third number, `floor,wall,depth` (hung blocks under that cell). The renderer takes it over the glyph `DEPTH` rows when present. `rpg_edit.c` fills it with 2 x (ring distance to the nearest air cell) - 1, cap 9, so each island ends in an inverted cone; bridges stay 1, rubble 2. Seen in a screenshot before the page was widened.
- Far island (width test): the page is now 56 cells wide (the town is the first 24; a first try at 220 was too far to reach by normal camera scrolling); a 10 x 7 island with a pine, a path and a campfire sits at x 38..47, y 5..11, with no bridge and no door. The interior stays 24 wide (`IW`). The viewer draws a map up to 256 wide whole; map.txt rows are read through a 1024-byte buffer; fog for this map is 90..250.
- Verified: relaunched board, camera scrolled to the far island: it draws (round island, campfire, tapered underside) at the 56-wide page.
- Startup: while the canvas has no engine frame the shared renderer used to fill #101014 (black); it now draws a sky-coloured "loading..." panel (khtpm_draw_core.c kh_draw_canvas_loading), and open_board.sh waits for the engine's first frame file instead of a blind sleep 8 before opening the map. A true cold start was not captured on screen (every relaunch I timed already had a session), so the black phase itself is unobserved.
- Controls note: arrows do not move the xelector on this board because `pieces/system/board.txt` is empty (bv_menu_input reads its size); the camera keys work. The board window can also sit flagged minimized (`#.desktop/livedesk_hq_windows_<pid>.txt minimized=1`), which pauses the engine: write `restore=1` to `#.desktop/livedesk_hq_restore_<pid>.txt` to undo.
