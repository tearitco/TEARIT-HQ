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
| `<board-viewer session>/pieces/display/.loop_guard.log` | the session loop guard's decision log, one line per ~11 s while the engine is idle-exited (2026-11-05: 2774 lines / 410 KB in 9 h, its comment said 'cap 40 lines' but no code trimmed it). NOW capped in `ops/loop_guard.c` `guard_log`: past 8 KB keep the last 2 KB (shared `trim_tail`, also used for frame_history). Harness `loop_guard_logcap.pal` (5 PASS; red on the old binary). Sessions run their own copy of the op, so already-running sessions keep the old log until they restart. NOT yet in cpu_guard hygiene_sweep (the independent second guard) |
| `#.desktop/heavy_runs.txt` | `ops/cpu_gate.c` (one row per heavy job) | 200 rows | `cpu_gate` trims on every write | `cpu_guard` `hygiene_sweep()`: past 64 KB keeps the last 100 rows |
| free disk under the house | everything | 5 GB free warning | n/a | `cpu_guard` `hygiene_sweep()` -> `disk_alert.txt` |
| `&.widgits/palettes/state/` palette-studio window: `studio_session.txt`, `palettes-studio_ui.txt`, `studio_preview.raw` + `studio_preview.receipt.txt`, `studio_saves/slotN.png` + `.studio.pdl`, `studio_undo.txt`, `studio_presets/slotN.txt` (N 1..9), `studio_bias.pdl`, `studio_gate.txt` | `palette_studio_ui_op.+x` (one short process per click, nothing resident; none of it is written while the window is idle) | session about 4 KB, vars about 1 KB, preview 1.77 MB (768 x 576 x 4) rewritten in place, saves capped at 20 slots (N rotates over 20: about 150 KB png + 1 KB record each, at most about 3 MB); the undo file keeps at most 20 snapshots (about 3 KB each, read cap 400 KB, rewritten whole on each change); at most 9 preset files of about 3 KB; `studio_bias.pdl` (the user layer of material edits) holds at most one BIAS and one KEY row per material (25 materials, hard cap 64 rows of each, about 8 KB), rewritten whole through a temp file that is deleted when the edit is refused; `studio_gate.txt` is under 1 KB, rewritten whole | the op: every file is rewritten in place (tmp + rename) and the slot number is `saves mod 20`; harness `palette_studio.pal` proves there is no 21st slot and that 25 changes leave exactly 20 undo records | none yet: add the folder to `cpu_guard` `hygiene_sweep()` (git-ignored folder, written only on a click, so the rate is 0 files per hour idle) |

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

## Observed 2026-11-05 with `growth_probe` (files larger than 1 MB that this standard does not name)

Method (OBS = observed by running it): `&.widgits/_shared-lib/ops/+x/growth_probe.+x <house> --standard <this file> --min-mb 1` lists every text-like file over 1 MB in the house runtime folders with its age and whether its NAME appears here, and exits 1 on a file written in the last `--live-min` minutes (default 120) that is not named here. A grep over the C sources cannot do this: 18 of the 19 append sites in `board-viewer/ops` build the path elsewhere. Harness: `growth_probe.pal` (12 PASS, mutation-tested). Run on the real house: 12 files, exit 0 at the 10-minute window; at the 120-minute window the rows below are the unnamed live ones.

| File (editor book unless noted) | Size / lines (OBS) | State | Note |
|---|---|---|---|
| `events/phase_{day,dusk,night,dawn}/master_ledger.txt` and `events/day_tick/master_ledger.txt` | about 1.8 MB, 13,000 lines each since 2026-10-22 | written when the page plays, NOT capped, not named here | about HALF of every file (6,523 to 6,531 of 13,0xx lines) is one repeated row: `[Prisc Error] Could not open ops file: .../101.mutaclsym...+18.0G/system/default_op.txt`, once per page play. The folder exists but its `system/` has no `default_op.txt`. Cause of the error NOT fixed (it changes how every event page starts: needs a harness case with `PLAY_PAGE` first). Derived rate (DOC, from counts over 14 days): about 130 KB per day per ledger |
| `state/relations.txt` | 1.5 MB, 58,882 lines | live, not capped, not named | writer in `rpg_edit_hq.c` (shared core); owner of the decision: trim policy |
| `state/world/messages.txt` | 1.2 MB | live, not capped, not named | same core |
| `@.apps/rpg-pvp-hq/state/decisions.txt` | 1.0 MB | stale | |
| `&.hq-apps/world-manager/state/animation_queue.txt` | 5.8 MB, 108,804 lines | STALE: last written 2026-10-05, not growing (checked twice, 12 s apart) | history, not a live writer; safe to archive by the owner |
| `state/events.txt` (3 books) | 2.2 to 3.6 MB | live | covered: the writer drops over 4 MiB (see the petslike events 4 MiB cap note), a drop-cap, not a trim |

Rule going forward: a file that appears in this table either gets a cap and a row in the standard above, or an owner decision saying why not. `growth_probe` is the check; the harness keeps it honest.

### Fix applied 2026-11-05: the repeated Prisc error in the phase ledgers

Cause (OBS): `prisc+x` loads a fallback `default_op.txt` next to its own binary and printed `Could not open ops file` when none existed; the tracked production binary `101.mutaclsym*/system/prisc+x` (built Oct 15, no source in that folder) had no such file, and every event page played through it logged the line. Two changes: (1) shared source `_shared-lib/system/prisc+x.c` treats the fallback as OPTIONAL (an explicit argv[4] ops file and bad includes stay loud); harness `prisc_optional_default_ops.pal` 5 PASS, red on the old source (1 FAIL) then green; (2) an intentionally empty `default_op.txt` (comments only) placed next to the production binary, because that binary is not rebuilt from the shared source. Checked: the production binary run without the file prints the error, with it prints nothing, exit 0 both. NOT observed: a new live page play after the change (ledgers only grow when a page plays); the existing 6,5xx error rows per ledger remain history. Still uncapped: the ledgers themselves (about 130 KB per day each before the fix, about half of that now expected); a cap is still an owner decision.

### Caps added 2026-11-05 (the two live uncapped files in the table above)

- `state/world/messages.txt` and `history.txt` (every events-hq `mr_*` op, through the shared `mr_log` in `events-hq/ops/mr_clock_common.h`): past 256 KB the newest 64 KB are kept, cut at a line, replaced by temp + rename. Harness `mr_log_cap.pal` 9 PASS, red without the cap. Cost: one stat() per log call.
- `state/relations.txt` (petslike core, all petslike games on their next build): COMPACTION, not truncation. The file is read for the friend counts ("friends: X x50") and the 25 s re-meet gate, so a tail trim would reset the counts. Past `relations_cap_kb` (default 128) old MEET rows fold into `TOT|lastts|a|b|count` rows and the newest `relations_keep` (default 200) MEET rows stay. Both readers understand TOT. Harness `relations_cap.pal` 10 PASS, red on the old binary. The verb `fire relations` compacts and prints the friend lines.
- NOT observed: the live house. The installed editor binary is rebuilt (310,336 bytes) but a running daemon keeps the old code until it restarts; the live 58,882-row file is compacted by the first meeting after a restart. The other petslike games (rpg-space, rpg-pet, rpg-dust, ...) keep the old core until rebuilt.
- Row left out on purpose: the phase `master_ledger.txt` files (cap is an owner decision: they are the page-play audit).
