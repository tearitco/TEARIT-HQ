# Incident 2026-10-05 - the working tree vanished (and how to get it back)

**Read this first if `git status` suddenly shows thousands of ` D` files, or windows
come up empty / entities and the pc-hq board are missing after a reset.**

## What happened

About 20:46-20:47 the contents of `x0.parent-level-dev-env-04.04/` were deleted
from disk: 31,571 tracked files, 321 empty `ops/+x` / `+x` output folders, every
compiled program, and every untracked file. Only `#.desktop/` survived (running
processes kept writing into it). The `.git` directory and all history were never
touched, so nothing that had been committed was lost.

Evidence (read-only, all checked):
- `git status`: 31,571 tracked files deleted, **unstaged** (nothing staged, so a
  commit would have been wrong but not destructive until someone ran `git add -A`).
- `git reflog`: no git operation after the 20:38 commit. The deletion was done
  outside git.
- The top-level folders' birth dates were still 2026-09-01 - the folders were
  emptied, not recreated.
- Nothing in the launch/reset path (`$.crypts/button.sh`, `crypt_autostart`,
  `world-manager/button.sh kill`) removes the tree. The only `rm -rf` there is
  `world-manager/button.sh clean`, which removes its own `system/` folder and is not
  called by `reset`.
- Nautilus was active at 20:47:37; `rezip-house.sh` ran at 20:47 on the already
  empty tree.
- **The cause was NOT determined.** Two `button.sh reset` runs (20:41, 20:45) came
  just before; they are the closest event but nothing in their code explains a
  recursive delete. Do not assume either way.

## What was and was not recoverable

| Thing | Recovered? | From |
|---|---|---|
| Tracked files (31,571) | yes | `git checkout` of the deleted paths |
| Untracked-but-real files (684) | yes, as of Oct 4 04:11 | the `.7z` snapshot from `rezip-house.sh` |
| Empty `+x` folders (321) + 80 other empty dirs | yes | recreated from the snapshot's layout |
| Compiled programs (`*.+x`) | rebuilt, ~540 | `$.crypts/button.sh build` |
| Uncommitted edits to tracked files | **no** | reverted to the committed version |
| New files created after the snapshot and never committed | **no** | (one session's nav-echo files were re-applied from conversation) |
| Running processes | kept running | they had the old binaries loaded in memory |

## Recovery runbook (do these in order)

1. **Stop.** Do not `git add -A`, `git stash`, `git clean`, or run `button.sh reset`.
2. Count: `git ls-files --deleted | grep -v /pieces/sessions/ | wc -l`. (Tracked
   `pieces/sessions/*` copies are deleted by the engine whenever a pc-hq window
   closes; a few hundred of those are normal.)
3. Safety zip first, from git: `git archive --format=zip --prefix=NNEST-12.00/ -o ~/Desktop/NNEST-backup.zip HEAD`.
4. Restore only the deleted tracked files (keeps your modified ones):
   `git ls-files -z --deleted | xargs -0 -n 4000 git checkout --`
5. Fill in what git never had: extract the newest `x0.parent-level-dev-env-04.04_*.7z`
   to a scratch folder, copy across only files that are missing now (never overwrite),
   skip stale `*.pid` / `*.lock`, and recreate empty directories.
6. Rebuild every compiled program: `sh $.crypts/button.sh build`
   (re-run any FAIL with `sh $.crypts/button.sh build <part of the path>`).
7. Only then relaunch (`sh $.crypts/button.sh reset`, and for pc-hq close the stale
   window and run `open_pchq_board.sh` - the engine copies its programs at launch).
8. Diff against the other branches for fixes that never reached this one:
   `git cherry claude origin/<branch>` (`+` = truly missing, `-` = already there under
   another hash). Cherry-pick with `-x`.

## Side effects of recovery to watch for

- **Stale runtime state re-opens modes.** pc-hq treats the presence of
  `pieces/display/move_range_matrix.txt` as "Move is open" and then arrow keys steer
  the Move placer instead of the xelector. If arrows seem dead after a restore, check
  `@.apps/piececraft-hq/pieces/display/` for `move_range_*.txt` / `placer.txt` (Esc in
  pc-hq closes it).
- **Never edit a CRLF file with Python's default text mode** - it rewrites every line
  ending and turns a 3-line change into a 12,000-line diff. Use `open(p, newline='')`.
  (`khtpm_taskbar_manager.c` is CRLF.)
- A relay-driven test can pass while the real keyboard fails (digits typed via
  `strip_history.txt` vs real keys in the window's own accumulator).

## Why "all the compiled programs" were lost - and the root cause behind a lot of "it broke after a merge"

`.gitignore` has `*.+x`, so **no branch tracks a single compiled program**, and git does
not track empty folders, so `ops/+x/` itself also disappears on a clean/branch switch/
fresh clone. Windows then come up blank with no error (see commit `e0c236482`'s message
for the three silent guards that fail). Every branch shows 0 compiled files.

## Prevention (what was changed)

1. **`ops/+x` and `+x` folders are now tracked** (a `.gitkeep` in each, commit `784fd864b`).
   `*.+x` only ignores names ending in `.+x`, so the folder and its `.gitkeep` are not ignored.
2. **`sh $.crypts/button.sh build [text]`** compiles everything (wraps `compile-runner.sh`),
   and `compile-runner.sh` now recreates the output folders before each build.
3. **`entity-cli/ops/build_entity_cli_ops.sh`** exists, so Act:Move's programs
   (`move_entity_init/tick`, `inventory_op`) are rebuilt by the sweep. They used to be
   hand-compiled and silently vanished.
4. **`rezip-house.sh` is safe now and full by default.** It used to delete the previous
   archive *first*, so running it on a wiped tree replaced a good 34 MB backup with a tiny
   one. Now it builds the new archive to a temp name, checks it (>= 1000 entries), refuses
   to run if >100 tracked files (not counting `pieces/sessions/`) are missing, and only
   then removes older archives. Default = whole working tree **including compiled programs
   and untracked files** (`--source-only` for the old policy). The `.git` folder and the
   vendored `#.NNEST_ASSETS` are excluded; keep `git bundle create ~/Desktop/NNEST.bundle --all`
   for history.
5. **Quit/reset kill lists now include `khtpm_entity.+x`** (found while checking: entity
   windows survived a taskbar quit because the lists still named the retired
   `tp_desktop_window*` programs).
6. Eight missing fixes were cherry-picked from `debian-clean` / `kilo` (pc-hq board black /
   Interact never engaged, dock sizing config, taskbar-settings Size bug, CPU-only board
   build, install-deps CJK check, op-ed interact guard).

## Habits (also in `AGENTS.md`)

- Commit early. Anything uncommitted during this incident (nav-echo work, state edits) was
  lost or had to be rebuilt from memory.
- After any reset, branch switch, or bulk script, run
  `git ls-files --deleted | grep -v /pieces/sessions/ | wc -l`. If it is more than a few
  dozen, **stop and follow the runbook above**.
- Keep an off-tree copy: `sh rezip-house.sh` (full) and a `git bundle` before risky work.
- One worktree per agent remains the real fix for the shared-checkout hazards in `AGENTS.md`.
