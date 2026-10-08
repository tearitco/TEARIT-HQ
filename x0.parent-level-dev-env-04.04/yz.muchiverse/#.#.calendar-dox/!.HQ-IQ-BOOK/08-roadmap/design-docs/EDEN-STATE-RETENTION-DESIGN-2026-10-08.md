# Eden state retention: how the game keeps its history and knows which saves are stale

Status: DESIGN ONLY, written 2026-10-08 by claude. Nothing here is built.
Owner brief (2026-10-08): the daemon must be stoppable; **the history is allowed to grow that big**; the game should know how to save game states and delete stale ones. "How would it know this?"

## 1. What exists today (measured 2026-10-08)

- `eden_history.txt` is append-only: **about 19 MB after 62,497 game days (17 hours of wall time), growing about 1 MB per hour**. Nothing ever shrinks it.
- Save slots exist: `max_slots=10`, menu rows Save 1-3 / Load 1-3 / Save slot... / Resave / Load, backed by `eden_op ... save-slot|load-slot|resave-slot` and `game_snapshot_op`. Today there are **no slots** except the one I made on 2026-10-08 (`slot1`, day 62,497).
- Nothing creates a save automatically, nothing labels a save, nothing deletes one, and the clock daemon has no disk guard. The disk was 93% used with 19 GB free.
- "Stop game" **despawns** (deletes participant and entity folders). Pausing only stops the clock daemon. A person pressing Stop game would lose Asa's and Ava's state unless a slot exists.

## 2. The honest answer to "how would it know?"

It does not know anything by itself. It follows **rules written down as data**, run by a **small deterministic op** at times a **clock event** chooses, using **evidence already in the files** (the day counter, the slot rows, the history's own DAY rows). That is the house pattern for everything else (grade tick, day tick), so retention is the same shape:

1. **A policy file** the owner can read and edit, `retention.pdl` in the conductor folder:
   - `CHECKPOINT | every_days=1000 | keep=5` (rolling automatic saves, oldest dropped first)
   - `PIN | slots=1,2,3` (manual saves, never touched by automation)
   - `HISTORY | hot_days=2000 | archive=gzip | delete_after_days=0` (0 means never delete, only archive)
   - `DISK | pause_below_free_pct=10`
2. **Three ops** inside `eden_op`, each a **dry run by default** with `--apply`, idempotent, and each writing one row to `eden_control.txt`:
   - `checkpoint`: write an automatic save into a reserved slot range (for example slots 11-20), labelled `auto day N`.
   - `prune-checkpoints`: a save is **stale** only if it is automatic, older than the newest `keep` automatic saves, not pinned and not labelled by a person. Evidence: the SLOT rows already carry `day` and `label`.
   - `archive-history`: rotate, never truncate. Close the current `eden_history.txt` at a day boundary into `history/eden_history.<from>-<to>.txt.gz` (verify the gzip checksum first), start a new file, and leave a one-line `SEGMENT|from|to|sha256` index row. Readers use the index, so history stays complete and still "grows that big", only on cheaper storage.
3. **A common event** `eden_housekeeping`, fired by the clock every N game days exactly like `eden_day_tick`, runs the three ops in that order and then checks the disk.
4. **A disk guard**: if free space is below `pause_below_free_pct`, the housekeeping event pauses the clock daemon (the safe Pause, never Stop game), writes a visible status line, and does nothing destructive.

## 3. Things that must stay true

- Never delete anything a person made (pinned or labelled saves). Never delete history unless `delete_after_days` is set above 0 by the owner.
- Anything that reads history (the planned `eden_history_to_feedback`, S2 of the roadmap) must read segments in order from the index, or it will silently miss days.
- Rules are data. A model may later **propose** values (for example "keep checkpoints around days where fitness spiked") through the same autonomy-0 gate the joints use: only a person applies them.
- Stop game keeps meaning "despawn". Add a confirmation row, or make the menu say so, and let the housekeeping op make a checkpoint just before it.

## 4. Build order (each step has a locked pal harness with a mutant)

1. `retention.pdl` parser plus `checkpoint` with a dry run and a fixture game folder. (W: a free worker can do this with the harness locked first.)
2. `prune-checkpoints` with cases: pinned kept, labelled kept, newest K kept, older automatic dropped, dry run changes nothing.
3. `archive-history` with cases: segment index rows correct, checksum verified, reading across segments equals the original file byte for byte, an interrupted rotation leaves the old file intact.
4. The `eden_housekeeping` common event and the disk guard, verified in a scratch house with `lc_clock` (the way Grade Tick was).
5. A status row the Eden window can show (see the visibility map in the session report): last checkpoint day, saves, history size, free disk.

## 5. Who can do it

Steps 1 to 3 are data and file handling with deterministic checks: tier W or M. Step 4 touches the clock and the common-event machinery: tier M with review. The decisions that are the owner's: the numbers in `retention.pdl`, and whether Stop game should ask first.
