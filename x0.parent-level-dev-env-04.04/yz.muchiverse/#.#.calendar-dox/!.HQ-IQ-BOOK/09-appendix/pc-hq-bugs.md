# pc-hq board / Interact Mode — open bugs (2026-09-04)

Written for hand-off to another agent/session. Every item below was
either directly reported live by the user or reproduced by the
current session via the file-relay testing method (never headless
`--dump-and-exit` — see `khtpm-house-standards` skill / the
`khtpm-shared-layout-caution` memory for why). Cross-reference:
`PLAN-pchq-interact-camera-pov.md` (same directory) for the original
arrow-key fix + the Part B mouse-camera design doc; `handoff-2026-09-04-
master.md` Rev 14/15 for the wider pc-hq refactor and dead-code audit
this branch also did today.

Relevant live files for reproduction:
- `44.xyz.01.00/@.apps/piececraft-hq/pchq-board.xhtpm` (template)
- `44.xyz.01.00/@.apps/piececraft-hq/ops/pchq_board_projector.c` (state publisher)
- `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (renderer, `g_interact_relay_on`/`kh_scan_interact_relay()`/`handle_key()`)
- `44.xyz.01.00/&.widgits/_shared-lib/khtpm_draw_core.c` (badge glyph logic, `is_scope`)
- `44.xyz.01.00/&.widgits/board-viewer/pal/main_module.pal` + `ops/bv_menu_input.c` + `ops/bv_render_3d.c` (the actual live camera engine for a board-viewer session)
- `44.xyz.01.00/&.widgits/_shared-lib/system/prisc+x.c` (`OP_READ_HISTORY` — the pal-script interpreter opcode that reads `interact_relay.txt`)

Repro recipe used throughout (file-relay, not headless):
1. Launch a board-viewer widget session:
   `NO_GL=1 RUN_PROFILE=widget setsid bash &.widgits/board-viewer/button.sh run-widget <house>/@.apps/piececraft-hq`
2. Launch the window: `khtpm_core_render.+x <house> <house>/@.apps/piececraft-hq/pchq-board.xhtpm`
3. Find its PID, use `#.desktop/entity_menu_history/<pid>.txt` (create empty, sleep 1s to establish the poll baseline, THEN append — a first-ever write is always skipped as "pre-existing" by design) to send `MOUSE_EVENT: 1 <x> <y> 1` (two, since `click_two_step=1` house-wide) at the `tb-in` toolbar item's coords (read from a `112`-triggered frame dump's `.frame.txt`) to arm Interact Mode.
4. Send `112` (the `p`-dump key) to get a fresh PNG at `/tmp/entity-menu-frame.png` + `.frame.txt`/`.receipt.txt` — this session added a fix (see Fixed section) so `p` now dumps even while Interact Mode is armed and swallowing keys.

## Fixed this session (for context, not still open)

- Arrow keys (Up/Down/Left/Right) now correctly remap khtpm's own
  200-203 nav-capture codes to the game engine's own
  `ARROW_UP/DOWN/LEFT/RIGHT` = 1000-1003 convention, applied only at
  the Interact Mode relay site. See `PLAN-pchq-interact-camera-pov.md`
  Part A for the full citation trail. Commit `cdfd064d`.
- The relay was writing to `pieces/apps/player_app/history.txt`, which
  the actual live camera consumer (`pal/main_module.pal`'s
  `read_history pieces/apps/player_app/interact_relay.txt x2, x1`)
  never reads. Retargeted to `interact_relay.txt`, matching
  `board_viewer.chtpm`'s own `<interact src="pieces/apps/player_app/
  interact_relay.txt" />` declaration (the same generic-relay-path
  convention tpmos's golden-standard `chtpm_parser.c` implements via
  its own `<interact>` tag / `interact_history_path`, PRIORITY 1 over
  the bare `history.txt` fallback - confirmed by direct read of that
  file, not assumed). Commit `cb6aee7b`.
- `'p'` now also fires a local frame dump even while Interact Mode is
  armed and forwarding every other key to the game (previously
  swallowed like every other key, by design — made an exception for
  `'p'` specifically since it's not on the documented camera/POV key
  table, so debugging visibility costs nothing). Not yet committed —
  see below.
- Letter/digit keys (`q e r t w a s d c v f 0 1 2 3 4`) confirmed at
  the WIRE level to forward as their literal ASCII values (matches
  what both `chtpm_parser.c` and `bv_menu_input.c` expect per
  `PLAN-pchq-interact-camera-pov.md`'s own citations) — but see Bug 1
  below, this does NOT mean the camera actually responds to all of
  them yet; only confirmed for camera *yaw* (`q`).

## Open bugs

### Bug 1 — POV/camera-mode number keys ('0' 2D/3D toggle, '1'-'4' camera_mode) not working

**Report (user, verbatim):** "some camera controls are working. pov
change (and "0" to switch between 2d/3d) isn't working yet"

**Status:** confirmed camera *yaw* (`q`) visibly moves the rendered
scene after the `interact_relay.txt` fix (character/backpack
orientation changed between two live PNG dumps in this session,
unprompted by anything else). `0`/`1`-`4` not yet confirmed either way
— not isolated with its own before/after PNG diff this session before
time ran out.

**Where to look next:** `bv_render_3d.c` reads `camera_mode`/
`render_mode` from a state file via `read_kv_int(state_path, ...)`
(around khtpm_core_render.c-style key=value parsing — see
`bv_render_3d.c:1495` `int camera_mode = read_kv_int(state_path,
"camera_mode", ...)`). Need to find what actually WRITES
`camera_mode`/`render_mode` into that state file in response to a `0`-
`4` keypress — almost certainly `bv_menu_input.c`'s own key handling
(NOT a separate watcher process — confirmed no `bv_menu_input.+x`
process is ever resident; it must be invoked per-keypress by something,
likely the same `pal/main_module.pal` loop that already reads
`interact_relay.txt`, via a `bv_menu_input x2`-style pal-script
instruction after `read_history`). Trace that path the same way the
arrow-key/`interact_relay.txt` bugs were traced (read the actual
consumer, don't assume from `bv_menu_input.c`'s own header comments)
before assuming this is even a khtpm-side problem — it may already
work and just needs a longer settle/different test methodology, same
false trail hit on Bug 3 below.

### Bug 2 — real user keyboard/mouse focus lost after clicking away and back

**Report (user, verbatim):** "for some reason keys aren't going thru
event when focus is on and interact mode is on, if user clicks away
from window, then goes back to it later. this was never a problem
before or in mutaclysm"

**Status:** NOT reproduced by this session (all testing here used the
file-relay method, which bypasses real X11 focus/click delivery
entirely — this bug can only manifest with genuine hardware
mouse/keyboard, which this session cannot generate). Real, reported
regression versus both the pre-refactor behavior and mutaclysm's own
interact mode.

**Where to look next:** `khtpm_core_render.c`'s `ButtonPress` handler
already has a real "click anywhere in window -> `kh_raise_and_focus()`"
fix from earlier this session (2026-09-03, see the comment starting
"REAL FIX 2026-09-03 (direct live report: 'its way to hard to get
window focus...'" around line 6114) — but that fix predates today's
Interact Mode feature and was never specifically tested against a
window with `g_interact_relay_on` engaged. Two candidate root causes,
neither confirmed:
  (a) `kh_raise_and_focus()`/`XSetInputFocus` isn't actually being
      called or is failing silently for this window class - would
      affect ANY window, not Interact-Mode-specific, worth checking
      the general case first (is this bug pchq-board-only or house-
      wide? not yet established).
  (b) something about `g_interact_relay_on`'s keyboard intercept at
      the top of `handle_key()` interacts badly with X11 focus state
      specifically after a focus round-trip (e.g. a stale
      `XGrabKeyboard`/no grab at all - Interact Mode currently does
      NOT call any keyboard-grab function when arming, unlike
      `cli_io`'s own `kh_grab_keyboard_retry()` — worth checking
      whether it needs one, given this house's own documented
      focus-follows-mouse policy and the `RMMV-CLICK-CAPTURE-
      INVESTIGATION` precedent about real hardware clicks not
      reaching an `XGrabPointer`-holding client under Wayland/Mutter).
This needs a REAL human tester (or xdotool against a real X session,
per house testing hierarchy's last-resort tier) since the file-relay
method cannot exercise real focus transitions.

### Bug 3 — "^" badge glyph never shows, even though the "In: ON" label and `interact-active` CSS class both update correctly

**Report (user, verbatim, asked multiple times):** "'^' isn't changing
yet. still shows '>' tho it is on. that should be an easy fix, i asked
u to fix many times."

**Status: reproduced live by this session**, own PNG dump on a fresh,
cleanly-launched process (not a re-used/stuck one) — confirmed
`In: ON` label text is current and `class="pchq-tb,interact-active"`
is present in the same `.frame.txt` dump, yet the visible badge glyph
in the corresponding PNG still reads `[>]2.` not `[^]2.`. Matches the
user's own screenshot exactly (`arm.png`, `[>]2. In: ON`).

**Root-cause investigation so far (incomplete):**
- The badge-glyph decision is `is_scope = ... || (g_interact_relay_on
  && e->relay[0])` in `khtpm_draw_core.c` (~line 719), feeding
  `elem_cursor_prefix(e, focus_nav, is_scope, ...)`
  (`khtpm_render_core.c:260`) which unconditionally returns `"[^]"`
  when `is_scope` is true — this logic reads correct in isolation, no
  bug found in either function by inspection.
- Both prerequisites for `is_scope` to be true LOOK satisfied from the
  `.frame.txt` dump: the item's own class list includes
  `interact-active` (so `kh_scan_interact_relay()` should have found
  it and set `g_interact_relay_on=1`), and its `relay=` attribute is
  non-empty (declared in the template as `relay="${bv_h1},${bv_h2}"`).
- **Left off mid-investigation**: was checking whether
  `render_tree()` (the function that actually walks the Elem tree and
  calls `draw_elem()` on each node, in `khtpm_draw_core.c:1060`) is
  even CALLED for this window's generic/default mode at all — a grep
  across the whole house found ZERO call sites for `render_tree(`
  with an actual argument anywhere (only the function's own internal
  recursive calls to itself, and comments referencing it) — meaning
  either (a) grep missed something (whitespace/macro obfuscation,
  worth re-checking with a different tool), or (b) generic/default
  mode draws via some OTHER path entirely, not `render_tree()`, and
  THAT path may be constructing/copying `Elem` structs that don't
  carry `e->relay` through correctly (same known bug CLASS as the
  `is_active_scope`/"draw copies have no parent pointers" warning
  already documented for db-hq's own frame-file round-trip a few
  lines above the `is_scope` computation in `draw_core.c` — worth
  checking whether pchq-board's generic mode has an analogous
  text-serialize-then-redraw round trip that silently drops newer
  `Elem` fields like `relay` that the serializer was never updated to
  carry).
- **Next step for whoever picks this up**: find the actual real
  call site that kicks off drawing for a generic/default-mode window
  (search for where `draw_elem`/`render_tree`/a page root gets passed
  in from `redraw()` - possibly under a different literal spacing,
  or check if `redraw()` itself inlines the tree walk rather than
  calling `render_tree()`), confirm whether `e->relay` survives to
  that exact draw-time `Elem` instance for `tb-in`, and add a
  temporary debug `fprintf(stderr, ...)` on `g_interact_relay_on` and
  `e->relay[0]` right at the `is_scope` computation to settle this
  empirically rather than by further static reading - this was the
  planned next step when this session ran out of ability to continue
  investigating.

### Bug 4 — chrome should get "!" and "_" buttons

**Report (user, verbatim):** "we should add '!' and '_' to chrome."

**Status: not investigated at all.** Meaning of `!`/`_` not
confirmed — likely candidates based on house convention (needs
confirming with the user before implementing, don't guess):
- `!` — a notification/alert indicator, matching a convention used
  elsewhere in the house's chrome bars (needs grep for an existing
  `!`-glyph chrome item on another window to confirm the convention
  before inventing one here).
- `_` — a minimize button (`MINIMIZE` is already a reserved onclick
  verb per this file's own verb table, per `XHTPM-PARSER-REFERENCE.md`
  §16 - `pchq-board.xhtpm` currently has no minimize item at all,
  only `close`).
Ask the user to confirm before implementing.

## Notes for whoever continues this

- The house's own testing-hierarchy rule applies doubly hard here:
  file relay > text state dump > PNG dump > xdotool/external tools.
  This session hit real flakiness with its OWN PNG-dump testing
  methodology mid-session (a `p`-dump silently no-op'd for several
  minutes because Interact Mode was swallowing the `p` key itself —
  now fixed, see "Fixed this session" above) and separately (a
  DIFFERENT, stuck/over-reused test process from repeated rapid
  re-arming appeared to show a frozen `clock=` value ~10 minutes
  stale) — always confirm you're talking to the PID you think you are
  (`pgrep -af`, check `ps -o lstart` against wall-clock reality) and
  prefer a fresh launch over reusing a process that's been through many
  rapid test iterations, per this session's own `khtpm-shared-layout-
  caution` memory.
- Bug 1/2/3 may turn out to share a root cause (a generic problem with
  how this window's live redraw/reparse path handles fields added
  after its original construction, e.g. `relay`) or may be three
  unrelated bugs - don't assume either way going in.
