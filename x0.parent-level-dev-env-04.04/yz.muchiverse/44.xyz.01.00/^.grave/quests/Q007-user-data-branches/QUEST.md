# Q007 — per-user data branches: take `xyzfs/users` out of code history

| field | value |
|---|---|
| status | active — steps 1-5 and 8 DONE on `claude` (2026-10-06); steps 6-7 + other branches open; data branches are LOCAL ONLY, do not push |
| tier | manager (claude) or a careful outside-agent; touches every branch |
| size | L |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | the 4 open questions in the design doc §6; OK before untracking on any code branch |

## Mission (one sentence)

Make each desk user's data folder its own git checkout on its own orphan branch (`user/jb` for the owner) and stop tracking `xyzfs/users` in the code branches, so code merges never fight live runtime state.

## Why it matters

Fast-forwarding `claude` on 2026-10-06 was blocked by 52 locally modified or untracked runtime files; 1,710 files under `xyzfs/users` are tracked and about 91 change on a normal day. Design: `USER-DATA-BRANCHES-DESIGN.md`.

## Read first

1. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/USER-DATA-BRANCHES-DESIGN.md` (all of it, esp. §4 order and §5 risks)
2. `AGENTS.md` (shared-index rules, never `git add -A`, never stash) and `03-pitfalls/INCIDENT-2026-10-05-WORKING-TREE-WIPE.md`
3. `hai-manager-role` memory: the 2026-10-06 key-file deletion lesson (a fast-forward past an untracking commit deletes the files from the working tree)

## Do (in this order, stop at each owner gate)

1. Read-only: `git ls-files xyzfs/users | wc -l`, per-folder counts, and every code/script/test path that reads `xyzfs/users/...` starter data (`grep -rn`); write the list into `## Log`.
2. Full backup of `xyzfs/users` (tarball + `sha256sum` list) OUTSIDE the repo; verify the file count equals the live tree.
3. Dry-run script (read-only) printing: files that would leave code tracking; files needed as seed data; sensitive files (`wallet.txt`). Paste its output here. **Owner reviews.**
4. (owner OK) Orphan branch `user/jb` + linked worktree at the owner's user folder; first snapshot commit; verify count and checksums against the backup.
5. (owner OK) On a code branch in a scratch worktree FIRST: `git rm -r --cached xyzfs/users` + `.gitignore`; check out another branch that still tracks the files and prove the live files survive (they must not be deleted). Only then do it on the real branches in one session.
6. Template user `xyzfs/_seed/`; test: fresh clone + `button.sh build` + first run.
7. Data-commit op for `user/<name>` (on desktop quit or schedule).

## Acceptance

- [ ] Backup verified (file count + checksums) before any change; path recorded in `## Log`.
- [ ] After step 5: `git status` on `claude` shows no `xyzfs/users` noise while the desktop runs; the live files are byte-identical to the backup.
- [ ] A fast-forward between two code branches succeeds with the desktop running and entities writing (the 2026-10-06 failure case).
- [ ] Fresh clone works with the seed user (step 6).
- [ ] Windows branch (`opencode-win32`) paths checked and noted.

## Rules

Everything in `^.grave/README.md` and `AGENTS.md`. No agent live in the checkout during steps 4–5. Never delete or `git clean` anything under `xyzfs`.

## Log

2026-10-06 | claude | step 1-3 done read-only. `dryrun.sh` (this folder): 1,725 tracked files under `xyzfs/users` (83 modified that day; owner folder 1,251, another user 349), 18 tracked `wallet.txt`, no code/script found reading `xyzfs/users` paths (grep outside .md/.txt only: not proof).
2026-10-06 | claude | backup: `/home/no/Desktop/github/work/nnest_xyzfs_backup_20261006/` (`xyzfs_users.tar` + `xyzfs_users.sha256`), 1,920 files = live count.
2026-10-06 | claude | TEST TREE (scratch worktrees only, all removed after; live checkout untouched; git 2.34 has no `worktree add --orphan`, used `--no-checkout` + `checkout --orphan`):
  - A untrack in place (`git rm -r --cached` + `.gitignore` + commit): disk files 1725 -> 1725, fingerprint identical; `git status` noise from xyzfs/users 0; tracked 0. **Safe.**
  - C merge a still-tracking branch (origin/kilo, 791 xyzfs files differ) into the untracked branch: 1 conflict (add/add), disk unchanged during the merge and after `--abort`. **Safe for disk** (limited: kilo is mostly behind claude, so this is not a full modify/delete stress test).
  - B move a tree from a TRACKING commit to the UNTRACKED branch with `git checkout`: **all 1725 files deleted from disk** (the 2026-10-06 key-file failure, reproduced). Safe way: on a tree already at the parent commit, `git symbolic-ref HEAD refs/heads/<untracked-branch>; git reset --mixed <untracked-branch>` (index only): disk 1725 -> 1725 untouched (1 leftover status line to look at).
  - D orphan data branch restored from the backup tarball: commit has no parent (shares no history), 1,920 files = backup, checksum list MATCHES.
  - Rule that falls out: **never `git checkout`/`ff` a live tree from a tracking branch onto the untracking commit; use the index-only move above, or untrack with `git rm --cached` on the live tree itself. Do it on every active branch in one session.**

2026-10-06 | claude | REAL TREE (owner: "lets do it"). Desktop quit first so the data was quiet. Backup `nnest_xyzfs_backup_20261006_quiet/` (tar + sha256, 1,920 files). Step 4: 24 orphan branches built with plumbing (temporary index, `--work-tree`, no working-tree writes): `user/jb` (owner, 1,446 files), `user/584a4c9b` (349), `user/e8bf229a` (23), ..., `user/guest-747d5f6c`; every branch verified blob-for-blob against the disk, total 1,920 = live, 0 problems, `user/jb` has no parent.
2026-10-06 | claude | Step 5 on `claude` only (commit `09cd41ae4`): `git rm -r --cached xyzfs/users` + `.gitignore **/xyzfs/users/`; disk 1,920 files with identical fingerprint before/after, tracked 0, `git status` noise 0. Desktop relaunched and healthy (17 entities).
2026-10-06 | claude | NOT done: other branches (opencode, kilo, grok, main, ...) still track the files: when merged into `claude`, resolve by keeping them deleted; NEVER `git checkout`/ff a live tree onto an untracked branch from a tracking one. Worktrees `NNEST-12.00-halo` and `.kilo/worktrees/attrition` still hold their own tracked copies. Steps 6 (template user) and 7 (`jb` code branch) open.
2026-10-06 | claude | Step 8 DONE: `$.crypts/save-user-data.sh` (plumbing only, no working-tree writes, compare-and-swap on the branch tip, idempotent; unchanged run 1.2 s) + `button.sh save-data` + automatic call after `quit` and `reset` kill the desktop. First real run advanced only `user/jb` (9f67d8b96); second run committed nothing. Bug found and fixed while testing: `git rev-parse --git-common-dir` is relative to the cwd, not the repo root (every user failed safe, nothing written). Data branches are local only (they hold `wallet.txt`).

## Result
