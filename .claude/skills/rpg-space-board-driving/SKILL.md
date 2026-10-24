---
name: rpg-space-board-driving
description: How to launch, drive, photograph and test the rpg-space board (and any pets-like board window with the 3D engine) like a human does - real keys, window relay clicks, frame dumps, the helm / ship flight, the yes-no popup - and the traps that cost hours. Use before any live test, screenshot or presentation of rpg-space, rpg-dust, rpg-pet or another petslike_core.c board.
---

# Driving the rpg-space board as a person would (verified 2026-10-24)

Paths below are from `44.xyz.01.00/`. App = `@.apps/rpg-space`. Quote every path (`&` and `@` are shell characters). Full test record: `#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/RPG-SPACE-HELM-INTERACTION-TEST-PLAN.md`; design of the event area: `EVENT-AREA-SELECTION-DESIGN.md`.

## 00. Use the harness: `board_ctl` (built 2026-10-24, use it instead of hand-typed xdotool/sed)

`&.widgits/board-viewer/tools/+x/board_ctl.+x` (source `board_ctl.c`, build `sh &.widgits/board-viewer/tools/build_board_ctl.sh`; header comment lists every command). Pass the app dir (`@.apps/rpg-space`).

| command | what it does / why |
|---|---|
| `preflight` | session folder alive, engine process alive (not "(deleted)"), `bv_state` lines, window MAPPED (a hidden window cannot be focused), daemon running, In: coherent with the engine's Interact mode, engine really renders after a nudge. Run first; any FAIL = you do not have control. |
| `recover` | kill the app's window/engine by pid, relaunch, wait for a live session, preflight. Worked twice on 2026-10-24 (the engine process died twice, cause unknown). |
| `key <xdotool key> [ms]` | moves the POINTER into the window (focus-follows-mouse), focuses, presses a real key, then nudges the engine 3 times (an idle engine only reads keys when woken). |
| `click <x> <y>` | pointer to the spot, then the relay `MOUSE_EVENT` press/release. Both are needed. |
| `camera <preset>` | absolute camera from `tools/camera_presets.pdl` (`iso-south`, `topdown`) written into `bv_state.txt`; the engine's camera keys are relative and `f` is not a reset. |
| `expect <state[:key]> <value> [s]` | assert/wait on `<app>/state` (`flag_int.txt 1`, `trainer.txt:y 4`, `ship.txt "4 3"`). Check after EVERY action. |
| `snap <out.png> [--state a,b:key] [--same-ok]` | waits until the engine rendered after a nudge, dumps the window, writes `<out>.receipt.txt` (hash + state values); exit 3 if identical to the previous picture. |
| `manifest <dir> <steps.txt>` | builds `manifest.txt` for `make_presentation_video.py` from the receipts: captions use `{ship.txt}`, `{trainer.txt:x}` placeholders, so numbers in captions are real; fails on identical neighbours. |
| game verb `rpg_space.+x setpos <room> <x> <y>` | put the controlled character on a cell (also moves the xelector so the daemon does not walk him back). Not `place` (that is furniture). |

The reference run: `RPG-SPACE-HELM-FLIGHT` folder in the media archive (`October-24/rpg-space-helm-flight-harness-20261024/flight_run.sh`).

Order of waits that mattered: after `planet <id>` wait ~40 s (the engine reloads the book and ignores keys); after a Yes click wait ~14 s before the camera preset; after each flight key about 7 s before a snap.

## 0. Prove you have control BEFORE any presentation or claim

Do this checklist first; do not start a recording until every line is true. Write the evidence down.

1. `sed -n 's/^bv_session=//p' @.apps/rpg-space/board/state/ui.txt` prints a path **and that directory exists** (`ls -d`). Empty = no engine session (see 3).
2. `cat @.apps/rpg-space/state/flag_int.txt` = 1 and `$S/pieces/display/active_gui_is_typing.txt` = 1 when you want In: ON.
3. One real arrow key changes `state/trainer.txt` (x or y) within 2 s.
4. A window dump differs from the one before it (a stale engine session gives identical dumps for minutes - that fooled me).

## 1. The processes and files

| What | Where |
|---|---|
| the window (khtpm_core_render) | find by size: `xwininfo -root -tree \| grep ' 1260x556+'`; pid: `ps -eo pid,args \| grep '[k]htpm_core_render' \| grep rpg-space` |
| the game daemon (ticks pets, reads keys) | `@.apps/rpg-space/ops/+x/rpg_space.+x <house> <app>` (3 args = resident; one flock `state/daemon.lock`) |
| the engine (3D renderer) | `&.widgits/board-viewer/ops/+x/bv_render_3d.+x --daemon`, session dir = `bv_session=` in `board/state/ui.txt` |
| the pc-hq book/world | `@.apps/rpg-space/rpg-space-pchq/` (`pieces/world_01/state.txt` desk_id, `pieces/xelector_01/state.txt`) |
| game state | `@.apps/rpg-space/state/` (`trainer.txt` hero room/x/y, `flag_int.txt`, `flag_helm.txt`, `flag_ask.txt`, `ship.txt`, `events.txt` rows `HELM/FLY/EVENT/DOOR/STEP`, `look_ledger.txt`) |
| the window's layout | **`@.apps/rpg-space/board/pchq-board.xhtpm`** (NOT `rpg-space.xhtpm`: that one is not the board window). Live reparse: edit, wait ~5 s. |
| the window's variables | `@.apps/rpg-space/state/ui.txt` (written by the daemon) |

## 2. Input - what works and what does not

- **Real X keys work** when the pointer is over the window (focus-follows-mouse) and the window is mapped: `xdotool mousemove --window <win> 400 350; xdotool windowfocus <win>; xdotool key Up` (one cell per press; `board_ctl key` does this). Always `windowfocus` in the SAME command (focus is lost between calls; a key without focus does nothing and says nothing). Camera-aware: Up is screen-up.
- **Window clicks: move the pointer onto the element AND use the per-pid relay (board_ctl click), not xdotool click.** Append (never truncate) to `#.desktop/entity_menu_history/<pid>.txt`: `MOUSE_EVENT: 1 <x> <y> 1` then `... 0`, x,y relative to the window (dump pixel coordinates). This pressed the popup's Yes. `xdotool click` did nothing on the popup.
- The relay's `KEY_PRESSED: 200` arrows do NOT move the xelector. The engine eats arrow keys itself and empties its own relay file, so the daemon only ever sees where the xelector went.
- Engine camera/level keys: `&.widgits/board-viewer/tools/+x/bv_keys.+x <session> <text>` (`f` camera reset - also puts the engine on the FIRST desk, `c`/`v` height, `r`/`t` pitch, `z`/`x` xelector level, `8` snaps the xelector to the character, `\e` Esc). Nine `z` presses sink the camera inside the planet = solid green screen; undo with `x` x16.
- In: on/off: the `int` verb (`ops/+x/rpg_space.+x int`) is a TOGGLE - read `flag_int.txt` first. Esc in the window leaves In: (and releases the helm).

## 3. Launch / relaunch (when bv_session is empty or the dump is stale)

Something wipes `&.widgits/board-viewer/pieces/sessions/*` (the engine launcher's exit trap does `rm -rf`; seen at 03:18 on 2026-10-24 while the owner/other agents were working). The window then keeps its last frame and ignores keys (`readlink /proc/<bv_render_3d pid>/cwd` says `(deleted)`). Recipe:

1. kill the window pid, the engine daemon pid (`bv_render_3d`) and `pchq_board_projector` by **pid**, not by pattern (a `pkill -f` pattern that matches your own shell kills it).
2. `sh @.apps/rpg-space/button.sh run` opens the window (log `/tmp/rpg-space-board.log`). If it says "starting engine session" but no `bv_render_3d` appears, start the engine yourself: `cd @.apps/piececraft-hq; PCHQ_ENGINE_MODE=1 PCHQ_PKG=<abs @.apps/rpg-space/rpg-space-pchq> setsid nohup sh button.sh run > /tmp/x.log 2>&1 &` and wait ~25 s for `bv_session=` to fill.
3. restart the daemon after a rebuild: `pkill -f 'rpg_space.\+x /home'`, then `(setsid nohup ops/+x/rpg_space.+x "<house>" "<app>" </dev/null >/dev/null 2>&1 &)`. A rebuilt binary does nothing until the daemon is restarted; a rebuilt engine only after the whole session ends.
4. Put the hero where you want him by stopping the daemon, editing `state/trainer.txt` (`room= x= y=`), starting the daemon. Hero wanders at once when In: is off, so set `int` right after.

## 4. Photographs (what actually works)

- **`&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <win> out.png`** captures the real window including the 3D canvas and the engine's own minimap (top-right). `scrot` is black on this GL path. Use it; take a dump after every key, wait 5-9 s for the 3D view to show a move (the book reloads on each flight step; the 2D map in `state/scene.raw` (400x416 RGBA) is faster).
- The engine's raw frame: `$S/pieces/display/rgb_frame.raw` (640x768 RGBA); convert with a 20-line PNG writer (zlib+struct), see the XO copy `raw2png.py` if present.
- If a dump is identical to the last, the engine session is stale (section 3). `z=-7` or a solid green/dark picture = camera inside the planet.

## 5. The helm (ship flight) in one paragraph

Hero next to the console (bridge 8,4; (7,4) or (9,4)), In: ON, press Enter -> `flag_ask=1` and a centred popup "HELM / Take the helm? Yes / No" (`events/helm/choice.pdl`, row `ask-ov` in `board/pchq-board.xhtpm`; delete the file and Enter plays the page at once). Yes (relay click, nav number, or `ops/+x/rpg_space.+x helm yes`) plays `events/helm/` (trigger `action`) -> `helm on`: the hero is put on the ship's cell in the `space` desk, the engine shows it, arrows move the hull one grid step (`FLY` rows, `state/ship.txt`), Esc (or In: off) releases the helm and lands over the planet under the ship. Planets without a `POS` row take the first free cell. Evidence rows: `state/events.txt`.

## 6. Traps that cost real time (all hit on 2026-10-24)

- `$S` empty after `sed` of ui.txt = no session; every `$S/...` path then points at `/pieces/...`. Always `ls -d "$S"`.
- A daemon started before the last build runs the OLD code. `ps` etime vs binary mtime.
- The helm page runs in its own process; the daemon must wait ~2 s after `helm on` before reading the xelector (stale offset = phantom FLY row). Any xelector offset other than exactly one cell is ignored.
- Hero blocked by furniture (row 4/5 of the bridge); walk (7,7) -> Up x3, or stand at (9,4). Walking through a door changes room: check `room=` in trainer.txt, not just x,y.
- `git commit -- <paths>` takes whole files; quote `&`/`@`; never `git add -A`; mp3 and screenshots/mp4 stay out of the repo (XO folder / media archive).
- Do not drive the window while the owner is; say so before using `xdotool windowfocus`.

## 7. Unsolved (2026-10-24), so do not trust the engine to stay up

- The engine process `bv_render_3d --daemon` died twice in one evening (03:18 and 04:12) with no log line; once the window had also become unmapped. `board_ctl recover` rebuilds it in about 60 s. If you find the cause, write it here.
- The per-pid relay arrows (`KEY_PRESSED: 200`) are logged but do not move the xelector.
- A hidden window (`xwininfo -id <win>` says IsUnMapped) makes every key fail with `BadMatch (X_SetInputFocus)`; `xdotool windowmap <win>` shows it again.
