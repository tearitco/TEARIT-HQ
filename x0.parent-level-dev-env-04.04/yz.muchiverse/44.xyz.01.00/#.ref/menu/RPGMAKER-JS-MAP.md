# RPG Maker MV to this house

For Claude, when Grok is not in the room. The installed game is The Sword of the Spirit (RMMV_TSOTS). Its scripts are the stock MV files under that project's `www/js/`. On this machine the tree is the desktop folder whose name starts with the robot emoji, then `xv...RMMV+sec]linux.../RMMV_TSOTS] LINUX.../__.Tearrmv SpaceShop388.m/www/js/`. Line numbers below are that copy, 2026-10-09.

House side lives under `44.xyz.01.00`. Events-hq reads `#.ref/menu/event_commands.registry.pdl` with no rebuild. Add Command shows every `COMMAND` block.

## Title, start, stop

| MV | File | What it does | House |
|---|---|---|---|
| New Game | `rpg_scenes.js` `Scene_Title.prototype.commandNewGame` line 509 (handler wired at 503) | `DataManager.setupNewGame()`, then the map | `COMMAND start_game` runs `@.apps/piececraft-hq/ops/doom_event.sh start_game <book> <desk>`. Writes `#.desktop/khtpm_play_mode.state.txt` `mode=on`, resets `pieces/system/maps/doom/state.pdl` when the verb is the Doom one, inbox `CONFIRM_START_MAP:<book>`. |
| Continue | `rpg_scenes.js` `commandContinue` line 516 | Opens the load screen | `COMMAND load_game` and Player menu Load N. |
| Open Save Screen | `rpg_objects.js` `Game_Interpreter.prototype.command352` line 10474 | `SceneManager.push(Scene_Save)` | `COMMAND save_game`. Scene_Save starts at `rpg_scenes.js` line 1681. Scene_Load at 1734. |
| Game Over | `command353` line 10482 | Gameover scene | No separate scene. Stop is the play flag. |
| Return to Title | `command354` line 10488 | Back to the title scene | `COMMAND stop_game` writes `mode=off`. Events do not fire while play is off. `verb.txt` of `start_game` or `stop_game` still runs from the bar so a stopped game can start. |

Player menu Play calls `player start`, Stop calls `player stop`. Both go through `pchq_board_action.sh` into `doom_event.sh`. That is the same script as the event commands. The old `toggle` verb is still in the script for the hascanvas template.

## Save slots

Stock count is `DataManager.maxSavefiles` in `rpg_managers.js` line 333 (MV returns 20). Save is `DataManager.saveGame` line 337 and `saveGameWithoutRescue` line 370 (`StorageManager.save`, `globalInfo[id] = makeSavefileInfo()` at 378 and 419). Load is `loadGameWithoutRescue` line 383.

This house uses 16 slots, the constant `GAME_SLOTS` in `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` (`livedesk_build_game_slots_menu`). Files:

`xyzfs/users/<uuid>/home/livedesk/savegames/slot_NN/`

- `book.pdl` is what pc-hq restores: `map_id`, `desk_id`, `mode`, plus a copy of that book's `state.pdl`.
- `meta.pdl` `SLOT | saved_at` is what the taskbar row label reads.
- `manifest.txt` is only written by `&.widgits/_shared-lib/ops/game_slot_op.c`. That op hashes the pal tree. It does not copy entities and its load does not put them back.

pc-hq Player menu (reopen the board window to pick up `pchq-board.xhtpm`) has Save 1-8 and Load 1-8. Slots 9-16 are the taskbar Player cell, and any slot 1-16 is `save_slot` / `load_slot` on `doom_event.sh`. A load writes one inbox line, `CONFIRM_SET_DESK`, because `pc_menu_input.c` keeps the first line only. The book id is already in `pieces/world_01/state.txt`. Do not `git add` anything under `xyzfs/users`.

## Map events the converter already emits

`Game_Interpreter` in `rpg_objects.js`:

| Code | Line | MV name | House today |
|---|---|---|---|
| 101 | 9058 | Show Text | `mr_show_text.+x` from the page `cmd_N.sh` |
| 121 | 9416 | Control Switches | Comment in `cmd_N.sh`, `exit 0`. Page conditions are not evaluated. |
| 122 | 9424 | Control Variables | Same. |
| 125 | 9591 | Change Gold | Same. |
| 201 | 9709 | Transfer Player | Same. Doom `next_level` is `CONFIRM_SET_DESK`, not this command. |
| 301 | 10130 | Battle Processing | Doom `start_battle` increments `battle` in `state.pdl`. |
| 311 | 10202 | Change HP | Doom `change_hp` adds 10 to `hp` in `state.pdl`. Not `mr_change_hp`. |
| 351 | 10465 | Open Menu | Not wired. |
| 352-354 | 10474-10488 | Save, Game Over, Title | `save_game`, `stop_game` as above. |

Packages: `pieces/system/maps/<book>/<desk>/ev/<id>/event_pkg/pages/page_N/`. Bar list is `bar.txt`. Touch while play is on: `ops/tsots_touch_event.sh`.

## Doom book and Toys

`pieces/system/maps/doom/`. Title desk events 1 and 2 are New Game and Quit (`verb.txt` `start_game` / `stop_game`). E1M1 pages call `doom_event.sh` with the verb.

Toys scans any directory that has `toy.pdl` (`livedesk_build_toys_menu` in `khtpm_taskbar_manager.c`). `@.apps/doom/toy.pdl` title Doom, launch `open.sh`. That sets the world to book `doom` desk `title`, runs `start_game`, then `open_pchq_board.sh`. Piececraft-HQ stays its own toy.

Freedoom wad is `#.NNEST_ASSETS/doom/freedoom1.wad`. Not in git. Map geometry for E1M1-E1M3 is already desks. Textures and sprites are not extracted.
