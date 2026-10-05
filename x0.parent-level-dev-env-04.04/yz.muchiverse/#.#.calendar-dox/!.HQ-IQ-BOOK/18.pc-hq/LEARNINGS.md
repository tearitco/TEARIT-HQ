# pc-hq learnings

Written 2026-09-30, after the yellow diamond was visible on the hero
in a dumped frame. Tip on branch `grok` at that writing: `a59f0852d`.
Read this before changing the board, the 3D daemon, or Synch.

The window is one `khtpm_core_render.+x` on
`@.apps/piececraft-hq/pchq-board.xhtpm`. The picture is a separate
process, `bv_render_3d.+x --daemon` (and `bv_render_2d` for the flat
view). Closing the window does not replace a daemon that is already
running.

## The diamond

The 3D range is a wire diamond around the hero, inside the pc-hq
window. It is not the livedesk X11 desktop overlay.

A cell is in range when `|dx| + |dy| + |dz| <= 2`. The center cell
is the hero and is skipped. A higher layer spends the same budget,
so it has fewer squares. It does not keep a full floor because Z
went up. Range 2 in every direction, including height. Color
`255, 220, 40`. One brick is one terrain voxel, drawn as a wire,
not a filled cube.

World axes in `bv_render_3d.c`: world `(X, Y, Z)` is
`(grid_x, height, grid_y)`. The hero box sits at `g_hero_z`.

Draw the diamond after the hero and the tree models. The GPU list
holds 128 boxes (`BV_GPU_MAX_BOX`). Wires added first fill that list
and the hero and trees never get a slot. That is why a large cage
once sat on the chicken while the platform went empty. The chicken
is a world entity. The diamond is on the hero only, and only when
`g_hero_present` is set.

The desk placer writes a `#` / `.` file through `tp_gen_range_matrix`.
That file is the shared shape the 2D board still reads as a flat
outline. The 3D diamond on `a59f0852d` is computed in C. It is not
yet one matrix file per Z layer the way `board_manifest.txt`
(`z_base`, `z_count`) stores a map. Hero Act Move still exits before
`tp_arm_placer_rmmv` when the entity path contains `/pieces/`, so
the hero does not start the desktop placer. A desk pal such as Asa
does.

## The green selector and the click line

A click arms `pieces/display/placer.txt` and draws one green wire
(`40, 255, 80`) on that cell. Arrows and `z` / `x` move it. Escape
clears `armed`. Those keys are supposed to move the selector until
Escape, the same way the desk placer works, and they return before
hero movement in `bv_menu_input.c`.

The HUD line is `click: <pos> <time>`, read from
`pieces/display/click_hud.txt`. The HUD array has to be at least 12
lines or the new row never appears; the older eight slots were
already full (time, pos, z, poss, pick, tick, map, pid). A dump that
still says `click: - -` means no click has landed since that build.
The click path is coded for 2D and for the 3D ray. A live click was
not in the frame that proved the diamond.

## Seeing a frame

Append `KEY_PRESSED: 206` to
`#.desktop/entity_menu_history/<window-pid>.txt`. The renderer writes
`/tmp/entity-menu-frame.png` when it is spinning. A queued 206 sits
forever if the daemon is idle. Key `p` does the same dump when no
text field is armed. Key `0` toggles `render_mode` (1 = 3D, 0 = 2D)
when the board has the keyboard.

Reopening the pc-hq window does not load a new `bv_render_3d.+x`.
The daemon keeps the binary it mapped at start. After a rebuild,
`/proc/<pid>/exe` shows `(deleted)` while the process still draws
the old picture. Kill that daemon by numeric pid. Do not reset the
house. Do not `pkill -f` a pattern that is also the text of your own
shell.

Start the new daemon from the session directory
(`@.apps/piececraft-hq/pieces/sessions/<id>/`) with
`PRISC_PROJECT_ROOT` set to that directory:

```
./ops/+x/bv_render_3d.+x --daemon
```

`project_root` defaults to `.` and then the daemon exits because
`pieces/display/.gpu_render_req` is missing. `button.sh` copies
`ops/` into the session at session start, so a rebuilt
`ops/+x/bv_render_3d.+x` is not what a running session executes
until that copy is refreshed or the daemon is started from a tree
that has the new binary. `strings` on the binary cannot confirm the
diamond: the marker in source is a C comment.

The window process is a different binary (`khtpm_core_render.+x`).
A canvas right-click change is invisible until that window is
relaunched. The projector (`pchq_board_projector.+x`) writes
`state/ui.txt` and is the parent-side label process. Kill it only
when the labels themselves changed, and only one instance should
own `ui.txt`.

## Book and page

The words match. The files do not. The owner wants them to be one
store. That choice is not made yet.

Livedesk taskbar `book:` / `page:` is the session and the desk in
`session.pdl` (`active_session`, `active_desk`). A desk file is
`xyzfs/users/<uuid>/home/livedesk/sessions/<id>/desks/<desk>.pdl`,
rows `DESK | name | path | x_px | y_px | …`. Pixel values at or
above 40 are cells times 80.

pc-hq Book / Page is this window's map and desk label. The projector
prints `book:<map_id>` and `page:<desk label>`. A proven pair on
the board is `book:test_walls` and `page:Desk 1`. Those names come
from the board, not from the livedesk session. On the day of the
diamond the livedesk active pair was session `s1`, desk `teru-test`.

Asa and the hero are two entities. Asa is a desk pal. The hero lives
under the board's `pieces/` tree. They move together only when both
windows read and write the same page file. Synch does not turn one
into the other.

## Synch

The agreed direction is `SYNCH.md`. The table below is what the
code on `079038706` does, and that behavior is not the target.

The row is under Player, not Menu. Menu stays for dynamic entries.

| Place | Control | What the press does today |
|---|---|---|
| pc-hq | toolbar tab 5, `tb-player`, item `pm-synch` | reads the active desk `.pdl` and writes `pieces/display/synched_entities.txt` (`name x y`) |
| livedesk taskbar | cell 9, `livedesk:synch-from-pchq` | appends a `hero_01` DESK row if that name is absent |

The board draws those names: a cyan mark in 2D, a cyan wire box in
3D. It does not open their pal windows, does not copy map layers,
and does not relaunch pals. Spawn keeps the first row per basename.
`cursword` is skipped by the desk spawner because it has its own
path. "Last active pc-hq" is not recorded when two boards are open.

A full copy would copy the book and the page, entities included.
Which tree wins (livedesk session/desk, or the pc-hq map project)
is still an open question. Do not start that until the owner picks
the store.

## Right-click

Button 3 on the canvas id `view`, and only when the chtpm path
contains `pchq-board.xhtpm`, runs `pc_canvas_rclick.sh`. That script
maps the click through `pieces/display/view_map.txt` (`ox oy cell W H`,
written by the 2D drawer), writes `pick.txt`, and launches
`pc_entity_ctx.sh` at the click. The hero menu is INSPECT, POSSESS,
ACT, STOP, EVENTS, INVENTORY, DIR. In first person (`camera_mode == 1`)
the menu is aimed at the center of the pc-hq window. Right-click on
the toolbar stays the window Cut/Copy/Paste/Place menu.

Before that special case, button 3 on any non-chrome hit opened the
window menu and returned before the game. The hero menu only ran
from an inbox `CTX_MENU`. `pick.txt` is written by the 3D renderer
unless the 2D click path or the right-click script writes it.

`pc_entity_ctx.sh` passes `x y` when `MENU_X` and `MENU_Y` are set.
`khtpm_core_render` treats argv[3] as a window position when argc is
at least 5 and argv[3] is not a directory.

## What is still open

- One shared BOOK:PAGE store. The owner said the two files should
  not stay different. Which store wins is unanswered.
- Synch as a real page copy: map layers, entity files, and both
  windows showing that page. Today's row is a name list plus, from
  the taskbar, one `hero_01` desk row.
- The 3D diamond as per-layer `#` files, read the way maps are read,
  instead of the computed radius-2 octahedron.
- Hero Act Move starting `tp_arm_placer_rmmv` instead of taking the
  `/pieces/` early exit.
- A live frame of the green selector moving, of a real `click:` line,
  of the 2D diamond, and of the hero menu opening on the sprite.
  The diamond frame is the one that was seen.

## Where the code is

Under `44.xyz.01.00/`:

- `&.widgits/board-viewer/ops/bv_render_3d.c` — scene, diamond, HUD,
  click ray.
- `&.widgits/board-viewer/ops/bv_render_2d.c` — flat frame, view map,
  synched names, 2D click.
- `&.widgits/board-viewer/ops/bv_menu_input.c` — placer keys.
- `&.widgits/board-viewer/ops/bv_gpu_raymarch.c` — `wire` on a box.
  Upload alpha `0.25` means wire.
- `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` — canvas
  button 3.
- `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` — cell 9
  Synch, `book:` / `page:` from the session.
- `@.apps/piececraft-hq/pchq-board.xhtpm` — toolbar labels and
  `pm-synch`.
- `@.apps/piececraft-hq/ops/pchq_board_projector.c` — `book_label`,
  `page_label`.
- `@.apps/piececraft-hq/ops/pc_synch_request.sh`
- `@.apps/piececraft-hq/ops/pc_canvas_rclick.sh`
- `@.apps/piececraft-hq/ops/pc_entity_ctx.sh`

Builds: `&.widgits/board-viewer/scripts/build.sh`,
`_.monads/_.livedesk-taskbar/ops/build_khtpm_strip.sh`,
`@.apps/piececraft-hq/ops/build_pchq_board_projector.sh`.

## Git

Commits for this stretch go on branch `grok` from the worktree
`/tmp/grok-cmd-wt`. The checkout stays on `claude`. Push
`origin grok` after the sprint. Scoped `git add` of named files.
Leave runtime state (pid files, logs, live `.pdl`) out of the commit.
