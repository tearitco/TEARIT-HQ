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
