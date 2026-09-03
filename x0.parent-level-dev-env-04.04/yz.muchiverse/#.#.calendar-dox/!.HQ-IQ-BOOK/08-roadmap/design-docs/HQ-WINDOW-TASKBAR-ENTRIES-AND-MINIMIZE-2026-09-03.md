# HQ window taskbar entries, real minimize (`_`), and bottom-toolbar overflow paging

**Status: DESIGN, not yet built.** Direct owner request, written before
any code, following today's real focus/chrome work in this same
session (co-lab-hai/chat-hai/open-hai's real `^`/`.` focus indicator,
`XSetInputFocus`-on-click, live `FocusIn`/`FocusOut` handling - all in
`khtpm_core_render.c`). This doc's own real minimize/restore flow reuses
that work directly rather than inventing a second focus mechanism.

## 0. The three real asks, verbatim

1. "we wanted to add x11-hq style windows to chrome when they open" -
   every real default-mode window (co-lab-hai/chat-hai/open-hai/db-hq/
   events-hq alike) should get a real entry in the taskbar's own bottom
   strip the moment it opens, not just exist as an untracked X11 window.
2. "add '_' minimize chrome" (corrected from an earlier '-' suggestion -
   direct instruction: "lets not use '-' (thats for zoom in or out in
   size later), lets use '_' underscore 4 minimize 2 toolbar") - a real
   third chrome button alongside today's real `!`/`X` pair.
3. "when clicked from tb they get focus or open from minimize and get
   focus" - clicking a taskbar entry for an already-open, non-minimized
   window gives it real focus; clicking one for a minimized window
   restores AND focuses it. Same real click-to-focus mechanism as
   today's `XSetInputFocus` fix, not a new one.

Plus, a related but genuinely separate concern raised in the same
message: "we will make a solution if more items enter bottom toolbar
than there is space, with +- nav elements on top right of bar to show
more rows... by adding another layer" - bottom-strip overflow paging,
scoped as its own section (§4) since it's a real, independent capacity
problem that exists even without minimize/restore ever being used (a
house with enough tile entities and a full AI-menu already approaches
this today).

## 1. Real, live-confirmed building blocks this design reuses (not invents)

- **Window creation already runs through one shared choke point**:
  `XCreateWindow()` in `khtpm_core_render.c`'s own `main()` (the same
  real spot this session's off-screen-clamp fix landed). Any new
  "register myself with the taskbar" step belongs right after this,
  once, at real launch - not scattered across every app's own
  `button.sh`.
- **Real click-to-focus already exists**: `hq_dispatch_xevent()`'s
  `ButtonPress` handler now calls `XSetInputFocus(dpy, win,
  RevertToParent, CurrentTime)` unconditionally (today's own real fix,
  direct report: "its way to hard to get window focus"). A taskbar-
  entry click needs the exact same call, made by the **taskbar
  process** against the **target window's own real X id** - `XSetInput
  Focus` takes any window id, not just one you created yourself, so
  this works cross-process with zero new IPC for the focus call itself.
- **Real live FocusIn/FocusOut already exists**: default-mode windows
  now redraw immediately on real focus change (today's own fix, direct
  report: "is there a way to tell if another window gets focus and
  remove focus from both?"). The taskbar's own entry highlighting
  (§2.3) can piggyback on the SAME real signal instead of polling.
- **Real chrome button convention already exists**: `#chrome-close`/
  `#chrome-fullscreen` (`entity_menu_default.css`, id-selector CSS
  rules - NOT struct assignment, see `HOUSE_CODE_PITFALLS.md` #12) at
  `btn_w=56` (room for the real `[ ]NN.` nav badge), positioned via
  `g_win_w`-relative math in `layout_sidebar_panel()`. A real `_`
  button is a third instance of this exact same pattern, not a new one.
- **Real per-process file-registry precedent already exists**:
  `#.desktop/livedesk_launched_pids.txt` (append-only PID list, used
  today only for shutdown cleanup, `khtpm_taskbar_manager.c` line ~89)
  and the PID-scoped frame-file fix from today
  (`entity_menu_frame_<pid>.txt`) both establish the real shape: one
  small, atomically-written, per-window record, keyed by a value only
  that window's own process can produce (its real X window id and PID).
  Confirmed NOT already tracked: no existing file records a *live*,
  queryable "which HQ windows are open right now, with real titles and
  minimized state" - `livedesk_open.txt` tracks desktop TILE entities
  (asa/ava/self/etc.), a different, older, unrelated concept.
- **Real generic nav-numbered control precedent already exists**:
  today's own generic scrollbar up/down arrows (`generic_sbar_register()`
  in `khtpm_core_render.c`) - a real, static-storage `Elem` pair, class-
  based CSS, real `nav_index`, real onclick verb intercepted in
  `activate_focused()` before the generic `dispatch()` fallback. §4's
  own `+`/`-` paging control is the same real shape, ported to the
  taskbar's own separate renderer.

## 2. Design: real per-window taskbar registration

### 2.1 The registry file

New, real, shared file: `#.desktop/livedesk_hq_windows.txt`. One line
per currently-open HQ window, written by that window's OWN renderer
process (not the taskbar), using the same real tmp+rename atomic-write
convention already used everywhere in this file family:

```
win=0x1000001|pid=2619617|title=chat-hai|x=140|y=80|w=700|h=520|minimized=0
```

Each renderer process:
- Writes its own real line to its own real per-PID temp file
  (`livedesk_hq_windows_<pid>.txt.tmp`), same PID-scoping lesson as
  today's frame-file fix - never a shared tmp path two processes could
  race on.
- The TASKBAR manager (not each app) merges these into the one real
  `livedesk_hq_windows.txt` the taskbar strip actually reads, polling
  `#.desktop/livedesk_hq_windows_*.txt` (glob) the same real way
  `dbhq_load_actors()`-style pollers already scan a directory for
  per-item files elsewhere in this house - avoids every renderer
  needing to coordinate a single shared write target at all.
- On real, clean process exit (`atexit`, matching `dbhq_cleanup_module()`'s
  own existing convention) the renderer deletes its own per-PID file -
  the taskbar's own merge step drops any PID that's no longer running
  (`kill(pid, 0)` check, same real liveness check `ktb_pid_alive()`
  already uses per `HOUSE_CODE_PITFALLS.md`'s own fixed false-positive
  entry) as a real second real safety net against a crashed process
  leaving a stale entry forever.

### 2.2 What the taskbar renders

One real cell per live registry entry, in the bottom strip (or its own
dedicated "Windows" section of the existing AI-menu structure -
open question, see §5). Label: the real `title` field. Click: taskbar
manager looks up that entry's real `win` id and:
- Not minimized: `XSetInputFocus(dpy, (Window)win, RevertToParent,
  CurrentTime)` + `XRaiseWindow` (bring it visually to top - real focus
  alone doesn't restack).
- Minimized (`minimized=1`): send it a real restore request first (see
  §3.2), then the same focus+raise.

### 2.3 Real-time highlight of which entry is "active"

The taskbar's own cell for the currently-focused window should look
different (matching how a real OS taskbar highlights the active app).
Real, generic way to know this without new IPC: each renderer's own
registry line ALSO carries a real `focused=0/1` field, flipped by the
SAME `FocusIn`/`FocusOut` handler this session just added (one extra
field write, no new mechanism) - the taskbar's own poll picks it up
next tick, same real "state file, not stateful protocol" shape as
every other cross-process signal in this house.

## 3. Design: real minimize (`_`)

### 3.1 The chrome button

Third real chrome element, `#chrome-minimize`, id-selector CSS (same
real border/background rule as `#chrome-close`/`#chrome-fullscreen`),
positioned to the LEFT of `!` (so left-to-right reads `_  !  X`, the
same real ordering convention every desktop OS uses - minimize,
maximize/fullscreen, close). Same `btn_w=56` sizing lesson from today
(room for the real nav badge, not just the bare glyph).

### 3.2 What minimizing actually does

Real X mechanism: `XUnmapWindow(dpy, win)` (this window is
override-redirect, so there's no WM to hand this off to - the app must
do it itself, same real reasoning as this session's own click-to-focus
fix). Sets `minimized=1` in this window's own registry line. The
renderer's own event loop keeps running (real state, real background
message ticks for chat-hai etc. continue) - only the real X mapping
changes, matching what a human expects from "minimize," not "pause."

Restore (triggered by a taskbar-entry click per §2.2, or a future
"un-minimize" affordance): the TASKBAR process cannot itself call
`XMapWindow` on another process's window usefully for input purposes
beyond mapping - real fix is a small, real request file per window
(`#.desktop/livedesk_hq_restore_<pid>.txt`, one line, the taskbar
writes it, the target renderer polls for it the same real way
`poll_agent_history()` already polls its own relay file every tick) -
the renderer itself calls `XMapWindow` + `XSetInputFocus` on ITS OWN
window once it sees the request, consistent with the real house rule
"a process only ever touches its own X resources" (never seen violated
elsewhere in this codebase, worth keeping true here too).

### 3.3 Open question, not yet decided

Does a minimized window's taskbar cell look different (dimmed, an
icon) from a normal one? Real, cheap answer once `minimized` is already
a real registry field: yes, trivially - a CSS class difference, same
real shape as `.msg-*` per-persona coloring already does. Not scoped
further here; a one-line addition once §2 is real.

## 4. Design: bottom-toolbar overflow paging (separate capacity problem)

### 4.1 The real problem

The bottom strip has a fixed real pixel width (`KTB_BAR_H`-tall,
screen-width-wide, per `khtpm_taskbar_manager_main.c`). Desktop tile
count, the AI-menu's own dynamic rows, AND (once §2 ships) one cell per
open HQ window all compete for that same real horizontal space. This
is real and already close today (`KTB_LIVEDESK_DYN_MAX = 24` exists as
a header-menu ROW cap, not a horizontal-cell cap, but the same class of
"real content can exceed real fixed chrome space" problem this
session's own generic-scrollbar work (§1) already solved once, for
scrolllists specifically).

### 4.2 Real design, reusing today's own generic-scrollbar shape

A real `+`/`-` pair (NOT `_`, which is now reserved for window-minimize
per the owner's own direct correction this session - a different,
non-ambiguous glyph pair) at the top-right of the bottom strip itself.
Real behavior: the strip's own cells are laid out in ROWS (not one
endless horizontal scroll - direct instruction: "show more rows...by
adding another layer"), one row visible at a time by default; `+`
reveals/pages to the next row (a real second, stacked strip layer
appearing above or below the primary one), `-` returns to the previous
row. Same real nav-numbered, CSS-styled, `activate_focused()`-
intercepted-onclick-verb shape as today's `SCROLLUP:<i>`/
`SCROLLDOWN:<i>` (`PAGEROW:+1`/`PAGEROW:-1` or similar), so this is
keyboard/AI-reachable from day one, not mouse-only - direct lesson from
today's own "chat-hai doesn't have the thumb navs" gap, applied here
before it's ever missing rather than after.

### 4.3 Real scope boundary

This lives in `khtpm_taskbar_manager.c`/`khtpm_taskbar_manager_main.c`
(the strip's own separate real render code, hand-rolled X11 - see this
session's own earlier correction: the strip is NOT a `.chtpm`/CSS-
driven window like the apps in §2-3, it draws itself directly, already
confirmed live via `WM_CLASS "MuchiverseLivedesk"` appearing inside
`khtpm_core_render.c` itself for entity tiles, but the STRIP's own bar
content uses its own bespoke draw calls, not `draw_elem()`/CSS). §2's
new window-list cells and §4's own paging both render THERE, not in the
shared default-mode CSS/Elem path §1/§3 use - two real, different
codebases for one visual bar, unchanged by this doc.

## 5. Decisions (confirmed with the owner, 2026-09-03)

1. **§2's window-entries get their own separate bottom-strip section**,
   not folded into the existing AI-menu's `KTB_LIVEDESK_DYN_MAX`
   machinery. Reasoning: window entries are a genuinely different real
   concept (live windows with focus/minimize state, restack on click)
   from a dropdown menu's own static items - a dedicated section keeps
   that state isolated and makes §4's overflow paging simpler to scope
   to just this section later, rather than the whole strip at once.

2. **Real scope correction, not a technical answer to the question as
   asked** - direct instruction: "we should be able to minimize
   entities also (and this would close their submenus; but could open
   them when unminimized)... we can put minimize option in their
   context menus." This is a real, larger scope than §3 as originally
   drafted (which only covered the `.chtpm`/CSS default-mode app
   windows - co-lab-hai/chat-hai/open-hai/db-hq/events-hq). Desktop
   TILE entities (asa/ava/self/m1_ninjadragon/etc.) need real minimize
   too, via their own existing real right-click context menu
   (`open_context_menu()`, data-driven per-entity via `meta.pdl` - a
   real, already-proven, already-data-driven row mechanism, confirmed
   live this session), NOT the `_` chrome button (tile entities have no
   window chrome bar at all - they're small icons, not `.chtpm`
   windows). A new real `meta.pdl` row (e.g. `Minimize`) dispatches the
   same real minimize verb §3 defines, scoped to that entity's own real
   window instead of an HQ app's. Minimizing an entity must also close
   any of ITS OWN currently-open submenus/popups (real, entity-specific
   state - which popup(s) trace back to which entity isn't tracked
   anywhere yet and needs its own real accounting, not assumed to
   already exist), and un-minimizing should reopen them - real,
   separate follow-up scoping needed once §3's own core minimize/
   restore mechanism (registry field + restore-request file) is proven
   for the simpler HQ-app-window case first. The still-open technical
   sub-question (whether `XUnmapWindow` behaves identically for a WM-
   managed vs. override-redirect window) remains real and unanswered -
   build and verify on override-redirect windows first (every case
   confirmed live so far this session), extend to any WM-managed case
   only once that's proven, per the original recommendation.

3. **`_`'s onclick falls through to `dispatch()`, matching `X`/`!`'s own
   existing pattern today** (`CLOSE`/`TOGGLE_FULLSCREEN`, both already
   special-cased inside `dispatch()` itself) - NOT intercepted early in
   `activate_focused()` like `ACTIVATE`/`SCROLLUP:<i>`. All three real
   chrome buttons stay handled identically; add a `MINIMIZE` (or
   `TOGGLE_MINIMIZE`) verb to `dispatch()`'s own existing special-case
   list alongside `CLOSE`/`TOGGLE_FULLSCREEN`, not a new interception
   point.

Nothing in §1-4 is built yet. Build order stays as the owner stated:
window registration + real click-to-focus first (§2), minimize second
(§3, HQ-app-windows before entities per the scope note above), overflow
paging last (§4) - each one independently testable via this house's own
real live-verification convention (dump_frame_png_op's geometry
receipts, the "^"/"." focus indicator, real xdotool clicks - not the
synthetic relay alone, per today's own hard-won lesson that the relay
can validate internal logic while missing real X-level regressions
entirely).
