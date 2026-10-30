# How to drive, document, and fix a board

Chapter `21.drive-doc-fix`. One place for any agent. Do this loop. Do not re-derive it.

House root, quote it:

```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

The edit board is `@.apps/rpg-edit-hq`. Grok's own reonboard is
`13.agent-coms/GROK/ONBOARD-GROK.md`. The open bug list is
`13.agent-coms/GROK/ROADMAP-EDITOR-2026-10-29.md`.

## What each run is for

The long-range stop is RPG Maker MV editing, then playing what that edit does, on this book and on the others. One run keeps going until a real failure blocks the next step, then fixes that failure.

The bar, in order:

1. Open a book and a page and see the place (the 3D pane is the page, not `loading...`).
2. Edit the environment on that page.
3. Edit event pages: the expected result and the effect.
4. Play it. Walk the page, fire the event, and read the result in the frame and in `events.txt`. An edit that cannot be played is not done.
5. The same edit, then the same play, on mineclonia (`mcl-hq`), Cataclysm (`cdda-hq`), Dwarf Fortress (`df-hq`), and any other book that uses the page file.

A failure-limit is a frame or a text file that shows the next one of those cannot be done. Chrome nits get written down and waited. They are not the stop. Do not open a second board to keep driving past a pane that has no session.

## Loop

1. Drive one thing a person would do. One key or one click.
2. Read the text (`trainer.txt`, `events.txt`, `board/state/ui.txt`) and one window PNG.
3. Write the mismatch on the list, with the frame path and what it ought to be.
4. Fix only the rows the owner has said to fix. Then drive that same click again. A new frame that changed is the proof. A compile is not.

Do not launch a second board while one engine is starting. `nice -n 15`. Stop only the pids you started. Do not `git add` under `xyzfs/users`. Commit on your own branch with an explicit pathspec. The pc-hq camera is the owner's. Do not restore it. The edit board's session camera is fair to drive.

## Launch

```
cd <house>/@.apps/rpg-edit-hq
nice -n 15 setsid nohup sh board/open_board.sh > /tmp/rpg-edit-drive.log 2>&1 < /dev/null &
```

Wait until the log says `board window launched`. The window is about 1260×556. Last time it was `0xa00003` at +140+96. Read the id from `xwininfo`. Do not reuse an old id.

`open_board.sh` reads `state/last_book.txt` (`book=` and `page=`). It links `maps/<book>` to that book's own map tree when the edit package does not ship it, then sends `CONFIRM_START_MAP:<book>` and `CONFIRM_SET_DESK:<page>`. A book is a directory: `game.pdl` lists `desk_N_id`, and each page is `maps/<book>/<page>/map.txt`. There is no combined sheet of every book.

## Pictures

| Question | File |
|---|---|
| 3D, what a person sees | `dump_frame_png_op.+x <window_id> /tmp/name.png` |
| 2D of the editor daemon | `<app>/state/scene.raw` plus `scene.receipt.txt` |
| Key `p` | `/tmp/entity-menu-frame.png` from `rgb_frame.raw` |

`p` is almost black on rpg-space and on the edit board. It is not the 2D map and not the 3D view. `scrot` is black on the GL path. Compare PNG bytes. A same hash means the click did nothing.

## Keys that stick

Arrows are 1000 left, 1001 right, 1002 up, 1003 down. Codes 200–203 do not move the xelector. `xdotool key Right` on the window did nothing.

The step that moves the 3D view is two lines on the **board session** relay, from `bv_session=` in `board/state/ui.txt`:

```
<bv_session>/pieces/apps/player_app/interact_relay.txt
1001
126
```

`126` wakes the engine. Key `1` is code 49 on the **daemon** relay `<app>/state/interact_relay.txt`. It is supposed to set pitch 6. On 2026-10-29 it did not change the picture. Do not claim it worked without a new hash.

A click that reaches the widgets:

```
DISPLAY=:0 xdotool mousemove --window <id> <x> <y>
printf 'MOUSE_EVENT: 1 <x> <y> 1\nMOUSE_EVENT: 1 <x> <y> 0\n' >> <house>/#.desktop/entity_menu_history/<render_pid>.txt
```

The render pid is the `khtpm_core_render` whose argv contains `pchq-board.xhtpm`. A bare `xdotool click` does not. After the click, read `menu_open` in `board/state/ui.txt`. Empty means the menu did not open.

## What the chrome is

`board/pchq-board.xhtpm`. The toolbar tabs use `onclick="ACTIVATE"`. The dropdown rows are `<item class="dropdown-child" target_id="tb-...">` in the side rail.

As of the fix in this same note:

- Book rows are `n_file_opts` / `f_*`, written by `pchq_board_projector.c` from `pieces/system/maps/*/game.pdl`. The action is `pchq_board_action.sh <bv_session> load-map <id>`.
- Page rows are `n_desk_opts` / `d_*` from that book's `game.pdl` desks. The action is `pchq_board_action.sh <bv_session> desk <id>`.
- The old page rows ran `rpg_edit_hq.+x fire goto` on the editor daemon's Islands rooms. That listed Bank while the bar said `rpg-space`. Do not put that repeat back.

The projector writes `board/state/ui.txt`. The window vars list the daemon `ui.txt` first and the board file after. Later files override. `book_label` in the board file is the one on the bar.

## Seen, and what ought to be

Still open unless a new frame says otherwise:

1. The 3D pane does not fill on the first frame. It filled only after the view changed. It ought to fill on the first paint.
2. In was off on open. Off ought to mean watching. On ought to mean the arrow moves the body in the picture.
3. Key `1` did not change the picture.
4. The bar can say `rpg-space:bridge` while `trainer.txt` says `room=store`. One key must not move two Harolds. `follow_xelector` in `petslike_core.c` already returns when `map_id` is not this app. A `STEP` in another room during a bridge view means something else is still stepping the daemon.
5. Debug text (`pos`, `ray`, `pid`) sits on the view. It ought to be a toggle.
6. Pets and chat stay the editor daemon's party. They ought to be the open book's event pages, or be labeled as the other game.

Fixed in the toolbar file, not yet re-driven:

7. Book and page menus now use the loaded map's `game.pdl` (file rows and desk rows) instead of the daemon's Islands list. Re-drive: open the book tab, confirm a row named rpg-space and a row named rpg-edit-hq, pick one, and confirm the bar and one frame.

## Harnesses, by name

From `&.widgits/_shared-lib/harness`, `prisc <name>.pal`. `find` does not run them.

| pal | what |
|---|---|
| `edit_last.pal` | last book, or Islands page `start` |
| `edit_doors.pal` | door FSM. One proved row: Islands `4,4` + up to Bank `12,13` |
| `edit_open.pal` | text open of `book:page`. Real call: `rpg-space` `bridge` event `col_breach` |

Add a door `RULE` only after a `STEP` line proves it.

## Stop

Kill the pids whose argv contains the session id from this launch, the `pchq-board.xhtpm` render, and `bv_render_3d.+x --daemon` if you started it. Leave a daemon you did not start.
