# Already drive-doc-fixed

Read this before opening a board. A row here has a frame or a file
proof in `PROOF-2026-10-29.md`. Do not launch a window to re-check it.
The next run starts at the first row that is not in this file.

The loop itself is `HOW2-DRIVE-DOC-FIX.md`. Open bugs that are written
and still open stay in `OPEN.md` and in HOW2 "Seen". Those are not
redone either. They wait for a named fix.

Check without a window: `sh harness/editor-skip.sh` from this folder.

## Closed

| row | what was shown | leave it |
|---|---|---|
| Event fire on the edit board | Bridge `ev/10` `player-touch` wrote `Play event page_1` after `touch_event_at` called piececraft `tsots_touch_event.sh` with the real project. | Do not point the touch script at the board-viewer session. |
| In is toolbar tab 1 | `tb-in` nav 1, `tb-book` nav 2. Matching LyfeDesk by moving In after the clock was the wrong move. | In stays option 1. |
| One page row, both pictures | A sprite row stamped the 3D overlay and the daemon `scene.raw`, then both went to 0 when the row was removed. | Both pictures, or it is not an edit. |
| Voice stays on | `flag_voice.txt` stays `1`. `speak()` returns when `no_session=1`. A closed window must not keep a wav playing. | Do not write the voice flag to 0 to test. |
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
