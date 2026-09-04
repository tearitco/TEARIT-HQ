# Forensic report — db-hq-pal "redraws with no change" flicker

**Date:** 2026-09-03
**Window:** `db-hq-pal` (`class="db-hq-pal database-window"`, generic
`<tabbar>` + sidebar/panel, live `<module>` prisc+x projector).
**Symptom (user):** while the window is active it does *complete
redraws for no reason* — repaints "#1 Harold" / the sidebar / both,
with no state change. "None of the other x11-hq windows do this."
**Renderer:** `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
(the shared generic renderer; `g_is_db_hq` is **false** for this
window — it rides the generic default/sidebar+panel path).
**Status:** FIXED. Commits on branch `chtpm-var-substitution`
(`bc18c5ad`, `2865e2cd`, `75d6833c`, + this report's commit).

---

## 1. TL;DR — root cause

The generic (non-`g_is_db_hq`) window had **no repaint-trigger
discipline**. The real `g_is_db_hq` window only blits when its
append-only `*_frame_changed.txt` marker grows
(`dbhq_marker_pilot()` / `mark_frame_changed()` /
`consume_frame_changed()` — the tpmos model). The generic path just
called `redraw()` immediately from every caller, and several of those
callers fire on events that are **not visible-state changes**:

| # | trigger | why it fired with no change |
|---|---|---|
| A | `FocusIn` / `FocusOut` → unconditional `redraw()` (the 2026-09-03 `^`/`.` title indicator) | A focused X11 window gets a `FocusOut(NotifyGrab)`+`FocusIn(NotifyUngrab)` pair **every time any process takes a pointer/keyboard grab anywhere on the desktop** — passive button grabs, the taskbar's rmmv root grab, menus, drags, alt-tab. Two full `XGetImage`+`XPutImage` blits per grab. Worst on db-hq-pal because it's the window that holds real focus while the user mouses around the rest of the desktop. |
| B | `Expose` → `redraw()` per event | X delivers **one `Expose` per rectangle** of a damaged region, and a fresh burst every time another window restacks above this override-redirect window. N blits per logical repaint. |
| C | `poll_agent_history()` counted **every** relayed `MOUSE_EVENT:` line — including bare pointer-moves — as consumed input (`if (nf>=3) n++`), so `hq_request_redraw()` fired on every mouse twitch over the window. This is tpmos **PITFALL #52 (MOUSE MOVE REDRAW SPAM)** verbatim. |
| D | no coalescing: `hq_request_redraw()` → `redraw()` synchronously, so multiple triggers in one event-loop iteration = multiple full blits. |
| E (2nd order) | `redraw()` corrective `XMoveWindow`+`XSync`+`XSetInputFocus` (the 2026-09-03 off-screen-chrome fix) compared raw `XGetWindowAttributes` `wa.x/wa.y` against `g_win_x/g_win_y`. Under a **reparenting WM** `wa.x/wa.y` are frame-relative, so the "mismatch" was ~always spuriously true → an `XMoveWindow` storm on every redraw, which itself perturbs focus/stacking → feeds A and B. |

None of these is one bug — they compound. The window that (a) holds
focus, (b) sits under the pointer, and (c) has a live 0.4 s projector
behind it is the one where all of them light up at once. That is
db-hq-pal.

---

## 2. Method

1. `grep` every `redraw()` / `hq_request_redraw()` / `mark_frame_changed()`
   call site and every `XSync`/`XMoveWindow`/`XGetInputFocus` in the
   renderer.
2. Read the tpmos reference render loop
   `1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/chtpm_parser.c`
   lines 3024-3155 — the marker-driven single-source-of-truth render
   trigger — and `PITFALLS_ACTIVE_2026-03-18.txt` #17 (ONE WRITER
   RULE) and #52 (MOUSE MOVE REDRAW SPAM).
3. Instrumented `redraw()` with an env-gated trace
   (`KH_REDRAW_TRACE=1` → `stderr` line per call: counter, monotonic
   time, `__builtin_return_address(0)`, `g_frame_dirty`,
   `g_default_scope_confine`, `g_default_active_tab_id`). Resolved the
   return addresses with `addr2line` against the runtime load base
   from `/proc/<pid>/maps`.
4. Ran a real db-hq-pal (`khtpm_core_render.+x <house> dashboard.xhtpm`
   — byte-identical to what `&.hq-apps/db-hq-pal/button.sh` execs),
   idle, 6-11 s, counted redraws. Repeated with the taskbar manager
   alive.
5. Watched the renderer + its prisc+x child via `/proc` `ppid` scan
   (NOT `pgrep -f` — the pattern string matches the watching shell
   itself; several earlier "process is respawning every 0.5 s"
   readings were this false match and are retracted).
6. Tight-polled `state/ui.txt` size (1.4 M `stat()`s in 4 s) to test
   the "reader catches a half-written frame" hypothesis.
7. `xmon` (tiny `XSelectInput` on `ExposureMask|StructureNotifyMask|
   VisibilityChangeMask|PropertyChangeMask`, window id read from
   `#.desktop/livedesk_hq_windows_<pid>.txt`) on an idle window.

---

## 3. What was ruled OUT (with evidence)

| hypothesis | verdict | evidence |
|---|---|---|
| Reader catches a **partial / truncated `ui.txt`** from a non-atomic projector write → hash flip-flop → reparse every tick | **DISPROVEN** | 1,442,943 `os.path.getsize()` samples over 4 s: size was **726 every single time**, never 0, never short. The prisc+x `sfopen`/`swrite` sequence is not observably non-atomic at this size. (Kept a defensive 2-consecutive-reads debounce on the vars hash anyway — see §5.) |
| **Dual writer** of the frame round-trip file `#.desktop/entity_menu_frame_<pid>.txt` (tpmos #17) | **DISPROVEN for the current code** | `grep` across `*.monads/` `.c`/`.sh`: the renderer is the only writer, and it is PID-scoped (the 2026-09-03 chat-hai/open-hai race fix). BUT the round-trip itself is an architectural smell — see §6. |
| Renderer **process respawning** (button.sh / manager keep-alive) | **DISPROVEN** | `/proc` `ppid` scan: renderer pid stable for the whole watch, exactly one stable prisc+x child, flat RSS (~11.7 MB). Earlier "new pid every 0.5 s" was `pgrep -f` matching the watcher shell. |
| Projector (`.pal`) **crashing/restarting** | **DISPROVEN** | same watch — child pid never changed. |
| `reparse_chtpm_if_changed()` firing every tick on a churny `vars=` file | **DISPROVEN** | `ui.txt` md5 stable across samples; content-hash gate (`g_vars_hash`) holds; trace showed `dirty=0` and no reparse-path redraws when idle. |
| `${ARG3}` instance-dir extra vars path (`<argv3>/.hq_manager/ui.txt`) written by a manager every tick | **N/A** | `button.sh` launches with exactly 2 args — no argv[3] — so `g_extra_vars_path` is empty for this window. |
| Periodic `XRaiseWindow`/`XConfigureWindow` from the taskbar manager | **UNLIKELY** | manager has exactly one `XRaiseWindow`, in `taskbar_raise_tab()`, event-driven (taskbar tab click), not periodic. |

After the fixes in §4, a directly-launched db-hq-pal logged **2
redraws in 11 s idle** (startup + one coalesced post-map `Expose`),
then silence; `entity_menu_frame_<pid>.txt` mtime frozen. The storm
is gone in isolation. Any residual flicker a user still sees after
this is a **stale pre-rebuild renderer process** for that window —
see §7.

---

## 4. The fix (what landed)

All in `khtpm_core_render.c`, all on the generic path only
(`g_is_db_hq`/`g_is_events_hq`/`chat-hai`/dock/popup untouched).

- **A — focus events.** `FocusIn`/`FocusOut` now ignore
  grab-synthetic (`NotifyGrab`/`NotifyUngrab`/`NotifyWhileGrabbed`)
  and pointer-derived (`NotifyPointer`/`PointerRoot`/`Inferior`)
  notifications — the same filter the dock branch already used — and
  only repaint when the tracked `^`/`.` owned state
  (`g_focus_owned_painted`) actually flips.
- **B — Expose.** Drain the whole `Expose` burst
  (`XCheckTypedWindowEvent(... Expose ...)`) before a single repaint.
- **C — mouse-move spam (PITFALL #52).** `poll_agent_history()` no
  longer counts a bare relayed pointer-move / plain release as
  consumed input; only real clicks and wheel notches (each `n++`'d
  in their own branch) dirty the frame.
- **D — coalescing (tpmos marker/dirty model).** New `g_frame_dirty`
  flag: generic-path repaint requests set it; `hq_run_event_loop()`
  consumes it with **one** `redraw()` at the tick boundary — exactly
  `chtpm_parser.c`'s `dirty` → `compose_frame()` shape, and the same
  throttle `dbhq_marker_pilot()` gives the real db-hq window.
- **E — corrective-move storm.** `redraw()` only issues the
  off-screen-chrome `XMoveWindow` when our *intended* position
  (`g_win_pos_applied_x/y`) actually changed since we last applied
  it, and then compares real **root**-translated coords
  (`XTranslateCoordinates`) — not frame-relative `wa.x/wa.y`.
- Housekeeping: `graphics_exposures=False` on the window GC so the
  `XCopyArea` present fallback stops emitting unhandled
  `GraphicsExpose`/`NoExpose`; env-gated `KH_REDRAW_TRACE` diagnostic
  left in place (zero cost when unset).
- Defensive: `reparse_chtpm_if_changed()` debounces a `vars=` hash
  change — requires the same new hash on two consecutive polls before
  reparsing, so a hypothetical half-written state file can never
  cause a blank↔populated reparse flip.

---

## 5. Comparison — why the other windows never flickered

| window | why it was immune |
|---|---|
| **legacy `chtpm_parser.c`** (tpmos) | Its render loop **never** composes unless `dirty` is set, and `dirty` is set **only** when an append-only marker file *grows* (`st.st_size > last_size`). Input handlers *write the marker*, they do not call the composer. No mtime checks, no hashes, no per-event redraw. "Need a new render trigger? WRITE TO THE MARKER." |
| **real `g_is_db_hq` window** (this same binary) | Already wired to the same model in this file: `dbhq_marker_pilot()` → `mark_frame_changed()` (append `.`) / `consume_frame_changed()` (size-grow check) / `dbhq_loop_paint_if_dirty()`. The generic path had the helpers but never called them. |
| **open-hai / signup-hq / co-lab-hai** | Compiled C projectors that publish their state file with **tmp + rename** (atomic), and — critically — **no live 0.4 s `<module>` projector poking the reparse path**, no `<tabbar>` scope, and they are not usually the focused window while the user works elsewhere. Triggers A/C/D exist in their code too but rarely all fire at once. |
| **entity-menu popups / swatch-picker** | Transient, short-lived, `override_redirect`, and never hold focus while the user grabs elsewhere. |

The generic default-mode path is **shared** — so the fixes above
protect every one of those windows too, not just db-hq-pal.

---

## 6. Known remaining smell (not the flicker, but fix it next)

The generic present path (`redraw()` ~line 9442) **serializes the
laid-out `Elem` tree to `#.desktop/entity_menu_frame_<pid>.txt`, then
immediately re-opens that file and paints the window from the text it
reads back** (`dbhq_serialize_frame_subtree()` → disk → `fgets` →
`dbhq_paint_frame_line()`), every redraw. It is PID-scoped so it is
not *currently* a dual-writer race, but it is a frame round-trip
through the filesystem on the hot path — fragile, slow, and one
`fopen("a")` from any future helper re-introduces tpmos **#17**. The
house standard (`CENTROID_GOLD_STD.md` §3.4) is that a renderer walks
the parsed tree directly (`render_tree()` in `khtpm_draw_core.c`).
Migrating the generic panel draw off the file round-trip and onto
`render_tree()` is tracked as follow-up work.

---

## 7. Verification / how to be sure you are on the fixed binary

```sh
HOUSE=…/44.xyz.01.00
# 1. rebuild
sh "$HOUSE/*.monads/*.livedesk-taskbar/ops/build_core_render.sh"
# 2. kill EVERY renderer (a window opened before the rebuild is still the old process)
for p in $(ls "$HOUSE/#.desktop"/livedesk_hq_windows_*.txt 2>/dev/null); do
  kill "$(sed -n 's/.*pid=\([0-9]*\).*/\1/p' "$p")" 2>/dev/null; done
sh "$HOUSE/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh" new   # whole stack on the new binary
# 3. open db-hq-pal, leave it focused and untouched, then:
KH_REDRAW_TRACE=1 …  # (or just watch) — an idle window must log ZERO redraws after the first ~2
```

Idle redraw count after the first two startup frames must be **0**.
`#.desktop/entity_menu_frame_<pid>.txt` mtime must be frozen while
the window is idle.

---

## 8. Doc updates made so this does not recur

- `02-architecture/CENTROID_GOLD_STD.md` §3 — new **rule 8**: repaint
  discipline (marker/dirty model; one writer; never a full redraw per
  raw X event; the specific anti-patterns A–E above named).
- This report linked from `CENTROID_GOLD_STD.md` §5 cross-references
  and the branch `HANDOFF-chtpm-var-substitution.md`.
