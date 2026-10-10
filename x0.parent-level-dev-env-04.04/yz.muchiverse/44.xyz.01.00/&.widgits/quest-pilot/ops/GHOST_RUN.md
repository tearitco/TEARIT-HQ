# ghost_run - one attempt of one delegated quest, end to end

Automates the manual q019 process (ROBOT-WORKFORCE-GAMEPLAN sections 3, 4, 10).
Build: `sh ops/build_ghost_run.sh` -> `ops/+x/ghost_run.+x` (needs `ops/+x/quest_check.+x` next to it: `sh ops/build_quest_check.sh`).
Self-contained C, fork+exec only (git, quest_check, backend, harness), no shell, POSIX.

    ghost_run <quest_dir> <target_branch_ref> <repo_dir> [--ghost <id>] [--worktrees <dir>]
              [--backend <exe>] [--backend-root <dir>] [--dry-run]

Defaults: ghost `ghost`, worktrees `/home/no/staging/ghosts`, backend `^.hai-horn/ops/+x/horn_chat_backend.+x`,
backend root = the house root (keys under `<root>/&.widgits/open-hai/state/`; never read by ghost_run).
The backend is run with `HORN_TOOLS=off`, `HORN_CURL_TIMEOUT=<max_seconds>`, `PRISC_PROJECT_ROOT`+cwd = backend root.

## Quest dir
`RUN.pdl` rows (exactly one each): `OUTPUT | <repo-relative file>`, `HARNESS | <command, split on spaces, cwd = worktree>`,
`VERDICT_FILE | <worktree path whose LAST line is the verdict: PASS and no FAIL>`. Plus `prompt.txt` (sent verbatim as argv[1]),
`scope.txt`, `LOCK.sha256`, `budget.pdl`, `harness/` (all as in quest_check). OUTPUT/VERDICT_FILE with `..`, `.git`, absolute or empty parts are refused.

## Steps (stop at the first failure)
1 precheck: quest_check LOCK/BUDGET/ATTEMPTS against repo_dir; N = existing attempts + 1, refuse if N > max_attempts.
2 `git worktree add -b ghost/<id>/<quest>-aNNN <worktrees>/<id>/<quest>-aNNN <ref>` (refuses if branch or path exists).
3 backend call under a max_seconds watchdog (process group killed, reaped). 4 first fenced block of the reply (unterminated fence: the rest;
no fence: `no-code-block`) written to OUTPUT only (dirs created under it; symlinked parts refused). 5 full quest_check (SCOPE and BASE must pass).
6 HARNESS (own max_seconds timeout), stale VERDICT_FILE removed first, then its last line judged. 7 append `attempts/NNN/`. 8 final line.

## Attempt record (append-only; an old attempt is never edited)
`attempts/NNN/`: `meta.pdl` (`ATTEMPT | n | ghost | ts`, `BACKEND | 0..6,10|timeout|signalN|exec-failed`, `PROVIDER | <stderr line>`,
`WALL | s`, `RESULT | ...`, `REASON`, `BRANCH`, `WORKTREE`), `reply.txt` (raw), `output.diff` (worker change vs base, taken before the harness runs),
`harness.log` (64 KB cap), `verdict.txt`, plus `backend_stderr.txt`, `check.txt` (quest_check lines).
Attempts are recorded once a worktree exists (backend failure, no block, violation and FAIL all spend an attempt). Precheck refusals and `--dry-run` write nothing.

## Output and exit codes
Last stdout line: `GHOST|<quest>|attempt=N|RESULT|<PASS|FAIL|backend-failed|no-code-block|VIOLATION-check|REFUSED-...|DRYRUN>|worktree=<path>`.
0 PASS, 10 harness FAIL, 11 backend failure/timeout, 12 no-code-block (or OUTPUT unwritable), 13 precheck refused, 14 scope/base/lock violated, 2 usage.
Never merges, pushes or touches the target branch; the worktree stays for the manager to review.

## Verify
`_shared-lib/harness/ghost_run.pal` + `cases/ghost_run.pdl` (fake backend + fake judge, no network, 197 checks, 19 scenarios).
From `_shared-lib/harness`: `/tmp/prisc_x ghost_run.pal`, read `results/ghost_run.txt.verdict.txt`.

## Approval gate
To have the owner see and approve every API call a worker makes, pass `--backend <house>/&.hq-apps/co-lab-hai/ops/colab_api_gate.sh`; see `&.hq-apps/co-lab-hai/COLAB-API-GATE.md` (give the quest max_seconds 1800+: approval waiting counts).
