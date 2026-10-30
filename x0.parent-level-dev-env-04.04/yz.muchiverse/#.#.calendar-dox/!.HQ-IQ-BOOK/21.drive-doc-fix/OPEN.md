# Open, seen on a real frame

## Book-menu thumb is in the rows (2026-10-29 23:00)

Owner frame of the edit board, book menu open, page Bank. The scroll
thumb cuts through the middle of the book rows. On pc-hq the same
thumb is at the right of the menu.

Do not invent a scrollbar. `khtpm_core_render.c` already has both
pieces. `layout_dock_bar` adds `DD_STRIP_W` (62) and shrinks each row
by that strip. That is the right-hand thumb. The edit board's book
rows are sidebar `dropdown-child` items, laid out by the other pass,
which registers the bar at `dw + 66` against the trigger width. The
pet column `side-l` is `ov-scroll`, so its thumb also draws at the
right of the pet column, through the open menu. pc-hq's board has no
`ov-scroll` pet bar.

Next edit: skip `ov-scroll` while a dropdown is open, and use the
existing `DD_STRIP_W` split for the sidebar dropdown. Do not revive
the 12-row pals cap in `04-bugs/bug_bounty.md`. Full note:
`13.agent-coms/GROK/ONBOARD-GROK.md`.

Research queue. A row stays until a new window PNG changes and the
text file agrees. Do not launch a second board to collect these again.

## Why the session died in one second (2026-10-29, researched, not fixed)

The 18:35 ledger row for widget pid `3135383` is ONLINE at 18:35:19 and OFFLINE twice at 18:35:20, same pid, same session. The widget trap is installed on TERM and on EXIT, so one SIGTERM runs it twice and writes two OFFLINE lines. A parser that exited on its own would write one. `open_pchq_board.sh`'s orphan reap uses SIGKILL, which would not run the trap. The signal was SIGTERM.

That SIGTERM is `kill_own_board_widget` in `@.apps/piececraft-hq/button.sh`. It is `pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR"`. It runs at the start of every `run` when no session directory is younger than 5 seconds, and again from the engine shell's EXIT trap. Either a second `run`, or the engine shell exiting, deletes the widget session. The projector then publishes `no_session=1` and the pane stays on `loading...`.

A second hole, different signal: the engine is started with `setsid`, so its session leader's parent is 1 from the first moment, and the reap treats parent 1 as a dead orphan. It spares a session only when the orchestrator or the widget bash is already visible to `pgrep`. A second `open_pchq_board.sh` in the gap before that process exists SIGKILLs the new stack.

The 18:58 launch stayed up. Its engine shell was `3202906`, not the pid written to `engine.pid`. Stopping the window did not stop that shell. `quit_flag.txt` was empty, `orchestrator.log` was only the ready line, and the parser was about 0.7% CPU. That shell was stopped after this reading. The editor daemon that was already running was left up.

Fixed in `piececraft-hq/button.sh` and `open_pchq_board.sh`, not re-driven. A start-of-run kill now skips a widget whose session directory still exists, and skips an orchestrator whose parent is still `button.sh`. The quit path still signals them. The orphan reap no longer treats a live `setsid` engine shell (parent 1) as dead, and it does not delete a session directory younger than 5 seconds. A widget is reaped only when its own session directory is already gone.

Stray pollers on deleted session directories were stopped after that reading: four board-viewer stacks (renderer, parser, rgb) and `pc_trigger_watcher` pid 82433. The editor daemon pid 1400329 was left up.

## Page is on screen (2026-10-29 18:58)

One niced relaunch. `board/state/ui.txt` had `no_session=` empty and `canvas_raw` set to `rgb_frame_3d_overlay.raw` of session `1793325472-3202959`. Window `0xa00003`. Frame `/tmp/edit-page.png`: the pane is the bridge (star field, a wall, `rpg-space:bridge`), not `loading...`. The board from that launch was stopped after the frame. The editor daemon that was already running was left up.

The launch before this one had registered `board-viewer:rpg-edit-hq-pchq` and gone OFFLINE one second later, and the session directory was removed with it. That is why the pane stayed on `loading...`. The widget and the engine now copy `parser.log` and `orchestrator.log` to `/tmp` before that delete, so the next one-second exit can be read.

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
