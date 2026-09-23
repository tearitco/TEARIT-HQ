# FRESH-AGENT HANDOFF — NB-JS network-browser engine cell
**2026-09-23 — distinct handoff for a fresh agent. House branch: `opencode` (this is the ONLY house with a git remote; never push to main/claude/grok).**

## If you are picking this up cold
This is the resident-JS network-browser worker cell (`&.hq-apps/network`). It is already a real,
shippable engine — rungs 3 (event loop + timers + lifecycle) and 4 (XHR/fetch via manager RPC)
are **SHIPPED and in-tree**, not future work. The plan doc's own §8.1/§8.2 prose lagged the tree;
the authoritative state is the SHIPPED-LOG in the plan (`§8.6`) plus the anchors below.

## Verified state (read before touching anything)
- `origin/opencode` parity with `HEAD`: **0 0** (fully landed; this handoff is the next single commit).
- Gate: `make nbjs` in the network ops Makefile must exit **0 = GREEN** before ANY push.
- Worker: `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.hq-apps/network/ops/nb_js_worker.c`
- Plan: `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md`

## The land law (hard; do not bend)
1. Bounded, one at a time. Verify (read-only) before you touch.
2. GREEN gate BEFORE any push; parity `0 0` before and `0 0` after; one non-force push
   (`git push origin opencode:opencode`); re-verify after.
3. No `--force`, no force-with-lease, no chasing a "moving" remote — if parity is non-zero,
   STOP and report; do not chase.
4. Never commit the runtime/never-source house files (`porcelain` will show many; they ride in
   the worktree by design, never into commits) — only commit the one thing the task names.
5. If unsure how to proceed: stop and ask. "Land a rung" means: read §8.6 SHIPPED-LOG first;
   if the rung you were told to land is already there, report it SHIPPED and re-point — do not
   re-land.

## Documented house pitfalls (nested in the house book; skim OPERATIONAL-LANDMINES.md)
git rebase --autostash refused on the never-source runtime files; parity misread as remote
movement — always verify remote-tip timestamp vs your own first artifact. Fully-documented in
the house landmine file (03-pitfalls).

## What this cell needs next (BOUNDED — pick ONE)
The rung order that is NOT yet in-tree (the actual frontier, to be verified by grep first):
1. **Real BOM remainder per plan §8.3** — history/URL real pushState, file-backed cookie jar,
   real location.assign (manager-level). 
2. **Client-side routing / back-forward** (rung 5 real).
3. Deferred (per plan, not a goal): layout/paint measure (getBoundingClientRect).

Commits land GREEN, each independently shippable, one per bounded pass.
