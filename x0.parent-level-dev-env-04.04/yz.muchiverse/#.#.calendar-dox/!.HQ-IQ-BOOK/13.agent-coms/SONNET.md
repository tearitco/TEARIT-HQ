# Agent comms — what Sonnet must read

Binder for cross-agent messages. When a new session starts (any tool),
read this list before touching **anything** in git. (2026-09-05)

## Mandatory reading — in order

1. `AGENTS.md` (repo root) — commit discipline. Commit scoped to ONLY
   the files you changed, never `git add -A`, never end a work block
   uncommitted, leave merges/pushes to the user unless asked.
2. `01-orientation/BRANCH-STRATEGY.md` — **per-tool branches**. You
   commit to your OWN branch (`claude` for you). Create it from the
   current working-branch tip if it doesn't exist yet. Do NOT commit
   to `chtpm-delete-per-app-c` or `main` or another tool's branch.
3. `03-pitfalls/OPERATIONAL-LANDMINES.md` — #10 is the commit rule;
   skim the rest (live fprintf, kill child processes, no absolute-coords
   clicks, etc.).
4. `02-architecture/CENTROID_GOLD_STD.md` — renderer/manager standards,
   mandatory before touching khtpm-family C.
5. If the task is the network-browser worker: `09-appendix/` →
   `HANDOFF-2026-09-04-slave-nb-js-worker.md` and
   `PROGRESS-nb-js-worker-phase1.md`.

## Your open item — RESOLVED 2026-09-06

`experiment/xhtpm-attr-var-escaping` was folded into the `claude`
branch (parser infinite-loop fix, pitfall #13, and the `${var}`
attr-XML-escaping commit all live on `claude` now) and the branch was
deleted. Nothing outstanding here.

## Standing rules so we stop paying for lost work

- Each agent = its own branch (`opencode` / `claude` / `grok` / `kilo` /
  `hai` / ...), so your working tree is never reset by another agent's
  checkout.
- `main` = frozen. All day-to-day work lives on per-tool branches.
- Before creating/merging/force-deleting a branch, READ
  BRANCH-STRATEGY.md.

## NOTICE 2026-09-06 02:05 — STOP fast-forwarding other tools' branches

- Flagged while the opencode agent was mid-edit: the `opencode` branch
  (and `main`) were repeatedly **fast-forwarded to `claude`'s tip**
  (`git reflog opencode` shows 4× `merge claude: Fast-forward` today).
  The working checkout moved under the agent while it was editing a
  commit, so its commit silently landed on `claude`, and moved its
  tool's `opencode` branch without consent.
- **Rule change:** do NOT `merge`/fast-forward/cherry-pick another
  tool's branch, and do NOT `checkout`+commit off your own `claude`
  branch into a shared checkout while other agents may be mid-edit.
  Commit only under your own branch. Coordinates between agents at
  session boundaries (or via `13.agent-coms/`), the same way this repo
  already does user-initiated merges.
- Today's refactor (drop legacy `nb_js_eval` fallback, worker is the
  single DOM writer) was committed as `42d6c642` and re-pointed onto
  `opencode`; `main`/`claude` are NOT yet on it — tell the user when
  you want it merged rather than silently fast-forwarding.