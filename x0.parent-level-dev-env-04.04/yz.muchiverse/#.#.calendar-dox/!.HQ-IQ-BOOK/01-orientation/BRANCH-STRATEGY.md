# Branch strategy

How work is organized in this repo (2026-09-05; worktree section added
2026-09-06).

> **New to git, or want the "why nothing is lost" explanation?** Read
> `GIT-WORKFLOW-FOR-BEGINNERS.md` (same folder) first — it covers what
> git is doing here, the agent-collision that keeps happening, and the
> fix below in plain language.

## THE FIX (2026-09-06): one folder per agent — `git worktree`

Per-tool *branches* alone did NOT stop agents stepping on each other,
because every agent runs in the **same folder**
(`/home/no/Desktop/github/work/NNEST-12.00/`) which has one shared
`HEAD` — so one agent's `git checkout` swaps files and moves refs for
the other. Recovered every time via `git reflog`; zero code lost; hours
wasted.

Real fix — give each agent its own folder locked to its own branch.
Run once, all agents idle:

```sh
cd /home/no/Desktop/github/work/NNEST-12.00
git worktree add ../NNEST-12.00-claude   claude
git worktree add ../NNEST-12.00-opencode opencode
```

- Claude sessions run in `../NNEST-12.00-claude/`, opencode in
  `../NNEST-12.00-opencode/`, etc.
- Original `NNEST-12.00/` stays on `main` (reference only, don't edit
  code there).
- `git checkout <other agent's branch>` from your worktree is then
  **refused by git** — collision impossible.
- `git worktree list` / `git worktree remove <path>` to inspect/clean.

Until that's set up, follow the discipline rule in
`GIT-WORKFLOW-FOR-BEGINNERS.md` §4 (know your branch; commit + push
ONLY your own; never touch another branch or `main`).

## One working branch per tool

Every coding agent gets its **own named branch**, never shared. This is
so nobody ever steps on another agent's working tree, uncommitted code,
or push — the exact failure that cost the nb-js-worker step-6/7 set
(2026-09-05).

- **opencode** — the opencode agent (this account).
- **claude** — Claude Sonnet.
- **grok / kilo / hai / <tool>** — any other agent: its own branch.
- **chtpm-delete-per-app-c** — the previous all-agents working branch;
  now superseded by the per-tool branches (kept as the base; never add
  new work to it).
- **main** — frozen point of reference; reserved for review-grade
  merges only.

## Rules

- An agent **only ever commits to its own branch**, scoped to exactly
  the files it changed (`git add <path>` per file, never `git add -A`).
  Runtime state files (`module_parent.pid`, `*.pdl`, logs,
  `cli_io_state.txt`) are noise — never swept into a code commit.
- Never end a work block with uncommitted code (see
  `03-pitfalls/OPERATIONAL-LANDMINES.md` #10). Mid-work snapshots may
  use `wip: <what>` as the message.
- Branch-native: create your branch from the current working-branch
  tip (`git checkout -b <tool> chtpm-delete-per-app-c`), commit there.
  The user made two standing exceptions explicit on 2026-09-06: agents
  ALWAYS push their own branch after a work block, and any branch
  riding on top of `claude` ALWAYS merges the latest `origin/claude`
  before working (see the Sync protocol below). Everything else —
  merging/cherry-picking between agents, force-pushing, deleting a
  branch with unique commits — still waits for the user.
- Delete dead local branches (they survive on origin). But **never
  force-delete a branch with unique, unmerged, local-only commits** —
  push it to origin first or leave it alone.

## Sync protocol (`push your branch, always`) — 2026-09-06

Standing user directive (in plain words: "always push to your branch,
pull Claude's fixes into yours"). Every work block starts and ends like
this from your own worktree:

```sh
git fetch origin                     # get everyone's latest
git merge origin/claude              # pull the latest Claude work in
# ... do your work, commit on your branch ...
git push origin <your-branch>        # push after EVERY commit, no exceptions
```

- **Push** your own branch after every commit. Unpushed work dies
  quietly; pushed work survives. This is the new default, not an
  exception.
- **Merge** (never rebase) `origin/claude` into your branch. A merge
  keeps your commits reachable no matter what another agent does next;
  rebases are exactly the thing that orphaned work before.
- Conflicts are normal in shared docs when both agents touch them —
  resolve on your side, keeping both agents' content.
- Never fast-forward, rebase, or delete **another** agent's branch.
- Riding branches thus always sit **on top of** the latest `claude`:
  a Claude rebase can't strand opencode work because opencode carries a
  forward-looking merge, not a stale parent.

## Open item for Claude Sonnet — RESOLVED 2026-09-06

`experiment/xhtpm-attr-var-escaping` was folded into `claude` (the
parser infinite-loop fix, pitfall #13, and the `${var}` attr-XML-
escaping commit are all on `claude`) and the branch was deleted.
`chtpm-delete-per-app-c` was also reset back to `d91b1798` to drop a
stray merge, and this branch (`opencode`) was rebased off it. Nothing
outstanding.