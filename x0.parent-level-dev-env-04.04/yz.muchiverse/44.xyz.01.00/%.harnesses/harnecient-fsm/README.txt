%.harnesses/harnecient-fsm

FSM/verification layer for delegating multi-step, deterministic-tool
work to Harnecient models (au11-hq/HARNESS-DELEGATION-PIPELINE.md).

  run_plan.sh <plan-file>       — general DELEGATE-step FSM driver.
                                   Read-only tool steps only (safe,
                                   proven surface as of 2026-08-13).
                                   Not yet used against a real plan
                                   this pass - built and unit-tested,
                                   waiting for a real target task
                                   (event/PDL authoring is the
                                   proposed next one, needs a
                                   write-approval stage added first).

  nav_intent_to_index.sh        — deterministic resolver for the
                                   "delegate navigation" pattern
                                   (DESCRIBE the target in plain text,
                                   don't ask the model to pick a raw
                                   index - see HARNESS-DELEGATION-
                                   PIPELINE.md §6 for the live A/B
                                   test: 2/4 wrong picking raw
                                   numbers, 3/3 correct describing by
                                   label). Live label source now
                                   exists (open-hai's receipt gained a
                                   companion nav-labels.txt dump,
                                   2026-08-13) - tested end-to-end
                                   against a real running session.
                                   TWO real caveats found, not just a
                                   clean win: (1) noisy session-
                                   snippet labels can confuse the
                                   model into replying junk - resolver
                                   correctly fails closed on this, but
                                   session labels need cleanup before
                                   this is reliable; (2) resolving to
                                   a REAL label is not the same as the
                                   model picking the SEMANTICALLY
                                   correct one - post-action receipt
                                   verification is still required, not
                                   yet wired into run_plan.sh for
                                   navigation-kind steps.

  tunables.conf                 — hand-tunable "joints": every magic
                                   number run_plan.sh uses (poll
                                   timing, relay delays, which model
                                   answers NAVIGATE goals), named and
                                   documented, sourced not hardcoded.
                                   The slot a future Stage 1 heuristic
                                   or Stage 2 learned-weight system
                                   would write into - see
                                   HARNESS-DELEGATION-PIPELINE.md §7.

  observations.log              — append-only Stage 0 data log
                                   (§7.2/§7.4): one row per step,
                                   goal/resolved-label/verdict. Every
                                   run_plan.sh run adds to this. This
                                   is the dataset any future learning
                                   approach (heuristic or RL) would
                                   need - built now because it's cheap
                                   and never wasted, even though actual
                                   weights/RL are deliberately deferred
                                   (not enough trial volume yet, and
                                   entity-AI/gameplay work is a better-
                                   fit environment for that when it's
                                   ready - see §7.1).

  run_queue.sh                  — task queue/automation runner
                                   (§9): processes every *.plan in
                                   plans/queue/ through run_plan.sh,
                                   files each into plans/done/ or
                                   plans/failed/ by real exit code.
                                   NOT wired to cron/autostart.pdl -
                                   deliberate, that's a separate,
                                   more sensitive step needing its
                                   own explicit go-ahead.

Status 2026-08-13: nav-labels gap closed, session-label cleanup done,
post-action nav verification wired, tunable joints + observation
logging both real and running, write-approval for mutating STEP tools
wired and FAIL-CLOSED BY DEFAULT (APPROVE and DENY both real, both
dynamically locate their real nav row live, both proven with actual
writes/denials), NEEDS_REPLAN wired via the MAX_RETRIES joint (real
Harnecient-suggested retries, proven with a real intentionally-failing
plan), task queue/automation runner built and proven (1 pass/1 fail
queued, correctly filed). Five real plans exist: navigate-to-model.plan,
write-unapproved-should-fail.plan, write-approved-should-pass.plan,
write-deny-should-pass.plan, retry-demo.plan - all passing/failing
exactly as designed. observations.log has 7 real rows across 4 outcome
types. See HARNESS-DELEGATION-PIPELINE.md §6-9 for full detail.
