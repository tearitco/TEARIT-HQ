# Harnesses for the drive loop: what exists, what is reusable, what is missing (2026-11-01)

Labels: **RAN** = observed this session. **READ** = read from a file. **MINE** = proposal.

## 1. What exists (READ, RAN)

The house harness is already the data-driven design. `&.widgits/_shared-lib/harness/`:

- **115 pals, 115 case files.** Every `<name>.pal` is the same 3 lines: `exec harness_case_op <name>.pdl`, `exec harness_verdict_op <name>.pdl`, (`exec harness_bank_op`), `halt`. (RAN: the three lines are the only non-comment content of the pals, counted 115 times each.)
- The part a person writes is `cases/<name>.pdl`: pipe rows, `SCRATCH`/`COPY`/`WRITE`/`RUN`/`EXPECT_*`/`SPAWN`/`WAIT_HAS`. Two compiled ops run every case. No new C and no new sh per check.
- So the answer to "too many harnesses" is: a new check is one new `.pdl` plus a 5-line `.pal`. It is data. The README table is the index.

## 2. Gaps the drive loop hits (RAN 2026-11-01, building `editor_skip`)

1. **`EXPECT_HAS` / `EXPECT_LACKS` / `EXPECT_EXISTS` only read paths under `$T`, and read at most 16 KB.** `petslike_core.c` and `pchq_board_projector.c` are far larger, so a needle past 16 KB reads as absent. Work-around used: `RUN | grep | -qF | -e | <needle> | <file>` then `EXPECT_EXIT | 0 | label` (no size limit, and `EXPECT_EXIT | 1` is the negative form). **MINE:** add `EXPECT_SRC_HAS | <abs path> | <needle>` to `harness_case_op.c` (one verb, grep semantics, any size) so cases stop spelling it as two rows.
2. **No verb says "a board is up".** Live state files (`board/state/ui.txt`) are stale once the window closes: on 2026-11-01 it still said `n_file_opts=003`, `default-legacy`, from an old run. A check on it passes or fails for reasons unrelated to the code. Work-around: two case files. `editor_skip` reads SOURCE and is always meaningful. `editor_skip_live` reads the state files and says in its header that a FAIL with no window is not a regression. **MINE:** a `REQUIRE_PROC | <pattern> | <label>` row that turns the rest of the case into SKIP rows when no such process exists.
3. **No picture verb.** Proof for a frame is still a hand-run `dump_frame_png_op`. **MINE:** `EXPECT_PNG_DIFFERS | <a.png> | <b.png>` by hash (the `snap` receipt in `board_ctl` already does this), so "the pane changed" is a row, not a look.

## 3. What the first conversion found

`editor_skip.sh` became `cases/editor_skip.pdl` (14 checks, source side) and `cases/editor_skip_live.pdl` (state side). Writing the source side found a real regression: **`@.apps/rpg-edit-hq/board/pchq-board.xhtpm` on `claude` had no `n_file_opts` repeat**, so the book menu showed only "blueprintz" and "cancel", while `DONE.md` called the book menu closed. The rows exist in grok's version (`fo-N`, `d4f818eff`) and had been lost in the editor-toolbar rework. They are restored in the commit that adds this file. A negative test (one needle replaced) fails exactly `menu-repeat-over-file-opts` and nothing else.

Lesson for this chapter: a DONE row without a check that can fail is a claim. Each closed row gets a `.pdl` row; run `editor_skip.pal` before a launch.

## 4. Rule of thumb for new checks (MINE)

- Reading source or a config: a `grep -qF` row. Cheap, no window, always meaningful.
- Reading live state: only in a `*_live` case, and name the process it needs.
- Driving a window: the existing `board_ctl` flow (relay + `bv_keys` + `snap`), not a new script. Its steps become `RUN | board_ctl | expect ...` rows once it is stable on one board, so a drive is replayable data.
- Do not add a harness for something one `EXPECT_` row in an existing case can say.
