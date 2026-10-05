# pc-hq reads the livedesk page file

Design change, 2026-09-30. Steps 1 through 6 are in the drawers
and in `pc_synch_request.sh`. `SYNCH.md` is the direction that
script now follows.

Book and session are the same thing. Page and map are the same thing.
A book holds many map pages. The livedesk page file is the older
standard. pc-hq changes to match it.

## The page file

A page is one desk file:

`xyzfs/users/<uuid>/home/livedesk/sessions/<session>/desks/<page>.pdl`

The session's `session.pdl` names the book (`STATE | name`) and the
open page (`STATE | active_desk`). A row is one entity:

```
DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n
```

`cursword`, `book-stack`, `door_civ`, the terumon, and `robot_chat_001`
on `teru-test` are already rows of this shape. Pixel values at or
above 40 are cells times 80.

pc-hq 2D and pc-hq 3D both read this file for the open book and page.
A move in either window writes the same row. The desk already writes
it when a pal moves. After that, the other window is looking at the
same file, so the move shows up there on the next draw.

3D is that same page stood up. It does not have its own entity list.

## What pc-hq stops using as position

These are private files. They are why a move on one window does not
show on the other.

| Today | Role |
|---|---|
| `pieces/hero_01/state.txt` | hero cell, read by the 2D hero mark, the 3D hero box, and the range diamond |
| `pieces/xelector_01/state.txt` | xelector cell and `possessed_id` |
| `pieces/world_01/phymoji_entities.txt` | trees, `name,x,y,z` |
| `pieces/world_01/animals.txt` | chicken, `name,x,y,z` |
| `pieces/display/synched_entities.txt` | a copied name list, not the page |

The hero is one row in the page file, the same shape as `asa` or a
tree. `load_hero()` in `bv_render_3d.c` and the hero block in
`load_actors()` in `bv_render_2d.c` stop being the place the cell is
stored. Trees and the chicken move into rows in the same file.
`pc_synch_request.sh` does not write a `hero_01` row of its own, and
it does not keep a second entity list.

Terrain glyphs for the map stay the map grid of that page. This
change is the entity list, not a new terrain format.

## Camera follows the xelector

The camera follows the xelector. It follows the hero only when the
xelector possesses the hero, because the xelector is then on the
hero. The camera does not read `hero_01` to decide where to stand.

Today `build_camera()` anchors modes 1 and 2 on `selector_x` /
`selector_y` from the board state file (`bv_render_3d.c`, around the
`anchor_x = selector_x + 0.5` line). The 2D view is centered on that
same selector. The xelector's live cell is a different file,
`pieces/xelector_01/state.txt`, loaded by `load_xelector()`. The hero
cell is a third source. Those three stop being separate authorities.
The xelector row in the page file is the cell the camera uses.

`possessed_id` is a field of the xelector row. When it names an
entity, the xelector's cell is that entity's cell. Possessing the
hero is that case. Possessing nothing leaves the xelector on its own
cell, and the camera stays there.

Modes 3 and 4 stay detached. `z` / `x` move the xelector. `c` / `v`
move a detached camera's height. That split stays.

The range diamond is drawn on the hero today (`g_hero_present` in
`bv_render_3d.c`). It moves to the xelector's cell, which is the
possessed entity's cell when the xelector possesses one.

## Xelector and camera as entities

Do this in the same pass if it falls out of the page-file read.
Otherwise it is the next pass. It is not a third storage format.

- The xelector is a row in the page file. Its cell and
  `possessed_id` live on that row. `pieces/xelector_01/state.txt`
  stops being the position file.
- The camera is a row in the page file. Mode, yaw, pitch, and pan
  live on that row. The board state keys `camera_mode`, `cam_yaw`,
  `cam_pitch`, `cam_pan_x`, `cam_pan_y`, `cam_pan_z`, and
  `cam_z_level` stop being a second copy of the camera.

## Order of work

1. Teach the board's 2D draw and 3D draw to read entity cells from
   the open page file, and to write a moved cell back to that row.
2. Put the hero, the trees, and the chicken on that file. Stop
   reading their private lists for position.
3. Point the camera anchor at the xelector row. Honor
   `possessed_id`.
4. Move the range diamond to that same cell.
5. Make the xelector and the camera rows in that file, and stop
   reading their private state for position and angles.
6. Synch, as `SYNCH.md` already says: the inheritor takes the
   sender's page, and the sender's book too when the books differ.
   Both windows are then on one page file, so a later move mirrors.

## Step 2, proved

The first 2D frame appends `hero_01`, each `tree_small`, and
`chicken` onto the open desk file when that name is absent. Later
frames read those rows. `pieces/hero_01/state.txt` stayed at cell
5,12. Moving only the `hero_01` row from cell 5 to cell 6 moved the
hero sprite from screen cell 7 to screen cell 8 (1845 and 1925
pixels, RGBA frame 1685x1102, view ox=-2 oy=3 cell=80). The row was
put back to 5,12. House checks for this are C.

## Steps 3 to 6, proved

The same first frame appends `xelector_01` (cell, z, and
`possessed_id` in the glyph field) and `camera_01` (mode, yaw,
pitch, pan, height packed in the glyph field). While
`possessed_id` is `hero_01`, the xelector cell is the hero cell.
A 2D frame then had 1216 yellow pixels in screen cell 7,9 and none
in screen cell 10,5. Clearing `possessed_id` and moving only that
row to cell 8,8 put 924 yellow pixels in screen cell 10,5. One 3D
frame on that same detached row kept `selector_x=5` and wrote
`cam_eye_x=8.50`. The row was put back to cell 5,12 possessing
`hero_01`. Modes 3 and 4 still ignore that cell.

A taskbar Synch wrote `book=pre-design` `page=teru-test` into
`open_book_page.txt` and left the hero row alone. A pc-hq Synch
with the board on `office` set the desk `active_desk` to `office`.
That session file was put back to `teru-test` after the check.
The desk is not switched to a page that has no desk file. The
running taskbar binary does not yet contain the snapshot change
that copies `hero_01`, `tree_small`, `chicken`, `xelector_01`, and
`camera_01` back onto the page when it rewrites window rows. That
change is in `khtpm_taskbar_manager.c` and takes effect the next
time that binary is built.
