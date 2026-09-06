# AGENTS.md

House rules for any coding agent working in this repo. Read before
starting work.

## Commit discipline (REQUIRED, unprompted)

- Never end a work block with uncommitted code. Uncommitted work is
  fire-able — the nb-js-worker step-6/7 set was lost exactly this way
  (2026-09-05).
- At the end of every session, commit scoped to ONLY the files you
  changed (git add `<path>` per file, never `git add -A`). Runtime
  state files (`module_parent.pid`, `*.pdl`, logs, `cli_io_state.txt`)
  are noise — never sweep them into a code commit.
- Mid-work snapshots may use `wip: <what>` as the message.
- Each tool commits to its OWN named branch (`opencode` for this agent,
  `claude` for Sonnet, `grok`/`kilo`/`hai` for theirs). Never commit
  to another tool's branch, `main`, or the old all-agents branch
  `chtpm-delete-per-app-c`. See HQ-IQ-BOOK `01-orientation/
  BRANCH-STRATEGY.md`.
- Never merge, cherry-pick across branches, push, or force-delete
  unprompted — leave that to the user.
- Match the repo's commit-message style (full thoughts in "Explain tracked code" tone:
  `fix:`, `docs:`, `scoped feature: ...`).

## Source of truth

Full house conventions live in the HQ-IQ-BOOK:
`x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/`
- 03-pitfalls/OPERATIONAL-LANDMINES.md — operational rules (#10 is the
  commit rule).
- 02-architecture/CENTROID_GOLD_STD.md — renderer/manager standards.

## Verification

Never report "fixed/done" without a fresh build + fresh run + real
evidence (diff, screenshot, state file). A clean compile is not
evidence.