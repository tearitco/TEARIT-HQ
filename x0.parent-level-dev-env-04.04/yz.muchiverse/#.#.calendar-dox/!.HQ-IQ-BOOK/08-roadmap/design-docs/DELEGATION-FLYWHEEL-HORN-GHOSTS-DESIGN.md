# The delegation flywheel: spec + locked harness + cheap workers (HORN/OpenRouter via ghosts and phones) + saved learning

Status: DESIGN, 2026-10-07, claude. Owner question: "what is something we can do that gives a wide blast radius, empowers the API ^.hai horn/halo interfaces to be delegated to, conserves tokens, gives maximum synergy and momentum in all these projects? Wouldn't it be delegating tasks to OpenRouter, even thru ghosts/phones, then having them save their learning harnesses when meta-building this stuff?"
Builds on (read, not re-invented): `HARNESS-DELEGATION-PIPELINE.md` (the "harnesses are our business, delegate to save tokens" instruction of 2026-08-13; the gap: no adaptive multi-step chaining), `$.claude-hai-budget.md` (the standing delegation habit), `GRAVEYARD-GHOSTS-DESIGN.md` (quests, ghosts, tiers worker=HORN/OpenRouter, student=local), `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` (phones: task/lease/release/result messages, ghosts report to their grave), `HARNESS-BEHAVIOR-BANK-DESIGN.md`, `HARNESS-STORE-AND-SCORING-DESIGN.md`, `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md` sections 11-12 (souls are ghosts that possess entities), the Joint contract in `XO/13.phymoji-engine/02-ROADMAP-TECHNICAL-SPECIFICATION.md` section 0.

## 1. Yes: this is the right lever, and this week already proved the shape
Everything built in this stretch followed one loop: **a spec with a machine-checkable acceptance harness, handed to a worker that iterates until the harness says PASS, then reviewed by diff and verdict, not by re-reading everything.** The workers were Claude subagents (expensive tokens). The same loop works with **cheaper workers** (OpenRouter models through HORN, local student models) because **the harness, not the worker, decides pass/fail** ("the model saying done means nothing"). That makes the cheap worker's weaker judgment irrelevant: it only has to grind until the referee is green.
The blast radius comes from three compounding things: (1) **cost**: Claude writes only the spec, the cases and the review; the iteration tokens go to cheap workers; (2) **parallelism**: many quests at once in separate worktrees, each with its own harness; (3) **compounding assets**: every quest leaves behind a saved, scored harness, case rows and a worker grade, so the next quest starts richer (the flywheel).

## 2. The flywheel (one quest)
1. **Spec** (Claude or owner): a quest packet `QUEST.md` (goal, files allowed, files forbidden, data rules) + **acceptance harness** (pal + cases + referee) + a **budget** (max iterations, max tokens, wall clock).
2. **Lock**: the harness, cases and referee are **hash-locked and read-only to the worker** (separate path, sha256 recorded in the quest). A worker that edits its own grader is the main cheating risk; the runner refuses a diff that touches a locked path, and re-verifies the hash before accepting.
3. **Dispatch**: the gravestone assigns the quest to a ghost; the ghost's brain is a HORN worker (OpenRouter model chosen by the router, section 5). Transport = **phone messages** (`task` to the ghost, `lease`/`release`, `status`, `result`, `ask-human` for gated actions), exactly as designed, so the whole conversation is a readable append-only history.
4. **Work in a sandbox**: the ghost works in its **own git worktree on its own branch** (`hai-<ghost>`; AGENTS.md: each tool commits to its own named branch), never the live tree, never `xyzfs/users`, never another tool's branch. HORN's approval gate for write/edit/exec stays on for anything outside the sandbox. Secrets: workers never read key files; the HORN runtime holds provider keys.
5. **Loop**: edit -> build -> run the locked harness -> read the verdict -> repeat, until `VERDICT|PASS` or the budget is spent. Fork+exec+waitpid with a watchdog for every run (house rule), results appended to the quest ledger.
6. **Return**: the ghost posts `result` (branch, commit, verdict line, iteration count, token spend). Claude reviews **the diff + verdict + referee output only** (a few hundred tokens), runs the regressions, and the owner decides any merge (no worker merges, pushes or force-deletes).
7. **Save the learning**: see section 4. Then the next quest.

## 3. Why "wide blast radius": what to delegate first
Rank by (cost saved) x (how well a harness can judge it) x (how often the pattern repeats):
| Task family | Why it fits | Already has the harness shape |
|---|---|---|
| New tiny ops (one verb, compiled C, header = usage and exit codes) | pure spec in, harness out; dozens needed | ring-board/footrace/eden ops |
| Rule variants and data (`*.pdl` for Booty Land, B.O.T.S. tables, Time-Fu PDF variants, TSC) | data authoring against a referee; placeholders marked | `race_audit_op`, `ring_audit_op` |
| Event-page sets for a rule list | repetitive, referee-checked | footrace (182 pages) |
| Mutation tester / sharpness scoring (mutate an op or page, count how many mutants the harness catches) | gives every harness a number and finds weak harnesses | harness store design |
| New harness cases from failures and from reviewer feedback | directly grows the asset | harness behavior bank |
| Doc indexing, XO sync, roadmap upkeep | mechanical, link/consistency checks | grep/diff referee |
| Relay scenarios and multi-model sweeps | named as the first offload in the budget doc | Phase 4 relay-harness design |
Keep in Claude: shared-state wiring decisions, taskbar/renderer files, anything touching live data, architecture, harness design for new areas (the spec), and final review.

## 4. Saving the learning ("save their learning harnesses when meta-building")
Every quest, pass or fail, leaves **durable, scored artifacts** (append-only, under the quest and the ghost):
1. **Trace ledger** `quest_ledger.txt`: iteration rows `ITER | n | action | verdict | tokens | wall_ms`.
2. **Failure -> case**: each distinct failure the harness exposed that the original cases did not name becomes a **candidate case row** (`CAND_CASE | quest | input | expected | basis=trace`) for the area's harness; accepted only after it passes on the final build and fails on a surviving mutant (so it adds sharpness).
3. **Worker grade** per (model, task family): `grade = (reward+1)/(reward+punish+2)` from accepted vs rejected quests (the NIGHT 34 formula); also first-try pass rate, mean iterations, mean tokens per accepted quest.
4. **Behavior-bank rows** (`HARNESS-BEHAVIOR-BANK-DESIGN.md`): the prompt/spec shape that worked for a task family, so the next packet of that family is pre-filled.
5. **Meta-building**: ghosts also build harnesses and ops that help build harnesses (mutation tester, case generators, quest-packet templates, assert ops). Those are themselves quests, harness-gated, and are **registered in the harness store** with sharpness/breadth scores (sellable in the store, per the owner's earlier decision).
Souls (ghosts possessing entities) use the same machinery at game scale: a soul's tasks are quests, its learning is the same ledger and grade, and its harnesses are the game's referees.

## 5. Router (cheapest sufficient worker) and escalation ladder
`router.pdl`: `ROUTE | family | min_grade | order = local_student, cheap_openrouter, mid_openrouter, strong_openrouter, claude`. A quest starts at the cheapest tier whose grade for that family is above `min_grade`; failing the budget at a tier escalates one tier up with the trace attached (the stronger worker sees what failed). Grades, `min_grade`, budgets and the order are **joints** (Joint contract: bounded steps, ledgered, gated, default autonomy 0; the router may propose routing changes, a person approves; nothing about merging or promotion is automated). New or unknown families start at a mid tier with a small budget.

## 6. Guardrails (the house has been burned; these are not optional)
- **Distrust by default, verify by harness** (the owner distrusts unverified agent commits; the grok branch precedent). Acceptance = locked harness PASS + regressions PASS + referee + diff scan (no locked paths, no user data, no secrets, no brand words, no new `.sh` tests, no `system()`), then human merge.
- **Prove the harness can fail** (every harness must have a negative case) before it grades a worker; workers cannot edit it.
- **Sandbox**: separate worktree per ghost, scratch `/tmp` roots for runs, no writes to `xyzfs/users`, no stash/reset/`add -A`, no push by workers (the manager pushes after review). Concurrent agents share one index in the live checkout: workers never operate there.
- **Spend limits**: per-quest and per-day token ceilings; a hard stop row `BUDGET|exceeded`; a kill switch (a flag file the runner polls, not `pkill -f`).
- **Data privacy**: only the quest packet and allowed files leave the machine; no wallet, key or user desk content in any prompt; a prompt linter op refuses paths on a deny list before dispatch.
- **Models never decide numbers or promotion**; they write code and data that deterministic referees judge.

## 7. What to build first (the enabling piece, then a measured pilot)
1. **`quest_runner` op** (compiled C, fork/exec/waitpid, budget, ledger): takes a quest dir, creates the worktree, runs the loop with a HORN worker, enforces the hash lock, writes the result. (Reuses `horn_turn`, `horn_tool_exec`, the FSM `run_queue/run_plan` ideas, `game_snapshot_op` for scratch state.) Harness for the runner itself: a fake worker that edits a locked file must be refused; a fake worker that passes must be accepted; a budget overrun must stop.
2. **Quest packet template + prompt linter** (data + small op).
3. **Mutation tester op** (sharpness for any harness).
4. **Pilot of three quests**, one per family, measured: (a) a new tiny op with cases written by Claude, (b) a Booty Land board/rules data set against a referee, (c) the mutation tester itself. Record: Claude tokens spent per accepted quest vs the same task done directly, first-try pass rate, iterations, regressions caught, review time, any cheating attempt.
5. If the pilot pays: router grades, ghost roster, phone transport as default, then scale to the backlog.
**Open facts I have NOT verified:** that HORN reaches OpenRouter from this machine right now (key presence, rate limits, current model prices), how well cheap models handle the house idioms (pal assembly with a 1024-instruction limit, no header sharing, pipe-row formats) without examples, and the real cost per quest. The pilot answers these before anything scales.

## 8. Metrics (counted, not asserted)
Claude tokens per accepted quest; worker tokens per accepted quest; first-try pass rate; mean iterations; regression escapes (merged work later found broken); harness sharpness before/after; cases added per quest; share of the backlog delegated; time from spec to merged. Reported in the quest ledger and rolled into the roadmap status table.

## 9. Open questions for the owner
1. Budget: a daily OpenRouter ceiling and which models are allowed per tier?
2. Which three pilot quests (my suggestion above), and may the pilot ghosts run now in a worktree under `/home/no/staging/` (not the live tree)?
3. Should the soul/ghost phones be the default transport right away, or start with a plain quest folder and add phones once the loop works (suggested: start plain, add phones second)?
4. Who reviews: you only, or may Claude merge worker output into alpha after harness + review (never into live)?

## 10. Pilot q001 result (2026-10-07) and the owner's next ideas
**First quest run by hand** (quest_runner not built): `clamp_op <value> <min> <max>`, a tiny C op. Packet and ledger in alpha `&.widgits/quest-pilot/q001-clamp/` (commit on `claude-alpha`), locked harness `quest_q001_clamp` (37 checks: compiles with `-Wall -Wextra -Werror`, 25 runs).
- **Harness proven before dispatch:** my reference implementation passed 37/0, a stub failed 25 checks, and the reference **found a flaw in my own case** (the case runner trims a leading space in an argument, so a correct program failed a "space" case); that case was removed before any model saw it.
- **Worker:** `nvidia/nemotron-3-ultra-550b-a55b:free` through OpenRouter, one call, 18.7 s, 1,161 tokens (906 completion, 443 of them reasoning), **cost 0**. Result: **`VERDICT|PASS passed=37 failed=0` on iteration 1**; locked file hashes unchanged; diff scan clean (stdio/stdlib/string/errno/ctype/limits only; no system/popen/exec/fopen).
- **Claude's cost:** the spec (about 940 bytes), the cases and this review. Not measured against doing it directly; one sample, one easy task: **no conclusion about harder tasks yet.**
- **Finding about HORN itself:** `horn_chat_openrouter.+x` failed on all three `:free` models after about 2 minutes (it sets no `max_tokens`, caps curl at 60 s, deletes the response before anyone can read the error, and needs `&.widgits/entity-cli/ops/json_parser.+x`, which is not compiled in the live tree). The same endpoint called directly worked. Fixing the op (log the error body, configurable timeout and max_tokens, build json_parser) belongs to the HORN lane; the pilot used a direct call. The key file was only read inside a shell expansion, never printed; prompts carried the spec only (HORN_DIR pointed at a scratch folder so no concept-bank context was sent).
**Owner ideas (2026-10-07):** (1) **another robot as a router**: a desktop robot entity whose job is the router (`router.pdl`: pick the cheapest sufficient worker per task family, escalate on failure, log the choice), reachable by chat and by phone; (2) an **`eden-test` page** (like `dsr-test`: a copy of the Eden page made with `desk_copy_op`, so tests and ghosts can run against a copy and leave the original Eden untouched) where the harness-gated quests run. Build order: fix the HORN op's error handling; `quest_runner`; a second pilot on a harder family (event pages or rule data against a referee); the router robot; the `eden-test` page.

## 11. Owner intent: pre-design eden-test
See `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md` section 13: `eden-test` is the pre-design/staging page (like `dsr-test`) where pilots, ghosts and quests run before the real Eden. Not built yet.

## 12. Correction to section 10 (same day): the HORN failures were a daily rate limit, and the op hid it
After `json_parser` and the HORN build were fixed (build lines added; `commit fc20d35a9`, `76645de67`), the real `horn_chat_openrouter.+x` still failed on all three models, now in about 1 second each. Reproducing its request by hand showed OpenRouter returning **HTTP 429 `free-models-per-day`: limit 50, remaining 0** (key is on the free tier; the earlier failed attempts and other use had used the day's 50 free requests; resets 2026-10-08 00:00 UTC). So my section 10 guess (60 s curl cap, no `max_tokens`) was **not shown to be the cause**; the confirmed defect is that the op **swallows the HTTP error body** and prints only "failed, trying next". Real fixes still open: log the error `message`/`code` to stderr, stop trying further `:free` models on a 429, optional `max_tokens`/timeout settings. Options to unblock: wait for the reset, or add credits (OpenRouter says adding $10 raises the free-model limit to 1000 requests/day): **owner's decision**, nothing spent by me. The pilot's q001 call (1 request) succeeded earlier the same day.

**CORRECTION (same day, found by reading the sessions folder):** "pre-design" is a **BOOK**, not a stage: `sessions/s1/session.pdl` has `STATE | name | pre-design`, and the taskbar labels sessions `book:<name>` and desks `page:<name>`. So `pre-design:eden-test` means the **page `eden-test` in the book `pre-design`**, next to `pre-design:dsr-test`. The staging-page purpose described above stands; the "separate stage" question is closed. See `EDEN-PLAYABLE-LOOP-AND-BOOK-PAGE-HARNESS-PLAN.md`.
