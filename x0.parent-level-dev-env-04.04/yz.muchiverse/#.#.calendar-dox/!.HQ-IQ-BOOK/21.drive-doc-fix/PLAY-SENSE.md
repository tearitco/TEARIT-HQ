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
| Edit board, 2026-10-29 19:40 | xelector 1, 1, z 9. Selector 8, 8. Eye `cam_eye_x=1.50` `cam_eye_z=8.50` | `rpg-edit-hq-pchq/pieces/xelector_01/state.txt` and `pieces/display/scene_receipt.pdl`. Book `rpg-space`, page Bridge. `possessed_id=none`, `last_possessed_id=hero_01`, `pre_possess` 9, 8. Session `1793328028-3346259`. `no_session` empty, canvas set. The body is in the corner. The selector is the page center. The eye followed the body on x and not the selector. Same split INTENDED already recorded, now on this board. Not moved. Die was 86 C and `pet_tts` was 145%, so the window was stopped. |

| Edit board, 2026-10-29 19:56 | piece 8, 8 and selector 8, 8. Eye `cam_eye_x=8.50` | The corner was the map fallback in `pc_generate_chunk.c`: no `START.txt` meant 1, 1, while the selector defaults to `board_w / 2`. Bridge now has `START.txt` at 8, 8. A page whose `events.pdl` marks `x=8 y=8` still starts at 1, 1 (the cdda door). Receipt `xelector_z` was 0 and `pos_z` became 17. Window stopped. Die 69 C. |

The 19:40 open started in the corner. The 19:56 open has the piece, the selector, and the eye on x at 8. The two heights are the same cell. `board_manifest.txt` says `floor_z=16`. The piece file stores the voxel, `pos_z=17`, which is one above that floor. `bv_render_3d.c` then publishes the receipt as levels above the floor: `pos_z - floor_z - 1`, which is 0. The hero file is the same cell, 8, 8, 17. No second writer. Do not chase receipt `xelector_z=0` against `pos_z=17`.

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
| Edit then play | a changed place, one event page, a walk, a fire, a new line in `events.txt` | 2026-10-29 19:59, session `1793329165-3404375`. Bridge `events.txt` is 5 sprite rows, sha256 `d3cb9831…` before and after. No event page with an expected result and an effect. `hotspots.txt` is four labels the 3D view outlines. `bv_menu_input.c` has no hotspot fire. A down arrow `1003` written to the engine `pieces/keyboard/history.txt` was still there one second later and `pos_x` stayed 8. The window had no X id for the renderer, so a real key could not be aimed. This is the stop. |
| Edit then play, second try | same bar | 2026-10-29 20:07. The cursor walks when `1003` is appended to the widget session `interact_relay.txt` (`pos_y` 8 to 9, relay consumed). The sprite list `events.txt` is not the event log. The page is bridge `ev/10` (`step-south`, `player-touch` on 8,9). The step used to call `tsots_touch_event.sh` inside the board-viewer session, which has no such script, so the page never ran. `touch_event_at` now reads `real_project_root.txt` and calls `piececraft-hq/ops/tsots_touch_event.sh` with that project. After the rebuild, session `1793329626-3432974`, one down step wrote `ev/10/master_ledger.txt`: `Play event page_1 (trigger=player-touch) for 10`. The same file also logged a common event `cdda_beartrap_touch` and a prisc miss for mutaclysm `default_op.txt`. `data/master_ledger.txt` stayed on the old move ticks because `possessed_id=none`, so MOVE does not append `touched_npc`. Window stopped. Daemon `3366898` left up. |
