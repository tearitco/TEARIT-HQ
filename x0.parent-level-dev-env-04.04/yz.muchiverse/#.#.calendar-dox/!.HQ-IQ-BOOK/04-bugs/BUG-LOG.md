# Bug log — append dated entries, do not rewrite history

*Convention: add a new dated entry at the bottom for anything newly
found/fixed. If an old entry goes stale, add a dated 🔄 CORRECTION
note under it — don't silently edit it away.*

## Open

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
- **"toys" menu launches nothing visible for piececraft-hq** (found
  2026-09-04, live-confirmed by direct user report: "nothing visible
  at all"). Traced the real dispatch chain: taskbar's toys dropdown is
  built by `khtpm_taskbar_manager.c`'s `toys_scan_add()`/`toys_scan_
  one_root()` (NOT anything in `pc_menu_input.c`, which only handles
  in-game menu selections like "View Board" AFTER the game is already
  running - a wrong assumption this session initially chased), reading
  each toy's own `toy.pdl` for a `launch=` command (defaults to
  `button.sh`), dispatched as `livedesk:open-toy:<path>` ->
  `setsid nohup sh -c 'sh "<button.sh>" run' >/dev/null 2>&1 &`
  (khtpm_taskbar_manager.c ~line 4042). Reproduced the exact same
  detached invocation directly: the game's own ASCII-UI backend
  (`system/orchestrator`) DOES start (writes real frame output showing
  "P I E C E C R A F T - H Q", a live "Nav >" prompt) but creates NO
  visible window of any kind - confirmed `button.sh`/`orchestrator.c`
  never spawn a terminal emulator (grepped for xterm/gnome-terminal/
  konsole - zero hits), so the ASCII UI is headless by design; the
  ONLY window-producing code path found so far is a "GL/RGB MIRROR"
  gated on `$DISPLAY` AND `REAL_GAME_STATE=playing` (button.sh
  ~line 300-373) - meaning nothing visible is expected to appear
  until some later in-game state transition, not at launch. Unclear
  whether this is a real regression or a misunderstanding of the
  intended UX (was "toys > piececraft-hq" ever supposed to show
  something immediately, or does the user need to separately attach a
  real terminal to the headless ASCII backend to drive it into a
  state where the GL mirror activates?). Not yet resolved - needs
  either a working comparison against another toys-menu app that
  DOES show something on launch, or direct clarification of the
  intended UX from the user.
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
- **`khtpm_core_render.c`'s `dbhq_load_actors()` loads real PDL data
  inline in the shared parser/renderer file** instead of via a
  separate manager process (violates `CENTROID_GOLD_STD.md` §3 rule
  2). An audit pass for sibling inline loaders (Classes/Skills/Items
  etc.) has not been done.

## Recently fixed (kept short — see 03-pitfalls for the general lesson each one produced)

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
