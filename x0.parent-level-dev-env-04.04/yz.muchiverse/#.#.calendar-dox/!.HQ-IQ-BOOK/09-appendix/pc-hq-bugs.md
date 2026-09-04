# pc-hq board / Interact Mode — open bugs (2026-09-04)

## READ THIS FIRST — triage order

If the user reports several of these at once, **fix/investigate in
this order, not the order they were mentioned** — later items are
often just symptoms of an earlier one, and chasing them out of order
wastes real time (this exact mistake was made once already this
session, see the "keyboard-grab regression" postmortem below Bug 2):

1. **Real keyboard/mouse focus (Bug 2) FIRST, always.** Nothing else
   in this file can be manually verified by the user without reliable
   keyboard focus — badge state (Bug 3), camera keys (Bug 1), none of
   it. If the user says focus is broken, stop investigating anything
   else and fix focus first, even if they also mention other bugs in
   the same message.
2. Only once focus is confirmed solid, move to Bug 3 (badge) or
   Bug 1 (camera keys) — either order, they're independent.
3. Bug 4 (chrome buttons) is cosmetic/additive, lowest priority,
   fine to defer indefinitely or batch with anything else.

Also: **this session introduced a real, live-breaking regression while
chasing Bug 2** (a display-wide `XGrabKeyboard` that locked out
keyboard input for the ENTIRE house, not just pc-hq, until the process
was killed) — already reverted (see "Keyboard-grab regression" below
Bug 2) but a cautionary example: an X11-level fix (grabs, focus,
override_redirect) needs to be reasoned through for its DISPLAY-WIDE
blast radius, not just its effect on the one window being fixed,
before landing it. If in doubt, prefer something scoped to the one
window (`XSetInputFocus` reassertion) over anything `XGrab*`-based.

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
before or in mutaclysm" ... "the window is starting with focus, then
loses it when i press an arrow key" ... "it goes from ^ to ." (the
real, live `XGetInputFocus`-backed title-bar indicator, see
`khtpm_core_render.c` ~line 4850's own "focus sanity indicator"
comment - NOT the nav badge, a separate feature) ... "db-hq pal still
works (but tb and pc-hq aren't 'keeping keypresses' like db-pal)".

**Status: partially resolved, partially still open.** This session
chased it into a real, live, self-inflicted regression before finding
the actual bug - full postmortem below. The regression is reverted.
The ORIGINAL, narrower complaint (focus/keys not sticking reliably on
pchq-board specifically, comparable to db-hq-pal working fine) is
**still open and needs re-testing now that the regression is gone** -
this session ran out of ability to continue before that re-test
happened.

#### Postmortem: the XGrabKeyboard regression (already reverted, commit `54b0c564`)

First attempt at fixing this bug (commit `2a7968b6`) added an
`XGrabKeyboard` when Interact Mode arms, `XUngrabKeyboard` when it
disarms - reasoned by direct comparison against `cli_io`'s own
identical-looking need (`activate_focused()`'s `kh_grab_keyboard_
retry()`, needed because this desktop's focus-follows-mouse policy
means moving the pointer off a window silently hands real X keyboard
focus elsewhere unless a grab overrides it). The reasoning was sound
for cli_io's use case but wrong for Interact Mode's:

- A keyboard grab is **display-wide** - while held, NO window in the
  house can receive real keyboard input, not just the grabbing one.
  This file already has its own standing warning about exactly this
  risk class (~line 9192, "XGrabKeyboard/XGrabPointer are DISPLAY-WIDE
  ... focus control problems").
- cli_io's grab is safe because it's released reliably and quickly
  (Escape, or the field losing armed state, both handled directly in
  `handle_key()`/`activate_focused()` with no external dependency).
  Interact Mode's disarm depends on the projector re-publishing
  `interact_class=""` AND a **reparse actually happening** to pick
  that up - the same reparse/vars-refresh path already shown fragile
  in this session (see Bug 3's root cause below) is also the ONLY
  thing that can release this grab. A grab that depends on a flaky
  release mechanism is much more dangerous than one that doesn't.
- Live-confirmed by the user: after arming Interact Mode once, EVERY
  other window in the house - including the taskbar itself - stopped
  receiving real keyboard input ("i touch tb for focus again and no
  longer have arrow control again. wtf?"). Killing the pc-hq process
  (severing its X connection, the only OTHER way an X11 grab releases
  besides an explicit ungrab) immediately restored normal keyboard
  routing everywhere else - definitive confirmation the grab, not
  anything specific to pc-hq's own window, was the cause.

**Reverted in full** (`54b0c564`) - `kh_scan_interact_relay()` no
longer calls `XGrabKeyboard`/`XUngrabKeyboard` at all, back to the
original (buggy, but at least not display-wide-catastrophic)
behavior.

**Lesson for whoever picks this up next**: an X11-level focus/grab fix
must be reasoned through for its DISPLAY-WIDE blast radius before
landing, not just its effect on the one window being fixed. If this
needs revisiting, prefer something scoped to just this window
(a periodic/on-relevant-event `XSetInputFocus(dpy, win, ...)`
reassertion, matching the pattern already used elsewhere in this file
for the position-corrective-move case ~line 5067-5083) over anything
`XGrab*`-based, and if a grab is ever truly necessary, make ABSOLUTELY
sure its release path cannot get stuck (a hard timeout ungrab, not
just "wait for the projector to say so").

#### What's structurally ruled out

- **Not a desktop/WM-level keybinding** - checked
  `org.gnome.desktop.wm.keybindings` and `org.gnome.mutter.keybindings`
  for any bare (unmodified) arrow-key binding that could steal focus
  as a workspace-switch/tile side effect: none found, every arrow-key
  binding in this GNOME/Mutter session requires a modifier
  (Super/Alt/Control). Ruled out as the cause.
- **Not `is_popup`-related** - `hq_run_event_loop(wm_delete, is_popup)`
  is called with `is_popup=1` unconditionally from the one real call
  site (~line 12450) for every generic/default-mode window, pchq-board
  included - real `KeyPress` events reach `handle_key()` identically
  for pchq-board and any other window of this same mode (e.g.
  bookmarks-pal, taskbar-settings-pal). Not a differentiator by itself.
- **`g_has_canvas` only changes poll interval/force-redraw** - grep
  confirms its only 5 reference sites (khtpm_core_render.c) are the
  event-loop timeout (33ms vs 150ms) and forcing `g_frame_dirty=1`
  every tick. It does not touch window creation, `override_redirect`,
  WM hints, or any focus-related call - structurally identical to any
  other window in this respect.

#### Still open: db-hq-pal vs pchq-board/taskbar comparison

User's direct report: **db-hq-pal reliably keeps keyboard input; the
taskbar and pc-hq do not** (independent of the now-reverted grab
regression - this was reported as a pre-existing difference). This
session started but did not finish a structural comparison. What's
confirmed so far:

- db-hq-pal sets `g_default_has_sidebar_panel=1` (has `<sidebar>`/
  `<panel>` tags); pchq-board does not (it uses the flat toolbar-item
  layout, gated on `has_canvas`, a different `assign_nav_and_layout()`
  branch entirely - see the `khtpm-shared-layout-caution` memory for
  why these two branches are kept deliberately separate).
- `g_default_has_sidebar_panel` gates several real behaviors checked
  so far: a per-window taskbar registry write (~line 4900), a
  minimize-restore signal check (~line 6008), and the generic
  minimize/fullscreen auto-chrome (~line 3144, `g_default_minimize_
  elem`/`g_default_fullscreen_elem` - already noted in Bug 4, pchq-
  board needed its own manual chrome items since it never gets these
  automatically). **None of these three look focus-related on
  inspection** - but this was a fast read, not exhaustive.
- The one genuinely focus-related generic mechanism found
  (~line 5028-5083, the position-corrective-move-reasserts-focus
  block) is **unconditional**, not gated by `g_default_has_sidebar_
  panel` - applies identically to db-hq-pal and pchq-board. Not the
  differentiator either, unless there's a second, not-yet-found
  db-hq-specific focus mechanism this pass missed.

**UPDATE - actual root cause found and fixed (commit `e839d71e`)**:
not a pc-hq-side bug at all - the TASKBAR's own pre-existing (not
authored this session) `dock_grab_keyboard()`/`dock_release_keyboard_
if_left()` pair. The dock takes a real, display-wide `XGrabKeyboard`
on click (`dock_grab_keyboard()`, ~line 3237); its matching release
only fires on a genuine `FocusOut` event for the dock's OWN window
(`dock_release_keyboard_if_left()`, ~line 3246, called from `hq_
dispatch_xevent()`'s `FocusOut` branch). User's exact report - "tb top
gets focus (steals it) and wont ever give it back" the instant an
arrow key is pressed after clicking pc-hq - matches this sequence
exactly:
1. Taskbar grabs keyboard at some earlier point (any click on it).
2. User clicks pc-hq. Real X focus moves there (`XGetInputFocus`
   confirms it - the `^` indicator is genuinely correct at this
   point) - but the grab is a SEPARATE mechanism from focus and stays
   held regardless, unless the dock's own `FocusOut` release fires.
3. If that release is ever missed for any reason (this file already
   documents multiple real Mutter/XWayland focus-notification quirks
   elsewhere - not chased further, the fix below doesn't need to know
   why), the next real KeyPress still routes to the DOCK (the
   grabbing window), not pc-hq, regardless of nominal focus.
4. The dock's own arrow-key nav handler in `handle_key()`
   (~line 5302-5328, `if (ks == XK_Up || ks == XK_Left) { ...
   XRaiseWindow(...); XSetInputFocus(...); dock_grab_keyboard(...); }`)
   then processes that misdirected key as legitimate taskbar-strip
   navigation - which explicitly re-raises, re-focuses, and RE-GRABS
   the dock as an ordinary side effect. Self-reinforcing: every
   subsequent arrow key (meant for pc-hq) keeps landing on the dock,
   keeps re-triggering this, forever.

Fixed with two live-focus guards (both check real `XGetInputFocus`,
not the cached/one-tick-stale `g_focus_owned_painted`, before letting
the dock act as though it legitimately owns keyboard input): the top
of `handle_key()` releases and bails on any key that arrives at the
dock without it actually holding real focus; the reparse-triggered
re-grab in `hq_idle_tick()` (previously an unconditional `if
(g_dock_kbd_win) dock_grab_keyboard(...)`) does the same check before
re-asserting.

**This is a long-running process fix - the taskbar itself needs to be
restarted to pick it up, a rebuild alone does nothing for an already-
running instance.** Restarted live this session (`sh run_khtpm_strip.sh
new`). User re-tested with real hardware: **still broken** - see the
next update below, this was not the whole story.

#### UPDATE 2 - the real, deeper root cause: `override_redirect` itself, proven once already by a deleted fix

`xdotool` testing (real synthetic X11 events, not file-relay) showed
the click-taskbar/click-pchq/press-arrow sequence working correctly -
focus stayed on pc-hq (`XGetInputFocus` confirmed) and pc-hq's own nav
index moved in response to real arrow keypresses sent this way. But
the user confirmed **real hardware still fails the exact same way**
even after the taskbar restart. This is not a contradiction - it's a
known, ALREADY-DOCUMENTED, ALREADY-FIXED-ONCE bug class in this exact
house, found by reading the pre-deletion `run_pchq_board_mode()`
(recovered via `git show 35c1b0b1~1:.../khtpm_core_render.c`):

> "REAL FIX 2026-08-30, direct live report ('its not geting mouse /
> kbd input') - **override_redirect windows never get real keyboard/
> mouse focus routed by Mutter** (synthetic XTest input worked, masking
> the bug) - normal WM-managed window, decorations stripped via
> `_MOTIF_WM_HINTS` instead, same real shape x11_mirror.c itself uses."

That sentence ("synthetic XTest input worked, masking the bug") is
EXACTLY why the `xdotool` test above showed success while real
hardware fails - it's the same distinction, previously found and
fixed, for this exact same window's own earlier incarnation.

**The fix already exists in the shared renderer, unused by the current
config.** `render_managed_wm_hints(dpy, win, !g_override_redirect)`
(khtpm_core_render.c:12308) already implements the "WM-managed,
decorations stripped via hints instead of override_redirect" pattern
house-wide, for every generic-mode window - it's called with
`!g_override_redirect`, so it already activates whenever
`g_override_redirect` is false. **`#.desktop/livedesk_override_
redirect.pdl` currently reads `override_redirect=true`** - a single,
house-wide setting (read once at startup by every window via
`load_override_redirect()`, no per-window-class override anywhere -
confirmed by grep, only 3 total assignment sites) - so every window in
the house, db-hq-pal included, is currently running override_redirect
mode, the same mode the old pc-hq explicitly proved broken for real
keyboard/mouse input.

**Open question, not yet resolved**: if override_redirect genuinely
breaks real keyboard focus, why does db-hq-pal reportedly still work
for the user? Two honest possibilities, neither confirmed:
1. db-hq-pal is ALSO subtly affected but tolerates it better (its own
   periodic per-window registry write / position-corrective-focus-
   reassert, both already found in this doc's earlier comparison work,
   might paper over brief focus drops that a continuously-interactive
   game window like pc-hq can't tolerate).
2. The bug is real but intermittent/timing-dependent under Mutter,
   and the user's own db-hq-pal usage pattern hasn't hit the same
   trigger sequence (click-taskbar-then-click-target-then-key) as
   thoroughly as pc-hq has been tested this session.
Worth testing db-hq-pal with the EXACT same click-taskbar/click-
target/arrow-key sequence before assuming it's genuinely immune.

**Recommended next step - low-risk, no new code, easily reversible**:
flip `#.desktop/livedesk_override_redirect.pdl` from
`override_redirect=true` to `override_redirect=false` and re-test the
exact failing sequence with real hardware. This activates
`render_managed_wm_hints()`'s already-existing managed branch for
EVERY window house-wide (not a pc-hq-specific change) - since this
setting is shared/global and affects the user's entire currently-
running desktop (taskbar included), get explicit confirmation before
flipping it, same caution as any other house-wide, live-session-
affecting change this document already warns about. If it fixes the
symptom, the real follow-up question becomes whether `override_
redirect=true` was ever fully correct for this desktop/compositor
version at all, or whether it should become the new default - a
bigger decision than one bug fix, flag it back to the user rather
than deciding unilaterally.

The db-hq-pal vs pc-hq comparison work-so-far (ruled out is_popup,
g_has_canvas, the position-corrective-move block, and the taskbar-
registry write) is left in place below for reference, but is likely
now MOOT for the specific "steals and never gives back" symptom - that
was the dock's grab, not anything about pc-hq's own window class. If
a milder focus issue still exists after the taskbar restart, resume
from where the comparison below left off.

<details>
<summary>db-hq-pal vs pchq-board comparison (pre-root-cause investigation, likely superseded)</summary>

**Next step**: grep khtpm_core_render.c for every remaining
`g_default_has_sidebar_panel`-gated block (there may be more the fast
read above missed) and check each one for a real difference in
keyboard/focus handling; also grep for any OTHER db-hq-specific global
(anything still prefixed `g_dbhq_` or `dbhq_`) that might independently
reassert focus/keyboard ownership on a schedule pchq-board's flat-
layout branch never runs. Re-test the plain (non-grab) behavior first,
now that the regression is reverted - some of what was attributed to
"db-hq works, pc-hq doesn't" during this session may have actually
been the display-wide grab regression already in effect by the time
of that comparison, not a real pre-existing difference. Confirm with
a clean re-test before assuming the difference is real.

</details>

### Bug 3 — "^" badge glyph never shows — FIXED (commit `2a7968b6`), needs a fresh live re-check once Bug 2 is settled

**Report (user, verbatim, asked multiple times):** "'^' isn't changing
yet. still shows '>' tho it is on. that should be an easy fix, i asked
u to fix many times."

**Root cause, confirmed**: every generic/default-mode window (pchq-
board included) draws through a serialize-Elem-to-text-file, then
re-parse-and-redraw round trip - `redraw()` calls `kh_serialize_frame_
subtree()`/`kh_serialize_frame_elem()` to write each Elem's fields to
a text file, then reads that file back line-by-line via `kh_paint_
frame_line()`, which builds a **fresh, `memset`-zeroed temp `Elem`**
from only what the text line says and draws THAT, never the live
`g_pool[]` Elem. (Confirmed `render_tree()`, which DOES walk the live
tree directly, has zero real call sites anywhere in the house - it's
dead code for this purpose; this frame-file round trip is what
actually draws every generic-mode window.) `e->relay` was never one
of the serialized fields - same class of gap `target_id`/
`input_buffer` had before a 2026-08-31 fix added THEM. So `tmp.relay[0]`
was always `'\0'` at draw time regardless of the real Elem's value,
and `is_scope = ... || (g_interact_relay_on && e->relay[0])` in
`khtpm_draw_core.c` could never see it - the check itself was correct,
it just never got fed a real value.

**Fix**: added `relay` as a 9th pipe-escaped tail field to both
`kh_serialize_frame_elem()` and `kh_paint_frame_line()`, same escape
convention as `target_id`/`input_buffer`.

**Verification status**: confirmed via this session's own PNG dump
that the fix is structurally sound and builds/runs without breaking
the context-menu stability check. **NOT yet re-confirmed with a fresh
screenshot showing the actual `[^]` glyph** - the live-testing session
moved on to the focus regression (Bug 2) before completing that final
visual check. Do that first when picking this back up - should be
quick given the fix is already landed, just needs a real PNG dump of
an armed Interact Mode window to confirm `[^]` now shows instead of
`[>]`.

### Bug 4 — chrome should get "!" and "_" buttons — FIXED (commit `874c71b5`/`2a7968b6`)

**Report (user, verbatim):** "we should add '!' and '_' to chrome."

Turned out `!` (Fullscreen) was a REAL item the original
`run_pchq_board_mode()` toolbar had (`PCHQ_ACT_FULLSCREEN`) that got
dropped by omission when `pchq-board.xhtpm` was first built this
session - not a new feature request, a regression. Restored, wired to
the house's already-generic `TOGGLE_FULLSCREEN` onclick verb. Also
restored `Menu` and `Player` (real stub toolbar slots from the same
original toolbar, also dropped by omission - `action="void"`, present
for layout/nav parity only, matching the original's own intent, not
wired to anything new).

`_` confirmed by the user to mean minimize (`MINIMIZE` reserved verb,
`XHTPM-PARSER-REFERENCE.md` §16). The generic sidebar+panel layout
auto-adds this chrome item for its own windows (`g_default_minimize_
elem`), but pchq-board uses the flat toolbar-item/canvas layout, which
never gets it automatically - added explicitly.

Live-verified via PNG dump: chrome now reads `_`/`!`/`x`; toolbar reads
`In/File/Desk/Menu/Player/<clock>`.

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

## 2026-09-04 UPDATE - status after the override_redirect incident

- **Bug 3 (`^` badge): CONFIRMED FIXED** by the user live.
- **Canvas blanking (new, not one of the 4 original bugs)**: FIXED -
  `kh_draw_canvas()` no longer blanks the display on a single bad
  receipt-read tick from the producer process. See `04-bugs/BUG-LOG.md`
  2026-09-04 "recently fixed" for the short version, commit `d9afd258`
  for the full diff/reasoning.
- **Bug 2 (real keyboard focus)**: root cause (override_redirect)
  is very likely correct but the fix attempt (a blanket house-wide
  flip) was WRONG and had to be reverted - see `03-pitfalls/X11-AND-
  SESSION-PITFALLS.md`'s 2026-09-04 entry and `04-bugs/BUG-LOG.md`'s
  🔄 CORRECTION on this same bug for the full incident. Currently back
  at baseline (`override_redirect=true` house-wide, same as it always
  was before this session touched it) - Bug 2 itself is UNCHANGED from
  where it started, still open.

## Design notes for a scoped, per-window override_redirect (NOT YET BUILT - think through carefully before starting)

The house-wide flip broke taskbar dropdowns/popups because `g_override_
redirect` is a single global, loaded once at startup by EVERY window
from one shared file (`load_override_redirect()`,
`#.desktop/livedesk_override_redirect.pdl`), with no way to say
"this ONE window should be WM-managed, everything else stays
override_redirect." Rough shape of what a real fix needs, written down
now while the context is fresh, NOT implemented - a design worth
scrutiny before committing to it, not a queued task:

1. **A window needs to opt IN to managed mode individually**, not via
   the shared global. Natural candidate: a new `<window>` attribute
   (e.g. `managed="true"`, parsed the same way `class=`/`vars=` already
   are) that pchq-board.xhtpm (and any other future persistent,
   continuously-interactive window) can set for itself, defaulting to
   override_redirect for every window that doesn't set it - preserves
   the current behavior for 100% of existing windows/popups/dropdowns
   by construction, zero blast radius on anything that doesn't ask for
   it.
2. **Every place that currently reads the single global
   `g_override_redirect`** (window creation's `swa.override_redirect`,
   `render_managed_wm_hints()`'s `managed` argument, the WM hints/
   `_NET_WM_STATE` block near the top of the file) would need to read
   a PER-PROCESS resolved value instead - straightforward in principle
   (this binary is one process per window already, so "per-window" is
   really just "per-process," no new IPC needed) - but audit every
   site, not just the obvious ones, the same "read every g_is_*
   site" discipline this session already had to relearn for the dead-
   flag cleanup earlier.
3. **The taskbar/dock itself must NEVER opt into managed mode** - it's
   exactly the kind of "short-lived popup/dropdown" this file's own
   pitfalls entry says must stay `override_redirect`. Whatever
   mechanism gets built, write a test that explicitly launches the
   dock and confirms its dropdowns still work BEFORE testing the one
   window that actually needs managed mode - inverted from what this
   session did (tested the target window first via `xdotool`, which
   gave false confidence, then broke the dock without a dedicated
   check for it until the user reported it live).
4. **`ktb_toggle_zorder_apply()`/`ktb_toggle_zorder_respawn()`** (the
   existing "always on top" toggle, currently coupled to this same
   global via `g_zorder_above`/`g_override_redirect`) would need
   re-examining once per-window becomes real - right now flipping
   "always on top" and flipping "managed vs override_redirect" are the
   same lever; they may need to become independent once a specific
   window can be managed while the rest of the house stays override_
   redirect + always-above.
5. Given `render_managed_wm_hints()` already exists and already does
   the right thing for the managed case (confirmed live this session -
   pchq-board rendered correctly, undecorated, when tested under
   `override_redirect=false`) - the RENDERING side of this is
   basically done. The real remaining work is entirely in (a) how the
   per-window opt-in gets threaded through to every `g_override_
   redirect` read site, and (b) proving the dock is unaffected before
   calling it done.
