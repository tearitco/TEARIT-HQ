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

## CPU first

CPU safety is paramount. It outranks the next click. Before a launch, and again whenever the machine feels hot or `uptime` load climbs, run:

```
sh '&.hq-apps/proc-mon/guard.sh' status
```

Read `busy=` and `top=`. If busy is up, open the newest file in `&.hq-apps/proc-mon/state/incidents/`. A process is house when its executable is under the house root, or its script path is, even if the guard labels the interpreter `outside` (python, rg). Add each house name to the list below, with the sample time and the percent of one core. Do not call it Chrome. Stop the board you started before you keep driving. Leave a daemon you did not start. After that review, delete `state/cpu_guard_alert.txt`. The rule in full is `03-pitfalls/hq-cpu-safety.md` section 8 and section 9.

House, seen on the guard, 2026-10-29:

| when | busy | what | note |
|---|---|---|---|
| 18:33 | 99% | `pet_tts_local.py` ~147%, `rg` 247% | pals line `hi Pal 15!` into `rpg-edit-hq/state/audio/`. Guard ended 0 house processes. |
| 19:20 | 99% | `rg` 186%, `pet_tts_local.py` 113%, board `prisc+x` 11% and 10% | Guard classed the python and rg as outside because the exe is not under the house. The script and the search are ours. Answer was Yes, so rg ended. The board from the book-tab click was still up. |
| 19:22 | 62% sample, then one core at 238% | `pet_tts_local.py` text `Pal 13! over here` | Board was already stopped. The editor daemon kept speaking. That process was stopped. The daemon was left up. |
| 19:28 | 42%, die 86 C | `pet_tts_local.py` 155% | After the player-menu drive. The speech process had already exited when checked. Daemon left up. |
| 19:37 | 100%, die 54 C | `rg` 296% (two pids), `pet_tts_local.p` 123%, `lc_clock.+x` 105% | Incident `20261029-193740-ACT.txt`. Guard classed rg and the python as outside. The clock pid 1400343 is the edit daemon's clock and was labeled HOUSE. Guard ended 0 house processes. Both hot processes had exited by the review. |
| 19:40 | 27%, die 86 C | `pet_tts_local.p` 145% | During the xelector-start open. Board stopped. Daemon 1400329 left up. |

`pet_tts_local.py` is the editor daemon speaking pal chat. It starts with no board window. Do not leave an edit board up while it is firing. 2026-10-29: there was no flag file, and the default is on. The wav cache was one file per pet, so the same line was synthesized again for each pal (278 files, 62 lines, matching pairs byte-identical). `reuse_line` in `petslike_core.c` hard-links an existing wav of the same words and the same voice. Voice stays on. Do not write `flag_voice.txt` to `0` for a test. The hot CPU was orphan speech after the window closed. `speak()` returns when `viewer_kinds()` is 0, and a line still playing is stopped from `state/audio/play.pid`. A closed window stays quiet.

## Loop

1. Drive one thing a person would do. One key or one click. Check the guard first. If busy is up and the top process is house, add it to the table above and stop. Do not drive through it.
2. Read the text (`trainer.txt`, `events.txt`, `board/state/ui.txt`) and one window PNG.
3. Write the mismatch on the list, with the frame path and what it ought to be. On the first frame, also fill one row of `PLAY-SENSE.md` for anything a person would notice before they know the bug list: where the xelector starts, whether that cell is center, a door, or a wall, whether the camera is on that cell, whether In is off, whether debug text covers the walk. Chrome rows go on `CHROME-COMPARE.md`. A feel row is written even when the owner has not named it. It is not fixed until they do. The see stage reads the book dropdown before it says a book can be edited. The rows are `n_file_opts` and `f_*_label` / `f_*_arg` in `board/state/ui.txt`. A book that is not an `f_*_arg` is not in the menu. Two rows with the same label are a miss even when the args differ. Opening that book's own board is not the editor.
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

The render pid is the `khtpm_core_render` whose argv contains `pchq-board.xhtpm`. A bare `xdotool click` does not. The toolbar sits at about y=48, not in the title strip at y=22. The book tab on a 1260-wide window is about x=221.

Clicks are two-step (`click_two_step=1`). The first click only focuses the tab (the `[>]` badge). The second click on the same spot runs `ACTIVATE` and the rows draw. `menu_open` stays empty either way. This template does not use `menu.txt`. Proof the menu is open is a row in `#.desktop/entity_menu_frame_<render_pid>.txt` with a y greater than 0 (`fo-0` at y=63 under the book tab, 2026-10-29). A closed dropdown is omitted from that file.

## What the chrome is

`board/pchq-board.xhtpm`. The toolbar tabs use `onclick="ACTIVATE"`. The dropdown rows are `<item class="dropdown-child" target_id="tb-...">` in the side rail.

Toolbar and menu rows follow LyfeDesk. Before you say they match, fill every row of `CHROME-COMPARE.md`. A sentence here is not that check. The clock is strip button 11. The pid cell is not a strip button and is `no-nav`. The source is `#.desktop/livedesk_taskbar.pdl` (`player_menu_*`, `strip_btn_*`). The edit board's tab order is In, then that strip: book, page, pals, player, db, shop, menu, clock. In stays tab 1. `731247c9b` moved it after the clock and the owner put it back. The pid and load cell is `class="no-nav"` so it draws and takes no `[ ]` number. Buttons the strip does not have (palettes, edit, plugins, network, tools, h-ai) are not invented here. Do not invent a row on book or db that LyfeDesk puts on player. blueprintz is `player_menu_4`, after stop, opened by `_.monads/_.livedesk-taskbar/ops/open_blueprintz.sh`. It is the debugger, not a book-menu row. Driven 2026-10-29: two clicks at (985, 48) on the player tab. `entity_menu_frame_3312177.txt` has `pm-stop` at y=153 and `pm-blueprintz` at y=183. It is not on the book menu.

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

7. The book menu used to show two rows, both labeled `🏠 RPG-Pet`. The see stage now reads `f_*_arg` before calling a book editable. As of `aef3bbf25` the editor open publishes `cdda-hq`, `mcl-hq`, and `df-hq` as their own labels. A pick that only rewrites `world_01` updates the bar and leaves the 3D pane on the boot frame: the viewer raymarches when `bv_screen_changed.txt` grows, and a book change did not grow it. Session `1793338561-26401` had `book:cdda-hq` / `page:Bank` while the overlay was still the 22:36 boot hash `d9abcbb8`. One appended byte redrew it (`b9117028`, a 13 by 8 room, `/tmp/cdda-bank-pane.png`). The projector now appends that byte when `book_label` or `page_label` changes after its first sample. A later `CONFIRM_SET_DESK:city` grew the marker from 1 byte to 2 and the overlay hash moved from `b9117028` to `98ebc85a` with the bar on `page:City`. The desk was set back to `bank`.

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
