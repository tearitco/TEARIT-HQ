# 2026-09-23 — Actors list, relay, frame history

Desk pals Ember, Glacine, Murmur, Solvent, Asa, Ava, and Cursword are
actor rows 5–11. The live Database window listed them (`Actors (11)`
in `db-hq-pal/state/ui.txt`). Rows were written into `actors.pdl` and
`db_hq_actors.state.txt`. Not fixed below; written down so the next
pass can pick them up.

## Frame history (done this pass, TPMOS pattern)

`1.TPMOS_c_+rmmp.0103.0001/pieces/display/renderer.c` keeps two files.
`pieces/display/current_frame.txt` is the live snapshot. On each real
render it appends that snapshot to
`pieces/debug/frames/session_frame_history.txt` under a
`--- FRAME UPDATE at <time> ---` line. A new process truncates that
log once and writes `=== NEW SESSION at <time> ===`. It does not
truncate on every frame.

db-hq-pal now does the same for its text frame. `state/ui.txt` stays
the live snapshot. `ops/frame_history.sh`, started by `button.sh`,
wipes `debug/frames/session_frame_history.txt` once per launch and
appends only when `ui.txt` changes. The key mailbox
`#.desktop/entity_menu_history/<pid>.txt` is still a different file
and the renderer still truncates that one on startup.

## Still open

1. **No add-actor control.** `dashboard.xhtpm` can show a row and edit
   a field. It cannot create an actor. The seven rows exist because
   the data files were edited. There is no GUI gesture for IRL to
   learn that step from.

2. **Click count is a setting, not something to infer.**
   `#.desktop/hq_ui.pdl` has `click_two_step=1`: first mouse click
   focuses, second click activates. `0` would be one click. Keyboard
   Enter does not read that key; one Return calls `activate_focused()`.
   Eight Downs and one Enter opened States without a two-step click
   on Actors. That is not a list-arrow bug, and it is not a reason to
   edit tab code. The k9 addendum that said one activation locks the
   sidebar was wrong and is corrected in the same file.

3. **`db_hq_history.txt` does not drive this window.** The harness
   `nav.sh` appends there for the old `g_is_db_hq` path.
   `class="db-hq-pal"` leaves that path dormant. Keys for this window
   go to `entity_menu_history/<pid>.txt`, and that file is truncated
   when the renderer starts. The frame log above is the review copy.
