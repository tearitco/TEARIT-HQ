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
