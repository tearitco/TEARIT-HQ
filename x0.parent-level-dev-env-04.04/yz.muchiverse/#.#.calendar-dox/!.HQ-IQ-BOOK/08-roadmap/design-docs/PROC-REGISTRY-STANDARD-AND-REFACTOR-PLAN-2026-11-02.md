# Process registry standard and refactor plan: "if it's not a file, it's a lie" for processes (2026-11-02)

Owner, 2026-11-02, after a day of strays (an orphaned picker manager, a board's daemon and game clock replaying for hours until the CPU guard ended the house, a taskbar
manager with no strip): *"we should scan for a code pattern that creates strays and replace it with one that always registers its procs to an auditable .txt or .pdl
file for killing this way. 'If it's not a file it's a lie' applies to procs also. I don't care if we have to spend the entire next week refactoring, this has to be
fixed."*

Labels: **RAN** (observed this session), **READ** (read in code/docs), **MINE** (proposal, the owner decides), **OPEN** (needs the owner).

Read with: `PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md` (the 2026-09-09 design this plan finishes), `PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md`,
`03-pitfalls/hq-cpu-safety.md` sections 8, 8b, `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, and the baseline file `PROC-REGISTRY-BASELINE-2026-11-02.txt` (every unregistered launch site).

## 1. The principle

A process the house starts is a **fact that must be written down by the process or its launcher before it does anything else**, in a file a person can read and a program
can act on. Any house process that is not in the file is a **violation** (it is reported, and, once enforcement is on, ended). Anything we kill, audit, restart or count is
found by reading that file, never by guessing from `ps` patterns, exe paths or "who is the parent". The same rule as state: if it is not in a file, it is not true.

## 2. Evidence

### 2.1 What went wrong today (RAN)
| Stray | How it was created | Why nothing reaped it |
|---|---|---|
| `swatch_picker_manager` + `taskbar_settings_projector` after the settings window closed | `button-pal.sh`: `setsid nohup "$MGR" ... &`; the projector started by hand | not in any ledger; closing the window never told the manager |
| rpg-edit `rpg_edit_hq.+x` resident + `lc_clock.+x` (16 h) | `open_board.sh`: `setsid nohup ... &`; the core's `clock_start` | nothing owns them; they outlive the board window by design |
| clock backlog replay at 100% CPU, ledger 1.1 MB | (separate cause, fixed in `lc_clock.c`) | the CPU guard had to end 40 processes, because there was no list to end them from |
| `khtpm_taskbar_manager_main` with no strip | strip renderer ended, manager is "protected" | the guard's CPU kill never touches the taskbar; no owner row says "my strip is gone" |
| `ui_sound_player`, killed by a restart, silently not back | `start_player.sh`: `setsid nohup`, pid in a private `state/player.pid` | a private pid file is not the house registry |

### 2.2 The scan (RAN, one-off, excludes tests/harness/`pieces/sessions`/`+x`/`xyzfs/users` copies)
* **523** production `.c/.h/.sh` files contain a spawn pattern; **262** of them can leave a detached or background process.
* Shell: **155** `setsid nohup ... &` sites, of which only **35** call a registry helper within three lines, so **120 are unregistered**. A further **173** bare
  background `&` lines (many are harmless, they still need triage) and **17** `nohup` without `setsid`. **16** shell scripts are polling loops (`while true` + `sleep`).
* C: **35** `setsid()` calls, **29** of them in files that never touch the registry; one `system("... &")`.
* **95** `.xhtpm` windows declare `<module>` children; those ARE registered, by the renderer (`kh_collect_and_launch_modules`, 2026-09-09).
* Unregistered `setsid` sites by app (shell): piececraft-hq 10, palettes 5, file-explorer 4, network 4, pets 3, `#.desktop/scripts` 3, events-hq 3, muchi-pet 3, stats-hq 3,
  db-hq-pal 3, then 2 each for rpg-space, rpg-dust, rpg-edit-hq, tpmos-hq, bookmarks, entity-cli and others. Full list: the baseline file.

### 2.3 What the house already has (READ)
* A registry file `#.desktop/livedesk_proc_list.txt`, line `<pid> <pgid> <master_pid> <starttime> <name>`, and `&.widgits/_shared-lib/kh_proc_registry.h`:
  `kh_proc_register_owned`, `kh_proc_self_register`, `kh_proc_reap_all`, `kh_proc_reap_subtree(master)`, a start-time guard against pid reuse.
* `kh_spawn.h` (C funnel: fork + setsid + execv + register) and `kh_spawn.sh` (shell funnel), `ops/proc_ledger_add.sh`.
* Registered today: everything the taskbar launches (`ktb_system_recorded`), every `<module>` of a window, `mpg123` under music-player-hq. **The petslike games do NOT
  register their resident daemon or `lc_clock`** (READ + RAN 2026-11-02: `rpg_edit_hq.c`, `rpg_space.c`, `df_hq.c` and `petslike_core.c` only READ the file's size in
  `viewer_kinds()`; none calls `kh_proc_register` or `kh_spawn`). So the count "45 of 262 launch files mention the registry" overstates registration: it includes readers.
* Reapers: the taskbar's quit reaper (reads the registry), `button.sh quit` (kills by **path pattern**, `all_khtpm_and_hq_pids`), `kill_hq_windows.sh` (name patterns),
  `mon_scan.sh` (heuristic classes: orphan, detached, dead-parent, stale-unreg), and since today the CPU guard's stray reaper (**exe-under-house heuristic**).

### 2.4 The gap, in one sentence
The funnel exists and is correct, but it is **opt-in** (at most 45 of 262 launch files mention it, and some of those only read it), the registry records no **owner kind, role or app**, shell rows have
`starttime 0` (no pid-reuse protection), the file is **truncated on quit** (no audit trail), and we have **four different kill mechanisms**, three of which are guesses
from `ps`. A process nobody registered is invisible to the one mechanism that is supposed to be authoritative.

## 3. Target design (MINE)

### 3.1 One registry, readable
`#.desktop/proc_registry.pdl`, one row per live house process, written by the **process itself at startup** (self-register) or atomically by the funnel that starts it:

```
PROC | pid | starttime | pgid | owner_kind | owner | role | app | name | started_iso | cmd
```
* `starttime`: field 22 of `/proc/<pid>/stat` (never 0, never "unknown": a row with a pid that fails the check is dead).
* `owner_kind`: `window` (a `khtpm_core_render` pid), `app` (an app's root/daemon pid), `session` (login), `house` (the guard, the taskbar manager), `oneshot`.
* `owner`: the pid (or file) whose death ends this process. **A process whose owner is gone is an orphan by definition**, no heuristic.
* `role`: `window | manager | daemon | projector | engine | clock | worker | sound | tool`. `app`: `rpg-edit-hq`, `dsr`, ... `cmd`: argv, for the audit.
* Append-only sidecar `#.desktop/proc_registry.log.txt` (capped, section 3.5): `START` and `END reason` rows, so "what ran, who started it, why it ended" is answerable next week.
* `livedesk_proc_list.txt` stays as a **mirror** while old readers exist (taskbar reaper, `kill_hq_windows.sh`), then is deleted (phase 5).

### 3.2 One way to start a process: the funnel, and what it guarantees
1. **Self-register before work.** The child writes its own row first thing (`kh_proc_self_register`, or `proc_reg.sh` as the first line of a script). This removes the
   race where the parent dies between `fork` and `register`, and it covers daemons that double-fork, which a parent can never see.
2. **Owner-death rule built in.** C: `prctl(PR_SET_PDEATHSIG, SIGTERM)` for children whose owner is their parent; for detached daemons a **owner watch** every 2-5 s
   (`kh_owner_alive()`: does the owner row's pid with that starttime still exist; is the owner window file present). The daemon exits when its owner is gone, which is what
   `swatch_picker_manager` does today by hand (`settings_window_alive()`), generalised.
3. **Exit unregisters** (`atexit` + signal handlers write `END`), so a clean exit leaves no row and a crash leaves a row the reaper can prove is dead.
4. **Shell:** `kh_spawn.sh` v2 runs `setsid nohup sh -c 'proc_reg.sh ...; exec "$@"'`, so the registered pid is the final process (exec keeps the pid), not a wrapper.
5. **pal / prisc:** the VM registers itself (already true in the canonical `chtpm_parser_pal.c`); `exec` of a custom op is a child of the VM, covered by owner = the VM.

### 3.3 The reaper reads the file
* `kh_proc_reap_orphans()`: every row whose owner is dead (pid+starttime check) is ended (TERM, wait, KILL), then logged `END orphan`.
* **Audit rule:** a process whose exe is under the house and that has no row for longer than `audit_grace_s` is a **violation**: logged to
  `#.desktop/proc_violations.txt` (pid, exe, cmdline, parent chain, first-seen), and the CPU guard's stray reaper treats it as a stray at once (no 120 s window needed).
* The guard's window-based stray reaper (hq-cpu-safety 8b) stays as the last-resort backstop; `button.sh quit` and `kill_hq_windows.sh` become thin callers of the
  registry reaper, and their name-pattern kills are demoted to "report only" once the audit is clean.

### 3.4 Never again by construction: the lint ratchet
A C op `spawn_lint` (house rule: C or pal, not python; the scan above was a one-off) plus a harness case `spawn_lint.pdl` that:
* finds the patterns in section 4, outside `kh_spawn.*` / `kh_proc_registry.h`, in production code;
* compares with the committed baseline (`PROC-REGISTRY-BASELINE-2026-11-02.txt`); **a site not in the baseline fails the case**, and the baseline may only shrink;
* is part of the standard harness gate, so a new `setsid nohup &` cannot land.
An allow-comment (`# proc-ok: <reason>`) is accepted for genuine one-shots and is itself listed in the lint output for review.

### 3.5 Registry hygiene (applies the runtime-file standard to this file)
Cap `proc_registry.log.txt` (rotate past 256 KB keeping the last 2,000 rows), prune dead rows on every reaper pass, one writer discipline (`flock`), tmp+rename for rewrites.

## 4. The code patterns to find and replace

| # | Pattern (what the scan looks for) | Replace with | Notes |
|---|---|---|---|
| P1 | sh: `setsid nohup X ... &` (120 unregistered) | `kh_spawn.sh <house> <owner> <name> -- X ...` | the canonical leak |
| P2 | sh: bare `X ... &` then the script exits (173 lines, to triage) | triage: (a) waited on in the same script = fine, mark `# proc-ok`; (b) outlives the script = P1 | many are `( ... ) &` one-shots |
| P3 | sh: `nohup X &` without `setsid` (17) | P1 | dies with the group on some paths, leaks on others |
| P4 | C: `fork` + `setsid()` + `execv` by hand (29 files) | `kh_spawn()` (C funnel) | |
| P5 | C: `system("... &")` | `kh_spawn()` | the sh -c layer hides the real pid |
| P6 | a daemon that never checks its owner (16 polling shell loops, every `*_manager`, the petslike resident, `lc_clock`) | add `kh_owner_alive()` to the loop; exit when false | this is what leaks for hours |
| P7 | private pid/lock files as the only record (`state/player.pid`, `daemon.lock`, `module_parent.pid`, `daemon.pid`) | keep for local use, but the **registry row is the truth** | e.g. `start_player.sh` |
| P8 | `<module>` children | already registered by the renderer | verify only |
| P9 | engine stacks (`bv_render_3d --daemon`, orchestrator, parser, `chtpm_rgb_render`, `pc_clock_daemon`) started by `button.sh` chains | owner = the board window; one funnel call at the root of the stack | the known "engine stack leak" |

## 5. Migration plan (about one week, in this order; each step ends with a commit and a harness case)

Rollout in three modes, switchable in `proc_mon_guard.pdl` (`proc_enforce=audit|warn|enforce`): **audit** (record violations only, kill nothing), **warn** (toast + log),
**enforce** (end violators). We stay in audit until the violation list is empty over a normal day of use.

**Phase 0 (done today).** The scan, the baseline file, this document, the AGENTS.md rule. Stray reaper (window-based) live. The picker manager exits with its window.

**Phase 1: registry v2 and the primitives (day 1-2).**
1. `kh_proc_registry.h` gains the PDL row, `owner_kind/owner/role/app/cmd`, the `.log.txt` sidecar, `kh_owner_alive()`, `kh_proc_reap_orphans()`; the old 5-field reader stays.
2. `kh_spawn.h` v2 (self-register, PDEATHSIG, owner watch helper), `kh_spawn.sh` v2 (exec-preserving), `proc_reg.sh` (first line of any script daemon).
3. Harness: `proc_registry_v2.pdl` (register, owner-death orphan, pid-reuse refusal with a forged start time, self-unregister on exit, double-fork daemon, crash leaves a row the reaper proves dead).
4. `spawn_lint` C op + harness case + the baseline ratchet.

**Phase 2: the guard enforces from the file (day 2).** `cpu_guard`: registry reaper pass, the audit/violations file, `proc_enforce` modes; the window-based reaper stays as backstop.

**Phase 3: Tier A, the things that actually leak today (day 3-4), one commit per app, each smoke-tested open/close x10:**
rpg-edit-hq, rpg-space, rpg-dust, rpg-pet, dsr-rpg, df-hq, cdda-hq, mcl-hq and the other petslike games (resident daemon + `lc_clock` via `kh_spawn`, owner = the board window,
`kh_owner_alive()` in `daemon_main`); `lc_clock.+x` itself (owner = the app that installed the clock); the pchq board stack (P9, `open_pchq_board.sh`, `pchq_board_projector`,
`bv_render_3d --daemon`); `ui_sound_player` (`start_player.sh`); `world_manager`; `colab_hai_manager`, `chat-hai`, `network_browser_manager`; the taskbar manager (owner = the strip, so
it ends when the strip is gone); `swatch_picker_manager` + `taskbar_settings_projector` (replace the hand-written window check with the shared helper).

**Phase 4: Tier B and C (day 4-5).** The remaining ~100 sh sites by app from the baseline (palettes, file-explorer, network, pets, events-hq, stats-hq, db-hq-pal, bookmarks,
entity-cli ...), the 29 C `setsid()` files, P2 triage of the 173 bare `&` lines (mark or convert), P3.

**Phase 5: cut over and delete (day 6).** Turn `proc_enforce` to `warn`, then `enforce` after a clean audit day; make `button.sh quit` / `kill_hq_windows.sh` thin callers of the
registry reaper; delete `livedesk_proc_list.txt` mirror readers and the name-pattern kills (or leave them report-only); update `OPERATIONAL-LANDMINES.md` #9.

**Phase 6: soak and KPIs (day 7).** Open and close every app 10 times, restart HQ 5 times, kill -9 a window, kill -9 a manager: after each, `proc_violations.txt` is empty and no house
process exists outside the registry; baseline file reaches zero rows (or only `# proc-ok` ones).

## 6. Acceptance (all measured, none claimed)
* K1: after quitting HQ, **zero** house processes and an **empty** registry; after closing any single window, the processes that window owned are gone within 10 s.
* K2: `spawn_lint` passes with a baseline of **zero** unregistered sites (allow-comments listed).
* K3: `proc_violations.txt` stays empty across a full normal day in `audit` mode.
* K4: `kill -9` of an owner (window, app daemon, manager) ends its owned processes within one watch interval; a forged-starttime row is never acted on (pid reuse).
* K5: the registry and its log respect the growth caps (section 3.5); idle cost of the owner watches measured with a browser running (house rule: footprint line in the commit).

## 7. Risks and guardrails (this is desktop-killing code if wrong)
* **Never kill by an unverified row.** Always pid + starttime; a mismatch is dropped, not acted on.
* **Audit before enforce**, always, per phase; a kill pass that would end more than N processes asks first (the guard already has a dry-run mode).
* **Never ends**: the guard and its ancestors, the splash, the user's shell, anything outside the house.
* **Startup races.** Self-registration closes the fork/register gap; the reaper ignores rows younger than 5 s and any owner that is itself still starting.
* **Do not break restart.** `button.sh restart/run/reset` must keep working through the migration; each app commit is smoke-tested open, close, restart.
* The shared core `petslike_core.c` serves 8 games: changes there go through the verb-script diff gate and a per-game harness run, one game at a time.

## 8. Open decisions for the owner (OPEN)
1. **Should a game's world keep running when its board window is closed?** Today the resident daemon and clock do (by design, for the world's sake). The standard says owner =
   the board window, so the world stops when the window closes. If you want a world that keeps running, it needs an explicit `owner_kind=app` with a visible "keep running" switch,
   and it still appears in the registry. My recommendation: owner = window by default; a per-game opt-in for always-on worlds.
2. File name and shape: `proc_registry.pdl` (PDL rows, as proposed) versus extending the existing flat text file. I recommend the new PDL file with the old one mirrored until phase 5.
3. Enforcement date: audit for how many days before `enforce`? Proposed: one clean normal day.

## 9. What changes in the rules (AGENTS.md)
A REQUIRED section was added: every process the house starts is registered in the registry file by the process or the funnel; the lint ratchet blocks new unregistered
launch sites; the baseline file is the list of what remains to convert.
