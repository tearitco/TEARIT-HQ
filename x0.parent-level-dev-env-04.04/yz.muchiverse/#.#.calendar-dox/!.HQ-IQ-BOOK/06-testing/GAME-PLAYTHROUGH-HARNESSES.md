# Scripted playthrough harnesses (df-hq, cdda-hq, mcl-hq) - 2026-10-17

Roadmap items D4 / C3. Written by the `play` lane (claude). Everything named here is built and was run; the last section lists what was NOT tested.

## 1. Purpose

A unit harness checks one op. A playthrough harness plays a whole game: it builds the game from source into a scratch copy, starts it fresh, and drives it to a meaningful end state using only the verbs the binary itself offers (`event`, `inventory`, `mine`, `craft`, `place`, `scavenge`, `strike`, `step`, `walk`, `dig_area`, `save`, `load`, `newgame`, `clock`, ...), the same verbs the window's menu rows call. After every phase it looks at three things: the verb's own answer, the ledger (`state/events.txt`, `state/jobs.txt`, `state/chat.txt`), and the rows the Tab bar is built from (`state/ui.txt`: inventory, craft, body, look, stocks, units). It proves:

- the three games still do what their how-tos say (`10-user-docs/DF-HQ-DIG-HOWTO.md`, `CDDA-HQ-CRAFT-HOWTO.md`, `MCL-HQ-BLOCKS-HOWTO.md`),
- the S1 ledger really is the state (`16.game/S1-EVENT-LEDGER.md`): a fresh game fed the same ledger has the same bags,
- and, because every run appends to a result ledger, which STEPS of a game story are reliable. That second part is what lets a pet or the `auto` conductor use the harness as a judge (see `17.ai/HARNESS-AS-GAME-BRAIN.md`).

The story is DATA (a phase table), the runner is generic. Nothing here edits a game file, launches a window, or touches live state.

## 2. How to run

Build the ops once (the harness ops are not in git): `sh '&.widgits/_shared-lib/harness/ops/build_harness_ops.sh'` (now also builds `playthrough_op`). Needs the live `&.widgits/livedesk-clock/ops/+x/lc_clock.+x` (the games call it for their clock) and `gcc`.

From `&.widgits/_shared-lib/harness/`, with the many-args prisc (README of that folder, `gcc -O2 -w -o <scratch>/prisc system/prisc+x.c -lm`):

```
nice -n 15 prisc play_cdda_hq.pal      # then read results/play_cdda_hq.txt.verdict.txt
nice -n 15 prisc play_df_hq.pal
nice -n 15 prisc play_mcl_hq.pal
```

Or without prisc, one call, any phase table (this is the AI entry point):

```
nice -n 15 ops/+x/playthrough_op.+x run play/phases/cdda_hq.phases.pdl     # exit 0 = every check passed, 1 = a check failed, 2 = bad table
```

`run` prints a PLAYTHROUGH line and one line per phase with its live weight:

```
PLAYTHROUGH|cdda_hq_broken|cdda-hq|FAIL|1701 ms|phases=7|failed=1
  p1_scavenge            PASS    81 ms  play.cdda-hq.p1_scavenge reward=2 punish=0 weight=0.7500
  p2_craft               FAIL   172 ms  play.cdda-hq.p2_craft reward=1 punish=1 weight=0.5000
  ...
  setup                  PASS (build + scratch house + viewer)
```

Measured 2026-10-17 on the loaded desk (load average 12 to 13): cdda-hq 7.8 s, df-hq 1.6 s, mcl-hq 1.6 s for a whole pal run (first cdda run includes the cold build; 11 to 17 s in other runs while the machine was busy). Checks: cdda-hq 39, df-hq 36, mcl-hq 43. No process and no `/tmp/hc_*` directory is left (checked with `ps` and `ls` after the runs). All commands are niced inside the case (`nice -n 15` in front of every game call and the compile).

## 3. How it is built (three pieces, no shell)

```
play/phases/<name>.phases.pdl --playthrough_op compile--> cases/play_<name>.pdl --harness_case_op--> results/play_<name>.txt (PASS|FAIL rows)
                                                                                          |
                    harness_verdict_op --> results/play_<name>.txt.verdict.txt            |
                                                                                          v
                    playthrough_op ledger --> play/results/play_ledger.txt (RESULT ledger, one PH row per phase, one PLAY row per run)
                                          --> play/results/ph/<game>__<phase>.txt --harness_bank_op--> play/bank/live/<game>__<phase>.behavior.pdl (WEIGHT)
```

- The **phase table** is the only thing a person or an AI writes.
- `ops/playthrough_op.c` has four modes: `compile`, `ledger`, `run` (= compile + case + verdict + ledger) and `mark` (a timestamp line the generated case calls at each phase start; a case file has no clock of its own).
- The generated case is judged by the existing `harness_case_op`, so every guard of that op (an empty expectation fails, extra fields fail, children are killed, the scratch is removed) applies. One verb was added to the case op for this: `COPYDIR` (copy a tree with a skip list). It has its own case, `cases/harness_copydir.pdl` with a fixture, 9 checks, shown able to fail.
- **Scratch house.** The games find their house by walking UP from the binary to a folder holding `&.widgits` and one holding `#.NNEST_ASSETS`. The generated case therefore builds a skeleton under `/tmp/hc_XXXXXX`: `@.apps/<game>/` (copied without `state board audit +x saves <game>-pchq .hq_manager events`), the text-included sources (`stb_image.h`, `stb_image_write.h`, `game_events.c`), `lc_clock.+x`, the pet DB (`db-hq/data/system.pdl` and `actors.pdl`), an empty `#.NNEST_ASSETS/rmmv-www-img`. The game is compiled there with `gcc -O0` and runs with the scratch as its house, so it cannot reach the live game even by absolute path (the sed rewrite of the old `XO/test_*.sh` scripts is not needed).
- **Stand-in window.** A game writes `state/ui.txt` only while a process whose command line contains `khtpm_core_render` and `<game>.xhtpm` exists. The case copies `/usr/bin/tail` to `$T/khtpm_core_render` and SPAWNs it as `khtpm_core_render -f /dev/null <game>.xhtpm` (a harmless sleeper, no GUI). CLEAN kills its process group.

## 4. The phase-table format

One pipe-delimited row per line, `#` comments, fields trimmed. `\n` `\t` `\p` (literal bar) are expanded when the case runs. Paths are relative to the game folder (`state/events.txt` means `$T/@.apps/<game>/state/events.txt`).

**Header (before the first PHASE)**

| Row | Meaning |
|---|---|
| `GAME \| <folder> \| <binary base>` | required: the folder under `@.apps` and the C file / binary name (`df-hq`, `df_hq`) |
| `INIT \| <path> \| <text>` | write a file into the game folder before the first phase (flag files, where the pets stand) |
| `NEEDS \| <src under $HOUSE> \| <dst under $T>` | copy one more file into the scratch house (default dst = same path) |

The table's file name gives the run name: `cdda_hq.phases.pdl` -> `cdda_hq`.

**`PHASE | <id> | <title> | <keywords>`** starts a phase. The id uses letters, digits, `_`, `-`, must be unique, and cannot be `setup`. The title becomes a sentence in the phase's bank seed; the keywords (optional, comma list) become bank keywords. A phase with no expectation row is refused.

**Action rows**

| Row | Meaning |
|---|---|
| `DO \| verb \| arg \| arg ...` | run the game binary, at most 12 args, one argv entry per field |
| `PUT \| <path> \| <text>` | write a file in the game folder = STATE SURGERY. Use it only to put a pet where a story needs it and say so in a `NOTE` row. Everything else must come from verbs |
| `CP \| <src> \| <dst>` / `RM \| <path>` | copy / remove inside the game folder (used to keep a ledger across a new game) |
| `MARK \| <var> \| <path> \| <key>` | var = the value of `key=` in a key=value file (the clock file) |
| `CAP \| <var>` | var = the last DO's output, trailing newlines trimmed |
| `SUM \| <var> \| <dir>` | var = a hash of every file under a directory |
| `SLEEP \| <ms>`, `NOTE \| <text>` | wait, comment |

**Expectation rows (the last field is always the label)**

| Row | Passes when |
|---|---|
| `SAY \| text \| label` / `NOSAY` / `SAYIS` | the last DO's output contains / lacks / equals (trailing newlines trimmed only) text |
| `EXIT \| n \| label` | the last DO exited n |
| `HAS \| path \| text \| label` / `LACKS` / `FILEIS` | the file contains / lacks / equals text (HAS and LACKS read only the first 16 KB of the file) |
| `EXISTS \| path \| label` / `NOFILE` | the path exists / does not |
| `COUNT \| path \| line-prefix \| n \| label` | exactly n lines start with the prefix (`grep -c "^prefix"`; use `\p` for a bar, a basic regular expression is allowed) |
| `UI \| key \| value \| label` | `state/ui.txt` has the whole line `key=value` (`grep -cxF`, so file size does not matter and an empty value is allowed) |
| `UIKEY \| key \| label` | the key exists with any value |
| `DELTA \| before \| after \| n \| label` | `after - before == n` for two MARK vars |
| `EQ \| var \| var \| label` / `NE` | two CAP / MARK / SUM vars are equal / differ |

A check's final label is `<phase>.<label>`; the part before the first `.` is how results are grouped, so labels may contain dots but phase ids may not.

### Worked example (cdda-hq, phase `p2_craft`)

```
PHASE | p2_craft | craft a bandage: one rag becomes a bandage and the clock moves by the recipe minutes | craft, recipe, bandage, clock
DO | clock | rate | hour
MARK | c0 | state/clock_root/#.desktop/clocks/rpg.pdl | game_time_epoch_ms
DO | craft | bandage
SAY | crafted bandage | craft-succeeds
MARK | c1 | state/clock_root/#.desktop/clocks/rpg.pdl | game_time_epoch_ms
DELTA | c0 | c1 | 60000 | clock-advanced-exactly-one-minute
DO | bag
SAYIS | rag 1, bandage 1 | bag-after-craft
UI | n_craft | 1 | craft-tab-has-the-recipe
UI | craft_0_label | Craft bandage 1m | craft-tab-row-shows-minutes
UI | inv_1_label | bandage x1 | inventory-tab-shows-the-bandage
```

compiles to (abridged; `$T` is the scratch dir, `BIN` the game binary in it):

```
RUN | PT | mark | .../results/play_cdda_hq.marks.txt | p2_craft
RUN | nice | -n | 15 | BIN | clock | rate | hour
RUN | awk | -F= | -v | k=game_time_epoch_ms | $1==k{print $2} | $T/@.apps/cdda-hq/state/clock_root/#.desktop/clocks/rpg.pdl
CAPTURE | c0
RUN | nice | -n | 15 | BIN | craft | bandage
EXPECT_OUT | crafted bandage | p2_craft.craft-succeeds
...
RUN | awk | BEGIN{print $c1 - $c0}
EXPECT_OUT_IS | 60000 | p2_craft.clock-advanced-exactly-one-minute
RUN | grep | -c | -x | -F | craft_0_label=Craft bandage 1m | $T/@.apps/cdda-hq/state/ui.txt
EXPECT_OUT_IS | 1 | p2_craft.craft-tab-row-shows-minutes
```

Read the generated `cases/play_<name>.pdl` whenever a row does something you did not expect; it is plain case-file text.

## 5. The shipped stories

| Pal | Phases | End state |
|---|---|---|
| `play_cdda_hq.pal` | p1_scavenge, p2_craft, p3_door, p4_strike, p5_saveload, p6_replay | 2 rags scavenged, bandage crafted (clock +60000 ms exactly), Rex through the stair door (`D1 06:01 Rex moves to the Cellar`, stamped), two strikes (zed rows in `state/wounds.pdl`, the second `wounds` answer differs from the first, `the zed hits Rex`), slot 2 survives New game, replay equal |
| `play_df_hq.pal` | p1_mark, p2_dig, p3_tabs, p4_saveload, p5_replay | 9 marked cells (one cancelled and re-marked: 10 DESIGNATE rows, 1 CANCEL), `walk` until 9 DIG = 9 DONE = 9 `give_item stone` = 9 EVRES, 0 REJECT, `pip:stone=9`, Stocks `Stone total 9`, Units `Pip, Castle, 9 stone`, slot 3 round trip (New game: no `jobs.txt`, stone 0), replay equal |
| `play_mcl_hq.pal` | p1_mine, p2_craft, p3_place, p4_saveload, p5_replay | stone + 2 logs mined (empty cell refused), 2x planks x4 (third craft refused, no log), hold + place with three refusals, a placed block stops the walker until mined, slot 1 round trip (New game brings the stone block back), replay equal bags AND the mined log cell reopens |

## 6. Results become weights

Every run does `playthrough_op ledger`:

1. It takes this run's PASS/FAIL rows (after the last `RUN|` row of the case results), groups them by phase, and reads the phase times from the marks file (time of a phase = next mark minus its own).
2. It appends to the append-only **RESULT ledger** `play/results/play_ledger.txt`:
   `PH|<run id>|<game>|<name>|<phase>|<PASS|FAIL|NOCHECKS>|<ms>|pass=N|fail=M|<first failed label>` and one `PLAY|<run id>|<game>|<name>|<verdict>|<total ms>|phases=N|failed=M`. The run id is the case op's `RUN` timestamp, so calling `ledger` twice for one run writes nothing the second time.
3. For each phase that ran checks it appends one `PASS|<game>/<phase>` or `FAIL|<game>/<phase>|<label>` row to `play/results/ph/<game>__<phase>.txt` and calls `harness_bank_op` on the phase's mini case. That op keeps `COUNTS | reward | punish | cursor` in `play/bank/live/<game>__<phase>.behavior.pdl` and writes `WEIGHT = (reward+1)/(reward+punish+2)`, the Laplace estimate that the phase passes. A `NOCHECKS` phase (never reached) changes nothing.
4. The shipped pal also runs `harness_bank_op` on `cases/play_<name>.pdl`, the whole-harness entry (hand-written seed `bank/play_<name>.behavior.pdl`); its counts are per check, not per phase.

Seeds for the phases are generated into `play/bank/` the first time a table is compiled (keywords from the table's verbs and keywords, the title as a sentence, slots pointing at the game and its verbs). They are never overwritten; the measured counts live in `play/bank/live/`. Only WEIGHT is measured: the SYNONYM / SLOT / CORPUS numbers in a seed are guesses, same rule as every other bank entry.

Observed (clean state, then): cdda-hq pal (PASS), df-hq pal, mcl-hq pal, then `play/proposed/cdda_hq_broken.phases.pdl` (the same table with the clock expectation changed from 60000 to 61000), then the cdda-hq pal again:

```
after the clean pal run                p2_craft reward=1 punish=0 weight=0.6667   (computed from the counts the next line shows; not printed by that run)
after the broken run  (p2_craft FAIL)  p2_craft reward=1 punish=1 weight=0.5000   (p1, p3-p6: reward=2 punish=0 weight=0.7500)
after the clean pal run again          p2_craft reward=2 punish=1 weight=0.6000   (p1, p3: reward=3 punish=0 weight=0.8000)
```

and `harness_bank_op find play/bank craft bandage` ranked `play.cdda-hq.p2_craft` first (1.200) with `play.cdda-hq.p1_scavenge` (0.800) and `play.mcl-hq.p2_craft` (0.667) after it.

## 7. Add a phase, a game, or a proposal

- **A phase:** add `PHASE ... DO ... expectations` to the table. Run once, read the failing labels, fix the expectation (never loosen a check until it passes without understanding why). Negative-test it: change the expected text, see `VERDICT|FAIL`, change it back.
- **A game:** copy a table, change the `GAME` row, write the INIT rows (flag files, where the pets stand; check what the game needs in `state/` first), add `play_<name>.pal` (copy of an existing one with the name changed) and `bank/play_<name>.behavior.pdl`. If the game text-includes more than `stb_image*.h` and `game_events.c`, or reads other data folders, add `NEEDS` rows.
- **A proposal:** write any table under `play/proposed/` (git-ignored) or `play/phases/` and run `playthrough_op run <table>`. `play/phases/example_proposal.phases.pdl` is a two-phase list (mine a stone, put it down) that passed on the first try: `PLAYTHROUGH|example_proposal|mcl-hq|PASS|...` with `mine_stone` and `put_it_down` at weight 0.6667.

## 8. Traps found (each one bit during the build)

1. **A scratch game under /tmp refuses to start** with `no #.NNEST_ASSETS / &.widgits above <app>`: the house is found by walking up. The skeleton in section 3 is the fix.
2. **`bad actor` on every verb**: the pets are rows of the db-hq data (`system.pdl`, `actors.pdl`), not of `party.pdl`; a scratch without them has no pets.
3. **No `ui.txt`** unless a process with `khtpm_core_render` and `<game>.xhtpm` in its argv exists. The stand-in is also seen by the LIVE games' viewer scan for a moment (harmless, it only makes a live daemon think a window is open).
4. **df-hq `newgame` and `load fortress` refuse** (`the backup to slot 0 failed`) when `state/` has no `events.txt`: they back the running game up first. The df story therefore starts with real play before it uses New game, and its INIT does not write an events file.
5. **A game call is one process**: the state of `walk` and of the pets is on disk, so nothing carries across calls except files. That is why a ledger can be kept over a New game by `CP`.
6. **`EXPECT_OUT_IS` trims only newlines.** `inventory` prints `rex:rag=2 ` with a trailing space; use `SAY` for it (two checks failed on exactly this).
7. **-O2 compile took 8 s at load 13, -O0 1.2 s.** The games' verbs are not CPU work; the scratch build is `-O0`. A whole df-hq run went from 54 s to 2 s. If a game ever depends on optimisation-only behaviour this hides a difference from the shipped `-O2` binary (nothing found in the three games).
8. **`HAS` reads 16 KB.** `state/ui.txt` is bigger than that in some games; `UI` uses `grep -cxF` and has no such limit.
9. **The other pets wander at random** (every game). Do not assert counts of `STEP` / `DOOR` rows or where a pet you do not control stands; assert the rows your own verbs wrote (`DIG`, `EVENT`, `CANCEL`).
10. **A `RUN` row has at most 16 argv entries.** `nice -n 15 <bin>` uses 4, so a `DO` has 12 arguments at most.
11. **Dev runs write real FAIL rows** into the ledgers (three failed development runs showed up as `punish` counts). Reset by deleting `play/results/`, `play/bank/live/`, `results/play_*`, `bank/live/play_*`; they are all git-ignored.
12. **`pkill -f` / `pgrep -f` with a game's name matches your own shell**; the harness kills only the process groups it SPAWNed.

## 9. Not tested / not built

- Nothing was run against a live window; the three stories use verbs only (the INT keys, mouse clicks and the real renderer are not exercised here; see `02-architecture/RELAY-DRIVE.md` for that route).
- `df-hq`: the resident daemon (pets digging by themselves in real time) is not covered; digging is driven by `walk`. cdda-hq: the zed acting in real time (INT off) and the daemon-driven day events are not covered. mcl-hq: `buy`, `use`, `remove` and the board views are not covered.
- A failed phase does not stop the later phases (the case op has no conditional); downstream phases then show their own FAILs, which counts against their weights. A `STOP_ON_FAIL` row would need a case-op verb.
- The per-phase weight is keyed by game and phase id only, not by the sequence before it; two proposals using the same id share one weight.
- Concurrency: two runs of the same table at the same time share the marks file and the ledgers (the shared names are `play_<name>.*`); run one at a time.
