# Branch strategy

How work is organized in this repo (2026-09-05).

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
  tip (`git checkout -b <tool> chtpm-delete-per-app-c`), commit there,
  and leave cross-branch `merge`/`cherry-pick` and `push` to the user
  unless explicitly asked.
- Delete dead local branches (they survive on origin). But **never
  force-delete a branch with unique, unmerged, local-only commits** —
  push it to origin first or leave it alone.

## Open item for Claude Sonnet

`experiment/xhtpm-attr-var-escaping` (tip `01aee649`, local-only) holds
unmerged experiment commits: "XML-escape \${var} values spliced into
quoted xhtpm attributes" on top of the docs commit `0ea2e326`. It is
NOT on origin and was deliberately NOT deleted (2026-09-05). Next
session: either push it to origin or fold its content into your branch,
then delete it. Do not leave it orphaned a second time.