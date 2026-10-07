---
name: tearit-git-workflow
description: Branch strategy, worktree setup, commit discipline, and sync protocol for TEARIT-HQ. Read before any git work.
---

# TEARIT-HQ Git Workflow

## Per-tool branches — never shared

Every coding agent gets its **own named branch**, never shared. This prevents agents stepping on each other's working trees.

- **opencode** — the opencode agent
- **claude** — Claude Sonnet
- **grok / kilo / hai / <tool>** — any other agent: its own branch
- **chtpm-delete-per-app-c** — previous all-agents working branch; superseded, kept as base
- **main** — frozen point of reference; reserved for review-grade merges only

## The fix: one folder per agent — `git worktree`

Per-tool branches alone did NOT stop agents stepping on each other, because every agent runs in the **same folder** which has one shared `HEAD`. Run once, all agents idle:

```sh
cd /home/no/Desktop/github/work/NNEST-12.00
git worktree add ../NNEST-12.00-claude   claude
git worktree add ../NNEST-12.00-opencode opencode
```

Claude sessions run in `../NNEST-12.00-claude/`, opencode in `../NNEST-12.00-opencode/`, etc. Original stays on `main` (reference only, don't edit code there).

## Commit discipline

- An agent **only ever commits to its own branch**, scoped to exactly the files it changed (`git add <path>` per file, never `git add -A`). Runtime state files (`module_parent.pid`, `*.pdl`, logs, `cli_io_state.txt`) are noise — never swept into a code commit.
- **Never end a work block with uncommitted code.** Uncommitted work is fire-able — it died once (nb-js-worker step-6/7 set lost when tree got reset). Mid-work snapshots may use `wip: <what>`. Push only when the user asks.
- Never merge, cherry-pick across branches, push, or force-delete unprompted — leave that to the user.
- Match the repo's commit-message style: `fix:`, `docs:`, `scoped feature: ...`.

## Concurrent agents in one checkout

If two agents share one checkout (same `.git/index`):
- Stage ONLY your own paths, and commit ONLY your own paths. If something you did not author is already in the index, unstage it (`git restore --staged <path>`) rather than sweeping it in.
- Never `git reset` with anything staged that you did not stage. If you must reset, use `git reset -- <path>` on your own files.
- Never `git stash` — it is shared and the other agent loses its work.
- Before committing, run `git diff --cached --name-only` and read the list. It must be exactly your files.

## Sync protocol

Every work block starts and ends like this from your own worktree:

```sh
git fetch origin                     # get everyone's latest
git merge origin/claude              # pull the latest Claude work in
# ... do your work, commit on your branch ...
git push origin <your-branch>        # push after EVERY commit, no exceptions
```

- **Merge** (never rebase) the sibling agents' tips into your branch. A merge keeps your commits reachable no matter what another agent does next; rebases orphan work.
- Never fast-forward, rebase, or delete **another** agent's branch.
- If genuinely unsure whether a target branch is clean-behind or has real divergent commits, check first (`git merge-base --is-ancestor` / `git log A..B`) rather than assuming either way.
