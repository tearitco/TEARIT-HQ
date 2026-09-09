# Process lifecycle — orchestrator-owned, PID-tracked teardown

**Status: WIRED + LIVE-VERIFIED (2026-09-09).** Steps 1–3 of §5 done;
unit + integration + a real live-desktop run all pass. Remaining is the
app-fork funnelling (§5 steps 4–7) — a window/manager NOT launched by
the taskbar (its own `button.sh` from a terminal, a `<module>`-spawned
manager, an engine child) still does **not** register.

Live run (2026-09-09, on the running desktop):
1. `run_khtpm_strip.sh new` → new manager (fresh binary) came up clean;
   `ktb_init`'s `kh_proc_registry_prune()` created
   `#.desktop/livedesk_proc_list.txt`; the 6 autostart desk entities
   each registered via `ktb_system_recorded` with real `pgid` +
   `/proc` start-time.
2. `echo 1003 >> strip_history.txt` (KSC_CLOSE_QUIT) → all 6 registered
   entities + the manager + the strip renderer **reaped**;
   `livedesk_proc_list.txt` **truncated to 0**. A pre-registry orphan
   `khtpm_core_render.+x` (from a run before the registry existed) was
   correctly **left alone** — that's the `kill_hq_windows.sh`
   name-pattern backstop's job.
3. `run_khtpm_strip.sh new` again → desktop fully restored (7 tabs,
   entities respawned).
4. Header-cell dropdowns still dispatch: `4006` (db) →
   `n_hqitems=12`, `4008` → `n_hqitems=5`. No override_redirect path
   was touched; no regression.
5. Legacy `livedesk_launched_pids.txt` is still written in parallel, so
   `kill_hq_windows.sh` keeps working unaided.

What landed:
- `khtpm_taskbar_manager.c` — `#define KH_PROC_REGISTRY_IMPL` +
  `#include "kh_proc_registry.h"`. `ktb_system_recorded()` now also
  reads back the launched PID and `kh_proc_register()`s it (real
  `/proc` start-time, the PID-reuse guard) into
  `#.desktop/livedesk_proc_list.txt`; the legacy
  `livedesk_launched_pids.txt` is still written for one release so
  `kill_hq_windows.sh` keeps working unaided. `ktb_init()` calls
  `kh_proc_registry_prune()` (not `_reset` — a `run_khtpm_strip.sh new`
  restarts only the strip pair, leaving prior windows alive; prune
  keeps live entries, drops dead/reused ones). New public
  `ktb_reap_launched()` = `kh_proc_reap_all(house_root, 200, 0)`.
- `khtpm_taskbar_manager.h` — declares `ktb_reap_launched()`.
- `khtpm_taskbar_manager_main.c` — calls `ktb_reap_launched(s->house_
  root)` at the **three** explicit-user-quit sites only (X.quit,
  `hq_quit_requested`, `KSC_CLOSE_QUIT`), right after the existing
  `ktb_stop_strip_renderers()`. A plain SIGTERM
  (`run_khtpm_strip.sh` restart) does **not** reap.
- `build_khtpm_strip.sh` — `-I "$SHARED"` on the manager-driver
  compile.
- `&.widgits/_shared-lib/tests/test_proc_registry_tb.c` +
  `build_test_proc_registry.sh` (runs both tests) — the integration
  test reproduces the `ktb_system_recorded` / `ktb_reap_launched` /
  `ktb_init`-prune bodies verbatim against detached `/tmp` children:
  both files get the 3 entries, reap kills all 3 + truncates the
  canonical registry, prune keeps the live entry and drops a stale
  one. 0 failures; no leaked children.

Full house-wide funnelling (every app's own long-lived forks;
`livedesk_hq_windows_<pid>.txt` fold-in; retiring the "kill your own
children" note) remains §5 steps 4–7. Prompted by the user: *"devs are expected to kill child
processes themselves — this isn't the desire. TPMOS's orchestrator
tracks its children's PIDs and runs an on-kill teardown. We should have
the same standard."*

---

## 1. What TPMOS actually does (the reference)

`1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/orchestrator.c`:

- **One spawn path.** Every child is launched through
  `launch_and_register(name, path, args, quiet)`. After `fork()`, the
  parent calls `log_pid(pid, name)` → appends `"<pid> <name>\n"` to
  `pieces/os/proc_list.txt`, under `flock(LOCK_EX)` + `fsync`.
- **Startup** truncates `proc_list.txt` and logs the orchestrator's own
  PID first.
- **Teardown** (`kill_all_tracked_processes()`):
  1. read `proc_list.txt`; for each line `kill(-pid, SIGTERM)` (whole
     process **group**) **and** `kill(pid, SIGTERM)`.
  2. `usleep(200000)` — 200 ms grace.
  3. re-read; `kill(-pid, SIGKILL)` + `kill(pid, SIGKILL)` +
     `waitpid(pid, WNOHANG)` to reap.
  4. truncate `proc_list.txt`.
- **`kill_all.sh`** (`pieces/os/kill_all.sh`) is a *backstop only*: ~80
  hardcoded `surgical_kill "<binary-name>"` calls + a "nuclear"
  `pkill -9 -f "/(pieces|projects)/.+/.+"` regex. It runs *after* the
  tracked reap, to catch anything that was never registered.

**Nuance that matters for the house:** on POSIX, TPMOS's own
`handle_sigint()` mostly leans on `kill(0, SIGTERM)` — every child is in
the orchestrator's process group, so one group signal reaches them all.
The explicit `kill_all_tracked_processes()` walk is the **Windows** path
(detached children, no shared group). **The house is in the Windows-like
situation on Linux**: every house launch is `setsid nohup …`, i.e.
deliberately a *new* session / process group detached from the taskbar,
so `kill(0, …)` from the taskbar reaches nothing. The house therefore
needs the explicit tracked-PID **group** reap, exactly like TPMOS's
Windows path.

---

## 2. What the house has today (scattered, grew reactively)

| Mechanism | Scope | Who tears it down |
|---|---|---|
| `#.desktop/livedesk_hq_windows_<pid>.txt` — each HQ window writes its own file; its `atexit` removes it | HQ windows only | read for the entity list / liveness; not a teardown source |
| `ktb_system_recorded(house_root, cmd)` in `khtpm_taskbar_manager.c` — wraps a launch, `echo $! >> #.desktop/livedesk_launched_pids.txt` | **every** launch the **taskbar manager** makes (`$!` = the `setsid` group leader) | nothing, automatically |
| `kill_hq_windows.sh <house_root>` — reads `livedesk_launched_pids.txt` + a hardcoded name-pattern list; `kill -TERM -$pid` (group), 1 s, `-KILL`; prunes the registry | HQ family + recorded launches | **only when invoked from the HQ menu, by hand** |
| `ktb_stop_strip_renderers()` | strip renderer pair | explicit-quit sites in `_main.c` only |
| `EMERGENCY_KILL.sh` / `EMERGENCY_CLOSE.sh` / `button.sh reset` | everything, name-based | human |

So the house already has **most of the pieces**: a group-leader PID
registry (`livedesk_launched_pids.txt`), a TERM→KILL group reaper
(`kill_hq_windows.sh`), and a name-pattern backstop. What's missing is
the **orchestrator discipline** tying them together.

### The real gaps

1. **No owner runs the teardown on exit.** `livedesk_launched_pids.txt`
   is written but never consumed automatically when the taskbar exits —
   you must pick a menu item or `button.sh reset`. TPMOS does it in the
   signal handler + normal shutdown path.
2. **Coverage is taskbar-manager-only.** An app/toy that forks its *own*
   long-lived children — a manager forking an ops binary, `mpg123 -R`
   under music-player-hq, prisc VMs, `<module>`-spawned managers — does
   not register those with anyone. `kill -9` on a render orphans its
   manager (pitfall #13.5). TPMOS funnels *every* spawn through
   `launch_and_register`.
3. **Two registry shapes** (`livedesk_hq_windows_<pid>.txt` per-window
   vs the flat `livedesk_launched_pids.txt`) plus a name list, with no
   single canonical `proc_list.txt`.
4. **Registry hygiene is weak.** `livedesk_launched_pids.txt` is bare
   PIDs, append-only, only pruned by `kill_hq_windows.sh`. PID reuse
   then makes a stale line dangerous (you group-kill a *reused* PID).
   No pgid recorded, no name, no timestamp, no start-time guard.

This is precisely why `03-pitfalls/OPERATIONAL-LANDMINES.md` #9 and
`00-compact/…` tell devs to "kill your own children after a test" — a
workaround for the missing standard, not the intended end state.

Related open items this closes / shrinks: `OPEN-ITEMS.md` #7
(toys-launch PID tracking — kill-all doesn't reach toys) and #10
(`ktb_pid_alive()` zombie false-positive), plus pitfall #13.5.

---

## 3. Target design

### 3.0 Is this a "master-pid + child-pid master-ledger", and is it the default?

**That is the target, yes** — and it's the right shape for this house
(it's what `master_ledger.txt` already is elsewhere: a flat,
append-only, cursor-read text ledger; no DB, no daemon). As of
2026-09-09 the *implemented* v1 is a **subset**: a flat registry with
**no master-pid column** and registration that is an **explicit
opt-in call** (`kh_proc_register`) made only by `ktb_system_recorded`
(taskbar-initiated launches). The rest of this section is the full
target; §5 steps 4–7 close the gap.

### 3.1 One canonical master-ledger
`#.desktop/livedesk_proc_list.txt`, one append-only line per tracked
process — **TARGET shape**:
```
<child_pid> <child_pgid> <master_pid> <starttime> <name> [k=v ...]
```
(v1 shipped without the `<master_pid>` column — the loader must accept
both the 4-field and 5-field forms.)
- `master_pid` — **the new column.** The PID of the process that
  "owns" this child for teardown purposes: the taskbar manager for a
  tb-launched HQ window/toy; the app's own root process for a child it
  forked itself (an engine daemon, a prisc VM, a `<module>` manager).
  This is what turns "reap everything" into **scoped teardown**: close
  one app → `kh_proc_reap_subtree(master = that app's pid)` reaps only
  its line + its descendants, not the whole desktop. Fixes
  `OPEN-ITEMS.md` #7 (toys) *properly* and lets `kill_hq_windows.sh`'s
  name-pattern list finally retire.
- `child_pgid` — the process-group to signal (`kill(-pgid, …)`). For a
  `setsid` launch `pgid == pid` (group leader). Recorded explicitly so
  the reaper never assumes.
- `starttime` — field 22 of `/proc/<pid>/stat`. The **PID-reuse
  guard**: re-read before signalling; gone or start-time changed ⇒
  stale line ⇒ skip. Defeats the whole `ktb_pid_alive()` zombie/reuse
  bug class.
- `name` — human label (`sql-hq`, `mpg123`, `prisc+x` …).
- optional trailing `k=v` — lets `livedesk_hq_windows_<pid>.txt`'s
  `win=/title=/x=/y=/minimized=/focused=` fields fold onto the same
  line (§5 step 7), collapsing the house's *two* registry shapes into
  one.
- Written `flock(LOCK_EX)` + `fsync` (TPMOS `log_pid` shape). Pruned on
  every manager start and on a slow idle tick (not blindly truncated —
  a strip-only restart leaves prior windows alive).

### 3.1a Why a flat text ledger IS the efficient architecture here
Not a compromise — the house-correct answer: append is O(1) + one
`fsync`; readers `stat()` the size to detect growth and do
cursor-incremental reads (`strip_input_history.txt` pattern); teardown
reads the whole file once — **dozens of lines, never thousands** — so a
full scan is trivial; the `starttime` column is an O(1) reuse guard, no
lookup table. A DB / long-running supervisor daemon would add a
failure mode (the supervisor itself) for zero real gain at this scale,
and violates "if it's not in a file, it's a lie" less cleanly.

### 3.2 The shared helper — `&.widgits/_shared-lib/kh_proc_registry.h`
Single header, C, POSIX + `_WIN32` guards, same house style as
`kh_plat.h`.

**v1 API (shipped 2026-09-09):** `kh_proc_registry_path`,
`kh_proc_registry_reset`, `kh_proc_register(house_root, pid, pgid,
name)`, `kh_proc_reap_all`, `kh_proc_reap_one`, `kh_proc_registry_prune`.

**TARGET API additions (§5 step 4):**
```c
/* the master-ledger register: `master_pid` is the owning process
 * (getpid() of whoever is doing the spawn). 5-field line. */
int  kh_proc_register_owned(const char *house_root, long pid, long pgid,
                            long master_pid, const char *name);

/* reap only the entries owned by `master_pid` (its line + any line
 * whose master_pid is that pid, one level; deep chains rely on each
 * intermediate having registered its own children). Same
 * TERM->grace->KILL + starttime guard. Rewrites the ledger without the
 * removed lines. This is "close one app", not "quit the desktop". */
int  kh_proc_reap_subtree(const char *house_root, long master_pid,
                          int grace_ms, int verbose);

/* a house binary that was exec'd outside a kh_spawn funnel registers
 * itself in main() and unregisters via atexit. Line:
 *   getpid() getpgrp() getppid() starttime <name> self=1            */
int  kh_proc_self_register(const char *house_root, const char *name);
int  kh_proc_self_unregister(const char *house_root);
```
The v1 4-field loader stays; it just treats a missing `master_pid` as
`0` ("owned by the orchestrator / reaped only by `reap_all`").

### 3.2b Making it the DEFAULT (not per-call opt-in)
Two mechanisms, belt + braces:

1. **`kh_spawn()` — the one funnel.** A shared
   `&.widgits/_shared-lib/kh_spawn.h` (`kh_spawn(house_root,
   master_pid, name, argv, flags)` = `fork` + optional `setsid` +
   optional `chdir(house_root)` + `execv` + `kh_proc_register_owned`
   in the parent), plus `kh_spawn.sh` for the shell launch sites
   (`setsid nohup "$@" & echo "$! $! $$ …" >> …`). House convention
   becomes: **spawn a long-lived child only via `kh_spawn` — never a
   raw `fork`/`setsid nohup … &`.** Same status as `kh_plat.h` (the
   one blessed platform surface). "Default" = the blessed primitive
   everyone reaches for; a raw `fork` that skips it is then a review
   smell, and the name-pattern backstop covers the slips.
2. **`kh_proc_self_register()` in `main()`** for the deep children the
   funnel can't wrap — an engine daemon started by a vendor script, a
   `<module>` binary re-exec'd by the renderer. It writes its own line
   (`master = getppid()`), `atexit`-removes it. This is exactly what
   `livedesk_hq_windows_<pid>.txt` already does per-window — fold that
   write into `kh_proc_self_register` and the two registries become
   one.

### 3.3-old (v1) API sketch, for reference
```c
int  kh_proc_register(const char *house_root, long pid, long pgid,
                      const char *name);          /* 4-field line */
int  kh_proc_reap_all(const char *house_root, int grace_ms, int verbose);
int  kh_proc_registry_prune(const char *house_root);
int  kh_proc_reap_one(const char *house_root, long pid, int grace_ms);
```

Implementation notes:
- `_start_time(pid)` reads `/proc/<pid>/stat` field 22 (skip past the
  `)` of `comm` first — the comm can contain spaces/parens).
- The reaper **must** compare live starttime to the recorded one and
  skip on mismatch — this is the load-bearing safety property.
- `kh_proc_reap_all` explicitly refuses to signal its own pid or its
  own pgid (belt-and-braces against a bad line).
- `_WIN32`: `kh_proc_register` still logs; the reaper uses
  `OpenProcess`/`TerminateProcess` per pid (no group concept), matching
  TPMOS's `win_kill`.

### 3.3 Who calls what

| Site | Call |
|---|---|
| taskbar manager, **startup** (once, before launches) | truncate `livedesk_proc_list.txt` |
| every taskbar launch site — replace the bare `ktb_system_recorded()` | spawn `setsid`, then `kh_proc_register(house_root, pid, pid, name)` |
| taskbar manager **SIGTERM / SIGINT handler** and the normal `ktb_quit_and_save()` exit path | `kh_proc_reap_all(house_root, 200, 0)` **then** `system("… kill_hq_windows.sh <house_root>")` as the name-pattern backstop |
| explicit-quit sites (`KSC_CLOSE_QUIT`, `hq_quit_requested`) — already call `ktb_stop_strip_renderers()` | add `kh_proc_reap_all()` alongside |
| a "close this one window" action | `kh_proc_reap_one()` |
| an app/manager that forks a **long-lived** child (engine daemon, ops loop, prisc VM) | `kh_proc_register()` on that child too, so a house-wide reap reaches it |
| `button.sh reset` / `EMERGENCY_KILL.sh` | additionally `exec` the reaper against the registry (so the manual path and the automatic path share one mechanism) |

### 3.4 What stays a backstop
`kill_hq_windows.sh`'s hardcoded name-pattern list and the
`EMERGENCY_*` scripts stay — they catch processes started *before* the
registry existed (a mid-session rebuild), or by a code path that
forgot to register. The registry reap is primary; the name sweep runs
after it, quietly.

---

## 4. Guardrails (this is desktop-killing code if wrong)

- **`kill(getppid(), SIGTERM)` once logged the whole desktop out**
  (pitfall #15 — it hit `systemd --user`). The reaper must:
  - never signal pid 0, 1, its own pid, or its own pgid;
  - only signal a pid whose recorded `starttime` still matches
    `/proc/<pid>/stat` — a reused PID is a *different* process;
  - treat a registry line it can't fully parse as a no-op, not a
    "kill everything" fallback.
- **Never flip a global to make this work.** No new house-wide PDL
  toggle; the registry file *is* the state.
- **The 200 ms grace is real** — a window mid-write to its own state
  file wants the TERM path, not an instant KILL.
- **Test the SIGKILL phase explicitly** — a child that installs
  `SIG_IGN` for SIGTERM must still die.
- **Don't `git`-track the registry file** — it's `#.desktop/` runtime
  state.

---

## 5. Migration steps

1. ✅ **DONE 2026-09-09.** `&.widgits/_shared-lib/kh_proc_registry.h`
   (single-header, `KH_PROC_REGISTRY_IMPL` idiom like `kh_plat.h`) +
   `tests/test_proc_registry.c` + `tests/build_test_proc_registry.sh`.
   The harness spawns its own detached `/tmp` children — **no house
   process is touched** — and asserts, all 16 checks passing:
   - 3 `setsid` children (one with `signal(SIGTERM, SIG_IGN)`) →
     `kh_proc_reap_all` → all dead (the immune one via the SIGKILL
     phase), registry truncated, return value = 3;
   - a dead-PID line **and** a live-PID line with a mismatched
     `starttime` (the reused-PID shape) are **both** skipped —
     `reap_all` signals nothing, the live process survives;
   - a registry line pointing at the test process itself / its own
     group is ignored;
   - `kh_proc_reap_one` kills one entry and leaves the other running
     with its registry line intact.
2. ✅ **DONE 2026-09-09.** `ktb_init()` prunes `livedesk_proc_list.txt`
   (not truncate — a strip-only restart leaves windows alive); every
   `ktb_system_recorded()` also `kh_proc_register()`s the launched PID.
   Legacy `livedesk_launched_pids.txt` still written for one release.
3. ✅ **DONE + LIVE-VERIFIED 2026-09-09.** `ktb_reap_launched()` =
   `kh_proc_reap_all(hr, 200, 0)` at the 3 explicit-quit sites. Live
   run confirmed register→quit→reap→truncate; dropdowns unaffected;
   restart unaffected (see the status block up top).
4. ✅ **DONE + LIVE-VERIFIED 2026-09-09.** `<master_pid>` column added
   (`<pid> <pgid> <master_pid> <starttime> <name>`); `khpr_load` accepts
   both the 4- and 5-field forms. New API in `kh_proc_registry.h`:
   `kh_proc_register_owned`, `kh_proc_reap_subtree(master_pid)`,
   `kh_proc_self_register` / `kh_proc_self_unregister`. New
   `&.widgits/_shared-lib/kh_spawn.h` (`kh_spawn()` = fork + setsid? +
   chdir? + execv + `kh_proc_register_owned` in the parent) +
   `kh_spawn.sh` (shell launch sites). `ktb_system_recorded` →
   `kh_proc_register_owned(…, getpid(), "tb-launch")`.
   `tests/test_proc_registry_master.c` (added to the runner) — 14
   checks, all pass: subtree reap isolates master A from B; a reap keyed
   on an app-root pid also drops that root's own row;
   self-register/unregister round-trips; a v1 4-field line and a
   master 5-field line are both reaped by `reap_all`. Live: restart →
   ledger lines now carry `master = <manager pid>` (verified against
   `ps -o comm`); KSC_CLOSE_QUIT still reaps all + restore is healthy.
5. Make it the default, app by app (one commit each, smoke per app):
   - ✅ **`<module>`s DONE + LIVE-VERIFIED 2026-09-09.**
     `khtpm_core_render.c` `#include`s `kh_proc_registry.h` (own IMPL —
     separate binary). `kh_collect_and_launch_modules()` calls
     `kh_proc_register_owned(house_root, module_pid, module_pid,
     getpid(), <id>)` for every `<module>` it forks;
     `kh_cleanup_modules()` (atexit) also runs
     `kh_proc_reap_subtree(g_house_root, getpid(), 1, 0)` to drop the
     render's owned rows. Live: opened sql-hq → ledger row
     `<projector pid> <pgid> <sql-hq render pid> … sql-hq` appeared;
     the projector was `setsid`-detached (own pgid) so a plain
     group-kill would have missed it; SIGTERM the render → the row is
     gone AND the detached projector reaped. Fixes pitfall #13.5.
   - ✅ **`music-player-hq` DONE 2026-09-09.** `music_player_manager.c`
     `#include`s `kh_proc_registry.h` (own IMPL; build_music_player_
     manager.sh gained `-I "$SHARED"`). `mpg_start()`
     `kh_proc_register_owned(...,getpid(),"mpg123")`; the dead-reap
     path `kh_proc_reap_one`s it; a new SIGTERM/SIGINT handler
     (`mp_on_term`) SIGKILLs mpg123 + drops its row + `_exit(0)` — mpg123
     -R is its own group leader so it does NOT die with the manager's
     group otherwise. Compiles clean; not runtime-smoked (needs a track
     playing).
   - **prisc-hosting `button.sh`s** (board-viewer + ~20 pal projects) —
     move `setsid nohup system/prisc+x … &` to `kh_spawn.sh`. Pairs
     with `PRISC-X-FORK-CONSOLIDATION.md` Phase B (same file set). TODO.
   - remaining `setsid nohup … &` sites in `khtpm_taskbar_manager.c`
     already go through `ktb_system_recorded` (registered) — leave.
6. ✅ **DONE 2026-09-09.** `OPERATIONAL-LANDMINES.md` #9 + the compact
   doc's "Kill your children" bullet rewritten: the taskbar reaps the
   ledger on quit and the renderer reaps its own `<module>`s; an
   engine child a *manager* forks, or anything from a bare `button.sh`,
   is still manual until that app moves to `kh_spawn` /
   `kh_proc_self_register()`.
7. **Superseded by `PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md`
   (2026-09-09).** Outcome of that research:
   - **DROP `livedesk_launched_pids.txt` — DONE.** `kill_hq_windows.sh`
     reads the proc-ledger (field 1 = pid, field 2 = pgid); the 12
     launcher scripts + `ktb_system_recorded()` append/register into
     `livedesk_proc_list.txt`; the bare-PID file is gone.
   - **FOLD `livedesk_hq_windows_<pid>.txt` — NOT done, by decision.**
     It is per-window UI-state rewritten every redraw tick; merging it
     into one shared ledger trades zero-contention per-PID writes for
     `flock` churn on the teardown file. HQ windows are already reaped
     via their `ktb_system_recorded` group leader. The recommended
     future move is a *split* (write-once identity → ledger via
     `kh_proc_self_register`; volatile geometry stays a per-PID tick
     file), not a merge — see that doc §1.
   - **prisc VM registration — DONE** (canonical `chtpm_parser_pal.c`,
     opt-in `-DKH_HAVE_PROC_REGISTRY`; `044.pal-chat-irc` migrated,
     rest are a mechanical rollout).

## 6. KPIs

- Quitting the taskbar (`[X]` / `X.quit`) leaves **zero** house
  processes in `ps aux | grep -E 'khtpm|_manager|prisc|mpg123'` — no
  manual step.
- A stale / reused-PID line in the registry is **never** acted on
  (unit-tested + a deliberate reused-PID soak).
- `run_khtpm_strip.sh new` (rebuild-restart) still works — the reaper
  does not race the fresh strip.
- Toys launched from the toys menu are reached by the quit reap
  (closes `OPEN-ITEMS.md` #7).

## 7. Related

- `1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/orchestrator.c`
  (`log_pid`, `kill_all_tracked_processes`), `pieces/os/kill_all.sh`.
- `*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c`
  (`ktb_system_recorded`, `ktb_stop_strip_renderers`,
  `ktb_pid_alive`, `ktb_quit_and_save`).
- `*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh`.
- `03-pitfalls/OPERATIONAL-LANDMINES.md` #9, `HOUSE_CODE_PITFALLS.md`
  #13 (module orphans) / #15 (`kill(getppid())` logout).
- `08-roadmap/OPEN-ITEMS.md` #7, #10.
- `02-architecture/STATE-AND-PDL-CONVENTIONS.md` ("if it's not in a
  file, it's a lie" — the registry is that file).
