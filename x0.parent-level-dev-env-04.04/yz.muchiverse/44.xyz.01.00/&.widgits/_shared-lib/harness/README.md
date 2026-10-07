# harness/ - pal harnesses (owner direction 2026-10-07: harnesses are pal + events, not sh, not C one-offs)

A harness is three small things:

| Piece | Where | What |
|---|---|---|
| **pal** | `<name>.pal` | assembly: `exec` the case op, `exec` the verdict op, `halt`. Run from this folder: `prisc+x <name>.pal` |
| **cases** | `cases/<name>.pdl` | data (pipe rows): scratch setup, the command to run, the expectations. This is the part a person edits, and the part that can later be stored as bank weights / training data |
| **ops** | `ops/harness_case_op.c`, `ops/harness_verdict_op.c` | compiled ops (build into `ops/+x/`, flags `-Wall -Wextra -O2`) |

**Why two ops:** prisc's `exec` ignores the child's exit status and discards its output (`prisc+x.c`, "exit status isn't consulted here"), so a pal cannot assert by itself. `harness_case_op` writes `PASS|<case>/<label>` / `FAIL|...` rows into `results/<name>.txt` (append-only ledger, one `RUN|` row per run); `harness_verdict_op` counts the rows after the last `RUN|` and writes `results/<name>.txt.verdict.txt` (`VERDICT|PASS|passed=N|failed=M|...` plus every FAIL row). **Read the verdict file** after the pal halts.

**Case verbs** (details in the header of `ops/harness_case_op.c`): `RESULTS`, `CASE`, `SCRATCH` (a `/tmp/hc_*` dir = `$T`), `MKDIR`, `COPY` (from `$HOUSE` into `$T`, keeps the mode), `WRITE`, `RUN` (fork/exec, **no shell**, fields are argv entries), `EXPECT_EXIT`, `EXPECT_FILE`, `EXPECT_NOFILE`, `EXPECT_HAS`, `EXPECT_LACKS`, `EXPECT_OUT`, `CLEAN`. Every command runs with its cwd in `$T`; a case that forgets `CLEAN` is still cleaned up. **Never name a live path in a case; work under `$T`.** (Shell expansion bites when you *generate* a case file with an unquoted heredoc: `$T` becomes empty. Use `<<'EOF'`.)

**Run (scratch prisc until a house one is built):** `gcc -O2 -w -o /tmp/prisc system/prisc+x.c -lm` from `&.widgits/_shared-lib/system/`, then `/tmp/prisc <name>.pal` here. It prints `[Prisc Error] Could not open ops file .../default_op.txt`; that is harmless (the harness uses no custom ops).

## Harnesses

| pal | cases | checks | covers |
|---|---|---|---|
| `pchq_playtest_action.pal` | `cases/pchq_playtest_action.pdl` | 8 | pc-hq `player` verbs `playtest` / `toggle` / `stop` on the real `pchq_board_action.sh` |
| `transfer_map_access.pal` | `cases/transfer_map_access.pdl` | 17 | real `mr_transfer_desk.+x`: play-mode map access, refusal rc 3 + reason + ledger, build/missing-mode/no-MAP-rows unrestricted, play-test follows play |
| `game_setup_parser.pal` | `cases/game_setup_parser.pdl` | 42 | the `game.pdl` parser (`khtpm_game_setup.c`) through `game_setup_query_op`: rows, maps, cells, edit marks, mode file, `gs_check_map_switch` + ledger |
| `player_menu_playtest.pal` | `cases/player_menu_playtest.pdl` | 17 | taskbar Player menu play-test row, play-mode file transitions (via `player_menu_query_op`, white-box) |
| `hotbar_minimize.pal` | `cases/hotbar_minimize.pdl` | 9 | a REAL `hotbar_manager` process: hide/show/toggle publish within 300 ms, two clicks 280 ms apart stay hidden |
| `close_listed.pal` | `cases/close_listed.pdl` | 19 | `close_listed.sh` + `close_on_restart.pdl`: real sleeper processes, other-house/unlisted untouched, caller survives its own match, `--relaunch` |
| `proc_ledger_add.pal` | `cases/proc_ledger_add.pdl` | 9 | `proc_ledger_add.sh` with the REAL reaper (`proc_reap_op`): registered pid reaped, PID-reuse guard, bad args |
| `game_slots.pal` | `cases/game_slots.pdl` | 22 | `game_slot_op` save/load slots on a scratch entity tree (SUMTREE: save and load change no entity data) |

Run them all (from this folder, with a prisc binary): `for p in *.pal; do prisc+x $p; done`, then read every `results/*.verdict.txt`.

**Extra ops** (all registered in `default_op.txt`, the house standard prisc reads from its cwd): `game_setup_query_op`, `player_menu_query_op`, `proc_reap_op`: thin front ends that let a case call code that is otherwise only reachable from C. Add one the same way when a new subject needs it.

**Lessons that are now guards in the op** (each one bit during the port): a `|` inside a field shifts the row, so use `\p` for a literal bar, an **empty expectation FAILs**, and a row with more fields than its verb takes FAILs; before the guards some ledger checks passed vacuously. A killed `SPAWN`ed child is reaped before `EXPECT_DEAD` (a zombie still answers `kill 0`). Generating a case file from a shell heredoc expands `$T`: use `<<'EOF'`.

A harness is only worth trusting if it can fail: copies with one wrong expectation gave `VERDICT|FAIL` for `pchq_playtest_action` and `transfer_map_access`.

## Not ported yet (still sh)

- `@.apps/board-hq/verify.sh` (24 headless-renderer checks), `^.grave/quests/Q009-.../verify.sh` and `loop_test.sh` (phone ops and the router loop): heavier, need their own subjects wired in.
- Others' harnesses (`^.hai-horn/halo_test_harness.sh`, `WSR_PAL-PREFERED/...`, Q003 `verify.sh`) are not mine and are untouched.
