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
