# Decision ledger and daily review (M3 / S3)

Built 2026-10-18 for df-hq, cdda-hq and mcl-hq. Roadmap rows: `GAMES-ROADMAP.md` M3, `CLAUDE-TODO-ROADMAP-2026-10-16.md` S3. Shared code (CHANGED 2026-10-27, see "Moved to an op" at the end): the op `&.widgits/_shared-lib/ops/dec_op.c`; the old text-include `game_decisions.c` is retired. Test: `XO/test_decisions.sh` (not in git; scratch copies, tag `dec`).

## What it is for

The game ledger (`state/events.txt`, see `S1-EVENT-LEDGER.md`) records what **happened**. It is the authority for game state. The decision ledger records what an **actor chose** and what came of it. That is the input a learner needs: the behavior bank (weights per choice), a concept bank, tomom, or a pet's own FSM/GOAP. It is deliberately a separate file so it can be cut, rotated or thrown away without touching game state.

## Where the rows come from

| Source | Rows | Where |
|---|---|---|
| a pet's autonomous step | `pick` (chose a new target), `walk` (moved a tile), `door` (the step crossed a door), `dig` (df-hq: dug the marked cell beside it), `walk` + outcome `blocked` (had a target, could not move) | `ai_step` in each game is now a thin wrapper around `ai_step_inner`. The wrapper compares the pet before and after and writes at most **one** row. The wrapped function is unchanged. |
| the zed (cdda-hq only) | `chase` (it stepped toward the controlled mover), `strike` with outcome `hit` or `refused` | `zed_turn` |
| nothing chosen | no row | an idle step (no target, none picked) is not a decision |

Not covered yet (say so, do not assume): pet greetings (`meet`), player-controlled moves (a person's input is not an actor decision), mcl-hq mobs (none exist), vehicles (not built), and the reason/score columns the harness doc sketches (`why`, `score`). The row has an `outcome` field now; a numeric score is the next column once something produces one.

## Row format

```
DEC|<unix>|<game stamp>|<actor>|<state>|<action>|<target>|<outcome>
DEC|1792308345|D1 08:20|rex|goal|walk|7,8|ok
DEC|1792308350|D1 08:25|zed|hunting|chase|rex|moved
```

* `stamp` is the in-game `Dn hh:mm` (the same arithmetic as the chat log), empty when the game clock is not set up.
* `state` is what the actor was doing before: `goal` (had a target), `idle`, `adjacent`, `hunting`.
* `target` is `x,y` or an actor id; `-` when none.
* Fields never contain `|` or a newline (replaced by `/` and a space) and are cut at 40 characters, so a row is always 8 fields.
* One `write()` with `O_APPEND` per row. A crash can tear at most the last line; every reader ignores a row without its newline.
* **Size growth is the change marker**, never mtime (house rule). A consumer keeps the byte offset it has read.

## Rotation

Past 2 MB (`DEC_MAX_BYTES`, override with `-DDEC_MAX_BYTES=`) the next append first renames `decisions.txt` to `decisions.archive.txt` (atomic; the previous archive is replaced), then starts a fresh file. This ledger is learning input, not authority, so replacing the oldest archive is acceptable. If a game wants to keep everything, copy the archive away before it rotates again.

## The daily review

`<game binary> review [day]` reads `state/decisions.txt` (the last 4 MB), keeps the rows of one game day (default: the latest day present) and writes `state/review.txt` atomically (tmp + fsync + rename):

```
day=D1
rows=37
actors=3
first=D1 00:05
last=D1 23:50
ACTOR|rex|21|19|2          actor, rows, ok rows, blocked/miss/refused rows
ACTOR|zed|9|7|2
ACTION|walk|ok|14          action, outcome, count
ACTION|walk|blocked|2
ACTION|pick|ok|6
```

Plain text, same style as the other state files, so a window, a harness case or a model prompt can read it with no parser. The verb answers `review: N decisions reviewed, written to review.txt`, or `review: no decisions yet`.

## Proof (2026-10-18, all three games)

`XO/test_decisions.sh`: 40 `walk` verbs each given a random target: never more than one row per step and a step that moved wrote exactly one; every row has 8 fields and a `Dn hh:mm` stamp; actions come from the known set; the review row count equals the rows of that day; ACTOR rows add up to the total; a torn last line is ignored; past 2 MB the file rotates and the live file is small again; a 30 KB garbage ledger exits cleanly and leaves a well-formed review; no ledger gives "no decisions yet". cdda-hq adds: a chase row while the zed walks toward rex, a strike row (hit) when adjacent, no zed row when it is in another room. The first run, before the test gave the pet targets, wrote no rows and failed (that is the negative control for "decisions were written").

## Traps found while building it

* **Every verb is a fresh process with the same random seed**, so in a scratch test the "pick a target" roll never fires and the pet never moves. Give the pet a target in `pet_<id>.txt` (`tx=`, `ty=`) before each `walk`, as the test does.
* **`grep -c` on a missing file prints nothing** and on zero matches prints `0` *and* exits 1: `grep -c ... || echo 0` yields two lines. Use `if [ -f ]; then grep -c ... || true; else echo 0; fi`.
* A test that only counts rows passes vacuously when the file is missing. The "decisions were written" check exists to stop that.
* The wrapper reads the pet's saved state (`others[i]`, `tr`) after the step, not the working copy `pet`, because `meet()` can run in between.

## The behavior-bank wiring (built 2026-10-18)

`dec_bank_update` (in `game_decisions.c`) turns the ledger into **bank entries**, one per action, in the same sidecar shape the harness behavior bank already uses (`harness/ops/harness_bank_op.c`, design `HARNESS-BEHAVIOR-BANK-DESIGN.md`):

```
state/bank/dec_<game>_<action>.behavior.pdl      e.g. dec_mcl-hq_walk.behavior.pdl
BEHAVIOR | dec.mcl-hq.walk
KEYWORDS | walk, mcl-hq, decision
SENTENCE | How often the mcl-hq choice 'walk' ends well (...) in mcl-hq's decision ledger.
SOURCE | measured from state/decisions.txt by dec_bank_update; not hand-authored
COUNTS | reward=31 | punish=4 | cursor=2210
WEIGHT | 0.8919                                  (reward+1)/(reward+punish+2), live Laplace estimate
```

* A row is a **reward** when its outcome is anything but `blocked`, `miss` or `refused`, a **punish** otherwise.
* The read position is `state/bank/dec_cursor.txt`: a byte offset into `decisions.txt`, an append-only marker, never mtime. Running the update twice counts nothing twice; a torn last line is left for the next pass; a ledger that rotated (shorter than the cursor) restarts the cursor at 0 and keeps the counts.
* It runs on the `review` verb and automatically every 200 decisions inside the game. Each entry is written tmp + rename; the cursor last. **A crash between the entry writes and the cursor write double-counts at most one batch** (at-least-once; the weight is an estimate, so this is acceptable and stated here instead of hidden).
* Because the files are in the house sidecar shape, the existing ranking works with no new reader: `harness_bank_op find <game>/state/bank walk` prints `score|id|file`, best first.
* Action names containing `/`, `.` or a backslash are skipped (the action becomes part of a file name); at most 32 actions are tracked.

### The read-back (the loop is closed for one choice)

`dec_weight(bank_dir, game, action)` returns the live weight, or **0.5 when there is no entry or the file is garbage** (no evidence changes nothing). The one choice wired to it so far is **how readily a pet picks a new target**:

```
pick chance = 45 + (0.5 - weight("walk")) * 60 , clamped to 15..85     (percent per step, was a constant 45)
```

A pet whose own walk choices keep getting blocked (weight under 0.5) gives up and retargets sooner; a pet whose walks succeed keeps its goal longer. The weight file is re-read at most every 10 seconds. `<game> decpct` prints the current chance and says where it comes from. Tested: weight 0.1 gives 69 %, 0.9 gives 21 %, no entry or garbage gives 45 %, and a build with the read-back removed fails exactly those checks.

**Honest limits.** The weight is per game, not per actor (a pet that never got blocked shares the number with one that always is). Only the pick chance reads it; no other choice does. The pick chance was tested through `decpct` and the code path (`rand() % 100 < dec_pick_pct()`), **not** by watching a population of pets change behavior over a long run. Nothing validates the effect (is retargeting sooner actually better?): that judgement belongs to a harness/review step that does not exist yet. The weights are measured counts, so nothing here is hand-authored.

## How a game AI reads it

Today: read `state/review.txt` for a day summary, `state/bank/dec_<game>_<action>.behavior.pdl` for a weight, or tail `state/decisions.txt` from a remembered offset. **Not built**: per-actor weights, a score or reason column, more choices reading their weight, and a judge that checks whether a changed weight helped.

## Adding a new game or a new decision source

1. `#include "../../../&.widgits/_shared-lib/game_decisions.c"` after `game_events.c`.
2. Call `dec_append(path, stamp, actor, state, action, target, outcome)` at the one place the actor commits to a choice, after the outcome is known. One row per choice.
3. Add a `review` verb that calls `dec_review(decisions_path, review_path, day_or_-1)`.
4. Add the game to `XO/test_decisions.sh`.

## Moved to an op, and pets in rpg-space now write the ledger (2026-10-27)
Owner: shared C should be ops/events, not text includes; hot paths write rows and an op digests them off the tick. So:
* **Hot path (inline in each game, one `O_APPEND` write per decision):** `dec_row()` in `df_hq.c`, `cdda_hq.c`, `mcl_hq.c`, `rpg_pvp_hq.c` and now `rpg_space.c` / `rpg_dust.c`. Same 8-field `DEC` row; checked identical to the old `dec_append` (timestamp aside).
* **Everything else is `dec_op`**, run by fork/exec (niced, one at a time, never in the tick path): `bank <decisions> <bank_dir> <game>` (cursor by byte offset, bank entries, **`dec_<game>_pick_pct.txt`**, and **ledger rotation** past 2 MB), `review <decisions> <out> [day]`, `weight <bank_dir> <game> <action>`. The games call `bank` every 200 decisions and on the `review` verb; the pick-chance formula (45 + (0.5 - walk weight) * 60, clamped 15..85) moved from the games into the op, which writes one integer the game re-reads every 10 s. Not yet a `POLICY` row (design: GAMES-AS-PACKAGES-AND-STORE-DESIGN section 15).
* **Observed:** on a 500-row synthetic ledger the op's bank files and review file are **byte-identical** to the old code's (and weight 0.5447 / pick chance 43 agree; an altered ledger changes 5 of 5 files, so the comparison is not vacuous); a second run consumes 0 rows; a 3 MB ledger rotates and resets the cursor; the torn last line is ignored. `df-hq` in a scratch house: `review` -> "200 decisions reviewed", bank files and `pick_pct` written, `decpct` moved from 45 to 43. Both rpg-space and rpg-dust gates: **83 PASS, 0 FAIL** with the rebuilt binaries.
* **rpg-space / rpg-dust pets now write the ledger** (they did not: wandering was `rand()` in the core). Same wrapper as df-hq in skin sections 34/35 (before/after state of each `ai_step`: `pick`, `walk`, `door`, `blocked`), and the skin's pick roll now reads `dec_pick_pct()` instead of the constant 45. Observed in a scratch copy: 6 `walk` verbs gave 3 `ok` and 3 `blocked` rows, `review` made the bank entry, and a pet that kept being blocked got a 66 % retarget chance. **Limits:** there is no game clock here, so the stamp is empty and `review` treats everything as day 0; the daemon's own cadence (every 200 decisions) was not run for 200 decisions; a `walk` verb is a fresh process (the doc trap: pick rolls never fire), so the live daemon's `pick` rows were not observed.
* **Fixed in all five wrappers:** a `blocked` row used to record target `x,-1` (the target was cleared before it was read); it now keeps the real target.
* **Still true:** per-game, not per-actor weights; only the pick chance reads a weight; no judge checks the change helped.
