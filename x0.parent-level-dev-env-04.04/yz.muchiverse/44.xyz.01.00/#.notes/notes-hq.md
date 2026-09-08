# notes - hq

_dev notes for the hq subsystem. Opened from HQ menu -> notes-hq._


## 2026-09-08 — toolbar disappears under fullscreen video (pals stay visible)

**Symptom:** YouTube (Chrome) fullscreen → desktop entities/pals still
visible (good), but the taskbar strip is covered.

**Likely cause (not yet fixed — investigation only):**

1. Inconsistent zorder state on disk:
   - `#.desktop/livedesk_override_redirect.pdl` = `override_redirect=true`
   - `#.desktop/khtpm_zorder_mode.state.txt` = `mode=above`
   `render_managed_wm_hints()` in `khtpm_core_render.c` does
   `if (g_override_redirect) return;` FIRST — so while the strip is
   override_redirect, the "above" mode never actually applies its
   `_NET_WM_STATE_ABOVE` + `_NET_WM_WINDOW_TYPE_DOCK` hints. The strip
   is a plain override_redirect top-level with no dock/above hint.

2. Under Wayland/GNOME, Chrome runs in Xwayland. When an Xwayland
   client goes fullscreen, mutter promotes that surface to the top of
   the Xwayland stack — **above** the house's override_redirect
   windows. So the fullscreen video covers everything house-side.

3. Why the pals survive but the strip doesn't: each pal renderer runs
   a continuous redraw loop that calls `XRaiseWindow(self)` every
   frame, so they pop back above the video within a frame. The strip
   uses the marker-gated ("DIAMOND") redraw — it only repaints/raises
   when *strip* state changes. A playing video changes nothing in
   strip state → the strip never re-raises → it stays buried.

**Options to consider later (do NOT do yet):**
- (best) set `override_redirect=false` so the strip is genuinely
  WM-managed + `_NET_WM_WINDOW_TYPE_DOCK` + struts. A real DOCK window
  with struts is kept visible by mutter even over fullscreen (that's
  how GNOME's own top bar behaves). Also fixes the inconsistent state.
- (cheap) low-frequency unconditional `XRaiseWindow(strip)` on a
  ~1–2 s timer, bypassing the marker gate — pops back like the pals,
  but fights the compositor / can flicker.
- (event-driven) watch other windows' `_NET_WM_STATE` /
  `_NET_ACTIVE_WINDOW` and re-raise the strip when a fullscreen
  appears.

## 2026-09-08 (later) — "nav stuck on 3" recurred

Same class as pc-hq-bugs.md Bug 5 (Interact Mode left armed -> renderer
forwards arrows to the game -> khtpm nav frozen on the File item).
Original fix commit: ccfed11a (engage_if_needed / restore_interact in
pchq_board_action.sh).

This recurrence, checked live: no pc-hq board window was even open; the
live board session's active_gui_is_typing.txt was already 0; grok's
6cf14364 kept the engage/restore pattern in file/desk/load-map. The
weak point that likely bit under load: restore_interact's wait for the
engine to reflect our engage was capped at 0.6s, and the box is at
~load 14 (Chrome ~225%), so the engine lags past that -> restore
skipped -> Interact left ON.

Done now: (a) hardened restore_interact - ~2s waits + VERIFY the toggle
took, retry once (uncommitted -> committed same day); (b) reset 3
stale active_gui_is_typing.txt=[1] on dead board-viewer sessions so a
relaunch can't inherit a trapped state.

If it's still stuck: the stuck window may NOT be pc-hq. Confirm which
window (strip? a specific app? the board?). If the board: open it, look
at the `In:` toolbar badge - `[^]` = engaged/trapped, click `In:` once
to release.

## 2026-09-08 (later²) — "stuck on 3" root cause: file cell → "load"

The strip's file cell (`[ ]3.`) menu → **"load"** row (`livedesk:load`)
called `ktb_hq_open(s, 100)`, which swapped the open menu IN PLACE for
the "session picker" sub-dropdown — a `which>15` pseudo-cell with no
real header cell behind it. That nested/replaced-popup shape has no
clean exit (its own comment: "13 is an inert cell and would close the
popup"); its nav latched and froze the strip focus on cell 3.

**Fixed:** `livedesk:load` now closes the menu and launches the real
**File Explorer widget** (`&.widgits/file-explorer/`) as its own X11
window (same setsid/button-script shape as `livedesk:open-settings`).
Navigable normally, closes with its own `[X]`.

**Follow-up (not done):** standalone the File Explorer widget just
browses — "pick a saved desk session → restore it" is not wired
through it. If "load" should restore a desk layout, that needs the
widget to return a pick and the manager to act on it. Or keep the
session picker but open it as its own window, not an in-place menu
swap. Same applies to the other `which` 100/101/102 internal sub-lists
(session picker, db-ez sections, common-events) — they're all the
fragile "replace the menu in place" pattern.

## 2026-09-08 (later³) — File Explorer is now a real picker

The widget was browse-only (published `result=` to its own state file,
nothing consumed it, couldn't set a start dir). Made real:

- **`file_explorer_manager.c`**: reads `<pkg>/fe_request.txt`
  (`mode=` / `start_dir=` / `result_file=`) on startup, unlinks it, and
  on pick/saveas/cancel writes the chosen absolute path (atomic) to
  `result_file`. No request file -> unchanged standalone behavior.
- **`&.widgits/file-explorer/fe-pick.sh <LOAD|SAVE> <start_dir>`**:
  modal helper - writes the request, launches the widget, waits for the
  pick (or window close), prints the path.
- **strip "load"** -> `#.desktop/scripts/pick-session.sh` -> fe-pick at
  the sessions root -> drops the session id in
  `#.desktop/livedesk_pending_open_session.txt` ->
  `ktb_poll_pending_session_open()` (new, called every main-loop tick)
  -> `livedesk_load_session()`. Real "load a saved desk session" now.
- **pc-hq board File -> "Open File Explorer"** (`file-hq` verb): now
  fe-pick at `pieces/system/maps`, then loads whatever `map.txt` was
  picked onto `chunk_0_0_z0` (same steps as the `load-map` verb).

Save As (strip) left as-is - it's an inline name field, not the picker,
and that's the right pattern for "name a new save".
