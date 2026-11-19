---
name: pchq-3d-board-driving
description: How to drive, measure and photograph the 3D board of any pc-hq window (Teario-Make, rpg-edit, a phone editor) with the ENGINE key file: which key changes which state value, the limits and traps found by real runs on 2026-11-19, and the vocabulary (xelector cursor vs possessed hero_01 vs the game's own sprite). Use BEFORE touching a 3D camera, possession, a floating or mis-facing piece, or the Teario 3D view.
---

# Driving a pc-hq 3D board (verified 2026-11-19 on the Teario-Make window; every row RAN unless it says READ)

Paths are from the house `44.xyz.01.00/`. Quote them (`&`, `@`). Companions: skills `rpg-space-board-driving` (launch / recover / board_ctl), BOOK `18.pc-hq/3D-CONTROLS-AND-VIEWS-REVIEW-STANDARD-2026-11-19.md` (the review standard) and `18.pc-hq/XELECTOR-ENTITY.md`, `INTENDED.md`. Ask the owner before driving a window he may be using.

## 1. Vocabulary (I conflated these for hours: do not)
| Name | What it is | Where |
|---|---|---|
| **the game's player** | the extruded RPG Maker sprite (a row of the play desk's `events.txt`); stands on the ground | `maps/<book>/play/events.txt`, written by `game_core` |
| **xelector** | the cursor; drawn as the CYAN cube; arrows move it, `z`/`x` change its LEVEL; with `possessed_id` set its cell IS that piece's cell | `pieces/xelector_01/state.txt` |
| **hero_01** | the engine hero piece; INVISIBLE while unpossessed (moving only it changed nothing, RAN); `game_core` does NOT drive it | `pieces/hero_01/state.txt` |
| **moon_01 / sun_01** | sky pieces; the pale cube floating in the sky IS the moon (RAN: move it, the cube vanishes, restore it, it returns) | `pieces/moon_01/state.txt` |
| **floor_z** | the ground layer (16); a piece at `pos_z = floor_z + 1` (17) is at HUD `z=1`; the launcher resets hero and xelector to it | `pieces/system/board_manifest.txt` |
HUD (top-left): `pos x,y` = the SELECTOR from bv_state (default = page centre, why a fresh Teario session says 10,6) and `z` = xelector pos_z - floor_z; NOT the xelector cell and NOT the hero; `poss hero_01` = what it possesses; `click` / `ray` = the last mouse pick.

## 2. The recipe: engine keys, measured
- Session: `S=$(sed -n 's/^bv_session=//p' @.apps/<app>/board/state/ui.txt | head -1)`; state: `$S/pieces/system/bv_state.txt` (camera, render mode); piece state: `@.apps/<app>/<app>-pchq/pieces/<piece>/state.txt`.
- Send: `&.widgits/board-viewer/tools/+x/bv_keys.+x "$S" <text>` (one char = one key; `zzzzzzzzzzzzz` = 13 presses; no owner focus needed). Wait 2 s, read the state, THEN photograph (`&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <window id> out.png`, find the window by size with `xwininfo -root -tree`) and open the PNG.
- Do NOT hand-edit `hero_01` / `xelector_01` state to move things: the HUD followed my edit but the drawn pieces did not reliably; use the keys (`z`/`x`) which the engine applies itself.
- Measured table (start from `f`, default mode 2, yaw 180, pitch 6, pan 0,10,0): `q` yaw -10, `e` yaw +10 | `c` cam_z_level +1, `v` -1 | `w` pan_z -1, `s` +1, `a` pan_x -1, `d` +1 | `r`/`t` pitch (see traps) | `1` mode 1 first person (pitch 6) | `2` mode 2 third person | `3` mode 3 (pitch -90, pan_y 17) | `4` mode 4 (pitch -90, pan 8,10) | `0` toggles 2D flat view (`render_mode` 0, `view_2d_style=tiles`) | `f` resets view | `z` xelector level down 1, `x` up 1 (13 `z` took pos_z 30 -> 17 and the cyan block dropped onto the ground) | `;` snap camera (can fly the camera far off: `f` brings it back).
- Order to recover a lost view: `f`, then `2`, then `8` (xelector to hero) if the cyan block is off.

## 3. Traps (each one found by a real run)
- **Pitch keys clamp to -80..-25** (`PITCH_MIN/MAX`, `bv_menu_input.c` line 59-60) but a fresh view and `f` set pitch +6: ANY `r` or `t` jumps the camera to -25 and `t` can never go back; only `f` restores the level, ground-hugging view. `t` after `r` stays at -25. (Controls row: DIFFERS from "pitch up/down as expected".)
- **Facing:** the default yaw 180 looks ACROSS a level that runs along x (first person shows only sky). `q` x9 (yaw 90) faces the goal end of a Teario level; `e` x9 (yaw 270) faces away. So the "facing" the owner sees on the minimap is the camera's yaw cone, not a hero heading.
- **Layer files are read again after input**, not by file write alone: my hand-written layers did not show in photos taken with no key presses after the write, and appeared after the owner pressed keys. The book load (`pc_generate_chunk`, `map:` mode) rewrites `board_full_z*.txt` from `map.txt`.
- The engine idles out after about 270 s with no frame request (not a crash): a frozen picture, next input respawns it.
- **Launch:** `sh @.apps/teario-make/button.sh run` through `cpu_gate`; a clean launch has a frame file within about 10 s (`ls $S/pieces/display/rgb_frame*.raw`). Do not set `default_render_mode=0` in the package `arrow_config.txt`: the session then rendered NO frame for 80 s. Never relaunch within minutes of the last launch.
- The Make popup moves between launches; the stamp bar fills only when a Make level is open (press `new level`, nav 5, by the window relay `KEY_PRESSED: 53` then `13`).
- A claim needs a CONTROL: restore the original state (byte compare) and repeat the same keys; I falsified three of my own claims this way on 2026-11-19 (see the Teario plan, "FALSIFICATION").
- Never use Python; scripts are not committed; this skill file and BOOK docs are the record.

## 4. Open (do not treat as known)
Which key reaches 2D "platform" play (none: platform mode is a GAME mode, not a camera); what raised the cursor to level 30; the pale cube above the ground (hero_01 placeholder? unproven); whether camera mode 3 / 4 photos were taken before the engine finished redrawing (5 s was too short once).

## 5. Update 2026-11-19 (late): the facing fix exists
`default_yaw=<0..359>` in a package's `pieces/system/arrow_config.txt` sets the yaw that camera modes 1 and 2 start at and `f` / the POV keys reset to (missing = 180, every other board unchanged; modes 3 and 4 keep 180). The Teario launcher sets it to 90, so a fresh first-person view looks along the level at the goal. Engine change in `bv_menu_input.c` and `bv_render_3d.c`, built with `scripts/build.sh` through `cpu_gate`; an OPEN board needs a NEW session (relaunch) to get the new binary. Proof recipe (RAN): fresh session -> key `1` gives yaw 90 -> `qq` then `f` gives 90; negative control: delete the line, `f` and `1` give 180, restore the line and byte-compare the file. The key is reread on every key press, so no relaunch is needed to toggle the config. Decision D96.
