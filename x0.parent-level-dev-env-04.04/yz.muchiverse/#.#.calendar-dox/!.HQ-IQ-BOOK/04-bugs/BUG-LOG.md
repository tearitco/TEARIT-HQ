# Bug log — append dated entries, do not rewrite history

*Convention: add a new dated entry at the bottom for anything newly
found/fixed. If an old entry goes stale, add a dated 🔄 CORRECTION
note under it — don't silently edit it away.*

## Open

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
