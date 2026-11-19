# 3D controls and views: inventory and the review standard (2026-11-19)

Owner 2026-11-19: "in 3D things should look very normal. We may need to review existing 3D controls and views and document. Standardize review processes for those." Platform mode is a **2D** view (a side-on game mode), never a 3D one. This file is the ONE place for the inventory and the review procedure; the camera model itself is in `INTENDED.md` (camera_01, bv_state keys). Status words: **READ** = from the source text only, **RAN** = observed in a real session with the evidence named, **UNTESTED**.

## 1. Views (render modes) and cameras
| What | Value / key | Where | Status |
|---|---|---|---|
| render_mode 0 | flat top-down 2D tile view (also written `view_2d_style=tiles` by key `0`) | `bv_menu_input.c` key `0`; `chtpm_rgb_render` draws it | RAN 2026-11-19 on the Teario-Make window (`bv_keys <session> 0` -> bv_state `render_mode=0`, frame `rgb_frame_2d.raw`, photo) |
| render_mode 1 | 3D voxel / land view, camera_mode 1-4 | `bv_render_3d.c` | RAN (default of a new session, frame in 8 s) |
| render_mode 2 | "side scroll" flat view, toggled with Tab (raw code 9) against mode 0 | `bv_menu_input.c` `key_pov_5` | RAN 2026-11-19: render_mode=2 written, NO visible change on a one-layer book; meaning unproven |
| maker_view | RPG Maker tile terrain in 3D (key `7`) | `bv_menu_input.c` `key_pov_6` | UNTESTED by me |
| Chinese / ASCII view | backtick (96) | `key_cjk_view` | READ |
| camera_mode 1-4 | keys `1`..`4`; default 2 = third person (`arrow_config.txt default_camera_mode`) | `default_camera_mode()` | READ (the owner calls 1 first person, 3 third person: reconcile) |

## 2. Keys (all from `bv_menu_input.c` lines ~559-586; names are the variable names, behaviour UNTESTED unless marked)
`0` render 0<->1 toggle | `1`..`4` camera POV | `Tab` render 0<->2 | `7` maker view | `` ` `` CJK view | `9` possess | `8` snap xelector to the hero (memory: hero_01, not a Teropoly player) | `z` `x` xelector level down/up | `f`, `5` reset view | `q` `e` yaw | `r` `t` pitch | `w` `a` `s` `d` pan | `c` `v` camera height | `;` snap to the player view | `5` `6` file / desk menus (`5` is also reset_view_alt: a clash noted in the file). `/` (47) is NOT bound in this file (the window renderer's own handling not checked). Arrow keys: xelector / hero, never the camera (owner 2026-10-27); with render_mode 2 the left/right mapping has its own comment (line ~689).
Input paths: a real key in the window goes through the khtpm relay and the engine key file; `bv_keys <session> <text>` writes the ENGINE key file (used 2026-11-19, it works on a live session); `board_ctl key` is the xdotool path. See `AGENT-TOOLBOX-AND-CONTROLS.md`.

## 3. The coordinate convention (owner file `^.cube-legend_v1.2.txt`)
X red left/right; **Y green UP**; Z blue forward/back; faces: top green, bottom brown, front (z-) white with eyes, back (z+) black, right (x+) red, left (x-) blue; API order `[z, y, x]` = layer, row, column, stored `[x, y, z]`. So the legend's UP is the engine's LAYER `z`, and the legend's depth is the engine's ROW `y`. The legend cube is drawn by an OLD renderer only (`300.rtp-xyz/ops/compose_rgb_frame.c`, whose comment lists Z+ white / Z- dark, the reverse of the legend: check before copying); the board engine has no legend cube (grep, 2026-11-19). A debug cube in the board engine is a TO-BUILD item.

## 4. The review standard (how a 3D control or view is "reviewed")
One row per control or view. A row is done only when ALL of these are filled:
1. **Intent** in the owner's words or the doc that states it (quote + path); if none exists, write "NO INTENT STATED" and ask, do not invent.
2. **Expected state change**: which file and key change (e.g. `bv_state.txt camera_mode`, `cam_yaw`, `render_mode`, `pieces/xelector_01/state.txt pos_*`).
3. **Method**: engine key through `bv_keys <session>` (preferred, no owner focus needed) or the window relay; never the owner's window while they use it (ask first); one change per run; start from a recorded state and restore it.
4. **Evidence**: the state diff (before / after values, from a real read: confirm the file exists before trusting a count) AND a photo you opened (`dump_frame_png_op`) saved next to the row; a picture alone is not proof of which key did it.
5. **A negative check**: the same read after a key that must NOT change it (a positive control), so the check can fail.
6. **Verdict** per owner expectation: MATCHES / DIFFERS (say how) / UNKNOWN, with the date and the build time of the engine binary used (sessions run their own copy of `bv_render_3d`: a rebuilt engine reaches an open board only through a NEW session).
Rows that DIFFER become a bug row with the owner's expected behaviour in his words. The runner for steps 2-5 is to be a harness case (pal + case `.pdl` + a C op; no Python, no new `.sh`), built after the owner approves the list of rows; until then every row above is READ or UNTESTED and a claim about it needs a fresh run.

## 5. Open items (the Teario symptoms that started this review, owner screenshot 2026-11-19 04:20)
In the 3D view of the Teario-Make window (In: ON, possessed hero_01): the level is a long strip running away from the camera, the hero and a white cube float above it ("we are not standing on the ground"), the minimap shows the hero facing sideways although it should face the goal, and the camera keys do not behave as expected for 3D. Hypothesis (UNMEASURED): the 2D platformer's (x, y-down) cells are placed into the 3D grid with the legend's depth/up axes swapped. First review rows to run on the Teario window once the owner says it is free: `1`..`4`, `q` `e`, `r` `t`, `w a s d`, `c` `v`, `;`, `f`, `8`, `9`, each with section 4.

## 6. First review run, 2026-11-19 (Teario-Make window, engine keys through `bv_keys`, state read before/after, `f` reset between; owner said to drive it; the owner's own driving guide is the skill `pchq-3d-board-driving`)
| Control | Expected state change (what the code/docs say) | Measured | Verdict |
|---|---|---|---|
| `q` / `e` | yaw -/+ | yaw 180 -> 170 / 190 (10 per press; x9 = 90 / 270) | MATCHES |
| `c` / `v` | camera height | `cam_z_level` +1 / -1 | MATCHES |
| `w` `s` `a` `d` | pan | `cam_pan_z` -1 / +1, `cam_pan_x` -1 / +1 (1 per press) | MATCHES |
| `r` (pitch down) | pitch -10 | 6 -> -25 first press (clamped), then -35 | DIFFERS on the first press (jumps 31) |
| `t` (pitch up) | pitch +10 | stays -25 from any start; never returns to +6 | DIFFERS: pitch keys clamp to -80..-25 (`PITCH_MIN/MAX` in `bv_menu_input.c`) but the default view and `f` use +6; only `f` restores the level view |
| `1` | first person | `camera_mode=1`, pitch 6, yaw 180: picture is only sky (the camera faces across the level, along rows, not toward the goal) | PICTURE DIFFERS from "player faces the goal"; yaw 90 (`q` x9) faces the goal along x |
| `3` / `4` | modes 3, 4 | `camera_mode` 3 / 4, pitch -90, pan set | state MATCHES; the 5 s photo of mode 3 looked side-on and small: UNVERIFIED (maybe an unfinished redraw) |
| `z` x13 | xelector level -13 | `pos_z` 30 -> 17, HUD z 14 -> 1, hero followed (possessed), the cyan cube landed on the ground | MATCHES |
Owner intent for these rows: NOT STATED in a doc I could find, except the owner's words "camera controls aren't what are expected for 3d" and "player should be facing goal": a DIFFERS row needs his statement of what he expects before it is a bug. Photos 15 to 17 in XO `nov-18/15.teario-own-window-first-launch`.
