# Already drive-doc-fixed

Read this before opening a board. A row here has a frame or a file
proof in `PROOF-2026-10-29.md`. Do not launch a window to re-check it.
The next run starts at the first row that is not in this file.

The loop itself is `HOW2-DRIVE-DOC-FIX.md`. Open bugs that are written
and still open stay in `OPEN.md` and in HOW2 "Seen". Those are not
redone either. They wait for a named fix.

Check without a window: run `editor_skip.pal` in `&.widgits/_shared-lib/harness` (see `HARNESS-REUSE.md`).

## Closed

| row | what was shown | leave it |
|---|---|---|
| Event fire on the edit board | Bridge `ev/10` `player-touch` wrote `Play event page_1` after `touch_event_at` called piececraft `tsots_touch_event.sh` with the real project. | Do not point the touch script at the board-viewer session. |
| In is toolbar tab 1 | `tb-in` nav 1, `tb-book` nav 2. Matching LyfeDesk by moving In after the clock was the wrong move. | In stays option 1. |
| One page row, both pictures | A sprite row stamped the 3D overlay and the daemon `scene.raw`, then both went to 0 when the row was removed. | Both pictures, or it is not an edit. |
| Voice stays on | `flag_voice.txt` stays `1`. `speak()` returns when `no_session=1`. A closed window must not keep a wav playing. | Do not write the voice flag to 0 to test. |
| Centroid pages on the editor list | 2026-11-06 relaunch. `state/ui.txt` `n_room=8` with `room_6_id=test` and `room_7_id=live`. `goto test` then `goto live` set `room_label` to test, then live. Session `1793960181-3682232`, engine pid 3682358, daemon up. | The pages are plain interiors. The 6×5 court is not drawn. Do not relaunch just to re-read the room list. |
| Centroid book opens on the engine | 2026-11-06. Inbox `CONFIRM_SET_DESK:live` then `test`. `page_label` followed. Relay `112` on pid 3682305 wrote `/tmp/entity-menu-frame.png`. Page live and page test both show a flat grass sheet, `ray miss`, click line stuck at 02:16:22. | The 6×6 page did not contain the xelector at 8,8, so the look ray left the map. Both desks are now 16×16 grass. A click at canvas 443,360 reads `ray 8,8,0`; 443,540 reads `ray 8,13,0` (`click_proj.txt` inside=1). Sky above the far edge (443,200) still misses. The xelector stays on 8,8 because `possessed_id=hero_01`. The court voxels are still not this floor. Next edit, 02:44: cell 8,8 set to slot 36. Same click reads `ray 8,8,0` and the pane shows that rock tile under the xelector. 02:47: the same cell is `1,36` (floor 1, wall slot 36). The pane shows a column about three blocks tall on that cell. A wall slot is what raises the column. 02:49: a 5 by 5 ring of those columns, cells x=6..10 and y=6..10, with the near middle cell (8,10) left as grass. The frame shows three walls and a gap. Nothing in the maker swings that gap. A door is still two voxels that move. |
| Follow stays | `follow_pchq` / `follow_xelector` keep the 3D view on the selected entity. Harold at 10,5 was that snap. An In-on arrow belongs on the widget `interact_relay.txt`. The daemon relay steps the pet and follow walks them back. | Do not remove follow to make a cursor step stick. |
| mcl touch page | `city/ev/20` played on a player-touch. Touch rows survive a `bar.txt` rewrite. | Do not drop `player-touch` / `event-touch` / `on-touch` lines when the cast bar is rebuilt. |
| Editor book menu | `n_file_opts` lists `cdda-hq`, `mcl-hq`, `df-hq` with their own labels. A label that is empty or exactly `RPG-Pet` becomes the folder name. | Opening cdda-hq's own board is not the editor. |
| A pick changes the pane | Bar and `world_01` can move while the 3D frame stays the boot paint. The viewer redraws when `bv_screen_changed.txt` grows. The projector appends one byte when the book or page label changes after its first sample. | Do not call the pick done from the bar alone. |

## Written, still open, do not re-research

HOW2 "Seen" items 1 through 6. The pane used to miss the first paint.
In off means watch. Key `1` did not move the picture. The bar and
`trainer.txt` can name two rooms. Debug text sits on the view. Pets
are still the editor party. The long-range stop is still edit-then-play
on mcl, cdda, and df inside this editor.
