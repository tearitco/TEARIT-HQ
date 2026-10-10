# Learning loop: banks, weights, and not re-sending tokens we already paid for (2026-10-07)

**Owner's question:** "are we training our own weights and attention scores for hand tuning / fsm irl while we do this? don't reprompt old tokens we already have answer banks for, right?"

## 1. Direct answers (honest)
- **Training our own weights/attention?** No model weights are trained anywhere today. What exists: the Joint contract (spec section 0: grade = (reward+1)/(reward+punish+2), moved only by a harness verdict) as a DESIGN, hand-scored exchanges and learning hooks in `^.hai-horn/` (`hand_scored_exchanges.txt`, `irl_apply_signal.sh`, `curricula_gen.sh`, compiled `irl_signal`, `obs_feedback_write`, `promotion_ledger`) that are NOT connected to the delegation loop, and a dormant tomom pipeline. "Attention" in this house means **deterministic selection weights over prompt pieces** (hand-set first, then counted), not transformer attention. The full audit of what is hand-recorded vs only designed is in `XO/13.phymoji-engine/meta-models.md` (written by a read-only investigation agent).
- **Do we avoid re-sending tokens we already have answers for?** **No, and it was measured today.** There is no answer bank in the delegation loop; every attempt starts cold, and repair prompts re-send old tokens.

## 2. Measured (pilot 4, `var_cmp`, free Groq worker, `quest-pilot/q020-var-cmp/quest_ledger.txt`)
| iter | what was sent | prompt | completion | total | result |
|---|---|---|---|---|---|
| 1 | task spec | 717 | 1,923 | 2,640 | does not compile (PATH_MAX undeclared, unused variable) |
| 2 | task + whole previous reply + compiler output | 3,107 | 2,146 | 5,253 | 160/161 (last line without newline) |
| 3 | task + whole previous reply + exact failing example | 2,762 | 2,111 | 4,873 | compile broke again (unused variable) |
| 4 | LEAN: only the failing file + the compiler line | 2,222 | 2,066 | 4,288 | 161/161 PASS |
Total 17,054 tokens. Waste: (a) iteration 2 re-sent about 2,700 of its 3,107 prompt tokens (87%) that the model had already seen; (b) every iteration re-emitted the WHOLE file (~2,100 completion tokens) to change 1-2 lines: ~6,300 of 8,246 completion tokens; (c) the same classes of mistake recur (missing include for PATH_MAX, unused variable under -Werror, newline handling).
Groq limits (Q014): 1000 requests/day, 8000 tokens/minute per model: the binding limit is tokens per minute, so wasted tokens are also wasted wall-clock.

## 3. Design (four mechanisms, cheapest first; none built yet)
1. **Patch-style repair (Q021).** Send only the failing lines +- context and the error; accept a replacement of one named function or a unified diff; a small deterministic op applies it. Expected repair cost ~300-600 tokens instead of ~4,300 (hypothesis, to be measured).
2. **Lessons bank + prompt composer (Q022).** Rows `LESSON | family | trigger | text | hits | helped`, prepended to prompts of that task family within a token budget. From today: `compile: include <limits.h> for PATH_MAX; no unused variables under -Werror` and `a last line without a newline must get one before appending`. Hand-set weights first; an entry is auto-included only after >= 2 observed helps; promotion stays a human act (describe-not-classify).
3. **Answer bank (Q023).** Key = sha256(normalized spec + harness hash + model). Store ONLY harness-verified outputs (and failed attempts as curriculum). Exact hit: reuse and re-run the harness (no model call). Near hit (same family): include the accepted solution as the example. Never reuse unverified output. Append-only index.
4. **Prompt-component weights (Q024).** Log, per attempt, which components were in the prompt (spec, example, lessons, file excerpt) with token counts and the verdict; weight = smoothed first-pass rate with vs without the component; choose components by weight per token under the minute budget. Writes `W|` rows (Joint contract), read by the router. This is the "hand tuning first, then counted" version of attention.
FSM / IRL fit: a quest attempt is an FSM (PRECHECK -> CALL -> EXTRACT -> CHECK -> HARNESS -> REPAIR(n) -> DONE|FAIL) whose transitions and token costs are logged by `ghost_run` (Q012, merged); IRL scores from live Eden play enter the same ledger.

## 4. Rules
Banks and ledgers are append-only. Only harness-verified answers are reusable. Models propose, code and a person decide: no automatic promotion of lessons or weights. Free models only.

## 5. The recursion, labeled (owner, 2026-10-07: "the process of managing the ml weights should be labeled and trained as well")
Every row that scores or moves something carries `layer=<name>`, so the same two files (an append-only FEEDBACK log and the promotion ledger) can grade each level by the level below it. No new format: `layer=` is one more `| k=v` field.
| level | what acts | what it changes | scored by | row label |
|---|---|---|---|---|
| L0 | Eden agents, entities | world state (acts taken) | the game / IRL signals | `layer=world` (future) |
| L1 | weights and joints (`weights.pdl`, spokes) | which act is picked | harness + IRL outcomes | `layer=weights` |
| L2 | the **tuner**: a person, a model proposer, `joint_tune` | the weights (bounded, ledgered) | each `TUNE` row's later outcome (FEEDBACK, `layer=weights-manager`, `concept=<key>`) | `TUNE ... layer=weights-manager` |
| L3 | the **worker models** that write code, phrases, proposals | files, candidate EDITs | locked harness verdicts (Laplace per worker x task family) | `layer=delegation` |
| L4 | the **rules that grade** (harness, bounds, autonomy, thresholds) | what counts as pass | a person only (never a model) | decisions logged in the nights/docs |
A tuning move that was followed by a worse outcome earns the tuner a punish; a model proposer's grade per joint decides whether its `autonomy` may rise from 0. The grade of a level never rewrites that level's own rules; only the level above (and finally the owner) does.

## 6. Started 2026-10-07 (first data through the pipe)
- `quest-pilot/ops/ledger_to_feedback` + `record_delegation.sh`: the four finished quest ledgers (8 iteration rows) became FEEDBACK rows under `quest-pilot/delegation-bank/workers/<worker>/obs_feedback_log.txt`; `promotion_ledger` grades each worker x task family: groq-gpt-oss-120b on `c_op` **0.375** (reward 2, punish 4), groq on `data_rows` **0.667**, nemotron-3-ultra on `c_op` **0.667** (one run). Paths all derive from the script's own location (or `HOUSE`/an argument).
- `promotion_ledger replay` ADDS to a candidate's counts: replaying one log twice double-counts (found while building this: the first run read 3/5 instead of 2/4). The driver therefore treats the ledger as derived data and rebuilds it from the append-only logs each run.
- `concept-bank/data/spokes/`: 4 hand-recorded spokes (mass->force 0.75, acceleration->motion 0.70, velocity->motion 0.65, kinetic_energy->energy 0.60), the four pairs the house had hard-coded in `irl_apply_signal.sh`; weights are hand-set starting values; mirror tables regenerated; one human EDIT validated (`edit_0004`).
- `quest-pilot/delegation-bank/lessons.txt`: 5 hand-recorded lessons (design item Q022, first version).
- `concept-bank/ops/joint_tune` + `eden/conductor/joints.pdl` (12 joints, autonomy 0) = the L2 weights-manager; harness `joint_tune` 30/0 (and 27/3 with the autonomy rule removed).
- `store/catalog.pdl`: five listed items (nothing installable yet).
Not touched on purpose: tomom (no data yet), the IRL ops (`irl_apply_signal.sh` still hard-codes `/home/debil/...`; derive it from its own location when the first 20 observations exist), and the auto-promote gate in `halo_chat_validate.c` (owner decision).

## 7. Errata found in `XO/13.phymoji-engine/META-MODELS-VISION-ALIGNMENT-AND-SOLUTIONS.md` (checked against disk)
1. "40 ITER lines": there are **8** across the four quest ledgers (1+2+1+4).
2. Worker grades: q001 with one pass is (1+1)/(1+0+2) = **0.67**, not 1.00; q020 had 1 pass and 3 fails = (1+1)/(1+3+2) = 0.33, not "(1+3)/...". Per task family the groq `c_op` grade pools q002 and q020: **0.375**.
3. `w_eat` is **3** in `weights.pdl` (the doc says 7); `hunger_level`, `water_level`, `plant_growth` are not keys in `weights.pdl` (so those proposed spokes had no real source and were not recorded).
4. Worker lessons cannot be concept-bank EDITs with `target=c-op-worker-output`: the validator only accepts a spoke that resolves to one of the 3 physics masters. Lessons go to `lessons.txt`; grades go to the `delegation-bank` promotion ledger.
5. "159 history files" is correct (counted). "phrases.pdl 47 rows" is **46**, 40 of them written by the Groq worker.

## 8. The first full consumer: pets that trade (2026-10-10)

The pets on the chain are where this loop is meant to run end to end: outcomes of real (later) and paper trades are the reward signal, the harness verdicts are the referee, the banks and weights are what moves, and the pets may propose edits to their own harnesses through the validator. Plan and rails: `44.xyz.01.00/@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md` sections 11-12.

### The soul of the ecosystem (owner, 2026-10-10: "that's the point ... it's the soul of the ecosystem")
The pets are not scripted traders. They are **learners that operate the house the way a human does**:
- They **drive the relay**: they use the exchange, auction, chain and other windows through the same input path a human's keys take (`#.desktop/entity_menu_history/<pid>.txt` KEY_PRESSED / MOUSE_EVENT lines and the windows' state files), with no private API. Every action is a relay line plus the ledger rows it causes.
- They **learn by reinforcement (RL)** with the **in-house models and harnesses**: outcomes become reward/punish (`entity_grade`, the feedback ledger `obs_feedback_log.txt`, the concept bank and skillbook, Laplace score `(reward+1)/(reward+punish+2)`), weights move only through bounded, ledgered edits (`joint_tune`), the models are the house's own (Gemma on the Mac via `ai_backend.pdl`, the HORN provider ladder), and the **harness verdict is the referee**.
- They **use and modify their own harnesses while learning**: a pet may propose new cases, weights and bank rows for the harnesses it is judged by, as candidate edits that must pass `concept_edit_validate` and the locked-harness rules; it never edits the grader that scores it (hash-lock, `DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` section 2); promotion tiers and human review apply (preschool/elementary never auto-promote).
- They **watch real human input**: `#.desktop/human_input/<pid>.txt` (real X events only) is demonstration data for trading and every other house task (`IRL-BOOTSTRAP-RECURSION-SPEC.md`). Humans are optional; when they are there, the pets learn from them.
- Why it is the soul: the pets trade for real needs (food, parts, miners, rent), so the exchange has steady, honest, need-driven traffic and the chain stays live **with no human users**; every window is also a training environment; the harnesses grow from use.
**Rails (not optional):** DESCRIBE never CLASSIFY; models never decide numbers or promotion; every change is a diff plus a verdict; paper trading (`play` unit) before real cones; per-pet trading limits, daily loss caps and a kill switch (a flag file the runner polls); managed accounts are only ever reached through capped leases; no keys or wallets in any prompt or log.
