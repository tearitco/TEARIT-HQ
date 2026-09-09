# pc-hq board camera / POV keys vs mutaclysm

**Status: RESEARCH (2026-09-09).** Direct report: on the pc-hq board,
`wasd` camera pan doesn't work, the `1/2/3/4` POV modes don't switch, and
`0` (2D ⇄ 3D toggle) doesn't seem to do anything. "Is there a reason in
the code (these may exist but be blocked by a bug) or are they not done?"

**Short answer: the camera model IS ported** (`&.widgits/board-viewer/
ops/bv_menu_input.c` is "a direct port of mutaclysm's `ops/camera_
control.c` dispatch"). Nothing is un-implemented. The three symptoms are:
one deliberate key remap the user isn't aware of, one "works as designed
but undiscoverable" gate, and two real bugs.

---

## 1. The mutaclysm reference (`101.mutaclsym…+18.0G/dox/ctrl-legend.md`)

| Key | Action | Condition |
|---|---|---|
| `1` `2` `3` `4` | POV `camera_mode` 1/2/3/4 (1st-person / 3rd-person / free-roam / bird's-eye) | `render_mode==1` (3D) |
| `0` | toggle 2D ⇄ 3D (`render_mode`) | — |
| `9` | release possession / reverse-jump | — |
| `w a s d` | pan | **modes 3/4 only** |
| `q e` | yaw ±10° | **modes 1/2/3 only** (no-op in 4) |
| `r t` | pitch ±10° | modes 1/2/3 only |
| `c v` | camera Z-level ±1 | all modes |
| `z x` | **hero** Z-level ∓1 | 3D only |
| arrows | hero / xlector cursor | — |
| `f` | reset view (per-mode default) | — |

Camera-mode capability table: mode 1/2 = rotate+Zonly (no pan); mode 3 =
pan+rotate+Z; mode 4 = pan+Z (no rotate).

## 2. What pc-hq actually has (`bv_menu_input.c` `handle_one_key`)

Every one of those is wired, reading/writing `<bv_session>/pieces/system/
bv_state.txt` (`cam_yaw`, `cam_pitch`, `cam_pan_x/y/z`, `cam_z_level`,
`camera_mode`, `render_mode`, `selector_x/y`, `current_z`):

- `key_toggle_render_mode` (`'0'`) → flips `render_mode`. ✔ wired
- `key_possess` (`'9'`) → possess/release + reverse-jump. ✔
- `key_reset_xelector` (`'8'`) → jump xelector to hero. ✔ *(see bug B2)*
- arrows → xelector `selector_x/y` + host `xelector_01/state.txt` +
  `MOVE` tick when possessing. ✔ (unconditional, 2D and 3D)
- `key_yaw_left/right` (`q`/`e`), `key_pitch_down/up` (`r`/`t`) → modes
  1/2/3. ✔
- `key_pan_*` (`w`/`a`/`s`/`d`) → modes 3/4, axis mapping differs per
  mode. ✔
- `key_cam_down/up` (`c`/`v`) → `cam_z_level`, **modes 3/4 only**
  *(divergence D1)*
- `key_z_down/up` (`z`/`x`) → xelector `pos_z` + `current_z`. ✔
  (works in 2D too — a documented divergence from muta's 3D-only gate)
- `key_reset_view` (`f`) → per-mode reset. ✔
- **POV switch is `'5'`–`'8'`**, not `'1'`–`'4'` → `camera_mode = key-'4'`
  *(remap R1)*
- all camera keys hard-gated: `if (!render_mode) { bump; return; }`
  before any camera handler runs — matches muta's `camera_control.c`.

Every key/step is also configurable via the focused host's
`pieces/system/arrow_config.txt` (`key_pan_forward=`, `key_yaw_left=`,
`left_dx=`, …) — defaults equal the chars above.

**Live proof it reaches the engine:** the running pc-hq board's
`bv_state.txt` shows `cam_yaw=160`, `render_mode=1`, `focused_project_root`
set — i.e. `q`/`e` presses HAVE been landing since the interact-format
(`002988fc`) + launcher (`da78a0a7`) fixes. The pipeline works now.

---

## 3. Why the three symptoms

### R1 — `1/2/3/4` do nothing: POV keys were moved to `5/6/7/8` (2026-08-31, on purpose)
`bv_menu_input.c` ~line 700, verbatim from the commit comment:

> *"direct instruction ('we are going to move the current camera controls
> to 5,6,7,8 … use 1,2,3,4 for if we ever do "one map" perspective style
> 3d … that will be our final trick') … ONE real shared control layer
> (5-8 mode switch, same camera_mode values 1-4 underneath) works
> identically in board-viewer's "piecemode" and cursword's desktop 3D
> view, keeping 1-4 reserved everywhere for the same future one-map
> mode."*

So `1-4` are **intentionally unbound**; POV switching is `5`→mode1,
`6`→mode2, `7`→mode3, `8`→mode4. The user's mutaclysm muscle memory
(`1-4`) simply doesn't match the current binding.
→ **DECISION:** keep the `5-8` remap (and surface it — §4 G3) or restore
`1-4` and give up the reserved "one-map" band.

### R2 — `wasd` does nothing: `camera_mode` defaults to **2**, and pan is modes 3/4 only
`default_camera_mode()` → `arrow_config.txt` `default_camera_mode`, fallback
**2** (third person). The live `bv_state.txt` has no `camera_mode` key →
mode 2 → **`wasd` is a designed no-op** (exactly like mutaclysm — pan is
modes 3/4). `q/e/r/t` DO work in mode 2 (hence `cam_yaw=160`).
To pan: press `7` (free roam) or `8` (bird's-eye) first. Nothing is
broken — it's undiscoverable. → surface a legend (§4 G3), and consider
`default_camera_mode=3` for pc-hq's `arrow_config.txt`.

### R3 — `0` toggle appears dead in the khtpm board window (REAL BUG B1)
`0` flips `render_mode` in `bv_state.txt` correctly. But:
- `render_mode==1` (3D): `bv_render_3d.+x` writes the pixel
  `rgb_frame_3d_overlay.raw`; the `<canvas sprite="${canvas_raw}">` shows
  it. ✔
- `render_mode==0` (2D): **`bv_render_3d.c` `main()` does
  `if (!render_mode) return 0;` — writes NOTHING.** The 2D emoji grid is
  written by `bv_compose_frame.c` to `view.txt` (TEXT) and composited into
  `rgb_frame.raw` by the `chtpm_rgb_render` daemon.
- The khtpm `<canvas>`'s `canvas_raw` (from `pchq_board_projector.c`)
  points at **`rgb_frame_3d_overlay.raw`** — the *raw 3D overlay*, not the
  composited `rgb_frame.raw`.

So pressing `0` → 2D → the canvas source stops updating → the window
shows the frozen last 3D frame (or dark). The last-good
`run_pchq_board_mode()` blitted **`rgb_frame.raw`** (composited, contains
BOTH the 2D text-emoji layer AND the 3D overlay + chrome text); the
static-xhtpm port regressed to the raw overlay. This is the same
"compositor bypass" flagged in `pchq-vs-tpmos.md` §A.4.
→ **FIX:** `pchq_board_projector.c` publish `canvas_raw =
<bv_session>/pieces/display/rgb_frame.raw` (+ its `.receipt.txt` for
dims). Then `0`/2D shows the emoji map, and 3D still works (the
compositor already blends the overlay in).

### B2 — `5`/`6` "FILE_MENU / DESK_MENU" is dead code (key-collision regression)
`bv_menu_input.c` has, in order:
1. ~line 718: `if (key >= '5' && key <= '8') { camera_mode = key-'4';
   … return 0; }`
2. ~line 860: `if (key == '5') send_action_to_host("FILE_MENU"); … `
   `if (key == '6') send_action_to_host("DESK_MENU"); …`

The 2026-08-31 `5-8` camera remap (#1) `return`s before #2 is ever
reached — the 2026-08-30 FILE/DESK dispatch on `5`/`6` (whose own comment
proudly says *"checked the FULL real key list … genuinely nothing else
claims 5/6"*) is **shadowed dead code**. Pressing `5`/`6` switches the
camera instead. Currently masked only because `pchq-board.xhtpm`'s
File/Desk toolbar was rerouted to the `file-hq` / `menu desk` verbs
(2026-09-09), so nothing sends raw `5`/`6` from pc-hq — but it's a live
footgun and the dead branch should be deleted or its keys moved
(File/Desk aren't camera concerns; `pchq_board_action.sh` still emits
`53`/`54` for its `file`/`desk` verbs — those also hit the camera switch).

### D1 — `c`/`v` (camera Z-level) gated to modes 3/4; mutaclysm allows all modes
Minor: `ctrl-legend.md` says `c`/`v` work in "all modes"; `bv_menu_input.c`
gates them to modes 3/4. Low impact (mode 1/2 camera Z is rarely useful),
but a real divergence to reconcile.

---

## 4. Gaps / recommendations

| # | Item | Action |
|---|---|---|
| **G1** | `0` → 2D shows nothing (B1) | projector: `canvas_raw` → `rgb_frame.raw` (composited). Highest user impact. |
| **G2** | `5`/`6` dead FILE/DESK code + `pchq_board_action.sh` `53`/`54` collide with the camera switch (B2) | delete the dead `key=='5'/'6'` branches in `bv_menu_input.c`; change `pchq_board_action.sh` `file`/`desk` verbs off `53`/`54` (they should use the `file-hq` / `menu` paths that `pchq-board.xhtpm` already uses, not raw keys). |
| **G3** | POV `5-8` + "pan needs mode 3/4" are undiscoverable | `bv_compose_frame.c` already writes a camera-status/legend block to `view.txt` — but the khtpm `<canvas>` doesn't show text. Options: (a) a small always-on legend strip in `pchq-board.xhtpm` (`In:` badge already shows mode? extend it), (b) projector publishes `camera_mode` / `render_mode` as vars and the xhtpm shows `Cam: free-roam (7)  3D`. |
| **G4** | `1-4` vs `5-8` decision | pick one. If keeping `5-8`: document it in `pc-hq-INDEX.md` + the legend (G3). If restoring `1-4`: also update cursword's `cursword_handle_camera_key()` (shared layer) and drop the reserved band. |
| **G5** | `default_camera_mode` = 2 for pc-hq | if pan-on-arrival is wanted, set `@.apps/piececraft-hq/pieces/system/arrow_config.txt` `default_camera_mode=3` (free roam) — data-only, no code. |
| **G6** | `c`/`v` mode gate divergence (D1) | reconcile with `ctrl-legend.md` (allow all modes) or document the intentional narrowing. |
| **G7** | verify end-to-end | after G1/G2: in the live board, Interact on → `0` toggles a visible 2D emoji map ⇄ 3D; `7` then `wasd` pans; `5/6/7/8` switch POV; `q/e/r/t` rotate. (Needs the launcher stable — `da78a0a7`.) |

None of this is engine-architecture work — it's a projector one-liner
(G1), two dead-code deletions + an action-script fix (G2), a small
xhtpm/legend add (G3), and a naming decision (G4). The camera math,
per-mode tables, yaw/pitch/pan, possession, and Z-levels are all already
implemented and (post the format/launcher fixes) reachable.

## 5. Sources
- `&.widgits/board-viewer/ops/bv_menu_input.c` `handle_one_key`
  (camera block ~360-900), `default_camera_mode`/`default_render_mode`.
- `&.widgits/board-viewer/ops/bv_render_3d.c` `main()` ~1375-1405
  (`if (!render_mode) return 0`).
- `&.widgits/board-viewer/ops/bv_compose_frame.c` ~490-780 (2D emoji /
  `view.txt`, camera-status legend).
- `@.apps/piececraft-hq/ops/pchq_board_projector.c` `find_board_session`
  + the `canvas_raw` publish; `pchq_board_action.sh` `file`/`desk` verbs.
- `101.mutaclsym…+18.0G/dox/ctrl-legend.md`, `dox/pov-cam.md`,
  `ops/camera_control.c`; `&.widgits/5-pov-widgit.md`.
- Related: `pchq-vs-tpmos.md` (§A.4 compositor bypass), `pc-hq-INDEX.md`,
  `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` (the
  `5-8` shared-layer instruction).

---

## 6. Landed 2026-09-09 + the `9`-possession research

### Decisions applied
- **POV keys restored to `1`–`4`** (`bv_menu_input.c`) — the 2026-08-31
  `5`–`8` remap (to reserve `1`–`4` for a cursword-shared desktop-3D
  mode) is reverted; "we aren't doing real 3D on desk." cursword's own
  `tp_desktop_window_rgb.c` remap is now moot — revert it separately if
  desired, it doesn't affect pc-hq. Restoring `1`–`4` also un-shadows
  the `5`/`6` → FILE_MENU/DESK_MENU dispatch (was dead code).
- **`0` toggle fixed (B1).** `pchq_board_projector.c` now reads
  `render_mode` from `bv_state.txt` and publishes `canvas_raw` by mode:
  `render_mode==1` → `rgb_frame_3d_overlay.raw` (clean 3D); `==0` →
  `rgb_frame.raw` (chtpm_rgb_render's composited frame — the only
  surface carrying the 2D emoji map). `kh_draw_canvas` /
  `khtpm_core_render`'s receipt parser learned `frame_w=`/`frame_h=`
  (the composited receipt's keys) alongside `overlay_w=`/`overlay_h=`.
  Verified live: `0` flips `render_mode` 1⇄0 and `canvas_raw` swaps
  `rgb_frame_3d_overlay.raw` ⇄ `rgb_frame.raw`.
  *Caveat:* the composited 2D frame still carries board-viewer's own
  text chrome (border/title/status). A chrome-free 2D pixel path
  (`bv_render_3d` rendering a flat top-down emoji frame into the overlay
  when `render_mode==0`, instead of `if(!render_mode) return 0`) is the
  clean follow-up.
- **Double-arrow fixed.** `khtpm_core_render.c`'s interact-forward now
  routes by keycode: `13`/`27` (parser state machine) → `keyboard/
  history.txt` only; every camera key → `interact_relay.txt` only. The
  bug: the engaged `board_viewer.chtpm` parser re-injects every key it
  reads from `keyboard/history.txt` into `interact_relay.txt`
  (`inject_raw_key`), so a key written to BOTH files reached the pal-VM
  camera twice = one press → two cells. Verified: one relay key → one
  `selector_x` step; three → three.

### `9` possession — where it lives in mutaclysm
**In `ops/choice.c` — a prisc op, NOT the manager, NOT hardcoded in the
pal or renderer.** `choice.c` "runs unconditionally every tick
(self-filtering)", `exec`'d from `main_loop.pal` / drained by
`game_dispatch.c`.

- **Enter possession:** `try_possess_at(hero_x,hero_y, xlector_x,xlector_y)`
  — press **Enter** while the xlector cursor sits on the hero's tile AND
  `hero/piece.pdl` has `possessable` (default 1). Not a dedicated key —
  it's the panel/Enter commit path. v1 = one target (hero); "functionally
  identical to exiting interact_mode."
- **`9` = release only.** If `possessed_id != "none"`: read the entity's
  `piece.pdl` `de_possessible` — if false, save `last_possessed_id`,
  snap the xlector to the entity's pos, set `possessed_id="none"`, log
  "You release control."; if true, no-op ("You cannot release this
  entity.").
- **`9` reverse-jump:** if `possessed_id=="none"` and
  `last_possessed_id!="none"` → snap xlector to that entity and
  re-possess.
- State: the piece's `state.txt` (`possessed_id`, `last_possessed_id`) +
  `piece.pdl` flags (`possessable`, `de_possessible`).

**board-viewer's `bv_menu_input.c` equivalent:** already a `9` handler
(also an op, no manager) that **toggles both ways** (possess ⇄ release)
because there is exactly one possessable target (`hero_01`) and no
scan/panel system — a documented deliberate simplification. It saves
`pre_possess_x/y/z` for reverse-jump-on-release. It does NOT read the
`possessable` / `de_possessible` piece.pdl flags and does NOT support
Enter-on-hero-tile. To reach full muta parity: add the flag checks +
the Enter-on-tile possess path (needs the xlector-on-hero-tile test,
which board-viewer already tracks via `selector_x/y` vs `hero pos_x/y`).
