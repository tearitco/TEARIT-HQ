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

## 8. The always-on CPU guard (2026-10-26)

**Why now.** A fifth hard freeze (2026-10-26 22:35; the journal stops mid-run, no OOM or panic) followed a long agent session that ran renderer gates, repeated builds, a fast `/proc` poller, live boards and the 3D engine together, none of it under `cpu-quota.sh`. The old `ops/cpu_watch_daemon.sh` only logged single processes over 30 % and nothing ever started it. Owner request: when CPU rises above normal, end our own children, tell the developers to check CPU safety, and ask before killing a browser.

**What it is.** `&.hq-apps/proc-mon/ops/cpu_guard.c` (launcher `guard.sh start|stop|status`, autostart row in `$.crypts/autostart.pdl`, config `#.desktop/proc_mon_guard.pdl`, created on first run). One sample per 2 s from `/proc/stat`, `/proc/*/stat` and the die temperature (`k10temp`); no forks while idle. Measured idle cost over 60 s: 0.6 % of one core, 2 MB.

| Level | Trigger (defaults) | Action |
|---|---|---|
| WARN | busy >= 70 % for 3 samples, or die >= 85 C | incident report in `state/incidents/`, log line, toast. Kills nothing. |
| ACT | busy >= 85 % for 5 samples, or die >= 92 C | 1. if the house is >= 10 % of the machine: end every house process (script hosts first so nothing respawns, TERM, then KILL after 1.5 s); the desktop shell (taskbar and its process groups) is never touched. 2. if still hot, or the house was not the culprit: a zenity dialog asks whether to kill the biggest outside program (all pids of that name summed, e.g. Chrome). Default answer is No; no answer is No. 3. writes `state/cpu_guard_alert.txt`: **the house needs a CPU-safety review**. |

Percentages are of the whole machine. "House" = process whose executable is under the house root, or a shell running a house `.sh`. An agent's `bash -c ...` tool shell is not a house process. The guard ignores nothing else: system processes, the guard itself and the desktop shell are protected.

**Evidence for the next freeze.** `state/cpu_guard_last.txt` is rewritten every sample and fdatasync'd every third, so after a hard freeze it shows busy %, die temperature and the top process of the last seconds. `STALL` log lines mean the guard itself woke much later than asked, i.e. the machine was nearly frozen. Log: `state/cpu_guard.log` (fsync'd per line).

**For every developer and agent: at the start of a session run `sh &.hq-apps/proc-mon/guard.sh status`.** If an ALERT is waiting, read the newest `state/incidents/*ACT.txt`, find which of our processes was on top, fix that before adding load, then delete `state/cpu_guard_alert.txt`.

**Verified 2026-10-26 (scratch house, niced spinners, thresholds lowered; nothing in the live house was touched).** House spinner ended and a protected `khtpm_strip_parser` spinner survived; an outside spinner survived the house kill; answer No: 3 outside processes survived; answer Yes: all 3 ended; dialog timeout: all survived; `dry_run=1` logged `WOULD-KILL` and ended nothing; real `zenity` arguments display (exit 5 on timeout); `button.sh quit` pattern does not match the guard's command line. **Not verified:** the ACT path on the real house stacks, the real dialog being answered by a person, the guard acting during a true multi-second machine stall, and whether it gets scheduled at all when the machine is already wedged (it cannot raise its own priority without root). It reduces the chance of a freeze and records evidence; it is not a guarantee.

**Limits and open decisions.** `kill_house` and the thresholds are the owner's to tune. The die ran 77 to 87 C in scratch tests with only 3 busy loops, so `temp_act_c=92` may be close to normal on this machine under real load; watch `cpu_guard_last.txt` before lowering it. A cgroup CPU cap for the house (section 7, tier 1) still needs the root `Delegate=` drop-in and would be the stronger fix.

## 8b. The stray reaper (2026-11-02)

Owner: "we need to guard in code so no more strays get created for sure." Strays kept appearing after every window was closed: an orphaned swatch-picker manager
and settings projector, a board's daemon plus its game clock replaying for hours, a taskbar manager with no strip left running, engine stacks. Fixing each leak at
its source is still the rule (a helper that outlives its window should exit when the window is gone, as `swatch_picker_manager` now does), but nothing can
guarantee that no future helper leaks, so the always-on guard (section 8) now carries an independent backstop.

- **Rule.** When no LyfeDesk window exists anywhere (no `khtpm_core_render.+x` under this house) and nothing is starting up (`livedesk_splash`, `button.sh`,
  `livedesk-launch.sh`, `boot-splash.sh`, `build_khtpm_strip.sh`, `compile-runner.sh`, `crypt_autostart`) for `stray_grace_s` seconds, every remaining house
  process is ended, **including the taskbar leftovers** (`khtpm_taskbar_manager_main`, `khtpm_strip_parser`, `run_khtpm_strip.sh`: protected from the CPU kill, but
  strays once there is no strip). It never touches the guard and its ancestors, the splash, or anything outside the house.
- **Settings** (`#.desktop/proc_mon_guard.pdl`, re-read live): `stray_reap=1`, `stray_grace_s=120` (minimum 30: a restart takes seconds, it must not look like a
  close), `stray_scan_s=30`. Off: `stray_reap=0` or the file `#.desktop/stray_reap_off`. `dry_run=1` logs `WOULD-KILL` only.
- **Log.** `STRAY | no LyfeDesk window, N house processes left: reaping in S s ...`, then `KILL | stray, ...` per process and `STRAY | reaped N ...` in
  `&.hq-apps/proc-mon/state/cpu_guard.log`. A window opening in the grace period cancels it.
- **Registry pass (same day).** The guard also registers itself and, every `stray_scan_s`, runs `proc_reg_op reap-orphans` (ends a registered process whose owner has been gone 20 s, applying the
  declared adoption rules first) and `proc_reg_op audit` (house processes with no row in `#.desktop/proc_registry.pdl`: logged as `AUDIT | N ...` when the count changes, listed in
  `#.desktop/proc_violations.txt`, audit only). Key `proc_registry=0` turns it off. Plan: `PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md`.
- **Tested** on a scratch house with stand-in processes (copies of `sleep` under the house path): window open = nothing touched; window closed + 30 s = the daemon
  and a leftover taskbar manager ended, an outside process and the guard untouched; a running `livedesk_splash` holds the reaper; `stray_reap_off` holds it and
  removing the file lets it reap.
- **Limits, honestly.** It runs only when NO window is open: a stray that outlives one closed window while another window stays open (an engine stack of a closed
  board, say) is not caught by this rule; `mon_scan.sh` classifies those (orphan, dead-parent, stale-unreg). It cannot stop a helper being created, it bounds how
  long a stray lives to about `stray_grace_s` + `stray_scan_s` after the last window closes.

## 9. The design principle: our programs run ALONGSIDE normal user activity (owner 2026-10-27)

The house is not the only thing on the machine. The owner has a browser (Chrome, Firefox) with many tabs, a terminal, media, and
everything else a person does open while the house runs, on a weak-CPU laptop. **A feature is not done if it only works when the
house has the whole machine.** Budget for coexistence:

* **Measure our footprint next to a browser, not on an idle box.** A board engine at 34% plus a launch spike (parser, forks, renderer) is
  fine alone and is the straw that trips the guard when a browser is already at 150-300%. 2026-10-27 01:03: busy 95% (normal about 34%),
  house 25% of the machine, Chrome 163% + Firefox 95% + a web process 112%; the guard ended 19 house processes, including a board
  that had just launched. The guard did its job; the lesson is that our spikes land on top of other people's load.
* **Launch heavy things niced and one at a time** (`nice -n 15`, `ionice -c3` for multi-minute work), never two relaunches back to
  back, and check `sh '&.hq-apps/proc-mon/guard.sh' status` first. Do not stack a relaunch on top of a test run.
* **Idle must be near zero.** Poll slowly, prefer marker-file size growth to scanning, stop work when no window is open (the engine
  idle exit and `loop_guard` exist for this), and never keep a render loop running for a window nobody looks at.
* **New features get a footprint line in their commit**: what they cost idle and while active, measured with something else running.
  A feature that cannot be made cheap needs an off switch (a `tuning.pdl` row) and a default of off.
* **A guard kill is data, not an obstacle.** Read `cpu_guard_alert.txt` and the newest `incidents/` file, find what we added, cut it.
  Never excuse the load as "the browser" (that is the standing rule in `feedback-keep-own-footprint-managed`).

* **A game daemon reports its own cost** (rpg-space, rpg-dust; owner 2026-10-27): `@.apps/<app>/state/tick_stats.txt` is rewritten every
  10 s with `ticks`, `cpu_permille` (this process's CPU over the window, 1000 = one whole core) and `uptime_s`. Read it before and after a
  change. Reference: rpg-dust at rest, one open board, 18 permille (about 10 ticks a second), idle engine 0. The harness cannot run a
  daemon, so this is how a per-tick cost is caught; the 2026-10-27 bug (an unthrottled world check and a land rewrite every 1.5 s) is
  the kind of thing it shows. Other games' daemons do not report yet: add `sk_tick_stats` from rpg_space.c when you touch their tick.

Docs and handoffs should repeat this where it matters (the Grok handoff does); when you write a design, add a "footprint" paragraph.


## 10. Ask BEFORE the machine is hot: `cpu_gate` and `cpu_lint` (owner 2026-11-03)

**The incident.** 2026-11-03 00:54:54: busy 92 % (normal ~25 %), 62 C, house 42 % of the machine; the guard ended 31 house processes, the whole editor stack. The newest `&.hq-apps/proc-mon/state/incidents/` file named the house share: `lc_clock.+x` 118 % of a core (the editor's game clock: about 19 game days in 25 minutes, each day fires ~6 events at about 1.8 s of CPU each, and EVERY daemon restart replays a bounded catch-up, I restarted that daemon six times), the board session's two prisc loops 78 % and a renderer 51 %, a `cc1` compile 87 %, and Chrome 187 %. Section 9 already said "launch heavy things niced and one at a time, never two relaunches back to back": the rule existed and nothing enforced it. The guard is reactive by design; these two tools are the missing proactive half.

**`cpu_gate`** (`&.widgits/_shared-lib/ops/cpu_gate.c`, build `build_cpu_gate.sh`) - the launch-time check:
- `cpu_gate <house> check [--max-busy P] [--max-temp C] [--max-house P]` samples one second: whole-machine busy, the house's own CPU share (processes whose command line names the house), the hottest thermal zone. Defaults come from `#.desktop/proc_mon_guard.pdl` (`warn_pct`, `temp_warn_c`), house limit 30 %. Exit 0 ok, 3 too busy (the reason on stderr).
- `cpu_gate <house> run <label> [limits] [--wait S] -- <cmd...>` the same check (waiting up to S seconds for the machine to calm), then ONE heavy job at a time (flock `#.desktop/heavy.lock`, exit 4 when another holds it, naming it), then the command runs at nice 15 with idle I/O priority; its exit status is returned; one row per run goes to `#.desktop/heavy_runs.txt`.
- **Standing rule (also in AGENTS.md):** a compile, an engine start or relaunch, a screenshot/nudge loop, a TTS or ffmpeg batch is started as `cpu_gate <house> run <label> -- <cmd>`, never bare, and never two at once.
- Tested 2026-11-03 (all observed): calm check ok (36 % busy); `--max-busy 0` and `--max-house 0` refuse (rc 3, negative controls); `run` shows `nice=15 io=idle`; the child's exit status 7 passes through; a second job while one runs is refused with rc 4 and names the holder; the lock frees afterwards; under 8 loops at nice 19 (all 8 CPUs) the check refuses at 100 % busy, and `--wait 20` waits then runs after the load ends; one loop whose command line names the house reads 13 % of the 8-core machine (1/8 = 12.5 %) and trips `--max-house 1`.

**`cpu_lint`** (`ops/cpu_lint.c`, build `build_cpu_lint.sh`, baseline `08-roadmap/design-docs/CPU-LINT-BASELINE-2026-11-03.txt`) - the static pre-check, the same ratchet as `spawn_lint` (a file may only get FEWER hits; `cpu-ok: <why>` on the line allows one on purpose):
- `c-busy-loop`: `for (;;)` / `while (1)` whose own body (found by brace matching) has no blocking call and no way out. Found 0 in the house today (an earlier version with a looser rule flagged 345, almost all vendored code or loops that exit; the house sleeps through `kh_plat_sleep_ms`, which the lint knows).
- `c-tight-sleep`: `usleep(N)` with a literal N <= 5000: 52 sites in the baseline.
- `sh-heavy-no-nice`: a shell line running gcc / cc / clang / make / ffmpeg / convert / montage without `nice` or `cpu_gate` on it or the line before: 516 sites in the baseline (mostly per-project build scripts; the first to convert are the ones the desktop runs by itself: `$.crypts/button.sh` reset/build -> `build_khtpm_strip.sh`).
- `sh-loop-no-sleep`: 2 (`proc-mon/mon_scan.sh:330`, `_.START_BUTTON/button.sh:135`: to review, not yet judged).
- Negative control run: a planted spinner, a 1 ms usleep, an un-niced gcc and a sleepless `while true` were all caught; a 200 ms loop, a loop with `break`, a niced gcc and a `cpu-ok:` line were not.
- It is a heuristic: it cannot see a hot loop made of fork/exec per tick (the `bv_dispatch` per-tick spawn) or a daemon whose cost is its work (the game clock). Those are found by measuring (`state/tick_stats.txt`, `cpu_gate check` before and after), not by lint.

**Second guard and files.** `#.desktop/heavy_runs.txt` (capped at 200 rows by `cpu_gate`; `cpu_guard`'s `hygiene_sweep()` keeps the last 100 rows past 64 KB: tested, 171,780 bytes -> 100 rows, logged) and `#.desktop/heavy.lock` (one line, a flock, released when the holder exits). One row per heavy job: a few rows per hour.

**Open (own footprint, not fixed):** the editor's game clock rate (`time_rate=sec` ran ~60x real time) and its restart catch-up are the largest house cost in this incident; a restart storm (six daemon restarts in 25 minutes) is what made it bad. Candidates: `cpu_gate check` inside `open_board.sh` before it starts the engine; a lower default clock rate for the editor; the clock paused while the window is not focused. The 3D engine daemon is started by `bv_dispatch.c` with a bare double fork (unregistered, not niced).

### 10b. Follow-up the same day (2026-11-03): what was wired, measured and fixed
- **Wired:** `open_board.sh` (editor) starts with `cpu_gate check --wait 90`; `$.crypts/button.sh reset` runs its build as `cpu_gate run reset-build --wait 120`. Observed on a real `reset`: the gate row `reset-build | rc=0 | 1s | busy_before=15%` in `heavy_runs.txt`; afterwards the desktop matched the geometry recorded at the start of the session (6 tiles: cursword 0,0, door_civ 160,0, book-stack 2320,400, ava 640,1520, asa 560,1520, ninjadragon 160x160 at 800,1520; 2 strips; hotbar), audit 0 violations, the six entities self-registered as `house/entity`.
- **Found: the guard's 00:54 action had also ended the taskbar and every tile** (no `khtpm_entity` process, no tile window): the desktop had been down since then until this reset. A guard kill takes the whole house, not just the thing that was hot: when you see the guard acted, check the desktop too.
- **Clock cost, measured (not the cause I assumed):** with INT on (clock paused) `lc_clock` costs 0 %; with the clock running at `time_rate=sec` (about 1 game hour per real second) it costs about 5 % of a core (60 s sample, children included) and the whole house 1-3 % of the machine; ONE daemon restart with the clock running showed no burst (house 1-3 % for the following minute). The 118 % seen at 00:54 was NOT reproduced: the likely cause is several restarts and a compile landing in the same minutes, which `cpu_gate` now prevents. A catch-up burst after the clock was stopped for a long time was not tested.
- **Engine daemon:** `bv_dispatch.c` now registers the engine it forks (`house` / `engine` / `bv-render-3d-daemon`) and lowers its priority (`setpriority` 10; observed nice 15 when started under the gate: a process cannot lower its own niceness, so the gate's 15 stays). New sessions copy the rebuilt `bv_dispatch.+x`; the spawn baseline went 134 -> 133. The piececraft `orchestrator` (the one process left unregistered on every board launch) is now mirrored into the v2 registry by `button.sh` (registered manually on the live one: audit 0; the script change takes effect on the next launch, not yet seen in a launch).
- **UI sound player vanished after every reset (open since 2026-11-02): reproduced and fixed.** Registry showed START then END "gone" 22 s later: it exited by itself. With a stripped environment (no `XDG_RUNTIME_DIR`) it exits at once with "no sound server (Connection refused)"; the reset ran from a detached shell and the sound server may also not be ready at login. Fix: `start_player.sh` defaults `XDG_RUNTIME_DIR`, and the player retries the connection for about 30 s (resident mode only). Verified with the stripped environment that failed: stays up 20 s, plays the startup chime, a cue gives a sound burst (sink recording). NOT yet seen across a real reset.
- **Sound player: the real killer found (2026-11-03).** The first fix (env + retry) was not the cause after a reset. The player now logs fatal signals with the sender's command line; a real reset showed `ended by signal 15 from ... close_listed.sh`: the taskbar's restart pass closes every row of `close_on_restart.pdl`, and the sound-player row had no relaunch command, so it was TERMed ~28 s after autostart started it. Fix: the row now has `sh '&.widgits/ui-sounds/start_player.sh' run` as its relaunch command. Observed after a real reset: first copy ended, second started 4 s later, still running 30 s on, one process, audit 0, 6 tiles.

### 10c. Second guard kill the same night (02:49:26): our own footprint again, and what changed
- **Incident file** `20261103-024926-ACT.txt`: busy 89 % (normal ~25 %), 63 C, house 204 % of one core = 26 % of the machine; Chrome ~317 % on top. House side: `chtpm_rgb_render` 40, `khtpm_core_render` 30, `bv_render_3d` 29, `rpg_edit_hq` 25, `prisc` 25, projector 14 (percent of one core). 29 house processes were ended.
- **What I had done in that minute:** a test walk of nine palette kinds, 14 level picks back to back with no gate between them. Each pick ran `timeout 3 palettes_manager` and `timeout 2 palettes_projector`: both are polling daemons that never exit on their own, so EVERY pick burned its full 5 s of polling plus a sprite scan; plus screenshot nudges keeping the 3D engine rendering; plus the editor with pets living in real time. Also the daemon in the list was `rpg_edit_hq.+x (deleted)`: I had rebuilt the binary under a running daemon (a stale daemon keeps running the old code).
- **Steady state is NOT the problem (measured after the fix, editor up, INT off, machine calm):** top house processes 4 % and 2 % of one core, whole house 2 % of the machine. The kill came from the burst of my test, not from the idle stack.
- **Fixes in `@.apps/rpg-edit-hq/ops/pal_side.c`:** (1) `machine_busy()`: no helper is started when the machine is over 70 % in a 250 ms `/proc/stat` sample (the view stays, the sidebar says "machine busy: shown as before"; observed under load 6.4 on 8 CPUs: no helper processes started); the 1-minute load average was rejected as the measure because it stays high for minutes after a spike; (2) `run_until_written()`: helpers run niced and are stopped the moment the file they exist to write changes (plus a 150 ms grace), with a hard cap (manager 2 s, projector 1.5 s), SIGTERM then SIGKILL, always reaped; (3) no more `timeout N` wrappers. **Measured: one level pick = 0.95 s wall, 0.02-0.03 CPU seconds, no helper left running (before: about 5 s of polling each).**
- **Lint:** `cpu_lint` flagged my new un-niced `gcc` line in `build_rpg_edit_hq.sh`; all compiles there are now `nice -n 15` (baseline 570 -> 569). `spawn_lint` 133 = baseline.
- **Rules this adds (also in the test habits):** (a) a test loop that starts helper processes goes through `cpu_gate check --wait` before EACH round, not once at the start; (b) a bounded helper must wait for its OUTPUT, never for a fixed `timeout` (a polling daemon always runs the full timeout); (c) never rebuild a binary under a running daemon without restarting it; (d) a test that changes live palette state backs it up and restores it by hash (done: 5 `_active.txt` files identical by md5; 4 `_active` files, 2 layout files and 3 sprite-cache folders that the walk created were removed; October caches left alone).
- **Open:** the palette sprite caches (`&.widgits/palettes/sprites/...`, tens of MB: 18 MB per big tileset) have no enforced cap or row in RUNTIME-FILE-GROWTH-STANDARD.md (older than this work; found while cleaning up).

### 10d. "CPU is still crazy" (2026-11-03 03:00): where it really is, and what was done
- **Measured, whole house, 20-40 s samples (per-process, children included; the sampler must also match the relative commands `./system/chtpm_rgb_render` and `ops/+x/bv_render_3d.+x --daemon`: a filter on the house path alone missed both and read "calm" for the wrong reason):**
  - editor stack idle, engine up, INT on or off, no input: **about 1 % of one core in total.** Event driven, nothing runs.
  - key presses through the window at human speed: 4 % (`khtpm_core_render`). Only keys that reach the 3D ENGINE cost anything.
  - **3D camera in use (camera keys 5/s through `bv_keys`, ~28 frames/s published): `chtpm_rgb_render` 53 % + `bv_render_3d` 37 % + `khtpm_core_render` 5 % + projector 5 % + parser 4 % = about 105 % of one core.** This is the guard's trigger when a browser is also running (incident 02:49: those same processes at 40 / 29 / 30 %).
- **Why per frame:** the renderer re-composes and publishes a 640x768x4 = 1.97 MB `rgb_frame.raw` for every changed frame. Isolated benchmark (`/tmp` harness, same buffer size): memset 0.06 ms, alpha fill 0.24, overlay copy 0.52, FNV checksum 2.51, write+rename 4.52 ms. The checksum is part of a contract with other programs (the GL mirror compares it), so it cannot change alone. Live it is 25 ms per frame, with 13-17 % of a core in KERNEL time: the cost is moving 2 MB files per frame, not arithmetic.
- **Done (`&.widgits/_shared-lib/load_governor.c`, text-included by `chtpm_rgb_render.c` and `bv_render_3d.c`):** both slow the frame stream when the machine is busy: minimum gap 40 ms calm (about 25 fps), 80 ms over 50 % busy, 150 ms over 70 % (a /proc/stat sample at most every 500 ms; nothing per frame; a render requested too soon is NOT dropped, the next pass renders it). Measured with the 3D camera in use: calm, renderer 53 -> 38 % of a core, published frames 29 -> 14 per second (engine 37 -> 36 %, unchanged: its cost is not per published frame); with the machine at 73-77 % busy (5 niced loops): renderer 29 %, engine 30 %, 11 frames per second.
- **Not solved, and honestly the real fix:** while the 3D view is being driven, the house still costs about 70-75 % of a core (calm) because every frame is a 2 MB file written, then read by the window. Real reductions need a different frame path: write only the changed region or the overlay, a smaller frame, shared memory instead of files (the wraith-alpha in-memory direction already noted), or frames only at the window's refresh need. The engine daemon's own ~25 ms of CPU per frame needs its `BV_GPU_DEBUG` profile ([load | gpu | write]) first; it was not profiled.
- **Also found:** after the 02:49 kill the guard's stray reaping ended the taskbar manager (02:51:48, "no LyfeDesk window for 122 s"), so the whole desktop (taskbar, tiles) stayed down until a reset. A guard kill takes the desktop shell with it.
- **Tooling constraints met:** `perf` and `strace -p` are blocked on this machine (ptrace scope 1, perf_event_paranoid): profile by running the program as a child under `strace`/`time` in a scratch copy, or with an isolated micro-benchmark.

### 10e. Leak / orphan check (owner hunch 2026-11-03: "3D alone does not make the cpu throttle, I feel there is a leak or orphans")
- **Tested, stack up ~10 min, then a 30 s 3D burst (150 camera keys), per process:** resident memory, open file descriptors and threads are IDENTICAL before and after for every one of the 18 house processes; no process was added; nothing older than 2 h; no house process re-parented to init (the `1059` parent is the user systemd, the normal parent of `setsid` daemons); idle stack about 1 % of a core. So no memory leak, no descriptor leak, no orphan process in a healthy stack. (A stack that was killed or replaced is a different case: three dead SESSION FOLDERS of 8-10 MB each were left on disk by my relaunches, no process behind them; removed.)
- **What DOES grow, found by diffing every file before and after a burst:** `frame_history.txt` in the engine session +250 KB per 20 s (uncapped: 604 KB -> 1.23 MB in 17 min, ~45 MB an hour of appends the longer a board stays open): now capped by `loop_guard` (see RUNTIME-FILE-GROWTH-STANDARD). `#.desktop/ascii_frames`: the FOLDER itself was 80 MB (ext4 never shrinks a directory; leftover of the 2026-09-28 58,000-file episode); a listing took 20-30 ms; recreated (175 files moved, 80,012 KB -> 1,116 KB). Two receipt files per frame are created and later unlinked by the window renderer (100 new files in 20 s of 3D): create/unlink churn on the frame path, not trimmed away.
- **Conclusion on the hunch:** the idle and steady cost is NOT a leak. The 3D-in-use cost (~105 % of a core, section 10d) is per frame: a 2 MB `rgb_frame.raw` written and read per changed frame, the engine's own ~25 ms per frame, the window's per-frame ASCII mirror and receipts. Shared memory for the frame (the wraith-alpha in-memory protocol) would remove the 2 MB file round trip, but the engine daemon's cost (37 %) and the window mirror are separate and should be profiled first (`BV_GPU_DEBUG` for the engine's [load | gpu | write] split; the window's receipts/ASCII mirror could be gated to when something is listening) before committing to the larger change.

### 10f. THE ORPHAN (owner hunch confirmed, 2026-11-03 03:55): an unowned game clock kept running with every window closed
- **Found by closing the editor window and looking at what survived:** `lc_clock.+x` (the game clock) stayed alive, registered `keep | owner 0`, `time_running=1`, rate `sec`. With NO window open it cost about 6 % of a core continuously (in the helper processes it spawns for events; its own CPU 0 %) and advanced the game from D5379 to D5493 in 3 hours. The same clock was `lc_clock` 118 % of a core in the 00:54 guard incident (catch-up bursts after restarts on top). It ended only when `cpu_guard`'s stray reaper got it, minutes later.
- **Cause:** the resident game daemon (`petslike_core.c daemon_main`) sets `LC_OWNER_PID` before it starts the clock, so that clock registers `owner`. But the clock is also started by short-lived VERB processes (the `In:` toggle button, clock verbs): they never set it, `lc_clock` took the `keep` branch (`owp > 1 ? "owner" : "keep"`), and a clock started by a click outlived the window. Every pets-like board with a game clock shares this (rpg-edit-hq, rpg-space, rpg-dust, ...), because `lc_clock` and the core are shared.
- **Fix, one binary (`&.widgits/livedesk-clock/ops/lc_clock.c`, `owner_from_daemon_lock()`):** with no `LC_OWNER_PID` the clock reads who holds the game folder's resident-daemon `flock` (`<state>/daemon.lock`, holder pid from `/proc/locks` by inode) and registers `owner` of that pid. No resident daemon holding it: unchanged (`keep`, the desk clock). Tested: scratch folder with a held lock -> `lifetime=owner owner=<holder>`, row removed on exit; no lock holder -> `keep/0` (negative control). End to end in the editor: clock after launch `owner=<daemon>`; clock killed, `In:` verb started a new one -> `owner=<daemon>` (before the fix: `keep`); window closed -> daemon and clock gone within ~70 s; the 3D engine daemon lingers up to its 270 s idle timeout at 0 % CPU (by design).
- **Not changed:** how fast the clock runs (rate `sec`, about 38 game days an hour) and its catch-up cost; only who owns it. The clock still costs ~6 % of a core while an editor window is open with INT off.

### 10g. The 3D cost: engine profile, the in-memory-frame question, and a compositor that fed nobody (2026-11-03 04:40)
- **Not editor-specific:** the same 20 s camera test on rpg-dust: engine 52 %, `chtpm_rgb_render` 41 %, window 5 %, projector 4 % (editor: 37 / 53 / 5 / 5). It is the shared board-viewer pipeline.
- **Engine per-frame profile (new: `touch pieces/display/.gpu_debug` in a live session writes `pieces/display/engine_debug.log`, one line per frame, cut at 256 KB; env `BV_GPU_DEBUG` does the same; the old stderr print went to /dev/null):** 256 frames on rpg-dust: wall 70 ms (the engine alone caps at ~14 fps), CPU 42 ms, of which scene load 10 ms, GPU raymarch + readback 31.5 ms, overlay file write 16 ms. The September note "3D is not I/O bound" was measured when the raymarch was still on the CPU; with the GPU path landed the I/O is a third of what is left.
- **In-memory frame transport (the wraith-alpha webcam path):** read. The webcam project writes frames through `tpmos_live_frame_cache.c` (one named POSIX-shm slot `/tpmos_live_frame_cache_v1`, 8 MB, mutex, generation counter; machine-global name, so per-session names would be needed). Measured what the medium is worth before building anything: the SAME test with the session's `pieces/display` folder on tmpfs (a symlink into `/dev/shm`, undone afterwards): engine write phase 16.1 -> 6.8 ms, engine CPU per frame 42 -> 32 ms, wall 70 -> 60 ms; the whole stack about 15-20 % cheaper. tmpfs IS shared memory, so a shm protocol would buy about that, no more, because the GPU phase and the scene load dominate. (Owner decision 2026-10-12 stands: marker lane for state; shared memory demo/A-B only.) Open option if that 15-20 % is wanted: put the display folder on tmpfs per session with a cleanup trap and a `cpu_guard` sweep of `/dev/shm/lyfedesk-*`.
- **The finding that mattered:** `pchq_board_projector.c` routes the khtpm window's canvas to `rgb_frame_3d_overlay.raw` (3D) or `rgb_frame_2d.raw` (2D) and says so in a comment ("replaces the old rgb_frame.raw / chtpm_rgb_render text-chrome hack"). Only GL-mirror and screen-recorder tools read `rgb_frame.raw`, and no mirror runs in a khtpm board (`NO_GL=1`). Yet every board's launcher still started `chtpm_rgb_render`, which composed and rewrote a 2 MB `rgb_frame.raw` about 28 times a second for nobody. Verified empirically (not from the comment): ended the session's compositor, 100 camera keys, the 886x640 canvas still changed over its whole area.
- **Fixed in `&.widgits/board-viewer/button.sh`:** the compositor is not started when `PCHQ_ENGINE_MODE=1` and `NO_GL` is set (what the shared piececraft launcher passes to every pets-like board); `FORCE_RGB_COMPOSITOR=1` starts it anyway. Measured on a relaunched rpg-dust board, same keys: no compositor process, 3D canvas updating, **whole house 106 % -> 72 % of one core during camera use** (renderer line gone; engine 53 %, window 5 %, projector 4 %, parser 3 %). Applies to every pets-like board using the shared launcher (rpg-edit-hq, rpg-space, rpg-dust, rpg-pet, ...); the editor itself was not re-measured after this change.
- **Remaining, in order of size:** engine GPU phase 31 ms (readback sync: not profiled further), scene load 10 ms, overlay write 7-16 ms (tmpfs option above), the scene-receipt FNV over 2.3 MB per frame (a few ms, serial), the window ~5 %.
