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

A harness is only worth trusting if it can fail: a copy of `pchq_playtest_action.pdl` with one wrong expectation gave `VERDICT|FAIL|passed=7|failed=1` (2026-10-07).

## Not ported yet (still C / sh, candidates)

- `&.widgits/_shared-lib/tests/test_game_setup.c` (parser unit test; needs a small query op to call it from a case)
- `_.monads/_.livedesk-taskbar/ops/test_playtest_menu.c` (white-box: includes the manager source)
- `@.apps/hotbar-hq/ops/test_minimize.sh` (timing test against a running manager)
