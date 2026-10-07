# Q001 — land HALO_CHAT v0.1 (it already exists on the `opencode` branch)

| field | value |
|---|---|
| status | review — ported + committed in worktree (`9028e0706`, branch `claude-halo-pull`); not on `claude`/`main` yet. HALO was on `origin/opencode` (`4b72a4bd7`) |
| tier | manager (claude) reviews; outside-agent or worker ghost may run the verification |
| size | M (review + verify + decide + land) |
| assignee | - |
| posted | 2026-10-06 by claude (manager); corrected the same day |
| needs-owner-decision | (1) tiered promotion: the opencode build auto-promotes edits for tier `master_phd` (`learning_limits.pdl`). The HORN handoff left "review file or direct to the bank?" to the owner: approve the tiered answer or change it. (2) grading: self-judge or separate judge (still open, belongs to the IRL sprint). (3) persistent chat history (still open). |

## Mission (one sentence)

Review, build and verify the HALO_CHAT v0.1 that `opencode` already contains, resolve the owner decisions it settled on its own, and land it on `claude`/`main` without disturbing HORN.

## What exists (read, not assumed — from `git show 4b72a4bd7` on origin/opencode)

- New ops: `^.hai-horn/ops/concept_bank_ctx.c` (loads top-weighted bank spokes for prompt injection), `halo_chat_describe.c` (Gemma DESCRIBE with bank context),
  `halo_chat_validate.c` (tiered promotion), `halo_chat.sh` (launcher: chat → DESCRIBE → validate), `halo_chat.pal` (orchestration sketch), `learning_limits.pdl`
  (`strength_of_training.max_tier: master_phd`); and a 43-line change to `horn_chat_openrouter.c` to inject bank context.
- Its commit message claims: "Verified: gravity_constant→force weight 0.60→1.0 via chat pipeline". **Not reproduced by the manager.** Checked so far: all four ops pass a `gcc -fsyntax-only` check (2026-10-06). Not built, not run.
- Branch situation: only `origin/opencode` has it. `claude` is 27 commits ahead of `opencode`; `opencode` is 67 ahead of `main`. A claude+opencode merge has not been attempted.

## Read first

1. `^.hai-horn/README.md`, `^.hai-horn/HORN_CHAT-HANDOFF.md` (the original mission; sprint 2 = HALO), `^.hai-horn/dox/02-DECISIONS-AND-TESTING.md` (the three open decisions)
2. `git show 4b72a4bd7` (or check out `origin/opencode` in a separate worktree) — the HALO files above
3. `&.widgits/concept-bank/ops/concept_edit_validate.c` (the validator HALO is supposed to gate through: does `halo_chat_validate.c` call it, or re-implement the check?)
4. `02-architecture/PRISC-OPS-ARCHITECTURE.md`; `ROBOT-CHAT-BLUEPRINT.md` §3.1

## Do

1. In a SEPARATE worktree (never the shared checkout; see `BRANCH-STRATEGY.md`), check out `origin/opencode`, build `^.hai-horn` with `scripts/build.sh`, run `halo_chat.sh` for one valid and one invalid edit, record the output.
2. Compare `halo_chat_validate.c` with `concept_edit_validate.c`; list any divergence. Flag what `master_phd` auto-promotion can write without a human.
3. Get the owner's answer on decision (1) through the manager.
4. Land: cherry-pick `4b72a4bd7` (and anything it needs) onto `claude` in the worktree; resolve conflicts; re-run the HORN e2e (`^.hai-horn/scripts/e2e.sh`) to prove HORN did not regress; then the manager fast-forwards.

## Acceptance

- [x] A fresh `scripts/build.sh` succeeds on the worktree (exit 0, all 10 ops incl. HALO's; needed a `-luuid` fix) — see `## Result (2026-10-06, claude)
- [~] One accepted and one rejected edit, with bank before/after hashes: DONE offline on a scratch bank copy via `scripts/halo_offline_test.sh` (5/5). NOT done through a live `halo_chat.sh` chat turn (needs a provider key).
- [ ] HORN e2e still passes after the cherry-pick.
- [ ] The owner's decision on auto-promotion is recorded here and in `^.hai-horn/README.md`.
- [ ] No key, token or raw provider response committed (checked for this commit: only code, build script, test script and a 4-line pdl).

## Rules

Everything in `^.grave/README.md`. The auto-promotion tier must not be enabled for any live bank until the owner approves it.

## Log

2026-10-06 | claude | created as "build HALO"; corrected after finding `4b72a4bd7` on origin/opencode while preparing the head-ff summary. Earlier statements that HALO does not exist on any branch were wrong (only `attrition` and `claude` had been searched).
2026-10-06 | claude | first pull attempt in a worktree hit a delete/modify conflict on `horn_chat_openrouter.c` (claude replaced it with `horn_chat_backend.c`); aborted, then redone and ported: commit `9028e0706` on branch `claude-halo-pull`.

## Result (2026-10-06, claude)


Worktree `/home/no/Desktop/github/work/NNEST-12.00-halo`, branch `claude-halo-pull`, commit `9028e0706` (from `claude` at `c41e7f935` + the HALO cherry-pick).

- **Conflict:** `4b72a4bd7` edits `ops/horn_chat_openrouter.c`, which `claude` deleted (replaced by `horn_chat_backend.c`, `2a60cbab2`). Resolved by dropping that file's edit and moving the bank-context injection into `halo_chat.sh` (prepends `concept_bank_ctx.+x` output to the user turn, calls `ops/+x/horn_chat_backend.+x "<prompt>"`).
- **Build:** `sh scripts/build.sh` exit 0, all ops built. `halo_chat_describe` needed `-luuid` (without it the link error stopped the build under `set -e`); fixed in `scripts/build.sh`.
- **Bank context op, run for real:** `concept_bank_ctx.+x` prints `[{"spoke":"gravity_constant","master":"force","weight":0.6000}]`.
- **Validator, offline, 5/5 PASS** (`bash scripts/halo_offline_test.sh`, scratch copy of the bank, repo untouched): tier `elementary_hs` → QUEUED_FOR_REVIEW and bank unchanged; tier `master_phd` → PROMOTED and bank hash changed; delta 0.9 → REJECT (bounds ±0.2); unknown spoke → REJECT; unknown master → REJECT.
- **Divergence found:** `halo_chat_validate.c` re-implements the checks (delta bounds, spoke/master exist, spoke has a slot to the master) rather than calling `concept_edit_validate.c`. Not yet diffed rule by rule. It also auto-promotes at tier >= 2 (`associate_bachelor`), not only `master_phd`: the header comment in `learning_limits.pdl` now says so.
- **Safety default:** `learning_limits.pdl` set to `elementary_hs` (auto-promotion OFF). opencode shipped `master_phd`. Owner decides.
- **Claimed weight change 0.60 → 1.0:** NOT reproduced. The validator caps one edit at +0.2, so a single promotion cannot do 0.6 → 1.0; either several turns promoted or the claim was approximate. Unverified.

## Still to do before the manager fast-forwards

1. Live `halo_chat.sh` turn with a real provider key (key files are ignored and live in the main checkout, not this worktree; run with `HORN_ENTITY_DIR` pointing at them, never copy keys in) and the DESCRIBE model call (Gemma over Ollama).
2. HORN e2e on this branch: `timeout 900 bash scripts/e2e.sh` (proves HORN did not regress).
3. Diff `halo_chat_validate.c` against `concept_edit_validate.c` rule by rule.
4. Owner decision on auto-promotion; then fast-forward `claude` to `claude-halo-pull` (manager does it, not pushed).
