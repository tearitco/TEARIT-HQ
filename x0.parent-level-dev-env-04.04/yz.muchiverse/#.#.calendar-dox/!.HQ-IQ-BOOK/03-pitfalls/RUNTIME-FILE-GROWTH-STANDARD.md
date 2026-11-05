# Runtime file growth standard (REQUIRED, since 2026-11-02)

Anything the house writes while it runs (frames, logs, ledgers, relays, snapshots, histories) must have a **cap that is enforced on disk**, a
**second guard that does not depend on the first**, and a **place where the owner can see it**. Written after a real incident.

## The incident (labels: RAN = observed this session)

- RAN: `#.desktop/ascii_frames` held **1,682,402 snapshot files, 7.6 GB on disk**; the disk was 94% full (17 GB free). Git ignored the folder, so
  nothing showed in `git status`.
- RAN cause: `kh_write_frame_receipt()` (`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`) caps `index.txt` at 50 lines and unlinks the snapshot
  files of the lines it drops. It read the index into a 512 KB buffer and **silently skipped the read when the file was bigger**. Once the index passed
  the buffer (about 2026-10-09) the trim never ran again: the cap existed on paper and in the comment, never on disk. `index.txt` reached 58 MB / 840,000
  lines. The 2026-09-28 fix for the same folder (58,000 files) had the same shape: the cap was written down, not enforced.
- RAN cost: about 67,000 new files on 2026-11-02 alone; two dozen windows each write a pair per frame change.

## The rules

1. **A cap is enforced where the files are made, and what it cannot read must fail toward trimming, not toward "do nothing".** An oversize input
   (index, ledger, history) is read from its tail, never skipped. "File bigger than my buffer" is a case to handle, not an early return.
2. **Every growing folder has an independent second guard.** The always-on `cpu_guard.+x` (`&.hq-apps/proc-mon/ops/cpu_guard.c`, started at boot, survives
   `quit`/`reset`) runs `hygiene_sweep()` every 2 minutes:
   - `#.desktop/ascii_frames`: over **3000** entries, deletes snapshot files (`<pid>.<seq>.objects.pdl` / `.receipt.pdl`, exact shape only) older than
     **900 s**, at most **20,000** per pass, and logs a `HYGIENE` line in `&.hq-apps/proc-mon/state/cpu_guard.log`.
   - disk: under **5 GB** free it logs `HYGIENE | disk nearly full` and writes `&.hq-apps/proc-mon/state/disk_alert.txt` (it reads like the CPU alert: delete
     it when handled).
   It never deletes outside this house's `#.desktop/ascii_frames` and never touches a name that is not an exact snapshot name. Tested on a scratch
   house: 4,000 old snapshots removed, 10 fresh ones, `index.txt` and a history file kept.
3. **A new writer adds its folder to the sweep** (or says in its commit why it cannot grow). Add the folder, the cap, the age rule and the name shape to
   `hygiene_sweep()`; add a row to the table below.
4. **A commit that adds a writer states its cost**: files or bytes per hour at idle and while in use, measured, with the cap it hits (same rule as the
   CPU footprint line in `AGENTS.md`).
5. **Gitignored does not mean harmless.** Ignored runtime folders are exactly where a leak hides. Check sizes after any session that ran many windows:
   `du -sh <house>/#.desktop/* | sort -h | tail`, and `ls <folder> | wc -l`.
6. **Negative-test the cap once.** Make the folder 10x over the cap in a scratch house and watch it shrink; a guard never seen shrinking something is
   only a belief.

## Caps in force

| folder / file | writer | cap | enforced by | second guard |
|---|---|---|---|---|
| `#.desktop/ascii_frames/index.txt` + `<pid>.<seq>.{objects,receipt}.pdl` | every `khtpm_core_render` window | 50 index lines, their files | renderer trim (reads the tail if the index is oversize, fixed 2026-11-02) | `cpu_guard` `hygiene_sweep()`: 3000 entries / 900 s |
| `<app>/state/clock_root/#.desktop/clocks/schedule_ledger.txt` (the clock's recurrence cursor) | `lc_clock.+x` (one per game clock) | 256 KB, then compacted to the last row per schedule id (that row is the cursor); env `LC_CLOCK_LEDGER_MAX_BYTES` | `ledger_append()` in `&.widgits/livedesk-clock/ops/lc_clock.c` (2026-11-02) | none yet (it was 1.1 MB, 28,866 rows, after the incident below) |
| `@.apps/rpg-edit-hq/state/sides_history.pdl` | `ops/pal_side.c` (one row per sidebar change) | 8 KB (~300 rows) | `pal_side` trims to the last 20 rows past 8 KB (measured: 700 changes never passed 8,216 bytes) | not yet in `cpu_guard` hygiene (a few rows per hour by hand clicks) |
| `<board-viewer session>/pieces/display/frame_history.txt` | the session's frame logger, one block per composed frame (~12 KB/s while the 3D board is driven) | 1 MB (trimmed to the last 256 KB) | `ops/loop_guard.c` `trim_history()`, run every ~8 s by the board's pal loop (tested: 2.36 MB -> 262 KB, no NULs, 500 KB control untouched; live 1.23 MB -> 262 KB in 25 s) | NONE yet: `cpu_guard` hygiene does not sweep session folders (the session dies with its window); dead session folders of killed windows (8-10 MB each) stay until removed by hand |
| `#.desktop/heavy_runs.txt` | `ops/cpu_gate.c` (one row per heavy job) | 200 rows | `cpu_gate` trims on every write | `cpu_guard` `hygiene_sweep()`: past 64 KB keeps the last 100 rows |
| free disk under the house | everything | 5 GB free warning | n/a | `cpu_guard` `hygiene_sweep()` -> `disk_alert.txt` |

Dead-pid leftovers (added 2026-11-02, same sweep, every 2 minutes, a file goes only when `/proc/<pid>` is gone and its name is exactly
prefix + digits + suffix): `#.desktop/entity_menu_history/<pid>.txt`, `#.desktop/entity_menu_frame_<pid>.txt`, `taskbar_settings_frame_<pid>.txt`,
`livedesk_hq_windows_<pid>.txt`, `livedesk_hq_restore_<pid>.txt`, and `#.desktop/ascii_frames/<pid>.{frame_history,frame,pulse}.txt`. Tested on a scratch
house: the 8 dead-pid files went, the live-pid files, look-alike names and config stayed.

Known, not yet capped: `petslike` `events.txt` (4 MiB cap drops silently, rotation is an owner decision, see its own note) and the history file of a LIVE
window (grows while the window lives; removed when it dies).

## Cleaning up by hand (what was done 2026-11-02)

Keep the newest 50 index entries; delete every other `*.objects.pdl` / `*.receipt.pdl`; trim `index.txt` to its last 50 lines; delete
`entity_menu_history/<pid>.txt`, `entity_menu_frame_<pid>.txt` and `taskbar_settings_frame_<pid>.txt` whose pid is dead (`[ -d /proc/<pid> ]`). Run it
niced and `ionice -c3`; 1.2 million unlinks took about a minute. Result: 7.6 GB -> 208 MB, 1.68M -> 1,840 files.
Windows started before the renderer fix keep the old trim until they are restarted; the guard covers them meanwhile.

## Second incident the same day: the game clock out-ran its own events (2026-11-02)

- RAN: `lc_clock.+x` showed at 100% of a core (inclusive of the event runs it launches) and the CPU guard ended 40 house processes at 19:56. The clock of rpg-edit-hq
  was at day 5022 while schedule `r5` was at about day 2522: 2,500 missed daily occurrences being replayed one by one, each a full event run (about 1.8 s of CPU).
  Cause 1: the rate was `sec` ("fast", 1 game hour per real second = one game day every 24 s) and the events of one game day cost more than 24 s, so the backlog
  could only grow. Cause 2: the catch-up cap was 400, i.e. a 12 minute burn per batch, and the 'caught up N' row that moves the cursor came only after all 400; a
  stop in the middle (a restart, the guard) restarted the burn. Cause 3: the append-only ledger grew without a cap.
- Fix: `CATCHUP_CAP` 12, ledger compaction (above), harness cases updated (`lc_clock_recur`, 57 checks) and one added.
- Still true: a fast rate costs real CPU. Events per game day times their cost must be well under the real seconds per game day, or the clock will always be
  catching up (the cap now makes that a short bounded burst instead of a runaway). The owner's saved rate for the edit board is `sec`.
| `@.apps/rpg-edit-hq/state/hai_context.txt`, `hai_proposal.txt`, `hai_calls.txt`, `hai_last_error.txt` (hai_op, 2026-11-04) | each rewritten whole (temp + rename): < 2 KB / < 9 KB / one line / < 400 B | on every ask / apply (event-driven, no poll); a day's asks are capped at 200 by hai_calls.txt itself | `.hai_hdr` (the API key header) and `.hai_req.json` / `.hai_resp.json` exist only during a call and are deleted after; `hai_chat.txt` keeps its 24 KB -> 8 KB cap |
| `<window package dir>/kh_focus_debug.log` (khtpm_core_render.c `kh_focus_debug_log`, a 2026-09 "temporary" probe) | was NOTHING: 49 files / 25 MB in the house, one window at 16 MB (2026-11-04). Now: past 256 KB it is renamed to `kh_focus_debug.log.old` (replacing the previous .old), checked every 100 lines, so <= ~512 KB per window | one line per key press / reparse / grab (hundreds a minute while typing) | windows started before the 2026-11-04 renderer build keep the old (uncapped) code until relaunched; the four big logs were trimmed to 64 KB by hand. `cpu_guard` `hygiene_sweep` is the second guard since 2026-11-04 (`trim_debug_logs`: past 1 MB -> last 128 KB, line aligned) |
| `#.desktop/active_session.pdl` (hai_op, 2026-11-04) | rewritten whole on each editor `context` call; one row per app, at most 16 rows, < 6.4 KB | event-driven (an ask, an editor open), no poll | rows are never trusted: `hai_op where` re-verifies the pid in /proc (alive + the right program) and skips stale rows; second guard since 2026-11-04: `hygiene_sweep` removes it past 16 KB (derived data, hai_op rewrites it) |
| `<chat node root>/pet_post_audit.txt`, `pet_post_pending.txt` (`pet_post.c`, 2026-11-04) | audit: one line per pet post attempt, capped at 256 KB (past it the older half is dropped, in the op); pending: one line per HELD post, removed on approve/veto | per attempt (a post is a rare event, a few per minute at most per pet, rate-capped by the op itself); nothing runs between posts | the pet-post cap (default 3 per 60 s per pet) bounds the audit growth; a second guard for the audit file is still to be added to `cpu_guard` `hygiene_sweep()` when the observer window ships; second guard (2026-11-04): cpu_guard hygiene_sweep trims audit past 1 MB to 128 KB and pending past 256 KB to 32 KB |
