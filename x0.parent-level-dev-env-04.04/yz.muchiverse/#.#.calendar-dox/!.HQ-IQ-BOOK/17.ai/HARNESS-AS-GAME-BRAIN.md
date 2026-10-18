# The harness as a game brain (2026-10-17)

The owner's direction, 2026-10-17: "we prefer to string them together as reusable for real ai in game use and concept banks, weights, etc". This page is the whole picture of how that is meant to work, and it separates, line by line, what is BUILT and was run from what is only DESIGN. The built half is documented as a how-to in `06-testing/GAME-PLAYTHROUGH-HARNESSES.md`; the harness folder README is `44.xyz.01.00/&.widgits/_shared-lib/harness/README.md`.

## 1. The idea in one paragraph

A game in this house is a set of verbs over an append-only ledger (S1, `16.game/S1-EVENT-LEDGER.md`): `mine`, `craft`, `strike`, `dig_area`, `save`... Anything that wants to PLAY the game, a person, a pet, the `auto` conductor, has the same problem: which step list works, and which steps can I trust? The pal harness already answers the second half for tests: it runs verbs on a scratch copy and records PASS or FAIL per check. If the step list is data (a phase table), if every run appends its verdicts to a result ledger, and if the verdicts feed the behavior bank (`WEIGHT = (reward+1)/(reward+punish+2)`), then the same machinery is a JUDGE for an actor's proposals and a MEMORY of which steps work. The actor does not need to be trusted: its proposal runs on a scratch copy, the harness (not the actor) says what happened, and only a measured weight is kept.

## 2. The whole picture

```
        actor (pet FSM/GOAP, tomom, auto conductor, a person)                    [DESIGN: the callers]
                |  proposes a step list = a phase table (data)
                v
   +---------------------------+    DO verb args / expectations        +-------------------------+
   | play/phases/<n>.phases.pdl|  ----------------------------------> | playthrough_op compile  |  [BUILT]
   +---------------------------+                                        +------------+------------+
                                                                                      | cases/play_<n>.pdl  (plain case file)
                                                                                      v
        scratch copy of the game  <------ RUN <game binary> <verb> ------  harness_case_op                [BUILT]
        (/tmp/hc_*, own house,                |                                      |
         stand-in window)                     v                                      v
              game verb  ----->  ledger state/events.txt, jobs.txt, chat.txt,   PASS|FAIL rows
                                 ui.txt tab rows (inventory/craft/body/look/       |
                                 stocks/units)  -- what the checks look at --      v
                                                                        playthrough_op ledger           [BUILT]
                                                                          |                |
                              RESULT ledger (append-only)  <--------------+                +--> play/results/ph/<game>__<phase>.txt
                              play/results/play_ledger.txt                                          |
                              PH|run|game|name|phase|verdict|ms|pass|fail|why                         v
                                                                                          harness_bank_op update    [BUILT]
                                                                                                    |
                                                                                                    v
                                                  play/bank/live/<game>__<phase>.behavior.pdl   WEIGHT = (reward+1)/(reward+punish+2)
                                                                                                    |
                              harness_bank_op find play/bank <words>  (ranked by keyword, synonym, concept slot, sentence x WEIGHT)   [BUILT]
                                                                                                    |
                    +---------------------------+---------------------+------------------+
                    v                           v                     v                  v
        concept bank (SLOT POINTS_TO=      tomom / TEARIT        pet decision      decision ledger
        crafting, ledger, save-load...)    promotion (design)    (FSM/GOAP picks   state/decisions.txt
        [BUILT as seed data; promotion     [DESIGN]              the step with     DEC|unix|stamp|actor|state|action|target|outcome
         is the owner/validation loop]                           the best weight)  [PLANNED, the lead builds it next]
                                                                 [DESIGN]
```

## 3. What is built (and was run on 2026-10-17)

| Piece | Where | Evidence |
|---|---|---|
| Game verbs over the S1 ledger | `@.apps/{df-hq,cdda-hq,mcl-hq}/ops/*.c` | existing; `XO/test_s1.sh` 0 failures for all three after this work |
| Phase table (data, separate from the runner) | `harness/play/phases/*.phases.pdl` | three shipped stories (cdda-hq 39 checks, df-hq 36, mcl-hq 43), one 2-phase proposal example |
| Generic runner | `harness/ops/playthrough_op.c` (`compile`, `ledger`, `run`, `mark`) | each shipped story passes on the real game, fails with one deliberately wrong expectation, leaves no process or scratch dir |
| Scratch house + real binary | generated case: `COPYDIR`, `gcc -O0`, stand-in `khtpm_core_render` | `ls /tmp/hc_*` empty after the runs |
| RESULT ledger | `harness/play/results/play_ledger.txt` | rows of every run, idempotent per run id |
| Per-phase weights | `harness/play/bank/live/` via the unchanged `harness_bank_op` | cdda-hq `p2_craft`: 1/0 -> (broken run) 1/1 weight 0.5000 -> (clean run) 2/1 weight 0.6000 |
| Search by word | `harness_bank_op find play/bank <words>` | `craft bandage` ranked `p2_craft` first |
| Whole-harness seeds in the same shape as every other | `harness/bank/play_{cdda,df,mcl}_hq.behavior.pdl` | `find bank dig stone` returns `harness.play_df_hq` |

## 4. What is design only (nothing below exists; do not rely on it)

1. **A caller.** No pet, tomom run or `auto` conductor calls `playthrough_op run` today. The AI hook below is a contract, not code. `auto` itself is design (`AUTO-AND-AUTO-STUDIO-DESIGN.md`; slices A0-A3 are built for the sfx toy only).
2. **The decision ledger (BUILT 2026-10-18 in df-hq, cdda-hq, mcl-hq).** `state/decisions.txt` rows are `DEC|unix|stamp|actor|state|action|target|outcome` (no `why`/`score` column yet; see `16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md` for what writes rows and the `review` verb). Its rows ARE wired into behavior-bank entries (`state/bank/dec_<game>_<action>.behavior.pdl`, live Laplace weight, ranked by `harness_bank_op find`) and one choice (a pet picking a new target) reads the weight back (built 2026-10-18); per-actor weights, other choices reading weights and a judge of whether a weight change helped are NOT built. Its role in this picture: every choice an actor makes (which verb, at what, why, with which score) is one row, so a decision can later be judged against what happened. How it joins to the harness is design: `DEC.verb` plus `DEC.target` would name a phase (or a `DO` row), the RESULT ledger's `PH` row for a run whose table contained that step would say whether it worked, and the pair becomes a labeled example for the sub-driver training loop of `AUTO-AND-AUTO-STUDIO-DESIGN.md` section 6 ("every ACT/DECIDE row plus the window's judge result is a labeled example"). The `score` column and the harness WEIGHT are different numbers: score is what the actor believed when it chose, WEIGHT is measured; the gap between them is the signal.
3. **Promotion.** Turning a high-weight phase into a concept-bank entry, a tomom training example or a pet's default plan is the attrition model (TEARIT + bank + tomom + FSM/GOAP; harness rows are Watch records; promotion is undesigned, a person reviews). Today a weight only ranks `find` results.
4. **Plans from a phase result.** A GOAP planner that reads weights to choose between steps does not exist. `ai_goap_plan` is a named event command that is not in the registry (`17.ai/AI.md`).
5. **Live-state judging.** The harness plays a SCRATCH copy. Judging an actor's proposal against the live game's current state (not a fresh game) would need a snapshot of the live state copied into the scratch (`game_snapshot_op` exists for entity trees; nothing wires it here). Today a proposal is judged from a fresh game, which is the right test for "does this recipe/step list work at all" and the wrong test for "is this good now".
6. **Stop on the first failure, and branching.** A phase list is linear; the case op has no conditional (section 9 of the playthrough doc).

## 5. The AI hook: how a caller would use it

Contract (all of step 1-5 is BUILT as a command line; the calling actor is DESIGN):

1. The actor writes a phase table, for example `play/proposed/<actor>-<n>.phases.pdl` (git-ignored). Minimum: a `GAME` row, `INIT` rows if the game needs a start position, and PHASEs each with `DO` rows (the steps it wants to try) and at least one expectation row (what it predicts will be true: `SAY`, `HAS`, `COUNT`, `UI`, `EQ`...). An actor that cannot state an expectation cannot be judged; the runner refuses a phase with none.
2. It runs `ops/+x/playthrough_op.+x run <table>` (a fork/exec, no shell; niced; about 1.5 s for a short list on a quiet machine, up to 17 s measured under load average 13; the first line of cost is the 1 s `gcc -O0` of the game).
3. It reads the exit code (0 all checks passed, 1 a check failed, 2 the table is bad) and the stdout: `PLAYTHROUGH|name|game|PASS/FAIL|ms|phases|failed` plus one line per phase `phase  PASS/FAIL  ms  play.<game>.<phase> reward=R punish=P weight=W`. For detail it reads `play/results/play_<name>.txt` (`FAIL|<game>/<phase>.<label>|<what was seen>`) or the `PH|...|<first failed label>` ledger row.
4. It reads weights without running anything: `ops/+x/harness_bank_op.+x find play/bank <words>` ranks the known phases for a goal in words (`craft bandage` -> `play.cdda-hq.p2_craft`), and the live file `play/bank/live/<game>__<phase>.behavior.pdl` holds `COUNTS` and `WEIGHT`.
5. It keeps phase ids stable on purpose: `mine_stone` proposed twice adds to one weight. A new id starts at 0.5000.

What exists today of that, exactly: `example_proposal.phases.pdl` (two phases, hand-written to look like a pet's output) was run through step 2 and 3 and passed. Nothing generated it.

Fit with the three AI tiers of `AUTO-AND-AUTO-STUDIO-DESIGN.md` section 5: T0 rules can be a phase table written by hand (the shipped ones are exactly that); T1/T2 (a small model as a chooser among enumerated verbs, tomom) would only choose which phase ids to include and in what order, never free text, so a proposal is always syntactically a table and the runner's refusals (`bad row`, `no expectation`) are the validity rate to measure; T9 (a person) reads the FAIL labels.

## 6. Why this does not become a second play button

`16.game`'s rule stands: driving the game means appending one command the game already knows. The harness appends them to a SCRATCH copy only, in a sandbox that has its own house, its own state folder and a stand-in window, and it kills what it started. A proposal cannot reach the live game, the user's data (`xyzfs/users`), or another agent's checkout: the only path the generated case names outside the scratch is the harness's own results folder. What a proposal changes is a number (a weight) and a row (the ledger). Adopting a plan in the live game is a separate act (design: an actor reads the weights and then uses the normal relay/event route with the owner gates of the `auto` design).

## 7. Related

- `06-testing/GAME-PLAYTHROUGH-HARNESSES.md` - the how-to, phase-table format, traps.
- `08-roadmap/design-docs/HARNESS-BEHAVIOR-BANK-DESIGN.md` - the bank file shape and the Laplace weight.
- `17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md` - conductor, tiers, "training the subs", safety rails.
- `17.ai/AI.md` - describe, score, store; which backend is which.
- `16.game/S1-EVENT-LEDGER.md` - the event line every game state is a replay of.
- `02-architecture/RELAY-DRIVE.md` - the window-level route (real input through the relay), the complement of this verb-level route.
