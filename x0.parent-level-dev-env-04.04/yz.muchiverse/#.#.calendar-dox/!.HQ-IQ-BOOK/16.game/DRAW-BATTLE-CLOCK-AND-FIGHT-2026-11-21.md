# Draw Battle: why they do not fight yet, and the player clock

Research only, 2026-11-21. Nothing here was turned on. `autotick_enabled` stays 0.

## Why the pieces stand still

A place writes `STROKE|team|id|x|y` and redraws `events.txt` from the ledger. Movement and damage happen only inside `db_battle_op tick`, which appends one `TICK|n` and then replays every tick over the strokes. The live ledger (`@.apps/draw-battle/battle/ledger.txt`) has strokes and **no `TICK|` line**, so every body is still on the cell it was painted on.

The game bar already has a Tick button. Its action is `db_battle_op step`, which appends one `TICK` and rewrites the desk. Nobody has pressed it in this session. The place listener does not call `tick`. Pressing Tick once is one step of the fight. It is not a clock.

## The world clock is a different clock

`draw-battle-pchq/pieces/world_01/state.txt` has `autotick_enabled=0`. Key `]` is `TOGGLE_AUTOTICK` and `[` is `CYCLE_TICK_SPEED` (`pieces/system/keybinds.pdl`). That flag is read by `pc_clock_daemon`: it advances `game_time_epoch_sec` and, once per game minute, the piececraft world tick and `tick_animals`. It does not read the Draw Battle ledger and it does not call `db_battle_op`.

Turning `]` on would move the sun and the piececraft animals. The painted soldiers would stay put. Do not turn it on to test the fight.

## A board where the clock works: rpg-space

Checked 2026-11-21 in the source, not by opening the window. rpg-space, rpg-pet, mcl-hq, cdda-hq and civ-rpgce all ship the same toolbar. rpg-space's own window is `@.apps/rpg-space/rpg-space.xhtpm` and its board copy is `board/pchq-board.xhtpm`. Both have `tb-time` labeled `${time_label}`. The menu rows call `rpg_space clock rate|advance|start|stop|reinstall`. That binary is `petslike_core.c`.

What that clock actually is:

- The label comes from `clock_ui.txt`: `time_label=D<n> HH:MM`, plus `time_rate` and `time_running`. The label is rewritten at most once per 5 seconds so the bar does not reparse every tick.
- Start / stop / rate go to the LyfeDesk clock (`lc_run`, `clock_install`), not to `pc_clock_daemon`. Play also sets `running=1` and starts that daemon. Stop freezes it. A second mode, `timemode=rogue`, moves time only when you act. `timemode=live` keeps running while In: is on.
- Due moments are rows in `time.pdl` (`AT | name | HH:MM | every:30min`). The resident daemon fires each one once and runs the event page. The menu's advance verb does not also fire them, or the page would run twice.
- Hunger on rpg-space is one of those firings (`hunger_ms` in the life notes). The fight step for Draw Battle would be the same kind of row: the clock fires, the page calls `db_battle_op tick` once.

`pc_clock_daemon` and `]` remain the sun clock. rpg-space does not use that daemon as the thing that steps the game.

## Player tab and time tab are two menus, and both are missing

Read from `@.apps/rpg-space/board/pchq-board.xhtpm` on 2026-11-21. The toolbar only declares the tab. The rows of a menu are sidebar items whose `target_id` is that tab. A tab with no matching items opens an empty menu. Draw Battle's `board/pchq-board.xhtpm` has In, book, and page, and the sidebar items for those two menus. It has neither tab.

`tb-player` is the run switch, not a person on the board. Its label is `${run_label}`. `petslike_core.c` writes that into the game `ui.txt` as `Player: playing` or `Player: stopped` (`run_on` is the `running` flag). The rows under it are INT, the rogue/live line, Play, Stop, Save, Load, and the two Synch directions. Play sets `running=1`, installs the clock, and `daemon-start`. Stop sets `running=0` and pauses the clock. `clock_sync` pauses whenever `running` is off or In: is on (turn mode).

`tb-time` is the clock face. Its label is `${time_label}` from `state/clock_ui.txt`. On rpg-space the rows under it are speed, skip, and reinstall. Start and pause are not on this menu. They are Play and Stop on the player tab. The window `vars` list names `../state/ui.txt` then `../state/clock_ui.txt`, then the board files. A later file wins, which is why the board's `canvas_raw` beats the game file, and why `time_label` has to live in `clock_ui.txt` or a later file. Draw Battle's vars are `game_ui.txt`, `pick_ui.txt`, and the board ui files. `clock_ui.txt` is not in that list, so a time tab added alone would render a blank label.

Draw Battle's In: tab must stay `pchq_board_action.sh interact`. On rpg-space that same tab calls the game `int` verb. `clock_sync` pauses when that flag is on (`turn_mode`). That flag is not the engine In: switch. This window's In: is on so arrows and the xelector work, and a place needs that. Copying `clock_sync`'s pause-while-int rule would freeze the fight for the whole session. The fight runs while Player is playing, whether or not engine In: is on.

The editor already has these tabs (`rpg-edit-hq/board/pchq-board.xhtpm`). Its Play starts `game.pdl`'s driver, which for this book is `game_core driver`. That driver ticks a document. It does not call `db_battle_op`. `GAME | clock | pause` only holds the editor's world clock while `gm_on=1`. It is not the fight clock, and the row stays. A second press on `tb-player` closes an open menu (`deskopoly/nav.pdl`). The rows to add on this window are Play and Stop under `tb-player`, and speed plus reinstall under `tb-time`. Save, Load, Synch, and INT stay on the editor.

## The top bar is the face of that clock, and it is not on this board

rpg-edit's board toolbar has the same tabs. The two that matter:

| Tab | What it is | Draw Battle board |
|---|---|---|
| `tb-player` | Play / Stop / save. Label is the run state. | absent |
| `tb-time` | The clock label, plus speed (turbo, 1 hour/sec, 1 min/sec, real time), skip, start, pause. Actions are `rpg_edit_hq clock ...` | absent |

Draw Battle's `board/pchq-board.xhtpm` has In, book, and page only. That is why the player clock and the top toolbar "are not added yet." They are the rpg-edit toolbar rows, not a second fight implementation.

## How they relate

They are the same idea and must not be the same process.

- The **clock** decides when a step is due (paused, one key, or real time). Minecraft-style realtime, CDDA-style turns, and DF-style pause are three sources for one step. Owner lock: two clocks, one world. A mode is a clock source, not a second battle loop (`16.game/RPG-DUST-DESIGN.md`).
- The **step** is already the op. One `TICK` row, then project `events.txt`. D93: a tick calls the op. An event page authors that call. The clock must not fork a new process every frame, and the fight must not move into `game_core` or into `pc_clock_daemon`'s animal tick.
- The **toolbar** only shows the clock and sends start / pause / speed. It does not move units. `tb-time` on this board should call Draw Battle's clock verb, the way rpg-edit's rows call `clock start` and `clock rate`.

## Built on the Draw Battle window (2026-11-21)

`board/pchq-board.xhtpm` now has `tb-player` (`${run_label}`) and `tb-time` (`${time_label}`). The rows are Play, Stop, the three speeds, and reinstall. They call `db_battle_op play|stop|clock`. The clock id is `draw`, root `state/clock_root`, and there are no reminders. While `run_ui.txt` says `running=1` the listener appends one `TICK` every 250 ms and bumps the screen. Stop pauses the clock and the steps. In: is still the engine interact tab. A proof play wrote three `TICK` rows and Stop ended it. The labels on the relaunched window read `Player: stopped` and `D1 06:00`.

## Research notes (the shape the build follows)

The binary is `&.widgits/livedesk-clock/ops/+x/lc_clock.+x`. rpg-space keeps a private root at `<app>/state/clock_root` and a private clock id. Draw Battle gets its own root and its own id, so it never writes the rpg clock. Install is the same argv rpg-space uses: `new`, `rate`, `settime`, `pause`, then one `reminder-add` per `AT` row. Start is `daemon-start` with `LC_CLOCK_EVENT_RUNNER` set. Stop is `pause`, then `daemon-stop`. The label is `state/clock_ui.txt` (`time_label`, `time_rate`, `time_running`), rewritten at most every 5 seconds. The tab is `tb-time` with `${time_label}`, the same rows as `rpg-space/board/pchq-board.xhtpm`, calling `db_battle_op clock ...`.

A fight step is not an `AT` reminder. The finest repeat is `every:1min` of game time. `lc_clock.c` `rate_mult` is game-ms per real-ms: `min` = 6 (about one game minute per 10 real seconds), `sec` = 360. A reminder at that speed forks an event page every firing. Measured on the pet clock: a page is about 1 CPU-second, and a fast schedule burst to 35–44% of a core. `events/tick/cmd_1.sh` already execs `db_battle_op tick`, so a reminder would pay that fork on every step.

The step stays in the listener that is already alive for clicks. While `time_running=1` it appends one `TICK` at a capped wall rate (4 a second), rewrites `events.txt`, and bumps `bv_screen_changed.txt`. Pause writes nothing. The manual Tick button stays one step. `]` and `pc_clock_daemon` stay the sun.

Confidence this shape is the one that matches the working boards and the CPU notes: high. The part that would be wrong is firing the fight from `every:1min`.

## What not to do

- Do not set `autotick_enabled=1` on the live world to make them fight.
- Do not call `tick_animals` for these units.
- Do not hard-code the step inside `game_core.c`.

## Where it opens from (2026-11-21)

The owner wants both. Today only the book list is wired. The live File menu and the live toys dropdown were not opened for this note. The finding is the scanners and the files they read.

**rpg-edit File menu, yes, by the book scan.** `pchq_board_projector.c` builds the File menu from `bp_list_books` (`&.widgits/_shared-lib/pchq_book_path.c`). That walk is every folder under `<app>/<app>-pchq/pieces/system/maps/` that contains `game.pdl`, plus the instance's own maps and the piececraft store. The cap is 64 names. On this house the app-book count is 19, and `draw-battle` is one of them. The file that qualifies is `@.apps/draw-battle/draw-battle-pchq/pieces/system/maps/draw-battle/game.pdl` (`GAME | label | Draw Battle`). A click is `load-map` with arg `draw-battle`, which opens that book inside the editor. It does not start a second Draw Battle window. The app-root `game.pdl` (`@.apps/draw-battle/game.pdl`) is not what the scan reads.

**Toys dropdown, no.** `livedesk_build_toys_menu` in `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` lists a folder only when `toy.pdl` is present (house root, `@.apps`, `&.widgits`, `&.hq-apps`). The row runs `livedesk:open-toy:<dir>/<launch>`, and `launch` defaults to `button.sh`. rpg-edit-hq has both files (`toy.pdl` title `rpg-edit-hq`, launch `button.sh`). Draw Battle has neither `toy.pdl` nor `button.sh`, so the scan skips `@.apps/draw-battle`. The window used in this session was started by `piececraft-hq/open_pchq_board.sh` with `PCHQ_PKG=draw-battle-pchq`, which is a third path and not either menu.

Added 2026-11-21. `@.apps/draw-battle/toy.pdl` (title Draw Battle) and `button.sh` export `PCHQ_PKG` and `PCHQ_BOARD_TPL` and exec `piececraft-hq/open_pchq_board.sh`. The taskbar picks the file up the next time the toys cell opens. No taskbar rebuild. The live menu was not clicked.

Robot Trap Auto is the same toys shape, not a board yet. `@.apps/rta/toy.pdl` (title Robot Trap Auto) and `button.sh` open the HTML reference named in `reference.path` (`rta-v3.html` under the Desktop RTH-X car folder, title "Robot Trap Auto – Charleston Battery"). That page is the GTA-like example, the way the Draw Battle HTML was an example. It is not in the rpg-edit book list: there is no `game.pdl` map folder, and `load-map` would not be a game. The port (audit, report, conversion, presentation) is still ahead. The book row is part of that port.
