# Would the wraith-alpha in-memory DB (house-wide) bring the CPU down? A measured report (2026-10-12)

Question (owner): "would using wraith-alpha in-mem db for the entire house db bring the cpu percentage down? I have a theory it would. Research and write up a report; we can switch it on and off and profile CPU at the end of the sprint."
Status: RESEARCH + MEASUREMENT REPORT, nothing built. Facts marked "measured" were taken today on this machine (8 cores) with the methods in section 2; "read" = from source; "estimate" = arithmetic on measured numbers. Related: `INMEM-DB-STATE-LAYER-PLAN.md` (the port plan, 2026-09-20), `03-pitfalls/hq-cpu-safety.md` sections 4 and 5.

## 1. Short answer
**Partly yes, but it is not the biggest lever.** In the busiest house processes today, about **43% of the CPU is kernel time** (`sys`: 18.1 of 42.2 percent-of-one-core across the 12 busiest house processes), and most of the kernel time that is not graphics traffic is **file polling** (hundreds to thousands of read syscalls per second per process, measured). An in-memory DB with change counters would remove most of that. Estimated gain if it works well: **about 7-9 points of one core, about 1% of the whole machine**. What it would NOT touch: the board window's **user-mode compute** (16.6 of its 20.4 points: CSS/layout/draw), the **X11 image traffic** (17 MB/s written by one board window, no shared-memory path in the renderer), and the **cost of launching processes** for events. The two earlier findings agree: the 3D raymarch is compute, not I/O (note `board-viewer-3d-perf-not-io-bound`, 0.023 s of syscalls per frame), and one events-hq page run costs ~150 ms of CPU for `sh + bash + prisc`, which no DB changes.
**The same polling saving is available without a DB** by the marker rule the house already has (CENTROID_GOLD_STD #8: act only when an append-only marker file grows; one `stat` per poll, no mtime, no hash). So the DB should be justified by what only it gives (atomic multi-key updates, no half-written files, one writer/many readers, no per-tick file opens), not by the CPU number alone.

## 2. Method (so the numbers can be repeated)
- Per-process CPU: `/proc/<pid>/stat` fields 14+15 (utime, stime) delta over a measured window with `date +%s.%N` around the sleep. (A first attempt that scanned all of `/proc` took 1.5 s per pass and one that added cutime/cstime inflated every parent: both discarded.)
- I/O volume: `/proc/<pid>/io` deltas: `syscr`/`syscw` (read/write syscall counts), `rchar`/`wchar` (bytes). These count reads and writes only, NOT open/stat/select/poll, so file polling is **under**-counted. `wchar` includes bytes written to sockets, i.e. the X11 connection.
- Not available here: `strace`, `perf`, `gdb` (ptrace blocked at paranoid level 4). gprof works (user code only). So the split of sys time between "file polling" and "X11/socket/scheduling" is **inferred, not measured**; section 6 says how to measure it properly.
- Run while a dsr-rpg board and a rpg-pet daemon were live, the machine otherwise quiet (load ~1), 10 s window.

## 3. Measured: where the house's CPU goes (percent of ONE core, 10.2 s window)
| process | cpu | user | sys | read syscalls/s | write syscalls/s | read KB/s | write KB/s |
|---|---|---|---|---|---|---|---|
| dsr-rpg board window (khtpm_core_render) | 20.4 | 16.6 | 3.7 | 236 | 228 | 2,945 | **17,206** |
| chtpm_rgb_render (2D engine) | 5.1 | 2.2 | 3.0 | 25 | 12 | 5,779 | 7,933 |
| dsr_rpg daemon | 4.4 | 2.4 | 2.1 | **896** | 26 | 2,801 | 2,805 |
| bv_render_3d --daemon | 3.3 | 1.2 | 2.2 | 583 | 15 | 266 | 3,026 |
| prisc+x, board-viewer engine session A | 1.5 | 0.1 | 1.4 | **1,476** | 53 | 2,206 | 0 |
| prisc+x, board-viewer engine session B | 1.5 | 0.1 | 1.4 | **1,886** | 71 | 2,800 | 3 |
| top-bar header window | 1.3 | 0.9 | 0.4 | 10 | 49 | 20 | 602 |
| pchq_board_projector | 1.1 | 0.1 | 1.0 | 97 | 8 | 7 | 3 |
| taskbar manager | 1.1 | 0.2 | 0.9 | 397 | 18 | 306 | 53 |
| 4 more prisc (dsr instance engine, pc-hq engine, world-manager ...) | 3.4 total | 0.4 | 3.0 | 0-656 each | 0-40 | ~185 each | ~1 |
| **12 busiest house processes, total** | **42.2 (about 5.3% of the 8-core machine)** | **24.1** | **18.1** | | | | |

Reading it:
- The **board window is 80% user time**: that is the renderer computing (profile below), not I/O.
- The window **writes 17 MB/s** with only 228 write syscalls/s: that is whole-window images (about 2.7 MB each) pushed to X, at ~6 repaints/s. `grep XShm khtpm_core_render.c` finds **0** uses; there are 24 `XPutImage/XGetImage` calls. This is graphics traffic, not state I/O; it also loads Xwayland and gnome-shell (4.8% and 4.9% when calm, 12.7% and 28.3% in the hot sample, measured with `top`).
- The **engine `prisc` loops and the daemons are the file pollers**: 1,500-1,900 read syscalls/s per prisc (1.4 points of sys each), 896/s for the dsr daemon, 583/s for the 3D daemon, 397/s for the taskbar manager. These are the processes an in-memory DB (or markers) would change. Together their sys time is about **8-9 points** of the 18.1.
- The window's `sys` (3.7) is mostly X11 socket work, not state files (236 reads/s is small).

## 4. What the profile of the window says (measured, gprof + KH_REDRAW_TRACE, see hq-cpu-safety section 5)
The 25-30% board window before today's fixes: ~3.5 repaints/s, each a full layout + style + draw (67 ms); `css_compute_style` was 32% of repaint time (14 M selector matches in 70 s); the fast game clock label forced ~0.6 reparses/s. Today's fixes (not DB-related) took it from ~28% to ~14%. Every one of these costs is **compute triggered by content changes**; a different storage layer would deliver the same content changes at the same rate.

## 5. What an in-memory DB would and would not change
Would change (the real wins):
1. **Polling reads become memory reads.** A reader checks a version counter in shared memory instead of `open/read/close` (or even `stat`) per tick. Estimate: most of the 8-9 points of sys time in the pollers (prisc loops, daemons, manager). Realistic saving 5-7 points of one core.
2. **No half-written files.** The window's reparse path debounces ("same new hash twice") because writers truncate then write; atomic multi-key updates remove that and the doubled latency (reparses are counted ~2x per change in the trace).
3. **One notification path** instead of ad-hoc markers (`strip_frame_changed.txt`, `load_changed.txt`, `frame_changed`...): fewer wakeups for idle windows. Not measured.
Would not change:
- the renderer's user-mode compute (CSS/layout/draw) and the redraw count per content change;
- X11 image traffic (needs MIT-SHM `XShmPutImage` in the renderer: a separate, bigger, well-understood fix);
- process launches (events-hq pages: ~150 ms CPU each; `lc_clock` at the fast clock 3-5% of a core through them): needs in-process event execution;
- the 3D raymarch (compute) and PNG encodes (stb_image_write 6.7 ms each);
- anything where a **shell script** must still read the state (needs mirror mode and CLI ops, which keeps file writes alive).
Costs and risks (from the plan doc, read): the verified wraith-alpha "DB" is `tpmos_share_kvp` on POSIX shm (128 text keys x 8 KB, 4 blobs x 4 MB, process-shared mutex, `file|shmem|mirror` modes, forked snapshot dumps); it was proven for one webcam streaming demo, not for the house's ~hundreds of state files; limits are small (8 KB values: the dsr `ui.txt` is ~2 KB, `scene.raw` is 1.1 MB so frames need the blob/frame slot); a crashed writer holding the mutex stalls everyone (needs robust mutex); Windows needs another shm backend; mirror mode keeps the file writes you want to remove, so the saving then comes only from the READ side.

## 6. How to test the theory (the on/off switch and the end-of-sprint profile)
Do the cheap experiment first and the DB second, with the same harness:
1. **Switch**: one environment/pdl key read by every converted program, `STATE_BACKEND=file|marker|shmem|mirror` (default `file` = today). A converted reader asks `state_changed(key)`; `file` = current behaviour, `marker` = one `stat` of an append-only marker file (house rule), `shmem` = version counter in the kvp DB. Same program, same data, three read paths: the CPU difference is then the read path alone.
2. **Targets (in order of measured payoff)**: (a) the board-viewer engine `prisc` poll loops (2 sessions x 1.4 points sys), (b) the daemons' per-tick file reads (dsr 896 reads/s, rpg-pet similar), (c) the 3D daemon (583/s), (d) the taskbar manager (397/s), (e) the window's per-tick vars hashing (the khtpm window reads and hashes every vars file each tick; reparse path).
3. **Harness (pal/C per house rule, not a new .sh)**: fixed scenario (taskbar + one dsr board + one rpg board, 60 s settle, 120 s measure), browser closed, three runs per backend alternating A-B-A-B; per process utime+stime (fields 14+15 over real elapsed time) plus `/proc/<pid>/io` syscr/syscw and wakeups (`voluntary_ctxt_switches`); report medians and spread. Negative-test it once (a deliberately busy loop must show up).
4. **Pass criteria** (set before measuring): `marker` or `shmem` lowers the summed sys time of the targets by >= 50% AND the window's repaint rate and behaviour are unchanged (screenshot + redraw trace count equal). If `marker` gets most of the saving, the DB is justified by atomicity and tooling, not CPU.
5. **Measure sys time properly** once the harness exists: replace the inference in section 3 with a count of syscalls by type (needs ptrace: either `sysctl kernel.yama.ptrace_scope=0` for the test, or a `LD_PRELOAD` shim that counts `open/stat/read` per call site).

## 7. What I could not verify (honest list)
- The split of each process's sys time between state-file polling and X11/socket/scheduling is inferred from `syscr` and bytes, not measured (no strace). `syscr` omits open/stat, so polling is under-counted; the true polling share could be higher.
- One 10 s sample on a quiet machine; the earlier hot samples (load 8-19) are dominated by a browser, the claude CLI itself (up to 70-134% of a core in `top`) and the compositor, none of which a house DB changes. The overheat crash of 2026-10-12 was not caused by the house alone.
- The ~7-9 point saving is arithmetic on today's table (what the pollers' sys time would drop to if reads became counter checks), not an experiment.
- Whether mirror-mode writes would add back what the read side saved: unmeasured.

## 8. Recommendation
1. Keep the DB direction (it is the owner's plan and has non-CPU benefits), but **do not promise a big CPU drop from it**: expect about 1% of the machine, about 15-20% of the house's own CPU, if the pollers are converted.
2. Take the same polling saving first with **markers** (no new infrastructure, house rule), behind the `STATE_BACKEND` switch, on the `marker` branch (OPEN-ITEMS 28). That also produces the baseline to judge the DB against.
3. Spend the larger CPU effort where the measurements point: MIT-SHM in the renderer (X traffic), fewer/cheaper redraws (canvas-only layout reuse already landed; per-element style cache), in-process event execution, and keeping a browser out of the measurements.
4. At the end of the sprint run section 6 for `file`, `marker`, `shmem` and put the three medians in this file.
