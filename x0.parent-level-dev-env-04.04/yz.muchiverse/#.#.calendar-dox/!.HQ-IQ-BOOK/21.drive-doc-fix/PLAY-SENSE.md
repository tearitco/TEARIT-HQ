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
| Editor book dropdown | cdda, mcl, and df are rows a person can pick | 2026-10-29. Was five rows, both books labeled `🏠 RPG-Pet`, no cdda, mcl, or df. After the link-on-open and the RPG-Pet label rule, session `1793338004` published `n_file_opts=015`. Args include `cdda-hq`, `mcl-hq`, `df-hq`, `rpg-space`, `rpg-edit-hq`, each with its own label. Window stopped. |
| Editor pick changes the pane | the 3D picture is the page the bar names | 2026-10-29 session `1793338561-26401`. Bar and `world_01` were `cdda-hq` / `bank` while the overlay stayed the 22:36 boot frame, sha `d9abcbb8`, 846×395. The viewer redraws when `bv_screen_changed.txt` grows. One byte made sha `b9117028`: a small room, not the bridge (`/tmp/cdda-bank-pane.png`). After the projector rebuild, `CONFIRM_SET_DESK:city` grew that marker 1 to 2 bytes and the overlay became `98ebc85a` with `page:City`. Set back to Bank. Voice stayed on. Die about 45°C. The window stayed up. |
| cdda-hq, see the page | the Bank pane, not loading | 2026-10-29 session `1793337735`. Bar `book:cdda-hq` `page:Bank`. World `map_id=cdda-hq` `desk_id=bank`. Overlay 759×430, `no_session` empty. In was off. No `pet_tts`. Window and the cdda daemon stopped. The editor daemon was down after the crash and was started again (pid 3840). Voice stayed on. The cpu guard process was not running; the live thermal zone was 48–49°C. |
| mcl-hq, one touch page | step onto a cell and the page runs | 2026-10-29 session `1793337358`. Page `city/ev/20`, trigger player-touch, pal at `event_pkg/pages/page_1/event.pal`. The cast `bar.txt` is rewritten by the daemon. A touch row is now copied back on that rewrite (`player-touch`, `event-touch`, `on-touch`). After a rebuild, session `1793337571` rewrote the cast (Harold and the pets moved) and `20 step-south 9 8 player-touch` was still the last line. Armed just before the step, one engine `1003` landed Rex on 9,8. Ledger: `Play event page_1 (trigger=player-touch) for 20`. The same second also logged `cdda_beartrap_touch` and a prisc miss for mutaclysm `default_op.txt`. In turned off. Window and daemon stopped. Voice stayed on. |
| mcl-hq, In on, one arrow | the controlled pet moves one cell and the xelector stays on that cell | 2026-10-29. Writing the arrow into the daemon relay (`state/interact_relay.txt`) did move Rex, then `follow_xelector` walked him back onto the cursor that had not moved. The ledger pairs are `8,7 -> 9,7 -> 8,7` and `8,7 -> 8,6 -> 8,7` in the same second. That snap is the feature: the engine owns the arrow, the pet follows the cursor, a bump snaps the cursor back. Session `1793337205`, In on, one `1003` on the widget relay only. Rex and the xelector both went from 8,2 to 8,3 and stayed (`STEP|1793337236|rex|city|8,3`). In turned off. Window and daemon stopped. Voice stayed on. |
| mcl-hq, see then walk | the Overworld pane, then one step south | 2026-10-29. The pane was `book:mcl-hq` / `page:Overworld`, overlay 826×430, 1640 colors. One down arrow was consumed and the cursor went from 8,8 to 10,5. That cell is Harold: `state/trainer.txt` is `room=city x=10 y=5`. `follow_pchq` writes the selected entity's cell into the xelector when that cell changes, and the first call counts (the last cell starts at -1). Design, 2026-10-11, `RPG-PET-DESIGN.md`: the camera and the xelector follow the selected entity so the 3D view stays on who you picked. Do not remove that to make a free cursor step stick. A play step on this book is In on, then an arrow that moves Harold, with the xelector coming along. The cursor was put back to 8,8 after the window stopped, which undoes that first snap until the next open. |
| One sprite on the open page | the same row in the 3D pane and in `state/scene.raw` | 2026-10-29 20:35 the overlay had it and `scene.raw` did not. Later the same row stamped both: 256 pixels in `scene.raw`, 318 in the overlay, then 0 and 0 one second after the row was deleted. Session `1793332823-3592394`. Daemon `3590405`. |
| Edit then play, second try | same bar | 2026-10-29 20:07. The cursor walks when `1003` is appended to the widget session `interact_relay.txt` (`pos_y` 8 to 9, relay consumed). The sprite list `events.txt` is not the event log. The page is bridge `ev/10` (`step-south`, `player-touch` on 8,9). The step used to call `tsots_touch_event.sh` inside the board-viewer session, which has no such script, so the page never ran. `touch_event_at` now reads `real_project_root.txt` and calls `piececraft-hq/ops/tsots_touch_event.sh` with that project. After the rebuild, session `1793329626-3432974`, one down step wrote `ev/10/master_ledger.txt`: `Play event page_1 (trigger=player-touch) for 10`. The same file also logged a common event `cdda_beartrap_touch` and a prisc miss for mutaclysm `default_op.txt`. `data/master_ledger.txt` stayed on the old move ticks because `possessed_id=none`, so MOVE does not append `touched_npc`. Window stopped. Daemon `3366898` left up. |
