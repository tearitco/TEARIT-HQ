# Taskbar startup latency - research + fix (2026-10-06)

**Reported (owner):** at startup, before pc-hq is opened, the livedesk bottom bar "opens, loads entities super slow". Asked to research why.
Earlier related entry: `bug_bounty.md` "taskbar takes a long time to appear on launch" (2026-09-22, per-entity /proc scan, fixed). This is a different cause.

## Verdict

The wait was **not the bar and not the entities**. It was the start script, `run_khtpm_strip.sh`, spending its time *before it launched anything*:
`strip_parser_pids()` found the running strip windows by walking `/proc` in shell and forking `tr` + `sed` + `printf` + `grep` for **every process**.
Measured: **1.9 s per call** on a 349-process box. `kill_khtpm` calls it (via `khtpm_pids`) at least once even when nothing is running, and 3+ times
on a restart (initial list, poll loop, final check).

## Evidence

Method: timestamped marks (new `&.widgits/_shared-lib/kh_boot_mark.h`, appends `<epoch_ms> <who> <what>` to `#.desktop/boot_timeline.txt`; the script
resets it at the start of a boot) + an external probe polling pids and marker files.

| stage (restart on a loaded box, load ~3.3 on 8 cpus) | before | after |
|---|---|---|
| script launched -> old bar torn down | ~2.4 s | (same, inherent: manager reaps 17 entities) |
| teardown done -> script reaches "launch" (`script boot started` mark) | **~3.2 s** (the /proc scans) | **~0.1 s** |
| launch -> manager `ktb_init` done (cursword + all 17 desk entities spawned) | 0.21 s | 0.21 s |
| launch -> `strip_ui.txt` first publish | 0.23 s | 0.23 s |
| launch -> bottom bar drawn (`base.txt`) | 0.39 s | 0.39 s |
| launch -> all 17 entities registered in the bar | 0.71 s | 0.71 s |
| **script launched -> bottom bar published** | **6.04 s** | **1.09 s** |

Isolated micro-measurements (same box):
- `khtpm_pids()` old: 1893 / 1904 ms. New (`pgrep -f` with the same match): **80 ms**, same two PIDs found (header strip window + manager).
- per-entity spawn: `system("ulimit; setsid nohup X &; echo $!")` 2.5 ms; `kh_proc_register_owned` (flock+fsync) 3.1 ms. x17 = ~100 ms total: **not** the problem.
- the manager's own `ktb_init` (the part the 2026-09-22 fix targeted) is 0.21 s for 17 entities.

## Fix

`run_khtpm_strip.sh`: `strip_parser_pids()` is one `pgrep -f '^([^ ]*/)?khtpm_core_render\.\+x .*(khtpm_strip_header\.xhtpm|khtpm_strip_bottom\.xhtpm|strip_header\.chtpm|strip_bottom\.chtpm)'`.
Same predicate as before (argv[0] ends in `khtpm_core_render.+x` and the args name a strip template).

## What this does NOT cover (be honest)

- **A true cold login was not measured** - the box was already running; I measured restarts. On a cold start there is nothing to tear down, so the saved
  time is the `/proc` scan(s) (~1.9 s each, more when the machine is busier at login than it is now). If the bar is still slow at a real login, run it
  and read `#.desktop/boot_timeline.txt`: the marks are always on now. Remaining unmeasured suspects at login: `autostart.pdl` row ordering (tool-bar
  is not necessarily first), cold-cache loading of 17 `khtpm_entity` processes, and CPU contention from other autostart rows.
- The "Loading livedesk..." strip (`livedesk_splash --boot`, 2026-10-06) is the user-visible mitigation; with the scan fixed it shows for ~1-2 s.
- Other shell loops that walk `/proc` per process exist in the house (`kh` scripts, `button.sh` kill paths); this pattern is a standing footgun on a weak CPU.
  Not swept - noted only.

## Rule of thumb (add to pitfalls if it recurs)

Never walk `/proc` in a shell loop that forks per process. Use `pgrep -f` / `ps -eo` once. Cost is ~5 ms per process per fork-chain on this machine.

## Files

`&.widgits/_shared-lib/kh_boot_mark.h` (new), `_.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh`, `khtpm_taskbar_manager_main.c`, `khtpm_core_render.c`
(marks), `build_khtpm_strip.sh` (manager hash gate includes the header).
