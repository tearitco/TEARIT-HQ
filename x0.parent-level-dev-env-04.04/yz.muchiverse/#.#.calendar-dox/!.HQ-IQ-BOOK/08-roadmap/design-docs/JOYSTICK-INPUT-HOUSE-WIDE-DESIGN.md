# House-wide joystick input + .pdl button remap (design spec, v1)

**2026-09-15.** Every x11-hq window should accept joystick/gamepad
arrow input, with every other button remappable via a `.pdl` file —
without adding per-window code, matching the same
"one generic mechanism, zero per-app dispatch" rule this house already
uses everywhere else in `khtpm_core_render.c`.

## 0. Baseline (confirmed by direct code search)

- **No joystick/gamepad code exists anywhere in `44.xyz.01.00` today**
  (`khtpm_core_render.c`, `khtpm_taskbar_manager*.c`, and every other
  `.c` file in the tree — zero matches for joystick/gamepad/`/dev/input`/
  evdev/XInput/SDL_Joystick).
- A sibling prototype tree, `1.TPMOS_c_+rmmp.0103.0001` (NOT part of
  this house, a separate/earlier codebase — do not port its files
  directly, read them for the API only), has real, working joystick
  code worth learning from:
  - `pieces/joystick/plugins/joystick_input.c` — reads raw
    `struct js_event` off `/dev/input/js0` (the Linux joystick API,
    not SDL/evdev/XInput2), with a real axis threshold+hysteresis
    (`THRESHOLD 16384`, deadzone `THRESHOLD/2`) worth reusing as-is —
    that math is the one genuinely reusable piece.
  - **What NOT to copy**: it's a standalone poll-loop that merges
    synthetic key codes straight into `pieces/keyboard/history.txt`
    (shared keyboard stream) with a bespoke focus-lock-file gate, and
    has **no button-remap config of any kind** — the 2000+N/2100+N
    codes are hardcoded. None of that fits this house's real per-
    window relay model (below), and there's no `.pdl` remap format to
    copy — that part has to be designed fresh here.

## 1. The real integration point: the existing per-window relay file

This house already has the exact right generic mechanism, built for
precisely this kind of external input injection — no new per-window
plumbing needed:

```
#.desktop/entity_menu_history/<pid>.txt
```

Every `khtpm_core_render.c` window already polls its own PID's file
every tick (`poll_agent_history()`), accepting `KEY_PRESSED: <n>` lines
— real X11 keypresses and agent-injected input already go through this
identical path today. Arrow codes are already reserved and working:
`200`/`201`/`202`/`203` = Up/Down/Left/Right, `204`/`205` =
PageUp/PageDown.

**A joystick daemon just needs to write `KEY_PRESSED: 200-203` (and a
remapped code for every other button) into the CURRENTLY-FOCUSED
window's own relay file.** That's the whole integration — no renderer
change required for the "arrows work" half of this request.

## 2. Focus routing (no new discovery mechanism needed)

Each window process already self-checks real X input focus via
`XGetInputFocus` at several existing call sites (e.g.
`khtpm_core_render.c` ~line 8429) — it already knows, every frame,
whether IT is the focused window. Rather than have an external
joystick daemon try to reverse-map "which X window is focused → which
PID owns it" (fragile — no `_NET_WM_PID` property is set anywhere in
this codebase today), the natural, minimal-change design is the
opposite direction:

- A new, small, standalone joystick daemon (own binary, `ops/`
  convention, real `.c` reading `/dev/input/js0` via the same raw API
  + threshold/hysteresis math as the TPMOS reference) writes every
  joystick event to **one shared, house-wide file**:
  ```
  #.desktop/joystick_history.txt
  ```
  (append-only, `KEY_PRESSED: <code>` lines — same format as the
  per-pid relay, just not addressed to a specific pid.)
- `poll_agent_history()` in `khtpm_core_render.c` gains a second, real
  read: **only when this window already knows it holds real input
  focus** (the existing self-check), it ALSO tail-polls
  `joystick_history.txt` (own cursor, same append-only/never-truncate
  contract as the per-pid file) and dispatches those codes exactly
  like any other `KEY_PRESSED` line. A window that isn't focused never
  reads it — so no event is ever double-delivered to two windows.
- This is a real, generic, ~15-line addition to one already-existing
  function, not a new subsystem — matches "without much fuss."

## 3. Button remap via `.pdl`

New file, house-wide (not per-window — one joystick, one remap table):
```
#.desktop/joystick_map.pdl
```
Row shape (matches every other `.pdl` in this house — pipe-delimited,
`SECTION | KEY | VALUE`):
```
SECTION      | KEY                | VALUE
BUTTON       | 0                  | 13        (Enter)
BUTTON       | 1                  | 27        (Escape)
BUTTON       | 2                  | 9         (Tab)
AXIS         | 0-neg              | 202       (Left)
AXIS         | 0-pos              | 203       (Right)
AXIS         | 1-neg              | 200       (Up)
AXIS         | 1-pos              | 201       (Down)
```
- `BUTTON | <button_number> | <key_code>` — physical joystick button
  index → the same `KEY_PRESSED` code space every khtpm window already
  understands (a real ASCII code, or one of the reserved 200+ codes).
- `AXIS | <axis_number>-neg / -pos | <key_code>` — lets arrows come
  from either a D-pad (buttons) or an analog stick (axes), per
  controller; v1 hardcodes the axis→arrow mapping as the DEFAULT
  (matches the direct ask: "arrows" always work), `BUTTON` rows are
  the user-changeable part.
- Loaded once at daemon startup (and on file-change, same
  `reparse_chtpm_if_changed()`-style mtime-or-marker check every other
  live-reloadable house config uses — **use the append-only-marker
  convention, not raw `st_mtime`**, per this session's own standing
  house rule) — no restart required to remap.
- A row absent from the file = that button/axis produces no event
  (safe default, not a crash).

## 4. Scope / what's NOT in v1

- Multiple simultaneous joysticks: v1 is one device
  (`/dev/input/js0`, overridable via daemon argv, same as the TPMOS
  reference) — real multi-controller support is a separate follow-up,
  not silently half-done.
- Per-WINDOW remap overrides (e.g. a game wanting different button
  semantics than a plain menu): out of scope for v1 — `joystick_map.pdl`
  is house-wide only. Flag if a real consumer needs this later.

## 5. Open items to verify before/while building

1. Confirm `/dev/input/js0` is actually readable without special
   permissions on the target house install (group `input` membership,
   udev rule) — the TPMOS reference doesn't document this either.
2. Decide where the daemon gets launched from (a new line in the same
   startup script that launches the taskbar manager, guarded so it
   doesn't hard-fail the whole desktop boot if no joystick is plugged
   in).
3. Confirm the exact reserved `KEY_PRESSED` code band doesn't collide
   with anything else added since `200`-`205` were reserved (grep
   `poll_agent_history`'s own switch/dispatch before picking new codes
   for buttons beyond the default remap table).
