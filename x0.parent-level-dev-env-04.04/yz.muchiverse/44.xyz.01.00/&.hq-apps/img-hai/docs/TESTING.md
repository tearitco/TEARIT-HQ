# Testing img-hai

Everything is tested through the **real window**, driven the way a person drives it (keys, typing, two-step clicks, on-screen numbers) with the relay driver `relay_drive`, plus plain command-line tests for the word-bank programs. A clean compile or a state file alone is not evidence: the cases also take screenshots, and the screenshots were looked at.

## 1. The relay driver in 60 seconds
`&.widgits/_shared-lib/ops/+x/relay_drive.+x` (build: `gcc -std=gnu11 -O2 -o +x/relay_drive.+x relay_drive.c`). Full contract and discoveries: book `02-architecture/RELAY-DRIVE.md`.
```
R='&.widgits/_shared-lib/ops/+x/relay_drive.+x'
PID=$(cat '&.hq-apps/img-hai/state/renderer.pid')           # the window's renderer pid
W=$($R findwin 1060 900)                                      # the window's X id (windows have no name/pid: found by size)
$R $PID press accept                                          # press a control by id (or label:text), the human way: Esc, its number, Enter
$R $PID tapid words                                           # two real mouse clicks in the middle of that control
$R $PID type "red door"; $R $PID key enter
$R shot $W out.png                                            # screenshot of that window
$R $PID run '&.hq-apps/img-hai/tests/img-hai-smoke.pdl' $W    # run a case file; exit 0 all pass, 1 any FAIL
```
`press`/`tapid` first send Esc and the key `p`, which makes the window write `/tmp/entity-menu-frame.png.frame.txt` (every element with its id, label, number and position); the driver reads that file, so tests do not depend on numbers that shift. One window at a time (the dump path is shared by all khtpm windows).

## 2. The cases (`&.hq-apps/img-hai/tests/`)
Run each from a **fresh window** unless noted (`sh button.sh run`, wait ~4 s, then `findwin`). "Remote" = starts a real draft on a real machine.
| Case | What it does | Remote | Time | Last result |
|---|---|---|---|---|
| `img-hai-negative.pdl` | expects states that never appear: **must exit 1** (proves the driver can fail) | no | 5 s | exit 1, 2 FAIL lines, as designed |
| `img-hai-smoke.pdl` | place the `test_tile`, rotate, flip, move right | no | 15 s | 9 / 9 pass |
| `img-hai-mix.pdl` | place, move left/down, rotate twice, hide, show, delete | no | 25 s | 15 / 15 pass |
| `img-hai-judge.pdl` | judge + Accept on the throw-away tile: Accept refused with no verdict, refused with one yes, written with both | no | 40 s | 14 / 14 pass; the real `accept_candidates.pdl` is never touched |
| `img-hai-human.pdl` | click the words field, type a prompt, Enter, press Draft, wait for the remote draft (~2 min), place a tile, rotate | yes (1 host) | ~3 min | first run 18 / 19 (my wrong guess of a nav number); the id version, re-run after the changes: 15 / 15 pass with a real remote draft |
| `img-hai-both.pdl` | one press of "Draft on ALL hosts": both hosts start | yes (2 hosts) | 30 s + the drafts | 9 / 9 pass; both finished (debil 119 s, Mac 118 s) |
| `img-hai-batch.pdl` | start the first batch (`default`): 10 waiting, 2 running, Mac job started | yes (all hosts) | 15 s + ~14 min | pass; 12 done, 0 failed |
| `img-hai-tune.pdl` | select a later batch file (`tune-1`, an overlay trial) and run it. Batch files are alphabetical: `default`, `retest`, `tune-1`: this case presses `batchsel` once, so it now selects `retest`; press it twice for `tune-1` | yes | 15 s + ~13 min | passed (5/5) when `tune-1` was second; **edit the case if the batch list changes** |
| `img-hai-retest.pdl` | select `retest` (2 wheel drafts that must not overwrite existing pictures) | yes | 15 s + ~3 min | 4 / 4 pass; the pictures arrived as `_r2` |
| `img-hai-words.pdl` | set free words, then clear them with an empty submit | no | 20 s | 8 / 8 pass |
Test data: `tiles/test_tile.png` is a throw-away copy of a draft. Its verdicts go to `&.hq-apps/img-hai/verdicts.txt` and its candidate to `state/accept_test.pdl`; both are git-ignored and **must be deleted after a run** (`rm -f '&.hq-apps/img-hai/verdicts.txt' '&.hq-apps/img-hai/state/accept_test.pdl'`) or the next "fresh" judge test starts with verdicts already set.

## 3. Writing or reading a case
Rows: `STEP | key|type|click|tap|press|tapid|sleep|shot|expect | ...`. `expect` waits up to its timeout for a file to **contain** (or `absent` = not contain) a text; paths are relative to the house root. A `|` inside the text is not allowed (it splits the row: found the hard way). Always read the printed `PASS`/`FAIL` lines, not just the exit code; a failing `expect` prints the text it wanted.
Pitfalls found while writing these (all real):
- A screenshot right after a state change can show the *previous* frame; assert on the state file first.
- An armed typing field swallows keys: `press` sends Esc first; if you send raw keys yourself, send Esc.
- Do not hard-code row numbers. They changed three times in one afternoon.
- A case that assumed the wrong starting state failed (mix started from a non-fresh window); say in the case header what state it needs.

## 4. Command-line tests (no window)
| Program | Test | Expected |
|---|---|---|
| `img_prompt.+x` | `img_prompt words.pdl parts.pdl car_door` | one PROMPT/NEGATIVE/USED/DROPPED block (car_door: "a car door panel, painted steel, dark red, seen from above, pixel art, ..." before tuning) |
| `img_prompt.+x` | each file in `data/img/bad/` (weight 1.8, duplicate id, bad kind in words; unknown word in parts) | exit 1 with an ERR line |
| `img_edit.+x validate` | `data/img/candidates/tune-1.cand` | `VALID: 9 edits, 0 invalid` |
| `img_edit.+x validate` | each file in `data/img/candidates/bad/` (9 files) | exit 1, the intended message (delta too big, unknown target, no subject word, weight would exceed 1, a pipe inside a value, no reason, status not candidate, duplicate id, unknown type). Run them with the real exit code: `$E validate ... >/dev/null; rc=$?` (an `echo $(basename ...)` hides it) |
| `img_edit.+x promote` | on a **scratch copy** of the bank: as the proposer (refused, bank untouched), as another name (applied; ledger lines; comments kept), repeat (refused: word exists) | as listed |
| overlay equality | for each part, the prompt with an overlay equals the prompt from the promoted bank | equal for car_door, car_windshield, person_torso, car_wheel |
| `img_sheet.+x` | one missing file among real ones; all missing | reported, exit 0; exit 1 |
Never promote the real bank in a test: copy `words.pdl`, `parts.pdl` and the `.cand` to a scratch folder first.

## 5. What is not tested (be honest about it)
Export, the cell-size buttons, the cursor buttons, `scan`, selecting from the Placed list, `front`/`back`, `clear`, the window chrome (close/fullscreen/minimise), typing a lone space to clear the free words, many tiles (over 256), a host being unreachable mid-batch, two windows at once, and the owner's real mouse (all relay clicks are synthetic). The generated pictures' quality is judged by eye, not by a test.
