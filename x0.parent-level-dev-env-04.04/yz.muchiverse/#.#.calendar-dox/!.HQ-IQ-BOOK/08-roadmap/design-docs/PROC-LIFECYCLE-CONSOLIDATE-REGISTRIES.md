# Proc-lifecycle: consolidate the registries + track the prisc VM

**Status: DROP + PRISC LANDED (2026-09-09); FOLD declined by decision.**
Follow-up to `PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md` §5 step 7 + the
prisc-VM gap. Three asks: **fold** `livedesk_hq_windows_<pid>.txt`,
**drop** `livedesk_launched_pids.txt`, register **the prisc VM**.
Researched first; the fold is deliberately *not* done and §1 says why.

**Landed this pass:**
- `livedesk_launched_pids.txt` deleted end-to-end (§2). `kill_hq_windows.sh`
  + `ktb_system_recorded()` + 12 launchers now use `livedesk_proc_list.txt`.
- `chtpm_parser_pal.c` (canonical) gained opt-in proc-ledger hooks (§3);
  `044.pal-chat-irc` migrated + builds clean. Remaining pal-VM projects
  are a mechanical rollout (§3.3) — same phased shape as prisc+x.
- Tests: `test_parser_module_reg.c` (11 checks) + `test_kill_hq_windows.sh`
  (4 checks), both wired into `build_test_proc_registry.sh`; full suite green.
- FOLD: not done. Recommended future approach is the identity/volatile
  **split** in §1, not a literal merge.

---

## 0. Current registry landscape (researched)

| file | written by | read by | shape / cadence |
|---|---|---|---|
| `#.desktop/livedesk_proc_list.txt` (**the canonical ledger**, new 2026-09-09) | `ktb_system_recorded()` (every tb launch) + `kh_proc_register*` (renderer's `<module>`s, mpg123) | `kh_proc_reap_all` / `_reap_subtree` / `_prune` on quit + `ktb_init` | append-only, 5-field `pid pgid master starttime name`, pruned on manager start |
| `#.desktop/livedesk_launched_pids.txt` (**legacy**) | `ktb_system_recorded()` **and** a raw `echo $! >> …` in ~13 app launchers (`&.hq-apps/*/button.sh`, `open_*.sh`, `&.widgits/open-hai/button.sh`, …) | `kill_hq_windows.sh` (HQ-menu kill row) + one spot in `khtpm_taskbar_manager.c` | append-only, **bare PID per line**, pruned only by `kill_hq_windows.sh` |
| `#.desktop/livedesk_hq_windows_<pid>.txt` (per HQ window) | `khtpm_core_render.c` — 4 sites: first map, minimize toggle, focus change, atexit unlink | **only** `khtpm_taskbar_manager.c` `ktb_merge_hq_windows()` → `s->hq_wins[]` (one clickable bottom-bar cell per live window) | **one line, rewritten every redraw tick**: `win=0x…\|pid=…\|title=…\|x=…\|y=…\|w=…\|h=…\|minimized=…\|focused=…` |

---

## 1. `livedesk_hq_windows_<pid>.txt` — **DO NOT FOLD**

The ask was to fold it into the one canonical ledger. After research:
**folding it is a net negative and it is not needed for teardown.**

- It is a **per-window UI-state** file (X11 window id, title, live
  geometry, minimized, focused), **rewritten on every redraw tick** by
  each window's own renderer. That is cheap *because* it is one small
  file per PID with a single writer and zero contention.
- Folding N of these into ONE shared ledger means every window rewrites
  the shared file on every focus change / drag / minimize —
  `flock` contention + `khpr_load`/`khpr_rewrite` churn on a file the
  teardown path also reads. That is the exact "one hot shared file, many
  writers" shape the house's one-writer rule exists to avoid.
- **HQ windows are already reaped.** A tb-launched HQ window's
  `setsid`-group-leader PID is in the canonical ledger via
  `ktb_system_recorded()`; `kh_proc_reap_all` does `kill(-pgid,…)` which
  reaches the render + its `<module>` manager. Nothing is orphaned.
- The renderer already `kh_proc_register_owned()`s its own `<module>`s
  (step 5a) and reaps them from its `atexit`.

**Decision:** keep `livedesk_hq_windows_<pid>.txt` as the per-window
UI-state file it is; it and the ledger serve different jobs (one =
"window state for the bottom bar", the other = "what to reap"). The
only optional add is a single `kh_proc_self_register()` in the
renderer's window-create path (base identity row, atexit-removed) as a
belt so a `kill -9`'d launcher-wrapper still leaves the render findable
by name in the ledger — low value, and it re-introduces a per-tick
concern to get the geometry in, so **also skipped**. Update
`PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md` §7 to record this.

---

## 2. `livedesk_launched_pids.txt` — **DROP** (folded into the ledger)

Real value: one registry instead of two; the ledger's `starttime`
PID-reuse guard + `pgid` column replace the bare-PID list.

### 2.1 Writers → the ledger
- `ktb_system_recorded()` in `khtpm_taskbar_manager.c` — **stop** the
  `echo $! >> …/livedesk_launched_pids.txt` tail; it already writes the
  5-field ledger line right after. One-line deletion + drop the
  read-back-from-the-old-file (read `$!` straight from `system()`'s
  wrapped `echo $! > …tmp` instead, or keep reading the ledger's own
  last line).
- The ~13 launchers doing `echo $! >> "$H/#.desktop/livedesk_launched_pids.txt"`
  → `echo "$! $! 0 0 <name>" >> "$H/#.desktop/livedesk_proc_list.txt"`
  (5-field: pid, pgid=pid — they're all `setsid` group leaders —,
  master 0 = orchestrator-owned, starttime 0 = liveness-only guard,
  a short name). A tiny shared snippet or `kh_spawn.sh` could replace
  the inline `setsid nohup … & PID=$!` but a one-line swap per launcher
  is lower-risk and keeps each launcher self-contained.

### 2.2 Readers → the ledger
- `kill_hq_windows.sh` — currently reads `livedesk_launched_pids.txt`
  (bare PID) and does `kill -TERM -$pid` (group). Repoint to
  `livedesk_proc_list.txt`, take **field 2** (pgid) for the group kill,
  field 1 for the fallback single-PID kill, and honour the
  `starttime` guard the C reaper uses (skip a line whose
  `/proc/<pid>/stat` field-22 no longer matches — reuse the same
  awk/`stat` check). Keep the hardcoded name-pattern list as the
  pre-ledger backstop.
- `khtpm_taskbar_manager.c`'s one read of `livedesk_launched_pids.txt`
  → read `livedesk_proc_list.txt`, field 1.

### 2.3 Order (each its own commit + the §4 tests)
1. `kill_hq_windows.sh` reads the ledger (still also reads the legacy
   file for one release — union of both).
2. `ktb_system_recorded()` stops writing the legacy file.
3. Launchers swap their `echo` line (one commit, ~13 files, each
   smoke-launched).
4. `khtpm_taskbar_manager.c` reader repointed.
5. Delete the legacy-file read from `kill_hq_windows.sh`; `git grep`
   confirms zero `livedesk_launched_pids` references remain; drop it
   from `.gitignore` too.

---

## 3. The prisc VM — **REGISTER** (via `chtpm_parser_pal.c`)

Every pal-VM project (`041.pal-*`, `@.apps/*` toys, mutaclysm,
board-viewer, …) runs its `prisc+x` VM as a persistent child forked by
`chtpm_parser_pal.c`'s `launch_module()` / `launch_extra_module()` from
a `<module src="system/prisc+x pal/…">` tag. That PID is tracked by
**nobody** — a taskbar quit / `button.sh reset` leaves it running
(exactly `OPEN-ITEMS.md` #7-class).

### 3.1 The hook
- `chtpm_parser_pal.c` (the shared `_shared-lib/system/` copy —
  `PRISC-X-FORK-CONSOLIDATION.md` sibling: it too is vendored per
  project, ~20 build scripts, most already carry a `$_SS` / `$SHARED_LIB`
  var).
- Includes: it already has `#define _GNU_SOURCE` at line 2 and
  `<sys/wait.h>`, `<signal.h>`, `<unistd.h>`. Add
  `#define KH_PROC_REGISTRY_IMPL` + `#include "kh_proc_registry.h"`.
- **house-root**: `chtpm_parser_pal.c` has `project_root_path` (the
  project/session dir), not house root. The ledger lives at
  `<house>/#.desktop/`. Walk up from `project_root_path` (or from
  `argv[0]`'s dir) until a dir containing `#.desktop/` is found — the
  same resolver the prisc `build.sh` conversion used. Cache it once.
- **`launch_module()`** (`current_module_pid = fork()` parent branch)
  and **`launch_extra_module()`**: after a successful fork, in the
  parent, `kh_proc_register_owned(house, mod_pid, mod_pid, getpid(),
  "prisc")`. The child is NOT `setsid`'d (`launch_module` just
  `fork`+`chdir`+`execv`), so `mod_pid` is its own pid and stays in the
  parser's group — but registering also lets `_prune` / a house-wide
  `_reap_all` reach it if the parser is `kill -9`'d.
- **`cleanup_module()`** (kills `current_module_pid`): after the kill,
  `kh_proc_reap_one(house, current_module_pid, 1)` to drop the row.
  Same for the extra-module cleanup.
- A **SIGTERM/SIGINT handler** in `chtpm_parser_pal.c`'s `main()` (if it
  lacks one) that calls `cleanup_module()` then `_exit` — so a plain
  kill of the parser takes the VM down too.

### 3.2 Per-project builds
Add `-I "$SHARED"` (the `_shared-lib` dir, where `kh_proc_registry.h`
lives) to every `build.sh`/`button.sh` line that compiles
`chtpm_parser_pal.c`. The `PRISC_CANON_SHARED_LIB` / `_SS` var most of
them already have from the prisc pass points at `_shared-lib`, so it's
`-I "$_SS"` / `-I "$PRISC_CANON_SHARED_LIB"`.

### 3.3 Order
1. `chtpm_parser_pal.c` (shared copy) gets the include + the
   register/reap calls + the term handler. Build the shared copy
   standalone (`-I` to `_shared-lib`) — compile-clean, no behaviour
   change with an empty/again house root (guards).
2. One pal-VM project's `build.sh` gets `-I "$SHARED"`; rebuild; run
   its `button.sh`; confirm a `prisc` row appears in
   `livedesk_proc_list.txt` and disappears on quit. (`044.pal-chat-irc`
   or `102.editor` — low stakes.)
3. Roll `-I "$SHARED"` to the rest, one commit per few, each
   `button.sh`-smoked.
4. (Consolidation, separate: `chtpm_parser_pal.c` is *also* vendored ~20
   times — same fix as prisc, own follow-up.)

---

## 4. Tests (before any implementation)

`&.widgits/_shared-lib/tests/` — extend the existing desktop-safe
harness (spawns its own `/tmp` children, touches no house process):

- **`test_proc_registry_master.c`** already covers `register_owned` /
  `reap_subtree` / `self_register` / 4+5-field coexistence — unchanged.
- **New `test_kill_hq_windows.sh`** (shell): make a temp `#.desktop/`,
  write a `livedesk_proc_list.txt` with (a) a live `setsid sleep`
  group-leader row, (b) a dead-PID row, (c) a live-PID/wrong-starttime
  row; run the ledger-reading `kill_hq_windows.sh` against it; assert
  (a) reaped, (b)+(c) untouched, file pruned. No house PID involved.
- **`test_parser_module_reg.c`**: reproduce `launch_module`'s
  fork+register + `cleanup_module`'s kill+reap_one bodies verbatim
  against a `/tmp` `sleep` "module"; assert the row is added on launch
  and gone after cleanup; a house-wide `reap_all` also gets it.
- **Live smoke** (per §2.3 / §3.3 step 2): one HQ launcher + one pal-VM
  project — `ps` shows zero strays after a taskbar quit; the ledger is
  empty; a normal restart is unaffected; the toys/HQ dropdowns still
  work (X11-pitfalls rule).

## 5. KPIs

- `git grep -l livedesk_launched_pids` → only this doc + a CHANGELOG
  note. The file is gone.
- After a taskbar quit: `ps aux | grep -E 'prisc\+x|khtpm|_manager'`
  (this house) → **zero**, with no manual step — including a pal-VM
  project that was open.
- `livedesk_hq_windows_<pid>.txt` still works (bottom-bar cells,
  minimize/restore) — untouched.
- A stale / reused-PID line in `livedesk_proc_list.txt` is never acted
  on by `kill_hq_windows.sh` (the shell test) or the C reaper (existing
  test).
- One registry file, one shape.

## 6. Related
- `PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md` (§3.1 line shape, §5, §7).
- `PRISC-X-FORK-CONSOLIDATION.md` (`chtpm_parser_pal.c` is the same
  vendored-copy problem — a shared follow-up).
- `&.widgits/_shared-lib/kh_proc_registry.h` / `kh_spawn.h`.
- `*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh`,
  `khtpm_taskbar_manager.c` (`ktb_system_recorded`,
  `ktb_merge_hq_windows`).
