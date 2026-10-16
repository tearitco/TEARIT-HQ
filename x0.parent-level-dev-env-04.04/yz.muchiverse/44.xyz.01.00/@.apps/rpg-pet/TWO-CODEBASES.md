# rpg-pet: the two front ends and the one daemon behind them

Written 2026-10-11 at the owner's request ("document, open up to .pdl and have more information about these different 2 codebases for pet"). Read this before touching INT, keys, selection or the scene. Companion docs: `HOW-TO-RPG-PET.md` (user guide + dated update log), `ROADMAP.md`, `tuning.pdl` (numbers), `keybinds.pdl` (keys).

## The owner's standing rule (said more than once, 2026-10-11)

> "this all works in rpg-pet old code. why dont u copy it?"  (said twice in one session: first about INT / arrow keys / selecting the pet, then again when the board's arrows and the click on Blip still did nothing)

**When the old window already does something, read how it does it and reuse that, do not invent a second mechanism on top of the engine.** Three times this session a board-only mechanism (the engine placer, the engine's interact script, the Enter watcher) drifted away from what the old window did and the owner found it broken by hand. The fixes that worked each came from asking "how does the old window do this?":

| Behaviour | Old window (`rpg-pet.xhtpm`) | Board (`board/pchq-board.xhtpm`) now |
|---|---|---|
| INT button | tab `onclick` runs `rpg_pet int` (the verb) | **same**: tab `onclick` runs `rpg_pet int` (was the engine's `pchq_board_action.sh interact`, which drifted) |
| INT on = roguelike, off = auto | daemon `rogue_mode()`; clock stops, pet timers wait | same daemon, same flag `int` (state/world/switches.txt) |
| Arrow keys in INT | window forwards keys to rpg-pet's OWN relay (`rp_h1`,`rp_h2`); the daemon's key loop does `do_step` on the controlled mover | window forwards keys to the ENGINE's relay (`bv_h1`,`bv_h2`); the engine's Interact mode moves the xelector; the daemon's `follow_xelector` moves the chosen monster one cell to it |
| Esc | daemon sees `KEY_PRESSED: 27` in its history file, INT off | engine leaves Interact; the daemon sees the board's Interact flag drop and turns INT off (and INT off sends Esc to the engine) |
| Select a pet | pets dropdown / sidebar row -> `rpg_pet select <id>`; **a click on the pet in the painted scene** | pets dropdown / sidebar row -> `rpg_pet select <id>`; **a click on the pet or the trainer in the 2D scene** (daemon reads the house file `#.desktop/pchq_canvas_click.txt`, maps pixel -> cell, same `select` verb) |
| View follows the selection | `calc_view()`: `follow` flag -> the room of the selected pet | same; the board's engine also gets the selected character's room exported (`follow_pchq`) so the xelector and arrows work in the new room |

## The two front ends

**Old toys-menu window** — `rpg-pet.xhtpm` + `rpg-pet.css` (98 lines). Same GUI as `@.apps/pet-trainer`. The scene is painted by the daemon itself (`render()` in `ops/rpg_pet.c`, RPG Maker tilesets and character sheets blitted into a raw RGBA frame) and shown through a `<canvas sprite="${canvas_raw}">`. Everything it shows is a variable in `state/ui.txt` (written by the daemon: `out_all`). Keys: the khtpm window, while the INT tab is `interact-active`, appends key codes to `state/interact_relay.txt` / `state/keyboard/history.txt` (`rp_h1`,`rp_h2`); arrows arrive as 1000-1003 (left, right, up, down), see `keybinds.pdl`.

**pc-hq board window** — `board/pchq-board.xhtpm` + `pchq-board.css` + `board/open_board.sh` (97 lines). The window is a pc-hq board (`piececraft-hq`'s board-viewer engine session under `&.widgits/board-viewer/pieces/sessions/<id>/`): the engine renders the 2D and 3D views, the camera, the sun and moon, the xelector. rpg-pet is a **module** of that board: the daemon exports its rooms as maker maps (`pieces/system/maps/<map>/<desk>/`, `follow_pchq`), publishes the clock and sky (`publish_sky`, `write_room_sky`), and in 2D hands the engine its own painted frame (`scene.raw`, named by the book's `view.pdl`: `scene_raw`, `scene_w`, `scene_h`). 2D styles: `scene` (rpg-pet's own look, default), `tiles` (engine tiles), `ascii`; `\` toggles scene <-> tiles, `0` toggles 2D <-> 3D. State the window shows is in `board/state/ui.txt` (also written by the daemon).

**Shared by both, owned by neither:** `ops/rpg_pet.c` (one C program, 3 modes: verb, resident daemon, `out_all`), `catalog.pdl`, `party.pdl`, `rooms.pdl`, `voices.pdl`, `time.pdl` (clock events), `toy.pdl`, `events/` (events-hq pages: play, stop, int_on, int_off, greet_*, reply_*, door_*, phase_*), `state/` (pet files, house ledger, flags, chat, switches), `tuning.pdl`.

## The daemon (`rpg_pet.+x <house> <app>`, 100 ms tick)

In order each tick: reload static data + `tuning.pdl` (every `reload_ms`) -> poll the two key files (own relay + history) -> INT keys (`do_step` on arrows, Tab = player/pet, Esc) -> board Interact edge (In: from the pc-hq key turns INT on) -> **canvas click selects** -> `follow_xelector` (INT: the chosen monster follows the engine's xelector) -> pets walk (INT off, every `step_min_ms`+jitter) -> day phase -> `follow_pchq` (export rooms, wake engine; only while a board window exists) -> render + write ui files (only while some viewer window exists). **No viewer window = nothing is rendered** (`viewer_kinds()`, house `hq-cpu-safety.md` section 4).

## What was moved out of C into `.pdl` (2026-10-11)

`tuning.pdl` (new): `TUNE | name | value | text | default` rows, re-read when the file changes, no rebuild. Holds the numbers that get changed often: `reload_ms`, `flag_cache_ms`, `follow_ms`, `viewer_scan_ms`, `far_repaint_ms`, `greet_page_ms`, `step_min_ms`, `step_jitter_ms`, `edge_poll_ms`, `engage_retry_ms`, `click_poll_ms`. Already `.pdl` before: `keybinds.pdl` (key codes), `time.pdl` (clock events), `voices.pdl`, `catalog.pdl`, `party.pdl`, `rooms.pdl`, `toy.pdl`, `board/side_style.pdl`. **Still hard-coded in C** (candidates for the next pass, each one is a comment-marked constant): the scene frame size and grid (`FW 660, FH 430, COLS 13, ROWS 8, T 48`), pet speech/greeting cooldowns inside `meet()`, the rpg clock default rate (`sec`), the engine nudge key (126 = unbound "render now"). Rule of thumb: a number a human tunes while watching the window belongs in `tuning.pdl`; a number the maps are built on (`T`, `COLS`) stays in C until the maps themselves are data.

## Where things live (the cheat sheet)

| Thing | File |
|---|---|
| INT on/off | `state/world/switches.txt` (`int=1`), mirrored by `state/flag_int.txt` |
| controlled mover | flag `ctl` = `pet` or `player` (`state/flag_ctl.txt`); view flag `follow` |
| selected pet | `state/active.txt` (pet id) |
| pet position | `state/pet_<id>.txt` (`room`, `x`, `y`), |
| clock | `state/clock_ui.txt` (`time_label`, `time_running`), livedesk-clock daemon `lc_clock` |
| engine Interact on? | `<session>/pieces/display/active_gui_is_typing.txt` (0/1) |
| engine selector / render mode | `<session>/pieces/system/bv_state.txt` (`selector_x/y`, `render_mode`) |
| engine placer (arrows captured while armed) | `<session>/pieces/display/placer.txt` — **never armed on painted maps** (3D click path and, since 2026-10-11, the 2D click path) |
| canvas click | `#.desktop/pchq_canvas_click.txt` = `cx cy cw ch` (house-wide, last click on any pc-hq canvas) |
| window key relay | `#.desktop/entity_menu_history/<window pid>.txt` (per window, append-only) |

## Pitfalls specific to having two front ends

1. A fix in one front end silently does not exist in the other: grep both `.xhtpm` files for the verb before declaring a feature done.
2. The engine only reads its relay when woken; `126` is the unbound "render now" key. The tab script `pchq_board_action.sh interact` writes Enter (13) to BOTH engine files; Enter is an ordinary key once Interact is on, only Esc (27) leaves it.
3. A 2D canvas click arms the engine placer by default (pc-hq feature); an armed placer takes the arrow keys. Painted maps must not arm it.
4. The render rule: never produce a frame for a window that is not there.
5. Test with **real** input: `xdotool` clicks and keys on the real window, not only the engine relay. Relay-only tests passed while the owner's hands still failed (see memory `relay-testing-may-mask-real-focus-bugs`).
