# The meta strategy: build more with fewer tokens, and end up needing no outside model (2026-11-22)

**Audience: an agent that has never seen this repo.** Read this page first, then the docs it points to. It explains the strategy, the vocabulary, what is BUILT (with a harness name), what is DESIGN (nothing runs), what to build next, and the rules you must not break. It is the ONE doc AGENTS.md routes to for this topic; add learnings here, not in a new file.

Authors: Claude (branch `claude`) with the owner, 2026-11-22. Both the owner and Claude act as the **human reviewers** while the system is being brought up. A copy for sharing with other agents is in the owner's XO folder: `XO/31.Nov/nov-22/5.META-STRATEGY-FOR-NEW-AGENTS-2026-11-22.md` (docs only; this BOOK file is the authority).

Labels: **BUILT** = a harness case or a run is named. **DESIGN** = written, nothing runs. **PROPOSAL** = my recommendation, owner can overrule. **READ** = I opened the file. **NOT CHECKED** = named, not verified.

---

## 1. The goal in two sentences

(1) Spend a large-model's tokens only on architecture and review; let cheap deterministic machinery (generators, harnesses, weights) do the repeatable building. (2) Over time stop needing any outside model at all: the house's own banks, weights and small local models take over, one narrow task family at a time. The owner calls this the **attrition model**: a war of attrition against needing an outside API.

## 2. Vocabulary (the house's words, plainly)

| Word | Meaning |
|---|---|
| **House** | the whole repo tree; apps live in `44.xyz.01.00/@.apps/<app>/` |
| **BOOK** | the docs repo `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/` (call it HQ-IQ-BOOK) |
| **Op** | a small compiled C program with a command line (`something_op <verb> args`), no shell, prints one result line, and a `--describe` line naming its inputs and outputs. Every game rule is an op (decision D92) |
| **Event page** | a data page (`events/<name>/event_pkg/pages/page_1/{condition.pdl,event.ir.pdl}`) that calls ops when a trigger and condition hold. The game's "what happens when" |
| **Ledger** | an append-only file of rows; the ONLY authority for game state (no ghost data) |
| **pdl** | plain row data (`KEY | value | ...`), no code |
| **pal** | a tiny assembly language run by the `prisc` VM; used for harness wrappers |
| **Harness** | pal + a case `.pdl` + C ops. A case lists steps and expectations (`EXPECT_OUT`, `EXPECT_HAS`...) and prints `PASS|case/label` or `FAIL|...`. It must be able to FAIL |
| **Phase table** | `harness/play/phases/<name>.phases.pdl`: a story of a game as data (`GAME`, `INIT`, `PHASE`, `DO | verb | args`, expectation rows) run by `playthrough_op` |
| **Bank / weight** | `harness_bank_op` stores reward and punish counts per case or phase and a weight `(reward+1)/(reward+punish+2)` (Laplace). `find` ranks phases by words x weight |
| **Concept bank** | `&.widgits/concept-bank/`: weighted nodes (concepts, spokes) a model or rule can look up |
| **Attrition model** | NIGHT 30 (`#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_30_THE_ATTRITION_MODEL.txt`): TEARIT describe-then-decide loop + concept bank + tomom (a hand-built local model, dormant) + FSM/GOAP decision layer. Outside models are a bridge, not the destination |
| **Tier** | a rung of the router ladder: rules -> local Gemma (the owner's Mac) -> free API -> stronger paid model. Start at the cheapest, escalate only on a judged failure |
| **Graduation** | the cheaper tier keeps its grade over N verified rows, so the expensive tier is retired for that task family |
| **Verified row** | a `DATA` row whose result passed a locked judge (a harness case). Only verified rows may become training examples |

## 3. The core idea: the harness is the loss function

Large-model training needs a signal that says "that was good". Here that signal already exists and is cheap: **a harness verdict** (`PASS`/`FAIL`, counts, weights). Therefore:

- A **tool call** (an op with arguments) is "good" if the case does better with it than without it. This is the same test as Toolformer's filter (below), with the verdict replacing the language-model loss. A with/without control run is also the house rule "falsify your premise with a control".
- A **tool choice** is a score over the known ops. The score is the concept-bank / `harness_bank_op find` ranking (keywords, synonyms, concept slots, sentence x weight). That is a sparse, inspectable, hand-tunable "attention".
- **Training** is counting: reward and punish per phase or per (context, op) pair. No gradient descent, no GPU. Neural training on this machine is explicitly out of scope (`16.game/SELF-CODING-HARNESS-PLANS-2026-11-06.md`).

## 4. The three published techniques, mapped to the house (from the owner's lecture notes, Prof. Diyi Yang)

| Technique | What it does | House equivalent (no spend) | State |
|---|---|---|---|
| **Toolformer** (Meta) | A model inserts tool calls into text; a synthetic dataset keeps a call only if it lowers the model's loss on the following tokens | Keep a (context, op, args) only if the case passes MORE checks WITH the call than WITHOUT (control run). Reward row if yes, punish row if no | weights BUILT; the log and the control run are being built (section 8) |
| **ToolkenGPT** | Each tool is a "token" with a learned embedding on a frozen model; when the tool-token fires, generation pauses, the tool runs, the result is plugged back | Each op is a concept-bank node whose "embedding" is its sparse spoke weights; the attention score is the bank ranking; fire when score >= a threshold RULE row; run the op; write the result to the ledger; arguments come from a deterministic slot filler over a state file | ranking BUILT; threshold + slot filler being built |
| **SFT on synthetic data from a frontier model** | Fine-tune a small model on reasoning traces from a big model | Generate scenarios ourselves from seeds, solve with a planner (GOAP / exhaustive over the tiny verb set), keep only traces the harness passed; train the smallest thing that holds the grade (table first, tomom later). Frontier traces only as an optional bootstrap | trace log being built; the distiller is DESIGN |

**Honest limit:** hand-tuned weights and attention choose among an ENUMERATED set (verbs, phases, templates) in a closed domain. They do not write novel code. A realistic target is **narrow in-house agents** for event pages, `.pdl` rows, pal ops, vehicle designs and gym lanes, each with a locked judge; a general in-house coding model is out of reach on this hardware (AI roadmap A7: "only if A4-A5 numbers justify").

## 5. What is BUILT (READ; commands a new agent can run)

1. `harness/ops/+x/playthrough_op.+x run <table.phases.pdl>`: runs a game story in a scratch copy; exit 0 all pass, 1 a check failed, 2 bad table; prints `PLAYTHROUGH|name|game|PASS|ms|phases|failed` and one weighted line per phase. About 1.5 s quiet, up to 17 s under load.
2. `harness/ops/+x/harness_bank_op.+x find play/bank <words>`: ranks known phases for a goal in words; live weights in `play/bank/live/<game>__<phase>.behavior.pdl`.
3. `harness/ops/+x/harness_case_op.+x cases/<name>.pdl` then `harness_verdict_op`: judge a case. Run all: from the harness folder, `/tmp/prisc <name>.pal` (build prisc: `gcc -O2 -w -o /tmp/prisc system/prisc+x.c -lm` from `&.widgits/_shared-lib/system/`), then read `results/<name>.txt.verdict.txt`. Pitfall: `RESULTS | name` must point to `../results/<name>.txt` (relative to the case file).
4. `&.widgits/_shared-lib/ops/game_new_op` (`game_new_op <house> new <name> --title T --kind ledger|realtime|editor`): a whole game from DATA templates on one shared binary `game_core` (decision D62; harness `game_new`).
5. Decision ledgers `state/decisions.txt` (`DEC|unix|stamp|actor|state|action|target|outcome`) in df-hq, cdda-hq, mcl-hq.
6. Entity chat endpoints: LAN Ollama on the owner's Mac (URL in `#.desktop/ai_backend.pdl`, `gemma_lan_url`, model `gemma3:270m`), `ai_chat.c`, `ai_chat_openrouter.c`, `ai_chat_groq.c`; contract `ai_chat*.+x <entity_dir> <house_root> <message>`. Rules: never print a key, never put private data in a prompt, never loop to a 429, a harness case never touches the network (`17.ai/ENTITY-LLM-ENDPOINTS-HOW-TO-TEST-AND-USE-2026-11-16.md`).
7. `ai_describe`: constrained DESCRIBE, reads an entity's history, writes `pending_review.txt` only.

## 6. What is DESIGN only (do not rely on it)

- A **caller**: nothing (no pet, no conductor) calls `playthrough_op run` today (HARNESS-AS-GAME-BRAIN section 4).
- **Promotion** of a concept (`&.widgits/concept-bank/AUTO-PROMOTION-RULE.md`): a stub. **Rule: auto-promotion is never built.** Promotion is a human-reviewed batch.
- `vis_new_op` (visuals from a palette), the distiller (verified rows -> small model), `ai_fsm_transition` and `ai_goap_plan` registration, the delegation flywheel beyond pilot q001.

## 7. The attrition loop and its rules (READ the AI roadmap)

Watch record (a harness ledger row, plus the owner's valence) -> a model DESCRIBES in ONE fixed line `TARGET: <existing node> | STRENGTH: high|medium|low | REASON: ...` (it never classifies freely and never sets a number) -> real code maps strength to a weight delta by a fixed table -> a candidate EDIT -> `concept_edit_validate` (only `spoke_weight_delta` exists) -> a review file a HUMAN reads -> promotion -> eventually tomom learns.

Standing rules, from the owner:
1. Rules before models: a deterministic check answers what it can, for free.
2. Fixed-shape, one-line outputs: cheap and machine-judged. Free text is expensive and unjudgeable.
3. Router ladder, cheapest first; escalate only on a judged failure; log every run as a `DATA` row.
4. Small packed context: one catalog row or one state file, not a book.
5. Cache and dedupe identical inputs.
6. Distill only from verified rows; graduate a family when the cheap tier holds its grade.
7. Heavy work via `cpu_gate`, niced, one at a time; offload GPU to the Mac/debil.
8. Measure tokens per task family in the row; "minimal tokens" must be an observed number.
9. The dataset is the asset: verified rows only, kept in the house. Private data (wallets, chat, `xyzfs/users`, `user/*` branches) is never training data.
10. Free models only unless the owner names a specific paid job; an outside model is a bootstrap that must graduate away. (2026-11-22: the owner offered a $10 OpenRouter budget as fuel for generating verified rows; treat it as a cap per named job, never a dependency. I do not know current model names or prices: look them up when spending.)

## 8. The build plan (started 2026-11-22; PROPOSAL; check git log for what has landed)

| # | Piece | Purpose | Gate (must be able to FAIL) |
|---|---|---|---|
| 1 | `tool_log_op` | append-only `DATA|task|input|op|args|verdict|tier|tokens|ts` rows, capped file, verbs `add`, `tail`, `stats`; harness-generated rows only | counts match; a bad row refused; cap enforced |
| 2 | `tool_ab_op` | with/without control run; credit a call only if WITH passes more checks | useful credited, useless not, harmful punished |
| 3 | `tool_score_op` | rank ops from goal words; fire at `RULE|tool_fire_min`; slot filler `SLOT|op|arg|from|key` builds args from a state file | right op ranks first; a wrong weight flips it and the case goes red |
| 4 | `mech_new_op` | generate a mechanic (verb row, event-page skeleton, RULE rows, a failing-capable case, a concept-bank proposal row) from args | generated case green, mutated case red, existing name refused |
| 5 | `vis_new_op` | visual from a palette, no third-party art | PNG opened and described |
| 6 | a caller: `build_loop_op <spec> <case> <worker>` | spec + locked case + worker with retry cap and spend cap; the verdict decides | red stub first, then green |
| 7 | first real jobs | Teario Play verb; wire `rta_life_op`; Minecraft mechanics from `16.game/MINECRAFT-EVENTS-HANDOFF-...` | their own cases |

Open defaults chosen by Claude on 2026-11-22 (owner may overrule): thresholds live as RULE rows in the concept bank (one authority); promotion stays manual (owner + Claude review); only harness-generated rows are training data (my own session transcripts are excluded: private paths and chat).

## 8b. The work router: first measured version (RAN 2026-11-22, Sonnet)
Labels: **RAN** = I ran it and saw it. **NOT DONE** = a gap.
- **The four ops of section 8 exist and pass** (RAN, harness runs 2026-11-22): `tool_log` 27, `tool_ab` 25, `tool_score` 28, `mech_new` 43, 0 fail each. `tool_log_op` already has the `tokens` field, so the "tokens column" step needs no build; what is missing is the habit of writing a row per real task.
- **Built: 20 WORK-INTENT entries** in `&.widgits/_shared-lib/harness/bank-work/*.behavior.pdl` (own folder, so the game and csv entries of `harness/bank/` are not mixed in). Each is a repeated job of mine (run a harness case, write a case, commit scoped, relay-type into a window, photograph a window, build under cpu_gate, click the taskbar, make a video, xo_export, log a tool attempt, A/B a tool, hai chat, launch hai-cli, probe the Mac, verify absence before building, CPU status, new game, mechanic as event, chain wallet, route to the right doc). A SEQUENCE row names the real command or doc. Weights on keywords and synonyms are hand-seeded; only the entry WEIGHT is measured, and it is 0.5 for all of them until real attempts are logged.
- **How matching really works (READ `harness_bank_op.c` lines 103 to 112, RAN):** a goal is split into words; a keyword or synonym scores only when it equals ONE whole word; a multi-word keyword never matches; a query word found inside a SENTENCE row adds 0.25. So entries use single-word keywords and synonyms, and the CALLER drops stopwords (the, a, to ...), else every sentence collects noise. My first version used phrases and scored 15 of 20; single words scored 18 of 20 on the same goals.
- **Measured accuracy (RAN):** DEV set (20 goals I tuned against) 18/20. HELD-OUT set (20 goals in new words, written after tuning and run ONCE) **18/20 = 90%**. A first held-out file was discarded unrun because I had picked keywords from it. The sets are small and written by the author, so 90% is an optimistic estimate: the real test is the goals that actually arrive.
- **The weak spots (RAN):** "test" and "window" are shared across entries (write a case vs run a case; relay-type vs launch hai-cli). Two of 20 held-out goals routed wrong, and `tool_score_op` still said FIRE on them: it said FIRE on all 40 goals, because `RULE tool_fire_min` (0.30) is low. The gap between the first and second score is lower on the wrong routes (0.075 to 0.375) than on most right ones, but some right routes also have small gaps (0.0 to 0.35), so a margin rule would trade wrong fires for more NOFIRE. **DONE the same day (owner: "you are the strong agent, go"):** `RULE tool_fire_margin` (the least gap between the best and the second score; 0 = off; a missing rule refuses) in `tool_score_op` and `concept-bank/tool_rules.pdl`, value **0.40**. A closer race prints `NOFIRE|best=..|min=..|margin=<gap><<rule>|second=<id>` and exits 1. On the same 40 goals: 30 right routes still FIRE, 6 right routes fall back to NOFIRE, the 4 wrong routes are blocked, so **no goal produces a wrong FIRE** (RAN, harness `work_router` 40/40). Cost: 6 of 36 right routes (17%) now need the route table. **0.40 is FITTED to those 4 errors (just above the largest wrong gap, 0.375) and the data are the same 40 goals, so it is likely too tidy: re-fit it as `tool_log` grows.** `tool_score` case 28 to 39 (margin boundary, tie, margin 0 = off, missing rule refuses, a fixture with margin OFF for the two old near-tie checks); with the OLD binary 7 of the new `tool_score` checks and 10 `work_router` checks fail. `tool_log`, `tool_ab`, `mech_new` unchanged (27, 25, 43 pass).
- **Harness case `work_router`** (36 of 36 pass, the two known-miss groups listed in its header and NOT asserted; a mutant that strips the commit entry turns 2 checks red). It locks the routes that are right today so a bank edit cannot quietly break them.
- **Token ESTIMATES (owner 2026-11-22: "a common average from statistics, ratios are fine for now") (ESTIMATE, not measured):** `&.widgits/concept-bank/tool_token_est.pdl` gives, per route, tokens WITH the route and WITHOUT it, from formulas stated in its header. Measured inputs: the mean HQ-IQ-BOOK doc is about 4,005 tokens (330 docs over 1 KB, bytes/4, off by up to about 30%); the harness `README.md` alone is 162 KB, about 40,500 tokens, the single biggest read in the house, so a route must say "read the case header or grep one row", never "read the README". Assumed inputs: 600 tokens per tool call, a read of a doc section is half a doc, and WITHOUT a route one wrong doc is read first and two extra search calls are made. Result under those assumptions: all 20 routes used once cost about 43,000 tokens with routing and about 148,000 without, **a ratio of 0.29**. The ratio is mostly the exploring assumption: with no wrong-doc read and three extra calls it is 0.55, with one wrong doc and no extra calls 0.35, with two wrong docs 0.21; a 10% wrong-route rate (the held-out miss rate) moves it only from 0.29 to 0.32. So "routing saves roughly half to four fifths of the per-task reading" is a hypothesis these numbers support only if I really do explore that much without a route, which no log shows yet. `tokens` in tool_log rows stays 0 = not measured; rows logged from now on carry the route estimate in the evidence text, marked `[EST]`.
- **Next, in order:** (1) STARTED 2026-11-22: `&.widgits/concept-bank/state/tool_log.txt` (capped at 2000 rows by RULE `tool_log_max_rows`, gitignored) holds 7 rows from this session, each with a harness verdict I observed (6 PASS, 1 SKIP where `cpu_gate` refused the lock), tier `paid_api` (the outside model, me). Recipe: `tool_log_op.+x &.widgits/concept-bank/tool_rules.pdl add <log> <task> <goal words> <entry id> <evidence> <PASS|FAIL|SKIP> <tier> <tokens>`. **`tokens` is 0 on every row and means NOT MEASURED**: neither I nor the harness can read the model token use, so the token baseline does not exist yet. It needs a number from outside (the owner usage figures per session, or a platform hook); until then the log only gives route frequency and pass rate. Keep logging every real routed task with a harness verdict (a task with no verdict is not logged), so the true held-out goal set grows; (2) the margin rule above: DONE, see the weak-spots item; (3) STARTED, 1 of 20 routes: `tool_ab_op` per entry, does following the route beat not using it. **`work.commit_scoped` CREDITED (RAN, harness `work_ab_commit_scoped`, 8 of 8):** in a scratch git repo with another agent file already staged, WITH the route (`commit_scoped.+x`) 3 of 3 checks pass (mine committed, foreign NOT swept in, foreign still staged), WITHOUT it (`git add` + a plain `git commit`) 1 of 3 (it sweeps the foreign file in): CREDIT. A control where the route equals the baseline comes out USELESS, so the A/B can fail. Logged in tool_log (PASS). NOT DONE: the other 19 routes (most need a real task with a pass/fail check to judge, which only arrives with real work; a route that is a doc to read cannot be A/B-ed this way, only logged), and the credit lands in a COPY of the entry, because `harness_bank_op update` keeps its counts in `harness/bank/<case name>.behavior.pdl` while the work entries live in `bank-work/` under other names: wiring the measured weight of a work entry needs either a sidecar per case or counts derived from tool_log (PROPOSAL: derive from tool_log, one authority); (4) only then a learned scorer (word weights from the log, then cosine over the concept bank) that must beat these hand seeds on logged goals.

## 9. How to use this as a new agent

1. Read AGENTS.md "START HERE" table and "Commit discipline"; read this page; read `17.ai/HARNESS-AS-GAME-BRAIN.md` and `17.ai/AI-ROADMAP-MICRO-AI-AND-IN-HOUSE-MODELS-2026-11-05.md` sections 3 to 6.
2. Pick a mechanic. Write the spec and a locked case, and make the case FAIL against a stub before anything else.
3. Generate (`game_new_op`, later `mech_new_op`); fill only what the generator cannot.
4. Run the case; read the verdict and the diff size, not a wall of code; mutate once to prove red.
5. Log a `DATA` row; update the bank; commit only your paths with `commit_scoped.+x`.
6. Never: Python, new `.sh` logic, `git stash/reset/add -A`, touching `xyzfs/users`, editing `game_core.c` for a rule, quoting a number you did not observe, or calling anything done without a fresh run and evidence.

## 10. Related

`17.ai/HARNESS-AS-GAME-BRAIN.md`, `17.ai/AI-ROADMAP-MICRO-AI-AND-IN-HOUSE-MODELS-2026-11-05.md`, `16.game/SELF-CODING-HARNESS-PLANS-2026-11-06.md` (D46), `16.game/GAME-MECHANICS-TRAINABLE-EVENTS-2026-11-06.md` (D47), `16.game/WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md`, `08-roadmap/design-docs/{HARNESS-BEHAVIOR-BANK-DESIGN.md,DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md}`, `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`, `16.game/MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md`, XO `31.Nov/nov-22/4.cheap-building-reusable-harnesses-training-hooks-2026-11-22.md`.
