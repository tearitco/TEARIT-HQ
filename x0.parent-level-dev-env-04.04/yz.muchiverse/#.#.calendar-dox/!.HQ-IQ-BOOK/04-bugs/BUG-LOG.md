# Bug log — append dated entries, do not rewrite history

*Convention: add a new dated entry at the bottom for anything newly
found/fixed. If an old entry goes stale, add a dated 🔄 CORRECTION
note under it — don't silently edit it away.*

## Open

- **network-browser address bar: keeps losing keyboard focus/backspace
  while typing - RECURRING, fixed 3+ times, still reported broken on
  real hardware** (found/re-found repeatedly 2026-09-10/11). User
  report (verbatim, most recent): "its the exact sae bg as we just
  fixed the last 3 times. no difference."

  **Real, confirmed root causes fixed so far** (all landed, all
  verified via the relay-driven testing method - see below for why
  that verification is now suspect):
  1. `network_browser_manager.c`'s `write_ui_projection()` used to
     derive the address bar's `addr_label` from `g_current_url` (the
     LOADED page), not the live-typed value - any unrelated reparse
     (status/tabs/history changing, ~every 300ms) re-seeded the field
     from the stale full URL, fighting every edit. Fixed to echo
     `cli_io_state.txt`'s own `address=` while non-empty.
  2. `khtpm_core_render.c`'s `reparse_chtpm_if_changed()` unconditionally
     disarmed + released the real `XGrabKeyboard` on EVERY reparse for
     `<cli_io>` (text_area got an equivalent fix 2026-09-08, cli_io
     never did until now). network-browser's manager reparses far more
     often than most windows (the ~300ms tick above), so it hit this
     constantly. Fixed: the grab is a Window-level resource, not tied
     to the Elem* - if the same field (by saved key) still exists after
     reparse, the grab already held is still valid; only release it if
     the key is genuinely gone. This fix is now shared/generic (helps
     open-hai/chat-hai too, not network-browser-specific) and the user
     separately confirmed it fixed open-hai's real, repeated click-arm/
     focus-loss failures on real hardware.

  **Both (1) and (2) are live, committed, and pass every scripted
  relay test this session ran** (click-arm via `MOUSE_EVENT`, type via
  `KEY_PRESSED`, wait 3+ seconds through several real manager reparse
  ticks, backspace - content and arm state both survive correctly,
  repeatable). **The user reports the real, physical-hardware behavior
  is unchanged - still broken, still losing focus/backspace.**

  **Open question, not yet resolved**: this exact split (synthetic/
  relay-driven testing shows success, real hardware still fails) is
  the SAME shape as the already-documented, already-solved-once
  `override_redirect` pc-hq focus bug (`09-appendix/pc-hq-bugs.md`
  Bug 2, "synthetic XTest input worked, masking the bug"). The relay
  mechanism this house's own testing hierarchy ranks ABOVE xdotool
  (`#.desktop/entity_menu_history/<pid>.txt`, real KeyPress/ButtonPress
  dispatch through the SAME `handle_key()`/`hq_dispatch_xevent()` code
  path a real X event takes) has not, until now, been suspected of
  having its own version of this masking effect - but the gap between
  "relay says fixed" and "real user says broken, unchanged, 3 fixes in
  a row" is now wide enough that it must be considered. Real
  differences between relay-driven and real-hardware input not yet
  ruled out: real `MotionNotify` events (the desktop is focus-follows-
  mouse; a human naturally moves the mouse while reading/scrolling a
  loaded page, the relay never does), real inter-keystroke timing
  (much slower/irregular than a scripted burst, giving more real
  manager reparse ticks a chance to land mid-edit), and real
  X-server-level grab contention from OTHER concurrently-open
  windows/the taskbar (the relay's `KEY_PRESSED`/`MOUSE_EVENT` lines
  are dispatched by this window's OWN process reading its OWN history
  file - it is not proven this reaches `handle_key()` via the exact
  same call path a genuine X `KeyPress`/`ButtonPress` event takes,
  e.g. it may bypass whatever real X grab/focus state a live human
  interaction depends on). **Needs live re-test with a human clicking/
  typing while an agent watches `cli_io_state.txt` and the real
  `XGetInputFocus` state in parallel, not another round of relay-only
  verification before calling it fixed.**

- **pc-hq board: real keyboard focus vs the taskbar** (found
  2026-09-04, see `09-appendix/pc-hq-bugs.md` for the full
  investigation): root cause found and proven once already, by a
  PRIOR session, in code deleted this session (`run_pchq_board_mode()`,
  recoverable via `git show 35c1b0b1~1`) - `override_redirect`
  windows never get real keyboard/mouse focus routed by Mutter
  ("synthetic XTest input worked, masking the bug" - exact quote from
  that prior fix). The house-wide `#.desktop/livedesk_override_
  redirect.pdl` currently reads `override_redirect=false` (flipped
  2026-09-04 to test this, from its previous `true` default) - the
  already-existing `render_managed_wm_hints()` managed-window path
  activates house-wide as a result. Taskbar + a fresh pc-hq window
  were both relaunched with the new setting; **not yet confirmed
  fixed by the user with real hardware** as of this entry - the
  session moved to investigating a second, apparently unrelated issue
  (toys-menu launch, see below) before that confirmation happened.

  🔄 CORRECTION (2026-09-04, same day): the `override_redirect=false`
  house-wide flip described above was tested and made things WORSE,
  not better - see the very next bug entry below (toys-menu) for what
  it actually broke, and **REVERTED** back to `override_redirect=true`
  (baseline, confirmed working again by the user). The root-cause
  diagnosis above (override_redirect breaks real keyboard focus for a
  persistent window) is very likely still correct - it's independently
  documented and already proven once by a prior session - but a
  BLANKET house-wide flip is the wrong fix. See `03-pitfalls/X11-AND-
  SESSION-PITFALLS.md`'s 2026-09-04 entry for the full incident
  writeup and the standing rule going forward (scoped per-window flag
  only, never the shared global again). This bug is still genuinely
  open - just not fixed the way this entry originally described.

- **"toys" menu launches nothing visible for piececraft-hq** (found
  2026-09-04) - **RESOLVED, was a self-inflicted regression, not a
  pre-existing bug.** Original live-confirmed report: "nothing visible
  at all." This session initially traced the real dispatch chain
  (taskbar's toys dropdown, built by `khtpm_taskbar_manager.c`'s
  `toys_scan_add()`/`toys_scan_one_root()` - NOT `pc_menu_input.c`,
  which only handles in-game menu selections like "View Board" AFTER
  the game is already running, a wrong assumption chased first) and
  theorized the headless ASCII backend + gated GL-mirror activation
  might be the intended (if confusing) UX. **That theory was wrong.**
  The user then confirmed directly: "i was able to open the pc-hq from
  menu before. but it stopped working after we investigated kbd focus
  issues" - i.e. a real, working feature broken by this SAME session's
  own `override_redirect=false` house-wide flip (see the pc-hq focus
  entry above). Confirmed by reverting that flip: toys-menu launches
  work again. No code fix needed here - the fix was the revert above.
  Root mechanism still not fully diagnosed (most likely: Mutter takes
  over click/focus/positioning for WM-managed popups in ways that
  break the "click a dropdown row" interaction toys-menu items need),
  but moot unless the override_redirect question is revisited with a
  properly scoped (per-window, not global) approach - see the pitfalls
  entry.
- **Toys-launch teardown gap** (found ~2026-08-28, still open): the
  taskbar's "toys" menu launches real apps (mutaclysm, my-chara,
  my-lawyer, piececraft) but never records the launched PID anywhere —
  "kill all" doesn't reach a toys-launched app.
- **`livedesk_taskbar.pdl` cell-14 menu rows are dead** (found
  2026-08-15): `strip_btn_14_menu_0/1_label`/`_cmd` rows exist but
  `livedesk_build_ai_menu()` doesn't read them; the menu is hardcoded
  in C instead. Fix: make cell 14 read the PDL like `ktb_hq_open()`'s
  HQ branch already does.
- **`ktb_pid_alive()` treats a zombie PID as alive** (found
  2026-09-01): `kill(pid,0)==0` succeeds for zombies too, so the
  bottom bar can show an entity as "open" when it's a zombie with no
  real window. Structural fix not yet done: also check
  `/proc/<pid>/stat`'s state field and treat `Z` as not-alive.
- ~~**`khtpm_core_render.c`'s `dbhq_load_actors()` loads real PDL data
  inline...**~~ 🔄 CORRECTION (2026-09-04): `dbhq_load_actors()` no
  longer exists anywhere in the code - confirmed by direct grep during
  `02-architecture/RENDERER-MODULARITY-AND-PERF-AUDIT.md`'s own pass.
  Already removed/migrated at some point after this entry was written;
  the entry itself was never updated. See that audit doc for the
  current, fuller modularity/reuse-compliance picture (its own real
  finding: `g_is_cursword`, nested inside `tp_main()`, is the live
  analog of what this entry was worried about).

- **2026-09-10 — three bottom-taskbar / cross-window nav bugs (user-
  reported, investigated, NOT yet fixed).** All live-confirmed on the
  running desktop; captured here so the next pass doesn't re-derive.

  1. **A new window's nav-jump number lands *before* older entities'
     numbers.** Opening the pc-hq board gave it strip nav `16` while
     pre-existing desk entities kept `17`–`23`, so the newer window
     jumps *ahead* of them in Tab/digit order. Root cause: strip nav
     is **positional, re-derived every frame** by layout x-order, not
     stable per-entity. `g_dock_header_nav_hi` (khtpm_core_render.c
     ~5065) is set to `g_n_nav` after the top strip row, then the
     bottom-bar cells "continue above that" (~6853/6868) in
     `khtpm_taskbar_manager`'s `s->tabs[]` order. A newly-registered
     HQ window is inserted into `s->tabs[]` by that manager (by kind /
     pid, not appended last), so every bottom cell after it shifts and
     the window sits at whatever slot the insert chose. Fix direction:
     append new HQ-window cells at the END of the bottom-bar tab list
     (after all entity cells), or give each cell a stable id-keyed nav
     that survives a relayout instead of a positional one.

  2. **Bottom-strip arrow/Tab nav caps ~2 short of the last cell**
     ("wont go past 22 - jumps back to 22 at 23, wont reach 24").
     Live frame dump at the time: `g_n_nav = 24`, `dock-page-plus` =
     nav 24. So navs 1–23 exist but focus won't cross ~22. Suspects,
     in order: (a) the `g_dock_header_nav_hi` / `g_dock_peer_win`
     split - `kh_nav_step`'s dock-confine loop
     (`while (!kh_elem_arrow_stop(...))`) plus the focus-follows-window
     handoff at khtpm_core_render.c ~7782/7797
     (`want = (g_focus_nav > g_dock_header_nav_hi) ? peer : win`) can
     strand focus at the top-strip/bottom-bar boundary; (b) a parked
     (`nav_index = 0`) or `class="no-nav"` cell near the end that the
     step skips into a wrap. Needs a live `g_focus_nav` trace while
     arrowing 20→24 to pin which.

     ✅ FIXED 2026-09-10 — neither suspect. Root cause was in the
     **taskbar manager**, not the renderer: the strip's "tab half" is
     entity cells (`s->n_tabs`) FOLLOWED BY hq-window cells
     (`s->n_hq_wins`), both positionally navigable, but every focus
     clamp/wrap only counted `n_tabs`. So with 7 entities + 1 hq-window
     cell, `tab_focus_idx` maxed at 6 and the last cell (the
     `🪟 piececraft-hq board` window) was unreachable — exactly "won't
     go past 22 / 2nd-to-last". Fixed by using `n_tabs + n_hq_wins`
     as the tab-half size in: `khtpm_taskbar_manager_main.c`
     `KSC_SET_FOCUS_BASE` handler; `khtpm_taskbar_manager.c` post-reload
     clamp (~726), `ktb_focus_delta()`, `ktb_nav_focus_delta()` (the
     real arrow handler — `total = KTB_STRIP_N_CELLS + n_tabs +
     n_hq_wins`); and `ktb_activate_tab()` gained an hq-window branch
     that just records the focus slot (the renderer's own `FOCUSWIN:`
     onclick does the activation — the manager is X-free). Verified
     live via the `strip_history.txt` relay: FOCUS_RIGHT now lands on
     `strip_focus_cell=-1 / tab_focus_idx=7` (the last cell) and wraps
     cleanly to `strip_focus_cell=0` on the next step.

  3. **Strip `+`/`-` pager sits off the visible right edge and isn't
     aligned with the cells.** `dock_place_pager()` (khtpm_core_render.c
     ~4306, changed 2026-09-10 to a horizontal `- +` pair) anchors the
     `+` at `win_w - 8 - aw`. On this box the bottom strip `win_w` is
     the Xwayland-reported width (wider than the visible monitor - the
     same HiDPI/virtual-desktop gap the fullscreen fix works around),
     so `x ≈ 2066` lands past the visible edge; the old lone `+` at
     `win_w - DOCK_PAGER_W + 8` (`x ≈ 2024`) was 42px further in and
     stayed visible. Also `DOCK_PAGER_W` (80px) still reserved on the
     right while the pair needs ~48, so there's dead space and the
     pair floats at the extreme right instead of "justified left"
     right after the last cell (user's preference). Fix direction:
     anchor the pair at `win_w - DOCK_PAGER_W + 8` (old safe zone) or,
     better, place it immediately after the last laid-out cell
     (`col_x`) so it left-flows with them; and cap the strip's usable
     right edge the way the `<footer>` grip fix already caps pc-hq's.
     Related standing issue: `dock-page-plus` alone is a no-op
     (`PAGEROW:+1` needs `g_dock_visible_rows < g_dock_packed_rows`) -
     it should only render when there's a row to page to.

## Recently fixed (kept short — see 03-pitfalls for the general lesson each one produced)

- **2026-09-04 — pc-hq "^" active-scope badge never showed.** Every
  generic-mode window draws through a serialize-Elem-to-text-then-
  reparse round trip, not `render_tree()` directly (confirmed zero
  real call sites for it anywhere in the house); `e->relay` was never
  one of the serialized fields, so the temp `Elem` `kh_paint_frame_
  line()` draws from always had `relay[0]=='\0'` regardless of the
  live tree's real value - the badge check itself was correct, it
  just never got fed real data. Fixed by adding `relay` as a 9th
  pipe-escaped field to the serializer/parser pair (same convention
  `target_id`/`input_buffer` already used). Confirmed fixed live by
  the user.
- **2026-09-04 — pc-hq canvas render blanking every ~10 seconds.**
  `kh_draw_canvas()` filled the canvas dark on EVERY call before
  attempting to decode that tick's frame, so a single receipt-read
  landing mid-write by the separate producer process (no atomicity
  guarantee on that side) blanked the display for one visible tick,
  however often the collision happened. Fixed: only fill-and-clear
  when there's no previously-decoded frame cached yet; a bad tick now
  just leaves the last good frame on screen instead of blanking.
- **2026-09-01 — stray zombie taskbar processes.** An old, retired
  `khtpm_strip_parser.+x` kept running in the background after the
  2026-09-01 consolidation into `khtpm_core_render.c`, forking and
  never reaping entity windows (18 zombie children found). Quick fix:
  find and `kill -TERM` any surviving old-binary-name process; the
  post-swap kill sweep can't see it because it only matches the
  current binary name.
- **2026-08-28 — open-hai + gemma3 "not responding."** Not a
  model/server issue. Every send path in `khtpm_open_hai_manager.c`
  silently dropped a new message if a previous request was still
  pending, with zero feedback. Fixed: dropped sends post a real
  `[dropped: ...]` message; switching models auto-cancels the stale
  pending request (`SIGTERM` + reap).
- **2026-08-19 — taskbar cell activation broken after frame
  unification.** A submenu's arrow-key focus snapped back to row 1 on
  reload because its `cells.pdl` dirty-signal shared a file
  (`strip_frame_changed.txt`) with the unrelated manager-state dirty
  signal — a fresh parse on that signal wiped the submenu's own state.
  Fix: gave `cells.pdl`'s signal its own dedicated file
  (`strip_cells_changed.txt`), split cleanly from
  `frame_changed_dirty()`'s own signal.
- **2026-08-16 — chat-hai text truncation (forward + trailing cutoff).**
  Root cause: `khtpm_css_parser.c` didn't support CSS descendant
  combinators, and the truncating rule targeted the wrong selector
  (`.messages-feed .data-item` instead of `.content .data-item`).
  Fixed: descendant-combinator support added, selector corrected, font
  dropped 15px→12px to match.
- **2026-08-17 (approx) — db-hq not opening from the taskbar; `[x]`
  close in chat-hai closed ALL desktop entities.** Root-caused and
  fixed in the same window-chrome refactor documented in
  `03-pitfalls/X11-AND-SESSION-PITFALLS.md` (persistent top-level
  windows needing real WM management, not `override_redirect`).
