# Open, seen on a real frame

Research queue. A row stays until a new window PNG changes and the
text file agrees. Do not launch a second board to collect these again.

## Failure-limit (2026-10-29): no engine session, so no page to edit

`board/state/ui.txt` still says `bv_session=` empty, `canvas_raw=` empty, `no_session=1`. That is why the pane stayed on `loading...` after the chrome had `book:rpg-space` / `page:bridge`. Nothing on the page can be edited until this row clears.

What the files showed, with the window already closed:

- Launch log `/tmp/rpg-edit-menu.log` started an engine and then said the board window launched. It did not print the "engine not registered" line.
- Engine log `/tmp/pchq-engine-rpg-edit-hq-pchq.log` reached "Board widget launching". The pid in `rpg-edit-hq-pchq/pieces/system/engine.pid` (3135209) is dead.
- `session_dir.txt` names `rpg-edit-hq-pchq/pieces/sessions/1793324117-3135212`. That directory is gone (the engine trap removes it on exit).
- The projector looks up `board-viewer:rpg-edit-hq-pchq` through `ledger_peers`. It published `no_session=1` the whole time the window was up, so that lookup never returned a live directory with a frame.

Next fix is that lookup, then one niced relaunch. Proof is `no_session` empty, `canvas_raw` set, and a window PNG of the page with no `loading...`. Do not relaunch while another board engine is starting.

## Stuck `loading...` (2026-10-29 18:40)

Frame `/tmp/stuck-load.png`, window `0xa00003`, 1260×556. The toolbar,
pets, and chat were drawn. The center pane was sky blue with the word
`loading...`. `board/state/ui.txt` had `canvas_raw=` empty,
`book_label=book:rpg-space`, `page_label=page:bridge`, `menu_open=`
empty. The paint stayed after `pet_tts_local.py` had exited, so the
speech burst is not what holds the pane.

The string is drawn by `&.widgits/_shared-lib/khtpm_draw_core.c`
`kh_draw_canvas_loading` (the literal `loading...`). The same symptom
is already written for rpg-dust and rpg-space: the projector wrote
`state/ui.txt` into the template folder and found no engine session,
so the pane stayed on that panel (`@.apps/rpg-space/README.md`).
`open_pchq_board.sh` now keeps the engine log and retries once
(2026-10-26). The edit board's copy of that path is the next read:
which session id the edit window's projector looked up, and whether
`canvas_raw` was ever set.

CPU next to it, same evening: guard sample 18:33, busy 99%, house
share `pet_tts_local.py` ~147% (text `hi Pal 15!` into
`rpg-edit-hq/state/audio/`) and `rg` 247%. Chrome was also at 98%.
Guard ended 0 house processes. The editor daemon `rpg_edit_hq.+x`
pid 1400329 was already running and can greet pals with no board
window. That is a separate cost from the stuck pane.

## Still open from the drive (same day)

1. First 3D paint does not fill the pane. It filled only after the view changed.
2. In was off on open.
3. Key `1` (code 49 on the daemon relay) did not change the picture.
4. Bar can say `rpg-space:bridge` while `trainer.txt` says another room.
5. Debug text sits on the view.
6. Pets and chat are the editor daemon's party, not the open book's events.
7. Book menu file rows stayed `n_file_opts=003`, label `default-legacy`. The xhtpm repeat is wired. The projector did not list `rpg-space` or `rpg-edit-hq`. Click at (200, 22) left `menu_open` empty. Frame `/tmp/ux-bookmenu.png`.
