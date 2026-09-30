# What this stretch intended

Written 2026-09-30 after a live check of commit `38b775390` on
`grok`. If a window does not match the list below, the change did
not take or it regressed. The check at the bottom is the one that
passed on that commit.

Book is the livedesk session. Page is one desk file
`xyzfs/users/<uuid>/home/livedesk/sessions/<session>/desks/<page>.pdl`.
The live page used for the check is user
`0a9558a7-7c74-4358-833c-2d5b21edc421`, session `s1`, book name
`pre-design`, page `teru-test`.

## Files

| File | Intended change |
|---|---|
| `&.widgits/board-viewer/ops/bv_render_2d.c` | Read the open page every frame. Seed `hero_01`, `tree_small`, `chicken`, `xelector_01`, `camera_01` once when the name is absent. Draw the hero, trees, and chicken from those rows. Draw the yellow wire on the xelector cell. |
| `&.widgits/board-viewer/ops/bv_render_3d.c` | Same page read. Hero, trees, and chicken come from the rows. Modes 1 and 2 anchor the camera on the xelector. The range diamond uses that cell. Camera mode, yaw, pitch, and pan come from the `camera_01` row when its glyph parses. |
| `@.apps/piececraft-hq/ops/pc_synch_request.sh` | Taskbar press writes the desk book and page into `open_book_page.txt`. pc-hq press writes that board's page into the desk `active_desk`. No `hero_01` rewrite. No `synched_entities.txt`. |
| `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` | A desk snapshot must copy `hero_01`, `tree_small`, `chicken`, `xelector_01`, and `camera_01` back onto the file after it rewrites window rows. |

The drawers the live session actually runs are the copies in
`&.widgits/board-viewer/pieces/sessions/<id>/ops/+x/` and the
piececraft session `ops/+x/`. `scripts/build.sh` does not update
those copies. A new session start copies from the widget `ops/+x`,
which is the checkout the house is sitting on. `grok` commits do
not reach that checkout until `grok` is merged.

## Rows

A row stays nine fields:

```
DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n
```

| Name | Cell | Glyph | Last field |
|---|---|---|---|
| `hero_01` | the hero cell | `.` | `0` |
| `tree_small` | one row per tree | `.` | that tree's z |
| `chicken` | the chicken cell | `.` | that chicken's z |
| `xelector_01` | the xelector's own cell | `possessed_id`, or `.` when it possesses nothing | z |
| `camera_01` | pan x, pan y | `m=<mode>,y=<yaw>,p=<pitch>,z=<pan_z>,h=<z_level>` | `0` |

There is one row per name except `tree_small`, which has one row
per tree. A second `hero_01` row is a bug. The old taskbar Synch
wrote one. The new script must not.

`page_has_name` seeds a name only when it is absent. A later frame
must not append another copy.

Private files are the seed only:

- `pieces/hero_01/state.txt`
- `pieces/world_01/phymoji_entities.txt`
- `pieces/world_01/animals.txt`
- `pieces/xelector_01/state.txt`
- `pieces/system/bv_state.txt` keys `camera_mode`, `cam_yaw`, `cam_pitch`, `cam_pan_x`, `cam_pan_y`, `cam_pan_z`, `cam_z_level`

After the row exists, drawing does not read those files for the
cell. The 2D and 3D drawers still read `bv_state.txt` for
`selector_x`, `selector_y`, `current_z`, and `render_mode`.

## Camera and diamond

`possessed_id` names a row. The xelector's drawn cell becomes that
row's cell. On the live page that name is `hero_01`, so the
xelector, the camera, and the diamond sit on the hero.

Modes 1 and 2 use that cell as `anchor_x` / `anchor_z`. Modes 3
and 4 stay on the selector and on pan. `c` / `v` still move a
detached camera's height. `z` / `x` still move the xelector.

The diamond is the yellow wire, color 255,220,40. In 2D it is the
cell border plus the `#` tiles in
`pieces/display/move_range_matrix.txt`, centered on the xelector
cell. In 3D it is the radius-2 Manhattan wire on that same cell,
drawn after the models. It is not on the hero unless the xelector
is on the hero.

Screen cell of a desk cell is `(cell_x - ox, cell_y - oy)` from
`pieces/display/view_map.txt`. At the check, ox was -2 and oy was
3, so desk 5,12 is screen cell 7,9.

`camera_01` was seeded from an empty camera block in `bv_state.txt`,
so the glyph is `m=2,y=180,p=6,z=0,h=0` and the pan cell is 0,0.
Pan is an offset on modes 1 and 2. Zero pan does not move the
anchor. A mode 4 camera would sit on that 0,0 pan. That is the
seed, not a second map.

The scene receipt is
`@.apps/piececraft-hq/pieces/display/scene_receipt.pdl`. A mode 2
frame with the xelector detached at cell 8,8 and the selector still
at 5,12 wrote `cam_eye_x=8.50` and `xelector_x=8`. If a later frame
shows the eye on the selector while mode is 1 or 2 and the xelector
row is somewhere else, the anchor change did not take.

## Synch

The Player Synch row used to call `ktb_hq_open` on cell 9 after the
script returned. That left `strip_state.txt` at `hq_open=9` and the
strip nav stuck on Player. The desk is the sender, so that press
must not hold the menu. It now calls `ktb_hq_close`. Play and Stop
still reopen the Player menu on purpose. A running manager keeps
the old text until it is replaced. The live one was replaced
2026-09-30 after this fix, and `hq_open` went back to 0.

`pc_synch_request.sh taskbar` is the desk sending. It writes

```
book=<session name>
page=<active_desk>
pdl=<that desk file>
```

into `@.apps/piececraft-hq/pieces/display/open_book_page.txt`. It
does not edit the desk file and it does not edit `session.pdl`.

`pc_synch_request.sh pchq` is the board sending. The board's book
and page are that `open_book_page.txt` when the file exists.
Otherwise they are `map_id` and `desk_id` in
`pieces/world_01/state.txt`. The desk `active_desk` changes only
when `desks/<page>.pdl` is already a file, or when the `pdl=` path
is already a file. A different book is taken only when
`sessions/<book>/` already exists. `test_walls` / `desk1` is the
loaded map. There is no desk file of that name, so a press with no
`open_book_page.txt` leaves the desk where it is and writes
`status=page-not-in-book`. That is the intended guard, not a
successful switch.

When `open_book_page.txt` names a real file, both drawers read that
file instead of the desk's `active_desk`. A later desk switch does
not move the board until the next Synch. That is the inheritor
staying on the page it was given.

The user scan must not stop at the first user directory. The first
directories have no `active_session`. The lookup keeps going until
one `session.pdl` has it. `field_trim` strips the newline. Leaving
it makes the desk path miss and the page draws nothing.

## Known hole

`khtpm_taskbar_manager.c` has the snapshot copy. The running
taskbar binary was not rebuilt in this stretch. Until that binary
is built and the taskbar is restarted, a desk snapshot can still
drop `hero_01`, `tree_small`, `chicken`, `xelector_01`, and
`camera_01`. If those rows vanish after a desk switch, that is
this hole.

Do not start a second `bv_render_3d.+x --daemon`. The idle sleep is
30ms and only runs when the view file did not change. Kill a stray
by numeric pid.

## Check that passed

2026-09-30, session `1790763621-628622`, one-shot
`bv_render_2d.+x` with `PRISC_PROJECT_ROOT` set to that session.
No daemon was running. The page file was byte-identical after
`pc_synch_request.sh taskbar`. `session.pdl` was byte-identical.
`active_desk` stayed `teru-test`.

- One `hero_01` at 5,12. Four `tree_small`. One `chicken` at 9,15.
  One `xelector_01` at 5,12, glyph `hero_01`, z 17. One `camera_01`
  with glyph `m=2,y=180,p=6,z=0,h=0`.
- View ox=-2 oy=3 cell=80, frame 1685x1102.
- Yellow pixels in screen cell 7,9: 1216. Screen cell 10,5: 0.
  The xelector is on the hero, so the empty cell stays empty.
- Taskbar status `board-follows-desk`, book `pre-design`, page
  `teru-test`.

An earlier detached check, restored before this one, moved only
the xelector row to 8,8 with glyph `.` and got 924 yellow pixels
in screen cell 10,5. The matching 3D one-shot wrote
`selector_x=5` and `cam_eye_x=8.50`. Both the row and the desk
`active_desk` were put back.
