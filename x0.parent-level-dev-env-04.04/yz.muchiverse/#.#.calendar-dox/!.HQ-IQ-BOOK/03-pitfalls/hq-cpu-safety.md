# hq-cpu-safety.md — how house CPU-throttling bugs get found, how to catch them with proc-mon, and how to avoid writing new ones

Written 2026-09-28/29 after a real, dated sequence of CPU-throttling
incidents on this same live house, across two separate subsystems in
one night. Not a hypothetical checklist — every technique below was
either used live tonight or is what's actually running right now.

---

## 1. How the kilo agent found the taskbar-manager throttling (real methodology, not guesswork)

Direct live report: "im getting throttling again after we had fixed it
for a few hours" — arrived with `/home/no/Desktop/cpu_loop_analysis.txt`,
a second, independent analysis (the kilo agent) that correctly
identified this was a **separate, new** throttling source from the
`world_manager_tick.c` bug fixed earlier the same night, not a
recurrence of it.

The methodology, in order:

1. **Live process CPU measurement** — `ps`/`top`-shaped, catching
   `khtpm_taskbar_manager_main.+x` and `khtpm_core_render.+x` at ~12%
   and ~5% respectively. This is the *symptom* capture, not the
   diagnosis — it tells you *something* is hot, not *why*.
2. **Direct source read of the hot process's own tick loop** — the
   real diagnostic step. Kilo read `khtpm_taskbar_manager_main.c`'s
   `main()` and found `ktb_reload(&st)` called unconditionally on
   every iteration of `while (g_running)`, at up to
   `POLL_INTERVAL_ACTIVE_USEC` (16667µs, ~60Hz).
3. **Enumerated what the hot function actually does** — `ktb_reload()`
   fans out into `load_tabs()` (flock + parse + per-line PID verify),
   `sync_tab_claims()`/`sync_strip_claims()` (claims-file I/O),
   `load_shortcuts()`/`load_theme()`, `ktb_load_zorder_mode()`, and
   `ktb_merge_hq_windows()` (an `opendir`/`readdir` scan of
   `#.desktop/` plus a liveness check per HQ window entry found) — six
   real file-touching operations, all re-run up to 60 times a second
   regardless of whether anything on disk had actually changed.
4. **Named the exact fix shape** — throttle the expensive call, not
   the tick's own sleep interval (the same shape this house already
   used for `world_manager_tick.c` the same night — see §3).

**One real correction, made during verification, not a knock on the
methodology**: the analysis claimed the 57,913 files then sitting in
`#.desktop/ascii_frames/` were "exacerbating" `ktb_merge_hq_windows()`'s
directory scan. Checking the actual code showed `opendir()` there
targets `#.desktop/` itself, non-recursively — `ascii_frames/` is a
single directory *entry* in that scan, never descended into. The real
entry count `readdir()` walks is `#.desktop/` itself (2,306 entries at
the time), not 58,000. The throttle fix is correct and sufficient
regardless of this correction — it's included here as a reminder that
**a plausible-sounding claim in an analysis still needs a direct code
check before it's treated as fact**, same standard this house holds
its own passes to.

---

## 2. How proc-mon (this house's own tool) can catch this class of bug

The single most important fact, established during the *first*
CPU-throttling incident tonight (`world_manager_tick.c`) and directly
relevant to why the taskbar-manager bug wasn't caught by `top`/`ps`
either: **default CPU% only reads a process's own `utime+stime`
(`/proc/<pid>/stat` fields 14-15). It misses two real things:**

- **A forked-and-reaped child's cost** — only shows up in the
  *parent's* `cutime`/`cstime` (fields 16-17), and only after the
  child is reaped. This was the entire reason `world_manager_tick.c`'s
  bug looked like "~1% CPU, not dangerous" on a first pass.
- **A long-lived process's own decayed average** — `ps`'s `%cpu`
  column is a lifetime average, not a live rate; a process that spiked
  hard for the last 5 seconds and was idle for the hour before that
  can still show a small, unremarkable-looking number.

`&.hq-apps/proc-mon/ops/cpu_watch_daemon.sh` (built the same night as
the `world_manager_tick.c` fix, still running, toggleable via
`#.desktop/proc_mon_cpu_watch.pdl`) exists specifically to close this
gap: it samples `utime+stime+cutime+cstime` (all four fields, the
same fields `top` ignores by default) per matched PID every 5s and
logs real spikes (>30% of a core) to
`&.hq-apps/proc-mon/state/cpu_spikes.log`. Point it at a live house and
it would have caught **both** incidents tonight, from the same
mechanism, with no code-reading required — the log is the fast way to
answer "did we get it" without re-investigating from scratch (this
house's own stated design goal for it).

**Its current, honest limitation**: it only watches PIDs matching its
own `PATTERNS` regex (a hardcoded list of known house binary names,
`mon_scan.sh`'s own pattern set duplicated for the daemon's own reasons
— see that file's header comment). A genuinely new, unlisted binary
name won't be sampled until it's added to that list. This is a real,
acceptable trade-off (matching every process on the machine would be
its own cost), not an oversight — but it means **adding a new house
binary that's meant to run persistently is also a real reason to add
it to `cpu_watch_daemon.sh`'s own pattern list**, not just to
`mon_scan.sh`'s.

---

## 3. Current techniques in use to avoid writing new ones

Two real, distinct throttle shapes are already live in this house,
chosen based on the process's own lifetime — using the wrong one for
the wrong shape is itself a mistake worth naming:

### 3a. Marker-file gate (for a process that re-execs fresh every tick)

`world_manager_tick.c`'s own fix: the binary itself has no persistent
memory between invocations (it's `exec`'d fresh by `world_manager.pal`
every tick), so an in-memory `static` variable can't survive to the
next tick. The gate has to live on disk:

```c
struct stat mst;
time_t now = time(NULL);
int due = (stat(marker_path, &mst) != 0) || (now - mst.st_mtime >= MIN_INTERVAL_SEC);
if (due) {
    FILE *mf = fopen(marker_path, "w"); if (mf) fclose(mf);
    /* ... do the expensive thing ... */
}
```

Self-healing (no counter to get out of sync — a missing or stale
marker file just means "due now"), no change needed to whatever it's
gating, no change to that data's own consumers.

### 3b. In-memory CLOCK_MONOTONIC gate (for a long-lived process)

The taskbar manager's own fix (this doc's own §1 incident), and the
same shape `ktb_self_heal_active_desk_registry()` already used for its
own 10s gate before this fix existed:

```c
static struct timespec s_last;
struct timespec now;
clock_gettime(CLOCK_MONOTONIC, &now);
double elapsed_ms = /* ... */;
if (elapsed_ms >= MIN_INTERVAL_MS) {
    s_last = now;
    /* ... do the expensive thing ... */
}
```

No disk I/O to implement the gate itself (the process is long-lived,
so a plain `static` survives across ticks) — strictly cheaper than the
marker-file shape when it's available, but only correct when the
process genuinely persists across the calls being throttled.

### 3c. The actual review discipline, not just the code shape

The pattern behind BOTH incidents tonight was identical even though
the fix shape differed: **an expensive, multi-file-I/O function called
unconditionally inside a tick loop, with no gate at all**, not a
misconfigured interval. The real standing rule this suggests, worth
holding future work to:

- **Any function added inside a `while (g_running)`/tick loop that
  touches more than one file, or scans a directory, or takes a lock,
  needs an explicit answer to "how often does this actually need to
  run" at the time it's added** — not as a follow-up once someone
  notices the fan is loud. Neither incident tonight was a new bug from
  a recent change; both were long-standing per-tick calls that had
  simply never been asked this question.
- **When throttling an existing per-tick call, only change the
  interval, not what happens when it fires** — both fixes tonight
  left the gated function's own internal logic completely untouched.
  `ktb_reload()` in particular has its own documented history of THREE
  real incidents from a past agent restructuring its *internal*
  self-heal logic (see that function's own header comment) — the
  lesson generalizes: pacing and behavior are separable, and touching
  behavior while you meant to touch pacing is how a simple throttle
  fix turns into a new bug.
- **Measure the real fix, don't assume it worked.** Both fixes tonight
  were verified with the same real technique — `/proc/<pid>/stat`'s
  four CPU-time fields, delta over a real wall-clock window, before
  and after — not "it compiles" or "the number sounds better now."

---

## 4. A game daemon must not drive a viewer that is not there (rpg-pet, 2026-10-11)

Found by the owner stopping work for "high CPU, maybe an orphan". Measured with all four `/proc/<pid>/stat` fields (children included), 30 s windows:

- The rpg-pet daemon nudged the pc-hq board engine (a redraw key into its relay) every time its scene changed, and re-rendered its own scene / ui files at every pet step, with **no window open**. The leaked engine stack (`chtpm_rgb_render` 7% + `prisc+x` 3-4%) kept re-rendering for hours; the daemon itself was 12% (7% own, 5% in reaped children). With no viewer the whole stack is now daemon 3% + lc_clock 3%.
- Per-100 ms-tick costs that never needed to run that often (profile by a timed copy of the daemon, because `perf` is blocked here at paranoid level 4): `load_data()` re-parsed static catalog / party / rooms / door pages every tick (now every 2 s); every `flag_on()` re-read two files (now a 150 ms in-daemon cache, dropped by `set_flag`); `replay()` re-parsed the append-only house ledger per pet step (now only when its size changes); `follow_pchq()` re-compared three exported rooms per tick (now at most every 400 ms and only while a board window exists); a pet stepping in a room nobody watches repainted the scene (now at most every 1.5 s).
- **Child launches are the hidden cost**: an events-hq page run (`sh` + `bash play_event.sh` + `prisc`) costs about 150 ms of CPU. A greeting page per meeting was ~1 launch a second; now at most one per 6 s and the C text (identical) is used in between. Plain `top` shows none of it; `cutime`/`cstime` do.
- **Rule that fixed the class**: before rendering or exporting for a viewer, ask whether one exists. `viewer_kinds()` in `rpg_pet.c` scans `/proc` every 2 s for a `khtpm_core_render` window of `pchq-board.xhtpm` / `rpg-pet.xhtpm`; a window opening forces one immediate render. The same applies to verbs: every event page that calls a verb used to render a whole 660x430 scene.
- Testing traps hit while measuring: `pgrep -f` / `pkill -f` match the tool shell itself (killed it once, leaving instrumented copies running: use `pgrep -x`, or verify with the count); a test that greps for a process name must not put the name in its own command line (build it at runtime).
- Not an rpg-pet bug but worth knowing: at the fast clock (`sec`, a 24 s game day) the five `time.pdl` rows cost `lc_clock` about 3-5% of a core through the event processes it launches; at `min` it is negligible.
- Window side (khtpm_core_render, 2026-10-11, measured with all four stat fields, 40 s, rpg-pet board live mode): 36% -> 24% of a core; INT-on idle 9% -> 2%. Three per-tick costs: a canvas window forced a full layout+draw+readback every 1 s (now 5 s); a canvas repaint re-read and per-pixel-converted an unchanged raw frame (now cached by inode+size+mtime); `hq_idle_tick` re-parsed every vars file about 18x/s (now one stat each, reload on change) and ran a SECOND full redraw after every ui-file reparse (now only for the dock or when the size moved). Profile with an instrumented copy compiled to /tmp (gdb/perf are blocked by ptrace_scope): per-phase layout 12 ms, draw 45 ms, present 10 ms per redraw. What remains in live mode is the repaint rate itself (~3 frame changes/s x ~67 ms).

---

## 5. A board window at 25-30% of a core: how it was found and what fixed it (dsr-rpg board, 2026-10-12)

Measured, not guessed. The order that worked, and the traps:

1. **Count first, per process, with real elapsed time.** `/proc/<pid>/stat` utime+stime (fields 14+15) over a measured window; the first attempt (a scan of every `/proc` entry) took 1.5 s per pass and a first reading that included `cutime/cstime` (reaped children) inflated every parent. Use a targeted pid list and `date +%s.%N` around the sleep. The top bar's `%` is the WHOLE machine (`/proc/stat`), not a window; a browser can make it read 90%.
2. **user vs system split + wakeups** (`/proc/<pid>/status` voluntary_ctxt_switches): the dsr board window was 24.5% user, 3.6% system, 131 wakeups/s (the 16 ms canvas poll tick).
3. **gprof works here, perf/gdb/strace do not** (ptrace). Build a copy of `khtpm_core_render.c` with `-pg -O1 -fno-inline` plus a 4-line shim that calls `exit(0)` on SIGTERM (`signal()` in a `__attribute__((constructor))`), or gmon.out is never written. It profiles user code only (libc/Xlib time is not attributed), so compare the call counts as much as the percentages.
4. **`KH_REDRAW_TRACE=1`** makes `redraw()` log `[REDRAW n] t=... ra=<return address>` to stderr. Build a **`-g -no-pie`** copy so `addr2line -f -e <bin> <ra>` names the caller. Result: 166 redraws in 50 s; 100 from `hq_idle_tick` after a vars/template reparse (line `assign_nav_and_layout(); redraw();`), 60 from the canvas-dirty flag in the event loop.
5. **Find which vars file changes**: copy each file named in the template's `vars=` once a second and diff. `clock_ui.txt` `time_label` changed 12 times in 20 s because the game clock runs at `sec` rate; every change reparsed and redrew the whole window.
6. **gprof call counts**: `css_compute_style` was called 59,059 times and `selector_tier_match` 14.2 million times in 70 s (32% of repaint time): the style code re-parsed every selector string (copy, strstr, strtok) once per rule per tier (3 tiers).

Fixes and results (dsr board window, 30 s windows, all measured):
- clock label written at most once per 5 s when only `time_label` differs (`clock_ui_write()` in both daemons): label changes 12 -> 4 per 20 s, window ~28% -> 17%.
- `css_compute_style_ex` matches each rule once with a cheap reject on the last selector segment, merges tier by tier in the same order: 17% -> 14%. **Differential test** (old vs new over 32,000 random elements and 4 real stylesheets: identical hash) and a **mutant check** (tier order reversed gives a different hash) so the test is known to be able to fail.
- layout reuse: `g_layout_fresh` lets a canvas-only repaint (nothing else dirty this pass, not the 5 s safety repaint, not a reparse) and the redraw right after a reparse skip `assign_nav_and_layout()`. Built and committed; its own CPU gain was NOT cleanly measured (a browser was loading the machine during the runs), so treat the number as unproven.
- The CPU tabs added to the boards the same day (`<pid> %<cpu>`) were A/B tested by removing them from the template live: 16% with, 18% without = not the cause.

Top-bar load label rules learned the hard way:
- Republishing the whole dock strip when the CPU figure moves (every ~2 s) made the top bar flicker (`strip_frame_changed.txt` grew 3 per 6 s). The dock is republished only when the warning flips (with hysteresis: stays on until 10 points / 1 window / 5 degrees below the limit); its `%` rides the once-a-minute datetime publish (so after a manager restart it can read `%0` for up to a minute); game windows follow `load_changed.txt` marker growth (<= one change per 10 s); dock windows are exempt from that marker.
- The red warning colour needs the header window restarted (css is read at start); `.dock-cell.dock-warn` is the rule.
- Testing traps hit: `pgrep -f` matched and killed my own tool shell again (use exact `comm` and pids); a profiled copy of a window left running keeps burning CPU; killing the window's keep-alive 3D daemon does not respawn it (relaunch the board).

Still open: ~1 canvas repaint per second when characters move; the 16 ms canvas poll tick (131 wakeups/s) is a polling loop where a marker-growth wait would do (the `marker` branch exists, nothing converted yet).

---

## 6. 2026-10-17: idle-cost gates, a duplicate-daemon hole, and a clean way to tell "ours" from "not ours"

Owner report: the machine was throttling again. Measured before touching anything (all `/proc/<pid>/stat` utime+stime, windowed, plus rss and fd count). Result: **our house processes were calm** (renderers 2-3% idle, taskbar manager under 1%, the df-hq board's 3D session under 2%). The sustained hog was a *foreign* process (another agent's CLI at ~120% of a core), with the browser and the shell/terminal behind it. Earlier 90% readings were my own scratch windows while I edited and relaunched them. **Always rank the whole machine first (`XO/top_cpu.sh` shape: sample every pid twice, 20 s apart, sort) before assuming it is the house.** What the check did find: two resident `df_hq` daemons on one game folder (see 6e).

### 6a. Cost is mostly syscalls, not CPU math

gprof of the df-hq daemon showed user time ~0.15% of a core. The measured idle cost (1.0% of a core, 555 file reads a second) was `open/read/stat` and a `/proc` scan. So the gates that paid off were all "do not touch the disk or `/proc` unless something changed":

- **Marker gate (preferred, house rule):** a file that only grows (`ent_changed.txt`, `world_epoch.txt`, `blocks_changed.txt`, `livedesk_proc_list.txt`). Remember its size; re-read the real data only when the size grew. Size is exact; `st_mtime` is not (seconds-only on some paths, racy within a second). 1.05% -> 0.57-0.70% for the df-hq daemon, 555 -> 193 reads/s.
- **Net under every gate:** a slow safety re-read (2 s for the pet files, 30 s for the viewer scan, 5 s full rebuild in the projector) so a missed marker can never freeze the picture for long.
- **When the input is a rewritten key=value file (no append-only marker exists)**, gate on (size, mtime in nanoseconds) taken *before* the read, and keep a ~100 ms racy-timestamp guard: a write inside the same timestamp tick as your read is treated as "changed" again next pass. Disclose this when you use it, it deviates from the marker rule (projector 0.93% -> 0.20%, 3D daemon 0.47% -> 0.33%).
- **Negative-test every gate:** rewrite the input with the same size and different content and prove the consumer still updates; run a stand-in viewer (`exec -a "khtpm_core_render fake <x>.xhtpm" sleep N`) to prove the "no viewer" shortcut turns off when a window opens.

### 6b. Throttle the expensive call, never the tick, and never write what has not changed

- A label that ticks with the game clock (`time_label`) re-lays-out and repaints the whole window on every change; write it at most once per 5 s unless something other than the label changed (`clock_ui_write`).
- `write_if_changed()` for every derived file (ui.txt, events.txt, map.txt): an identical rewrite still wakes every watcher.
- A re-export that is expensive (the glyph/3D book) runs on a **hash of the thing it exports** (the block table), the first pass only learns the hash, idle passes do nothing.
- Chat/log files are trimmed (`chat_trim`); the S1 ledger has a 32 MB read cap. An append-only file with no cap is a slow leak (bug_bounty item 7).

### 6c. How to measure so the number is real

- `/proc/<pid>/stat` fields 14+15 (add 16+17 for children), over a measured wall-clock window of 20-40 s, old and new binary **alternated** on the same load. One 5 s reading is noise; the first reading after a relaunch or a reparse is a burst.
- Also record `VmRSS` and the fd count at both ends: a flat rss/fd over 90 s is the leak check (a leak shows as growth, not as CPU).
- `perf`/`gdb` are blocked here (paranoid 4); `gprof` works on a `-pg` copy, `strace` works only on a child you start.
- **Per-session binary copies:** each board engine session runs its own copy of `bv_render_3d` etc.; a rebuilt binary only reaches a new session (memory: reference-board-engine-session-binary-copies).
- Never report a number you did not observe; say what window it was.

### 6d. Do not drive a viewer that is not there (generalised)

Every daemon that renders or exports for a window asks first whether the window exists (`viewer_kinds()`: scan `/proc` cmdlines, but **only every 2-30 s and only when `livedesk_proc_list.txt` changed**), and a window that is minimized or closed gets nothing. A `/proc` scan every pass is itself the cost.

### 6e. One resident daemon per game folder (found today, fixed in all eight)

`df_hq`, `cdda_hq`, `mcl_hq`, `rpg_pet`, `civ_rpgce`, `dsr_rpg`, `gta_hq`, `tcg_rpgce` are started from two places (the board launcher and the standalone window's `<module>`). Nothing stopped both: two daemons ticked the same `state/`, doubled the work, and raced on every write. **Rule: every resident loop takes `flock(state/daemon.lock, LOCK_EX|LOCK_NB)` first and exits if it cannot** (close-on-exec so a leftover child never holds it; the kernel drops it on death so a crash needs no cleanup). Test: second instance exits at once, a new one starts after the first dies (`XO/test_lock.sh`). A new long-running binary gets this in its first ten lines.

### 6f. Leaks and strays to check after any test session

- `lc_clock` is left behind when a game daemon dies (bug_bounty item 4): scan `/proc/*/exe` and kill the ones whose cwd/args name your scratch folder.
- Engine stacks (`khtpm_entity` x6, 3D daemon, projector) of a scratch board keep running after the window closes. Close them by pid, then `rm` the scratch folder.
- **Never `pkill -f` / `pgrep -f`** with a pattern that also appears in your own command line (the tool shell dies, exit 144, instrumented copies keep running). Put the scan in a script file whose own command line does not contain the pattern.
- A scratch copy that rewrites absolute paths (`sed s#@.apps/x/#@.apps/x-test/#`) must be checked before running: a copy once flipped the LIVE switch through an absolute path.

- **A test script must only touch processes it started (2026-10-17 incident).** An A/B script that picked "the process whose exe is the live renderer" and then `kill`ed it matched the OWNER'S live windows (the df-hq window and the df-hq board) and closed them; two of my scratch windows then sat on top of the same screen position, so the remaining buttons looked dead. Rules: take the pid from `$!` or match on the exact scratch path in the command line AND the exe; print what was matched before measuring; never `kill` a pid you did not just verify belongs to the test; put scratch windows away from the live ones; if you did close a live window, relaunch it with its own launcher at once and say so.

### 6g. Best-practices checklist (use when adding or reviewing code)

1. Any call inside a tick loop that touches more than one file, scans a directory/`/proc`, takes a lock or launches a child needs a written answer to "how often must this really run". Default: only when a marker grew, plus a slow safety net.
2. Gate with an append-only marker size, not `st_mtime`. If you must use (size, mtime ns), add the racy guard and say so.
3. Do not write a file whose content did not change; do not repaint for a label that changed alone.
4. No work for an absent viewer; check for it rarely and only when the proc ledger changed.
5. One resident daemon per folder (flock). Close-on-exec for the lock.
6. Profile before you gate: syscalls (count reads with `/proc/<pid>/io` syscr) beat guessing at math.
7. Verify with a windowed `/proc` sample, both fields, old vs new alternated, plus rss/fd flat over 90 s.
8. Negative-test the gate (same size, new content; viewer appears; marker missed).
9. Heavy background work (builds, TTS, ffmpeg): `nice -n 15 ionice -c3` (`XO/guard.sh`); the taskbar must stay responsive.
10. Rank the whole machine before blaming the house; a foreign process (another agent, the browser) can be the hog.
11. Clean up after every test: stray `lc_clock`, scratch board stacks, scratch folders; no `pkill -f`.
12. Record the measured before/after in the commit message, with the window length.

## 7. Hard freezes and the CPU quota guard (2026-10-17)

**What happened.** The machine (Surface Laptop 3, Ryzen 7, 8 threads) froze hard four times: 2026-10-15 twice, 2026-10-16 about 20:09, 2026-10-17 about 00:40. The journal of each boot simply stops mid-activity: no kernel panic, no out-of-memory kill, no disk or NVMe error, no machine-check line. The kernel logs `ACPI thermal: [Firmware Bug]: No valid trip points`, so Linux is not enforcing thermal limits on this firmware, and the `linux-surface` kernel is not installed (stock 6.8.0-138-generic). Temperatures read normal when idle (36 to 43 C), so a live catch was not possible. Cause is **not proven**; the likeliest is sustained all-core load (test suites, builds, parallel agents) meeting firmware thermal/power handling.

**What we do about it.** A cap on how much CPU our own heavy jobs may take, which is cheap and removes the likeliest trigger.

| Piece | Where | What |
|---|---|---|
| `cpu-quota.sh <cmd...>` | `$.crypts/cpu-quota.sh` | the one entry point; env `QUOTA_CPU` (percent of one core, default 300 of 800), `QUOTA_MEM` (default 3G), `QUOTA_NICE` (15). Prints the tier it used on stderr. Passes the command's exit status through. |
| tier 1 | systemd user scope with `CPUQuota=` | a kernel-enforced cap. Needs the cpu controller delegated to user services. **Not delegated here** (measured: `user@1000.service` has only `memory pids`), so the script detects this and skips it. |
| tier 2 | `$.crypts/ops/cpu_cap.c` -> `ops/+x/cpu_cap.+x` | our own limiter: runs the job in its own process group and, every 100 ms, compares the group's CPU time (read from /proc, children included) with `cap x elapsed`; when ahead it SIGSTOPs the whole group until the budget catches up, then SIGCONTs. Only ever signals its own child group. Memory is capped through the user scope (`MemoryMax`) when one is available. |
| tier 3 | nice + ionice only | used if neither of the above exists. No cap. |

**Measured (2026-10-17, 4 to 8 busy loops, other agents running, so noise is real).** Uncapped: 340 to 516 %. Cap 300: 288 to 305 %. Cap 200: about 330 % (4 loops, overshoot). Cap 100: 150 to 200 %. So it holds a cap of 300 well, and is loose (up to about 1.8x) for small caps because the group runs a burst after every release. It is a ceiling against pinning all 8 threads, not a precise governor. `cpulimit` 2.4 (installed) was measured too and is NOT usable: it throttles only the top process (732 % for a job whose children did the work), and `timeout` puts its child in a new process group, which also escapes cpu_cap (use `timeout --foreground`).

**Use it.** `$.crypts/cpu-quota.sh gcc ...`, `QUOTA_CPU=200 $.crypts/cpu-quota.sh sh XO/run_all.sh`. All multi-minute suites, builds, TTS and ffmpeg runs, and every subagent brief, should use it. Build the limiter once: `gcc -O2 -Wall -Wextra -o '$.crypts/ops/+x/cpu_cap.+x' '$.crypts/ops/cpu_cap.c'`.

**The real fix needs root (owner action, one time).** Delegate the cpu controller so tier 1 works: `sudo mkdir -p /etc/systemd/system/user@.service.d && printf '[Service]\nDelegate=cpu cpuset io memory pids\n' | sudo tee /etc/systemd/system/user@.service.d/delegate.conf`, then log out and in. Also worth doing, as freeze mitigations: install the `linux-surface` kernel and check for a firmware/BIOS update for the Laptop 3. Both need sudo and are not done.

**Rules.** Never run more than two CPU-heavy things at once; bound every test loop; a freeze that leaves no log is a hardware/firmware suspect first, our code second, but our code is the part we can fix.
