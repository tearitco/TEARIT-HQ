# Q007 — per-user data branches: take `xyzfs/users` out of code history

| field | value |
|---|---|
| status | open — design written; step 2 (backup) and 3 (dry run) may start; steps 4+ need the owner's OK |
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

## Result
