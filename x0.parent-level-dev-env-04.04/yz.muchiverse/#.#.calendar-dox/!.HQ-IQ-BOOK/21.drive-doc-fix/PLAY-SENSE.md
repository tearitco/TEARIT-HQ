# Play sense — things a person notices that are not a named bug

Required on the first frame of a drive, before the next click. The
chrome sheet is menus. This sheet is the body, the camera, and whether
the page is playable. Fill a row when something looks wrong. Do not
fix it until the owner names it. A blank "what the frame showed" is a
miss. Add a row when a new kind of thing shows up. Do not delete an
old row. Mark it seen-ok with the frame path.

`CHROME-COMPARE.md` does not cover this. "Follow LyfeDesk" does not
cover this.

## What to look at, every open

1. Where the xelector is on the first frame, in cells, from
   `selector_x` / `selector_y` or `pieces/xelector_01/state.txt`.
2. Whether that cell is the page center, a door, the starting-position
   entity, or inside a wall.
3. Whether the camera eye is on that cell or on a different cell.
4. Whether In is off, so arrows move a cursor the picture does not follow.
5. Whether debug text (`pos`, `ray`, `pid`) covers the place you would walk.
6. Whether the first paint is the page or `loading...` / a letterbox.

## Xelector start, read 2026-10-29, no new board

The edit board was not opened for this row. Die had been 94 C on the
previous launch. These are the files that already exist.

| where | cell | note |
|---|---|---|
| Engine default when `selector_x` is missing | `board_w / 2` | `bv_render_3d.c` `read_kv_int(..., "selector_x", board_w / 2)`. A 16-wide page starts at 8. |
| House `44.xyz.01.00/pieces/xelector_01/state.txt` | 8, 8, z 15 | center of a 16 page. `possessed_id=none`. |
| `piececraft-hq/pieces/xelector_01/state.txt` | 6, 6, z 26 | possesses `hero_01`. `pre_possess` is 9, 8. This file is leftover state, not a fresh open. |
| `18.pc-hq/INTENDED.md` | selector still 5, 12 while the xelector row is 8, 8 | A mode 2 receipt wrote `cam_eye_x=8.50` and `xelector_x=8`. The selector and the body had split. That is the documented "off" case. |
| Design, not built | one starting-position entity per game | `XELECTOR-CURSWORD-POSSESSION-DESIGN.md` section 5. Play is supposed to start there. There is no such entity on the edit board yet. |
| Edit board, this drive | not read | Next open writes the cell here, from that frame's `xelector_01/state.txt`, and says whether it matches 8,8, 6,6, or a wall. |

Until that frame exists, do not call the edit-board start wrong and do
not call it fine. The thing that already looks off, on the standard
project, is the split in INTENDED: the selector stayed at 5,12 while
the body and the eye were at 8.

## Other first-frame rows already seen

These are copied from the how-to so they live on this sheet too. They
stay open until a new frame clears them.

| thing | ought to be | frame |
|---|---|---|
| First 3D paint | the page, full pane | filled only after the view changed |
| In on open | off means watch. On means the body moves. | In was off. Arrows and the picture can disagree. |
| Key `1` | pitch 6 | picture did not change, 2026-10-29 |
| Debug overlay | a toggle | `pos`, `ray`, `pid` sit on the view |
| Two bodies | one key, one Harold | bar `rpg-space:bridge` while `trainer.txt` said `room=store` |
