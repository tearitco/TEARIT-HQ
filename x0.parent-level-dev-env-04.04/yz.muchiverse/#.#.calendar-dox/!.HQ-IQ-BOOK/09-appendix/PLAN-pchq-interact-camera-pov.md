# PLAN — pc-hq Interact Mode: camera/POV keyboard fix + mouse camera controls

Written 2026-09-04. Delegable — self-contained, no other conversation context required.

## Goal

pc-hq's board window (`@.apps/piececraft-hq/pchq-board.xhtpm`) has a
working "In" (Interact Mode) toggle that arms a generic keyboard relay
in `khtpm_core_render.c` (see `g_interact_relay_on` / `kh_scan_interact_relay()`
/ the top-of-`handle_key()` intercept). While armed it forwards every
keypress verbatim to the live board-viewer session's own
`pieces/apps/player_app/history.txt` and `pieces/keyboard/history.txt`.

User report: turning "In" on correctly sets the badge/label, but arrow
keys / camera / POV controls don't work in the live game once forwarded.
User also wants mouse-driven camera/POV (drag-look, scroll-zoom, etc.),
"like mutaclysm has."

This doc is the confirmed root cause + a concrete two-part fix plan.
Part A is a real, small, well-understood bug fix. Part B is new,
unprecedented work — flagged as such, not a port.

## Part A — arrow-key code mismatch (real bug, concrete fix)

### Root cause (confirmed via direct code read, not guessed)

Two different numeric key-code conventions coexist in this house:

- **khtpm's own convention** — `khtpm_core_render.c:kh_key_history_code()`
  (`*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c:5710-5719`):
  `Up=200, Down=201, Left=202, Right=203, PgUp=204, PgDn=205`. This is
  what the Interact Mode relay currently forwards for every keypress
  (`kh_key_history_code(ks, ch)` call at line ~5184).

- **tpmos/board-viewer's own convention** — `ARROW_LEFT=1000,
  ARROW_RIGHT=1001, ARROW_UP=1002, ARROW_DOWN=1003`, defined in BOTH:
  - `1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/chtpm_parser.c:67`
    (enum) and consumed at `:2999-3005` (the `onClick="INTERACT"`
    branch of `process_key()`).
  - `44.xyz.01.00/&.widgits/board-viewer/ops/bv_menu_input.c:60-63`
    (`#define ARROW_LEFT 1000` etc.) — **this is the file that
    actually dispatches camera pan/selector movement** for pc-hq's
    board (direct port of mutaclysm's `ops/camera_control.c`, see its
    own header comment).

Both consumers read the raw integer directly out of
`pieces/keyboard/history.txt` (`chtpm_parser.c:3060-3079`: `int key =
atoi(colon + 1); ... process_key(key);` — no translation). So whatever
number khtpm's relay writes IS what the game engine acts on. Since
khtpm writes 200-203 and the engine expects 1000-1003, **every arrow
keypress while in Interact Mode is silently ignored by both the
selector-cursor code and the camera dispatcher** — it's not "not
implemented," it's a code-mismatch, and it's the sole reason arrows
don't work today.

Everything else — `wasdqertcvf` and `0`-`9` — already forwards
correctly, because `kh_key_history_code()` returns the literal ASCII
value for any printable character (`:5711`), and both tpmos and
board-viewer expect that same literal ASCII value. **No fix needed for
letter/digit keys.**

### Fix

Add an Interact-Mode-specific arrow remap, applied only at the relay
forward site (do NOT change `kh_key_history_code()` itself — it's
shared by the unrelated house-wide nav/dump-key capture path, which
must keep 200-205).

In `khtpm_core_render.c`, at the `g_interact_relay_on` block
(`~line 5183-5189` as of this writing — search for
`if (g_interact_relay_on) {`):

```c
if (g_interact_relay_on) {
    int code = kh_key_history_code(ks, ch);
    /* tpmos/board-viewer's own convention (chtpm_parser.c's ARROW_*
     * enum, bv_menu_input.c's own #defines) - NOT khtpm's house-wide
     * 200-205 nav-capture convention, which the shared
     * kh_key_history_code() correctly still returns for every OTHER
     * caller (history capture, 'p' dump, etc.). Interact Mode is the
     * ONE relay path that must speak the game engine's own dialect. */
    if      (code == 200) code = 1002; /* Up    -> ARROW_UP    */
    else if (code == 201) code = 1003; /* Down  -> ARROW_DOWN  */
    else if (code == 202) code = 1000; /* Left  -> ARROW_LEFT  */
    else if (code == 203) code = 1001; /* Right -> ARROW_RIGHT */
    for (int i = 0; i < g_interact_relay_n; i++) {
        ...
```

That's the entire fix. No changes needed anywhere else — not in
`bv_menu_input.c`, not in `chtpm_parser.c`, not in the xhtpm template.

### Verification (follow the house's live-relay testing standard, not headless dump)

1. Launch a board-viewer widget session + `pchq-board.xhtpm` against
   it (same procedure already proven working this session — see
   `handoff-2026-09-04-master.md` Rev 14 for the exact steps: digit-
   jump to "In", Enter to arm, confirm `active_gui_is_typing.txt`
   flips to `1`).
2. Send an arrow key through the window's own relay/history file (the
   same PID-scoped `entity_menu_history/<pid>.txt` mechanism used
   throughout this session) and confirm the corresponding `100x` code
   lands in the bv session's `player_app/history.txt`.
3. PNG-dump the board canvas before/after to confirm the selector
   cursor or camera actually moved (not just that the byte arrived).
4. Standard context-menu (`menu.chtpm`) CPU/RSS stability re-check
   before committing, since this touches shared `handle_key()` — see
   `khtpm-house-standards` skill / the `khtpm-shared-layout-caution`
   memory for the exact procedure. This specific change only affects
   the `g_interact_relay_on` branch (already gated to armed Interact
   windows only), so risk is low, but the check is still mandatory.

## Part B — mouse-driven camera/POV (NEW feature, no reference to port)

### Confirmed: this does not exist anywhere in the house today

Checked and confirmed empty (grep for `mouse`/`drag`/`scroll`/`wheel`/
`ButtonPress`):
- `chtpm_parser.c` — `handle_mouse()` exists but only for UI-nav
  clicks (focus/activate elements), never camera.
- `&.widgits/board-viewer/ops/bv_menu_input.c` — 100% keyboard.
- `101.mutaclsym🧟‍♂️️19.00/ops/camera_control.c` and `ops/choice.c`
  (the original source `bv_menu_input.c` itself says it ports from) —
  100% keyboard.
- `&.widgits/5-pov-widgit.md` §2e (the authoritative camera design
  doc, cited by `bv_menu_input.c`'s own header) — documents ONLY the
  keyboard scheme below; no mouse section exists.

So "mouse camera controls like mutaclysm has" is not accurate — neither
mutaclysm nor tpmos nor board-viewer has ever had this. It would be
new design + new code, not a port.

### Reference: the full existing keyboard camera scheme (for parity/consistency when adding mouse)

From `5-pov-widgit.md` §2e (cites `ops/camera_control.c` +
`ops/compose_rgb_frame.c:1564-1606,821-823` for the math) and live in
`bv_menu_input.c`:

State: `cam_yaw`, `cam_pitch`, `cam_pan_x/y/z`, `cam_z_level`.
`YAW_STEP`/`PITCH_STEP` = 10°/keypress, pan = 1.0 map-unit/keypress —
continuous repeatable increments, not snap jumps.

| camera_mode | q/e (yaw) | r/t (pitch) | w/a/s/d (pan) | c/v (z) | f (reset) |
|---|---|---|---|---|---|
| 1 first-person | ±10° | ±10° [-89,89] | locked to hero | — | yaw=180,pitch=6 |
| 2 third-person | ±10° | ±10° | locked to hero | — | yaw=180,pitch=6 |
| 3 free-roam | ±10° | ±10° | w/s→pan_z, a/d→pan_x | ✅ | pan=0,yaw=180,pitch=-90 |
| 4 bird's-eye | no-op | no-op | w/s→pan_y, a/d→pan_x | ✅ | pan centered on hero |

`'0'` toggles `render_mode` (2D/3D, bidirectional). `'1'`-`'4'` pick
`camera_mode`, only live while `render_mode==1`. For pc-hq's board
specifically (no "hero", only a selector cursor), mode 4 (bird's-eye,
absolute map coords, camera detachable from any entity) is the
natural default per `5-pov-widgit.md`'s own recommendation.

### Design questions to answer before implementing (do not guess — ask)

1. **What should each mouse gesture map to?** Natural candidates,
   but need a real decision:
   - Click-drag → yaw/pitch (look-around), replacing or supplementing
     q/e/r/t.
   - Scroll wheel → zoom / z-level (`c`/`v` equivalent), or pan_z in
     free-roam.
   - Right-click-drag → pan (w/a/s/d equivalent) in modes 3/4.
2. **Does this live in `bv_menu_input.c` (extend the existing
   dispatcher to accept a `MOUSE_EVENT:`-shaped input, same file
   `player_app/history.txt` already carries `MOUSE_EVENT:` lines for
   UI click-forwarding — see chtpm_parser.c:3082-3093 for the existing
   format) or does khtpm's own Interact Mode relay need a NEW mouse-
   motion/wheel relay path (currently keyboard-only, see
   `g_interact_relay_paths`)?** Likely both: khtpm needs to start
   forwarding mouse motion/button/wheel events (currently it doesn't
   capture mouse motion at all while armed, only clicks via the
   existing generic `MOUSE_EVENT:` capture — need to confirm whether
   raw motion, not just clicks, is captured), and `bv_menu_input.c`
   needs a new `MOUSE_EVENT:`-consuming branch parallel to its
   existing keycode-consuming one.
3. **Scope**: mouse-look while a button is held (drag), or free-look
   whenever the mouse moves inside the canvas (more game-like, but a
   bigger UX/capture change, may fight with the window's own normal
   pointer usage for clicking chrome buttons)?
4. Should mouse camera control be its own opt-in (separate from
   keyboard Interact Mode), or automatically available whenever
   Interact Mode is armed?

### Suggested implementation shape once the above is answered

1. Extend khtpm's Interact Mode arm/relay (`kh_scan_interact_relay()`
   + the `g_interact_relay_on` block) to also capture raw pointer
   motion/button/wheel events while armed — likely a new
   `kh_interact_relay_mouse(...)` alongside the existing keyboard
   forward, writing `MOUSE_EVENT: <button> <dx_or_x> <dy_or_y>
   <is_press>`-shaped lines to the same relay paths (reuse the
   existing wire format tpmos already parses, don't invent a new one
   — see `chtpm_parser.c:3082-3093` for the exact 4-field shape it
   expects).
2. Add a new branch to `bv_menu_input.c` that recognizes these mouse
   events (parallel to its existing single-`<keycode>` argv
   dispatch — will likely need its own argv shape or a second binary
   entry point, since `bv_menu_input.c` is currently invoked as
   `bv_menu_input.+x <keycode>`, one shot per keypress) and maps them
   to yaw/pitch/pan per the camera_mode table above, respecting the
   same per-mode gating (`render_mode==1`, mode-specific pan targets).
3. Live-verify via the same PNG-dump-before/after procedure as Part A.

## Files referenced (for whoever implements this)

- `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` —
  `kh_key_history_code()` (~5710), `g_interact_relay_on` block (~5183),
  `kh_scan_interact_relay()`.
- `44.xyz.01.00/&.widgits/board-viewer/ops/bv_menu_input.c` — camera/
  selector dispatch, `ARROW_*` defines (~60-63), camera key handling
  (~790-890).
- `44.xyz.01.00/&.widgits/5-pov-widgit.md` — authoritative camera/POV
  design doc, §2e for the full key table.
- `1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/chtpm_parser.c` —
  `process_key()` (~2707), `INTERACT` branch (~2999), `inject_raw_key()`
  (~1299), history poll loop incl. `MOUSE_EVENT:` parsing (~3054-3098).
- `44.xyz.01.00/101.mutaclsym🧟‍♂️️19.00/ops/camera_control.c` and
  `ops/choice.c` — original source `bv_menu_input.c` ports from.
- `44.xyz.01.00/@.apps/piececraft-hq/pchq-board.xhtpm` +
  `ops/pchq_board_projector.c` — the live template/projector this
  session built; Interact Mode's `tb-in` item is the arm trigger.
