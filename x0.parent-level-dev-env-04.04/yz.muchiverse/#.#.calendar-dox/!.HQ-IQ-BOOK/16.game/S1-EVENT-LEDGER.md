# S1 — the event line and resolver (built 2026-10-17)

Sprint item S1 of `GAME-MECHANICS-SPRINT.md`, shared by df-hq, cdda-hq and mcl-hq. Code: `44.xyz.01.00/&.widgits/_shared-lib/game_events.c` (text-included, pure file I/O,
prefix `ge_`), three thin verbs in `ops/df_hq.c`, `ops/cdda_hq.c`, `ops/mcl_hq.c`.

## The rows (one ledger: `state/events.txt`, no second file — grok's answer 2026-10-16)

```
EVENT|ts|actor|verb|target|at|queued       written by the `event` verb (or a menu row / pet); validated first
EVRES|ts|<offset of the EVENT row>|verb|result   written once, by the resolver
REJECT|ts|<offset of the bad row>|reason         written once, for an EVENT-looking row that is malformed
```

`verb` is one of `give_item take walk dig build craft`. `at` is `-` or `<room>:<x>,<y>`. No field may contain `|`, CR or LF. The existing `STEP DOOR DAY DIG RESET` rows are not touched and
their readers ignore the new row types (checked in df-hq: a dug cell and a rock cell behave the same with and without EVENT/EVRES/REJECT rows in the ledger).

## Verbs (all three games, same syntax)

| verb | what it does |
| --- | --- |
| `event <actor> <verb> <target> <room:x,y or ->` | validates, appends one EVENT row, resolves it. A bad field refuses with the reason and writes nothing |
| `resolve` | resolves every legal EVENT row that has no EVRES yet and rejects every malformed one that has no REJECT yet. Idempotent: a second run appends nothing |
| `inventory [actor]` | replays the ledger and prints `actor:item=n`. Writes nothing |

## Deviation from the sprint line: no cursor file

The sprint asked for a cursor over a size-growth marker. Resolution status lives in the ledger instead (an EVRES row names the offset of its EVENT row). Reason: Save, Load, New game and
reset swap `events.txt` as a whole, and a separate cursor file would disagree with the restored ledger. The state (inventory) is a pure function of the file, replayed from byte 0 on each call
(a ledger is 100 KB in df-hq; the call happens on a verb, not per tick). A daemon that wants to resolve live should call `resolve` when the file's size grew (marker, not mtime).

## What is built and what is empty

Built: validation, append, replay, EVRES/REJECT, inventory effect of `give_item` (+1) and `take` (−1, result `none` when the actor has none; no state change).
Empty on purpose (result `todo:<verb>`): `walk dig build craft`. The rule bodies are grok's: labor, mood, recipes, wounds, mob weights. A game wires a rule by defining `GE_RULE_HOOK(actor, verb, target, at)`
(returns a result text or NULL for the default) before including `game_events.c`.
`TODO(grok)`: df-hq `dig` already exists as DESIGNATE/CLAIM/DIG rows (`jobs.txt` + `events.txt`) and is not routed through EVENT yet; `build`/`craft` bodies belong in
`ops/df_hq.c`, `ops/cdda_hq.c`, `ops/mcl_hq.c`.
`khtpm_inventory.c` was looked at and not reused: it models a desk entity's inventory as a directory of entity packages, which these games do not have.

## Tested (scratch copies, paths rewritten, verbs only; live games untouched)

Same script on each game (`XO/test_s1.sh`): 8 scripted events give `pip:stone=1`; a STEP row already in the ledger is untouched; 8 EVENT + 8 EVRES rows; six malformed `event` calls
are refused and the ledger size does not change; three hand-written bad rows (3 fields, 8 fields, non-numeric time) get 3 REJECT rows, a second `resolve` changes nothing (sha1 equal) and the inventory
is unchanged; a fresh game given the same ledger replays to the same inventory and resolving it appends nothing; negative control: turning the first `give_item` into `take` in the copy
changes the inventory (so the equality checks can fail). Result: 0 failures in df-hq, cdda-hq and mcl-hq.
## First wired use (df-hq)

A finished dig (`dig_now` in `ops/df_hq.c`, run by the pet that digs) appends `EVENT give_item stone <room:x,y>` for the digger and resolves it; the Menu note line shows `<pet> dug stone at x,y and carries N`.
Tested on a scratch copy with a scratch daemon (INT off) on the shipped test fortress: in 100 s the pets dug all 9 marked cells, 9 DIG rows = 9 give_item rows = 9 EVRES rows, 0 rejects, and for each digger the number of digs equals the stones carried (pip 5, orin 3, gus 1). The haul to a stockpile is NOT built: that is a labor rule, grok's.

Not tested: the verbs from a menu row or a pet (no menu row exists yet), a live game, a ledger over 32 MB (refused by design), concurrent writers.

## Compaction and archives (built 2026-10-17, tag led)

### The problem

`ge_scan` replays `state/events.txt` from byte 0 on every call, reads the whole file into memory, and used to refuse a file over `GE_MAX_FILE` = 32 MB (return 0, the verbs print "ledger unreadable"). A game that ran long enough would therefore stop starting at all: no
error beyond that one line, and the only way out was to edit the file by hand. Owner, 2026-10-17: "fix ledger, that is a must". Real sizes at the time: df-hq 120 KB, cdda-hq 36 KB, mcl-hq 4 KB; rpg-pet (S1 not enabled there) 15.8 MB. At the measured row mix of the synthetic generator (about 144 bytes
per game step: two STEP rows, one EVENT, one EVRES) 32 MB is about 230,000 steps. Two things were wrong, not one: the hard stop, and that a replay got slower with every row (also the duplicate check for "does this
EVENT already have an EVRES" was a linear search per event; it is a binary search now).

### The scheme: a checkpoint block, an archive, an atomic rename

The state of a game is a pure function of the ledger. Compaction replaces the first N rows by the state they produce, written as rows the replay understands, and keeps the old file.

```
CKPT|ts|events|rejects|last        starts the block: events = legal EVENT rows counted so far, rejects = REJECT rows counted so far, last = ge_last (result of the last legal event)
CKINV|actor|item|n                 one row per ge_inv entry, IN TABLE ORDER (zero counts too: find-or-create order is part of the state)
CKG|<anything without a newline>   the game's own tables, written by GE_CKSAVE_HOOK, read back by GE_CKLOAD_HOOK
CKEND|n                            ends the block; only a CKPT that has its CKEND later in the file is used (a block cut short is ignored, the rows around it replay as before)
```

A compact `events.txt` is: the checkpoint block, then every row that is not a ledger row and not history (DIG, RESET, any unknown type: kept verbatim and in order), then the tail (below). `ge_replay` handles a valid `CKPT` by resetting the tables
(`ge_ninv = 0`, `GE_RESET_HOOK()` restores the base state exactly as at the start of any scan) and loading the block, so the replay of `checkpoint + later rows` is the replay of the old file. Nothing else in the replay changed: EVENT rows after the checkpoint are applied in order,
EVRES / REJECT rows after it mark rows done, statistics (`events`, `rejects in all`, last result) start from the checkpoint's counters.

What is dropped from the live file (and lives on in the archive): all EVENT / EVRES / REJECT rows before the cut, and the **history rows `STEP`, `DOOR`, `DAY`**. Nothing in df-hq, cdda-hq or mcl-hq reads them back (grep over the three games and the shared library: they are only appended; the DOOR rows the games
*read* come from `rooms.pdl`, not from the ledger). They were also about half of the bytes in the synthetic ledger (80,000 STEP rows of 5.7 MB = 2.6 MB), and every walking pet writes one per step: without dropping them, compaction would only delay the cap. Compile with `-DGE_KEEP_HISTORY` to keep them. **A new reader of a history row type must either be moved to its own file or be added to the keep list in `ge_is_history_row`.** DIG
stays because df-hq's cursor reader (`world_sync` / `ev_row`) rebuilds the dug cells from it; the cursor already restarts when the file gets shorter (`fsize < ev_off`), which is exactly what a compaction does, and the DIG rows are still there to be re-read.

### Offsets

EVRES / REJECT rows name their EVENT by byte offset. In the normal case there is nothing to rewrite: compaction runs after a resolve, so every EVENT in the file has its EVRES, the cut is the end of the file, and the new file holds no EVENT rows at all, only the checkpoint. The only EVENT rows that can survive are ones appended by another process between
the resolve and the moment the file is read again (and malformed ones are rejected by that resolve first). For those the **cut** is the offset of the first EVENT-looking row with no EVRES / REJECT; everything before it is folded into the checkpoint, everything from it on is the **tail**. Tail EVENT rows are copied verbatim and move,
so their EVRES / REJECT rows are rewritten with the new offset (a map old offset -> new offset built while copying); a done row in the tail whose EVENT was folded (its offset is before the cut) is dropped, and a dropped REJECT is counted into the checkpoint's `rejects` so the statistic does not change. A torn last line (no newline) is copied byte for byte and stays torn,
exactly as `ge_scan` already ignores it. The replay of the compact file never needs an offset into the old one.

### Trigger, tune value, verb

* After a successful **resolve** (the append of the missing EVRES / REJECT rows worked), `ge_scan` looks at the file size; at or above `GE_COMPACT_BYTES()` it calls `ge_compact`. Never from a replay-only scan (`resolve` argument 0: the inventory verb, the tab builders, the daemon's size-change rescans).
  The tables the caller sees afterwards are rebuilt from the new file (identical by construction).
* The limit is the tune value **`ledger_compact_kb`** in each game's `tuning.pdl` (default 4096 = 4 MB; 0 or a very large number effectively turns it off). The daemon reads `tuning.pdl` itself; a verb process never ran `load_tune`, so `ge_compact_limit()` in each game reads it on first use.
* Verb, all three games: **`compact`** forces it now and prints `compact: <before> -> <after> bytes, old ledger kept as events.archive.<n>.txt` (or `compact: not done (<why>)`). It first resolves anything pending, so the log lines of pending events still reach the chat.
* A second process that finds the compaction in progress does nothing (`compact: not done (another compaction holds the lock)`): the lock is an `flock` on `state/events.txt.lock`, which the kernel drops when the holder dies, so a `kill -9` leaves no stale lock.

### Crash analysis

Order inside `ge_compact` (the file is written next to the old one; `events.txt` is never open for writing): (1) read the ledger, build the new content in memory; (2) write `events.txt.compact.tmp` and `fsync` it; (3) if the live file's size is still the size that was read, go on, else redo from step 1 (up to four times);
(4) **`link(events.txt, events.archive.<n>.txt)`**, a hard link, first free `n`, never overwrites (`EEXIST` tries the next number; without hard-link support the archive is written from the buffer to a `.part` file and linked into place); (5) `rename(tmp, events.txt)` (atomic); (6) `fsync` the directory.

| the process dies | what is on disk | what the game sees |
| --- | --- | --- |
| before (4) | old `events.txt`, a stale `.compact.tmp` | the old ledger; the next compaction overwrites the tmp (`O_TRUNC`) |
| between (4) and (5) | old `events.txt` and an archive that is a copy of it | the old ledger; the next compaction takes archive number n+1, so the archive is stored twice. Harmless; delete the older of two identical archives by hand if the space matters |
| after (5) | new `events.txt`, archive | the new ledger |

`events.txt` is therefore always either the complete old file or the complete new one, and both replay to the same tables. Tested with real kills (below) and with deterministic crash points (`GE_CRASH_AT=1|2|3`, compiled in only with `-DGE_TEST_CRASH`, which production builds do not use).
**The one window that is not closed:** a row appended by another process to the old file after step (3) and before step (5) goes into the old inode and is not in the new file (the game's own appenders use plain `O_APPEND`, not the lock). It is the time between a `stat` and a `rename`, microseconds, but it is not zero; the row is still in the archive. A
daemon and a verb both appending in that instant is the only way to hit it.

### Hard cap

`GE_MAX_FILE` is now 256 MB (it is read whole into memory; this is the "I would rather you looked at it" limit), and a file between 32 and 256 MB loads and compacts (tested with a 42 MB ledger: loads in 0.3 to 0.9 s, compacts to a few KB). Over the cap `ge_scan` still returns 0, but now `ge_why` holds
the reason and the `GE_FAIL_HOOK` of each game writes it to the note line, for example `ledger is 5 MB, over the hard cap of 1 MB: move state/events.txt aside (see S1-EVENT-LEDGER.md)` (seen in `state/note.txt` of a scratch copy built with `-DGE_MAX_FILE=1048576`). Way out for a ledger over 256 MB: move it aside, and `compact` needs
to read it: run the game build with a larger `-DGE_MAX_FILE`, run `compact`, rebuild normally.

### How a new game's hooks stay compactable

A game's tables must be a **pure function of the ledger prefix plus static data**, and everything the replay carries between events must be serialisable:

1. `GE_RESET_HOOK()` rebuilds the base state from the game's static data (recipes, rooms), not from anything the ledger wrote. It runs at the start of every scan **and** before a checkpoint is loaded.
2. `GE_RULE_HOOK` may read and write only the tables that `GE_CKSAVE_HOOK` writes, `ge_inv`, and static data. No clock, no random, no file written by the game: a replay must give the same answer on the second day.
3. `GE_CKSAVE_HOOK(FILE *o)` writes one `CKG|...` line per table row, in table order, and **every counter that influences later events** (cdda-hq: `nstrike`, which seeds the wound-part hash, so the order and count of strikes matters). `GE_CKLOAD_HOOK(char *payload)` reads one line back.
   If the base state has rows the ledger can remove (mcl-hq's blocks), the first line must clear them (`CKG|MBCLEAR`), because the checkpoint holds the whole table, not a difference.
4. Save things by id, not by index (rooms are written by id and looked up with `room_idx` on load).
5. Prove it the way `XO/test_ledger_compact.sh` does: dump the tables, `compact`, dump again, byte-identical, and then play the same follow-up events on a compacted and a never-compacted copy and compare again (that catches state a dump does not show, like the strike counter).

Written for the three games: **df-hq** has no hook tables (stone per dwarf is `ge_inv`), nothing to add; **cdda-hq** `cd_cksave` / `cd_ckload` (`CKG|NSTRIKE|n`, `CKG|WND|who|part|left|max`); **mcl-hq** `mb_cksave` / `mb_ckload` (`CKG|MBCLEAR`, `CKG|MB|room|x|y|id`).
A table over its capacity (`GE_MAX_INV` 512 inventory rows, cdda 64 wounds, mcl 512 blocks) is saved as far as it goes, exactly as the replay would have stopped growing it.

### The 32 MB history

Before this section the cap was `GE_MAX_FILE = 32 MB` and there was no way to shrink the file. The cap was never hit: the three games' ledgers were 4 KB to 120 KB at the time of writing. Still, the replay is O(file) on every verb and on every scan the daemon makes after a size change, so a ledger of a few MB already costs a measurable
slice of the CPU. Measured on the weak machine: a replay of a 42 MB ledger takes 0.3 to 0.9 s (a 4 MB one roughly a tenth of that), and a compacted ledger replays in a few milliseconds.

### Restoring from an archive

Archives are `state/events.archive.<n>.txt`, n = 1, 2, ... in the order they were taken. Each is the complete ledger as it stood just before that compaction (the live file at that moment is archive n plus, in the worst crash case, nothing else). The game never reads or deletes them. They are not part of a Save slot (a slot copies `events.txt`, and that file is self-contained since the checkpoint holds the state).
To go back to the state before compaction n: stop the game, `cp state/events.txt state/events.after-n.txt`, `cp state/events.archive.<n>.txt state/events.txt`, start the game (the replay is slow only for a big file, and the next resolve compacts again, which creates a new archive and so loses nothing). To look at what
happened (history rows, who did what when), read the archive with `grep`. Archives of one game folder add up to roughly the total of all rows ever written: delete old ones by hand when disk matters; the live ledger never needs them. New game clears `events.txt` but leaves the archives (they are of the previous game).

### Tested (`XO/test_ledger_compact.sh [game ...]`, generator `XO/ledger_gen.awk`, scratch copies tag `led`)

Per game: a synthetic ledger of 40,000 steps (5.7 MB: give_item, take, walk, dig, build, craft, strike mixed, REJECT rows, three pending EVENT rows) -> tables dumped (inventory per actor, plus cdda `wounds.pdl`, mcl `export3d` map.txt and cells.txt, df stone per dwarf) -> replay-only scans leave the file alone -> the first resolve compacts by itself ->
tables identical; the `compact` verb: byte-identical dump, equal statistics, archive is a byte copy of the old ledger, new file under 10% of the old; 80 follow-up events on a compacted and a never-compacted copy give equal dumps; a 42 MB ledger (over the old cap) loads and compacts; 20 `kill -9` rounds at random moments of a compaction and three deterministic crash points: the game loaded and replayed identically every time; negative
controls: a ledger with one more event gives a different dump, a truncated ledger gives a different dump, a build with a 1 MB hard cap refuses the file and the note line says why, a torn last line is ignored and survives compaction untouched. Last run (the machine was loaded, load average 7): 5.7 MB ledger -> df-hq 6,912 bytes, cdda-hq 1,557, mcl-hq 3,271 (the df file keeps its DIG rows); 42 MB ledger -> 48 KB / 1.6 KB / 3.3 KB; replay of the 42 MB ledger 0.3 to 4.6 s, compaction of it 0.7 to 6.4 s (disk and load dependent); 20 of 20 kill rounds identical in each game (mixed old/new live ledger); crash points 1, 2, 3 identical in each game.
Result lines: `RESULT df-hq: 0 failures`, `RESULT cdda-hq: 0 failures`, `RESULT mcl-hq: 0 failures`. Not tested: a live daemon compacting while pets walk (the verbs and the scan trigger are tested, the daemon path is the same function), two processes appending during the final rename, rpg-pet.
