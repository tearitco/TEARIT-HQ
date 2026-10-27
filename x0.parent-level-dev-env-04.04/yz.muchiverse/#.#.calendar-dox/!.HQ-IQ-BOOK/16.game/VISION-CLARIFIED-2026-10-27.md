# The vision, clarified (2026-10-27): one model for the engine, the owner, the agents and the test games

Written after the owner said: "look over prior documentation, use your common sense and wisdom since I don't exactly know; try to understand my desires for this as a game engine for others, but also for myself and agents, and our current test games; build and flesh out opaque ideas into clarity." Everything here is **my synthesis from the docs listed at the end plus today's work**. Decisions made under that delegation are marked **[delegated]**: they are mine, revisable by the owner at any time. Labels: FACT (read/observed), PROPOSAL, OPEN.

## 1. Who it is for (four audiences, one engine)
| Audience | What they need | What the engine gives them |
|---|---|---|
| **Outside creators** (later) | make whole games, swap parts, ship demos, improve from insights, without touching engine C | `game.pdl` parts and slots, data-only packs, events/registry commands, a Store with screening, generated settings windows |
| **Players** | games that open fast, run beside a browser, feel alive | the cheap-default/opt-in-depth rule, `cpu_guard`, tick cost files, courts and escrow so purchases are safe |
| **The owner** (a person with limited time and a weak machine) | to steer, see evidence, never be blocked by invisible state | every claim ends in a screenshot/state file/ledger row; docs as handoffs; Knowledge HQ / Store / Courts windows; decisions asked in chat |
| **Agents** (Claude, Grok, free models, later the house's own AI) | clear contracts, cheap iteration, safe rails | ops + registry commands, pal gates, a one-line judge, the dev lane (so agents learn how to develop), describe-never-classify, review before promotion |
The conflict rule that falls out of this: **safety for outsiders and the owner beats speed for agents**; within safety, **fewer tokens and less work for agents beats elegance**.

## 2. The core model in five sentences (what the opaque ideas share)
1. **Laws, choices, state** (from `RPG-DUST-DESIGN.md` section 1): *laws* (physics, chemistry, scaling, unit maths) are written by hand, deterministic, unit-tested and **never weighted**; *choices* (which body plan, which proportions, which words) are **weights in the concept bank**, changed only by candidate edits a person promotes; **state** (this entity's hunger, strength, level) is **per-entity data changed by use**, bounded and ledgered, not promoted. Every feature should say which of the three each of its numbers is. **[delegated: I add "state" as the third kind; the docs have laws and choices]**
2. **Everything is a row, resolved through a scope chain**: `TUNE`, `LIFE`, `POLICY`, `NEED`, `WORLD`, `ACCESS` rows with a default and a range; most specific scope wins (engine -> game -> world -> kind -> entity); caps flow down, values flow up; changes are logged; a validator refuses out-of-range values.
3. **Behaviour is ops, events and pal, not C branches**: a row says what, an op/registry command does it, a ledger row records it; the hot path writes a row, a slow op digests it; text includes are transitional.
4. **Two fidelity levels over one set of weights**: an abstract cheap level (a stat, a counter) and an optional complex level (anatomy, chemistry), both reading and training the same weights; the default is cheap, depth is opt-in.
5. **The AI learns in a loop it cannot short-circuit**: watch -> describe (one fixed line, never a decision) -> code decides -> validate -> review -> a person promotes; runs are counted in lanes (**dev** = how to build, **live** = how to play); the game's own gates are the judge.
The engine **grows capabilities and compacts** as it grows (design doc section 21): every slice pays a compaction tax.

## 3. What the test games prove (so each has a job)
| Game | Job | Status (FACT, today) |
|---|---|---|
| **rpg-space** | the hand-authored playable game: ships, asteroids, pets, life; proves the loop and the gate pattern | both gates 83 PASS; pets now write the decision ledger; life tick is an op |
| **rpg-dust** | the AI / world-generation testbed (owner): scale ladder planet -> molecule, generators, later anatomy and chemistry by zoom | a copy of rpg-space today (needs a shared base so it can diverge) |
| **df-hq, cdda-hq, mcl-hq, rpg-pvp-hq** | the decision-ledger and learning testbeds (weights already read back for one choice) | on the `dec_op` op |
| **store-hq, court-hq** (windows) | the platform: ownership, purchases, disputes | mockups; events (`ev_chain`, `ev_store`) are real and tested |
| **Knowledge HQ** | where knowledge domains (chemistry now, anatomy next) are browsed and scored | chemistry exams (P2) built; exam scores shown there |

## 4. Decisions under delegation (the five open questions, with reasons) [delegated]
| Question | Decision | Why (source) |
|---|---|---|
| **Q1 Where weights live** | **One file per entity, `weights_<id>.txt`** (`name=per-mille`, bounded, every change a ledger row), for body/need **state** weights (strength, endurance, rate multipliers). The game-level `life_weights.txt` stays as the **base**; the effective value is **base x entity weight** (layers multiply, like the scope chain). Skill **levels/EXP stay in the skill book** (`entity_grade`), and a small mapping table (`SKILLMAP | exercise | strength | +N per level`) makes the RPG level and the physical weight two views of one value | the skill book already says "using a skill is experience, not an exam" and keeps level/EXP/MP per entity; one source of truth avoids drift; matches the owner's "both share weights to set stats" |
| **Q2 Who chooses abstract vs complex** | **All three, layered, in this order:** the **world rule** sets the ceiling (`WORLD | fidelity_max`), an **entity/case opt-in** can raise it within the ceiling (the owner's 10-25 principle: "turn it on in special cases"), and **attention** picks at run time within that (complex near the focus, abstract far, summaries consistent with detail: "only simulate what you look at") | `RPG-DUST-DESIGN.md` section 5 (lazy generation, summaries are laws, multi-rate clocks); the memory note on opt-in fidelity |
| **Q3 Training** | **By use only**, where **deliberate practice is simply a costed skill use** (it spends MP/time, gives EXP, nudges the weight a bounded step) | skill book: skills cost MP, `rest` refills it, use gives EXP; no second path to change a weight; the dev/live lanes keep developer runs out |
| **Q4 Next build** | **`part_tree_lint` for the generic body first, then the repeatable pal harness cases** for the exam verbs, `ev_chain`/`ev_store` and `sig_op` | anatomy is the next verifiable domain (plan P4); the harnesses are the only way others can reproduce my hand-run evidence, so they come right after, not last |
| **Q5 Anatomy order** | **Add `muscle` as its own part.** Build order by dependency: **lungs + blood** (air need), **stomach** (hunger), **bones + muscle + limbs + fingers** (manipulation, strength lives in muscle), **brain** last of the first set (control; links to the entity learner) | needs come first in play; strength and manipulation need muscle attached to bone; the brain is the integrator and the most "judged" (not verifiable by code) |
| **Plants** | **share the life op** as a `kind` with its own `NEED` rows and slower rate weights; photosynthesis is the first atom-conserving reaction row | compaction (one ticker, one table); a separate ticker only if measured cost demands |
| **World rules home** | a **`world.pdl` in each world's folder**, layered under game policy, with caps flowing down | design doc 23.4 |
| **Magic default** | **off**: a world turns it on; a game may forbid it | safest default for outsiders; "depending on the world constraints" (owner) |

## 5. Glossary of the opaque words (from the docs)
* **Attrition model**: the house's strategy of wearing down the need for an outside API. Four parts: TEARIT, Concept Bank, tomom, FSM/GOAP.
* **TEARIT**: the describe-then-decide loop (watch, describe in one fixed line, code decides, validate, review, promote).
* **Concept Bank**: weighted nodes (masters and spokes) the loop reads and writes; weights change only through validated candidate edits.
* **tomom**: the hand-built local model, dormant until there is a review history worth learning from. **terumon**: the creature/pet entity class (its spec is a separate doc, only partly read).
* **Behavior bank / bank lanes**: per-phase counts and Laplace weights; the **dev lane** counts developer runs (how to build), the **live lane** runs made for the AI/game.
* **Phase table / gate**: a data file of steps with checks that a pal harness runs in a scratch copy; PASS/FAIL is the judge.
* **Decision ledger**: one row per autonomous choice; `dec_op` digests it into weights.
* **Registry command**: one block in `event_commands.registry.pdl` that wraps an op so any event page can call it.
* **Text include**: shared C pasted at compile time; transitional, being replaced by ops.
* **Laws / choices / state**: section 2.1. **Dual fidelity**: section 2.4. **Compaction tax**: growth must retire something.

## 6. Owner answers to the clarifying questions (2026-10-27)
* **Conflict order confirmed:** safety for outsiders and the owner, then agent token/work savings, then elegance.
* **Biggest promise (and it changes the priorities):** all of creators / living worlds / trade, **but the biggest thing is the learning flywheel**: hand-tune weights, see when tasks, learnings and trainings can be **micro-delegated to Gemma 270M and the APIs as soon as possible**, get things running and **train our own in-house reusable weights, models and datasets**, and compact and refine them. Aim at what serves that and also encompasses the rest. See section 8.
* **Showcase:** **one space with several small games that can be switched between, to prove that one central editor works**: pong, space invaders, slots, a pet game with a faucet (showing a subchain), a quick PvP tactics Elo game like BOTS, etc. See section 9.
* **Save truth:** **the shared weights are the truth; both fidelity levels are views.** (Saves store per-entity weights and ledgers; switching levels loses nothing.)
* Remaining open questions are listed in sections 8.5 and 9.4.

## 7. Sources read for this synthesis
`RPG-DUST-DESIGN.md` (sections 1, 5, 6.1), `ROADMAP-AND-STATE-OF-PLAY-2026-10-07.md` section 7, `^.hai-horn/skillbook.pdl` and `curriculum.pdl`, `HARNESS-AS-GAME-BRAIN.md`, `DECISION-LEDGER-AND-DAILY-REVIEW.md`, `AI-BACKENDS-DISAMBIGUATION.md`, `AUTO-AND-AUTO-STUDIO-DESIGN.md`, `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md`, the owner's memory notes (opt-in fidelity, attrition model, ecosystem soul, delegation flywheel, run-alongside-users), `AGENTS.md`, and today's sessions. **Not read in full:** the rest of the rpg-dust designs (sections 7-10), `PALCRAFT-DESIGN.md`, `LLMUD-HACK.md`, `A-TEARIT-IS-ALL-YOU-NEED.md`, the terumon spec beyond its head.

## 8. North star: the learning flywheel is the spine of the engine (owner)
Owner: "as we go, hand tuning weights, seeing when tasks and learnings and trainings can be micro delegated to Gemma 270M and the APIs asap, to get things running and training our own in-house reusable weights, models and learning data set, compacting and refining that."
### 8.1 This is already the house's design; what was missing is making it the organising idea
FACT (read): `DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` (spec + **locked harness** + cheap workers + **saved learning**; a **router** `local_student -> cheap_openrouter -> mid -> strong -> claude` that starts at the cheapest tier whose **worker grade** for the task family is above `min_grade` and escalates with the trace; worker grade = Laplace (reward+1)/(reward+punish+2); a first pilot passed); `AUTO-AND-AUTO-STUDIO-DESIGN.md` (tiers T0 rules, T1 Gemma on the LAN Mac as a **chooser among enumerated options only**, T2 tomom, T3 free APIs rarely, T9 human); the attrition model (describe, never classify); bank lanes (today). The engine's job is to make **every judgement step in every game and tool look like one of these tasks**.
### 8.2 The micro-task contract (PROPOSAL)
Every place a game, tool or agent needs a judgement (pick an action, describe a state, propose a need or a pack, take an exam, write an event page) is expressed as a **micro-task** row so it can be routed, judged and learned from:
```
TASK | id | family | input (fixed fields) | output (ONE fixed line) | judge (locked harness or deterministic check) | budget | min_grade
```
* **Judge first:** no task exists without a locked judge that can fail (a negative case). Verifiable judges (code) let a model be tried freely; judged tasks wait for a person.
* **Router:** the cheapest tier whose grade for that family is >= `min_grade` runs it (rules, then Gemma 270M on the Mac, then free APIs, then a person); a failure escalates one tier with the trace.
* **Every run leaves a dataset row:** `DATA | task | input | output | verdict | tier | lane | ts` (append-only, lane dev/live). The rows ARE the in-house dataset; verified rows become **training examples, weights and rules**.
* **Graduation (the measure of attrition):** when a family's cheap tier holds the grade over N judged rows, the router stops paying for the stronger tier for that family. **KPI: the share of tasks served by the in-house tier, per family, rising over time; API calls per family trending to zero.** Regressions drop the family back up the ladder automatically (the grade falls).
* **Artifacts are reusable and shippable:** weights files, small models (tomom), datasets, router rows, task families and their judges are **data-only packs** that a game or creator can include or the Store can distribute (the harness store idea), subject to the same screening.
* **Compact and refine:** datasets are deduplicated, hard negatives kept, stale rows retired (compaction ledger, design doc 21).
### 8.3 What to delegate first (ranked by how good the judge already is)
1. **Take the chemistry exam** (judge = `exam-grade`, verifiable, seeded, repeatable): ask Gemma 270M on the Mac for the fixed answer line, grade by code, record a **worker grade per question kind** (mass, atoms, symbol, valid). This also tells us honestly what a 270M model can and cannot do (arithmetic is the likely weak spot: that is a finding, and the router escalates). *This is the smallest complete flywheel turn and every part of it exists except the taker.*
2. **One-line `why` for a decision row** (judge: a fixed shape check plus a deterministic cross-check against the row).
3. **Proposals judged by a gate:** a pack, an asteroid model, a `NEED` row (judge: `pack_check` / the game's phase table).
4. **Event pages for rule variants** (the flywheel doc's existing family; judge: referee harness).
5. **Game-balance tuning of the arcade games** (section 9), judged by each game's gate and by bot-vs-bot Elo.
### 8.4 How the rest of the engine serves it (so nothing is wasted)
Decision ledgers, life ledgers and gates are the **dataset sources**; bank lanes keep developer work as a **separate learning corpus** (the AI also learns to build); the Store distributes **packs of weights/datasets/harnesses**; courts, signatures and REP keep **delegated workers accountable** (a worker's key, a cap, a grade); the compaction rule keeps the dataset and the code small; the creator tools expose the same task rows so **creators get cheaper, better tasks for their own games**.
### 8.5 Open
Which families to register first; the tier list and daily budgets (free providers only, Mac for Gemma); the `min_grade` and N for graduation (first guesses to be measured, per the house habit); where the dataset lives (a `data/` pack per family, not in git history if large); whether the in-house student is tomom or a small distilled model off this machine (debil/Mac) later.
## 9. Showcase milestone: one space, several small games, one central editor (owner)
Owner: "one space where there are several small games that can be switched between (to prove one central editor works): pong, space invaders, slots, a 'pet game' faucet (showing a subchain would be nice), a quick PvP tactics Elo game like BOTS, etc."
### 9.1 What it proves
That the engine is a **game-making engine**: several unrelated tiny games run as **bundles of data and events** (section 2 of the design doc), are **switched without restarting**, and are all **edited by one central editor**. It is also the best **dataset generator** for section 8 (bots playing, balance tuning, tasks with judges) and the first Store/Library content.
### 9.2 What already exists (checked) to reuse, not rebuild
`rpg-pvp-hq` (a PvP game), `chess` events with an **Elo file** (`elo.pdl`: start 1200, K=32, per the roadmap index), `bet-rpg` (chips and seats; Elo explicitly left for later), B.O.T.S. tables (named in the flywheel doc as data authored by cheap workers), `game-dev-hq` (an app with events, board and a **playthrough harness** `play_game_dev_hq.pal`), `rpg-edit-hq` (the RPG-Maker-style editor skin, life layer wired), `layout-studio`, the chain ops with a **faucet** (`chain_new`, `chain_faucet`: a sub-chain per game is `chain_new <id>`), and `ev_chain` / `ev_store` for events. I did **not** find a pong, space-invaders or slots game (searched app names and docs).
### 9.3 Design (PROPOSAL)
* **The hub:** one window (a `game switcher` over the Library idea): a list of installed small games, one active at a time; **only the focused game ticks** (the lazy rule; the others cost nothing), measured with `tick_stats`.
* **Each mini-game is a bundle** (`game.pdl` with parts: rules rows, events, art, layout, a gate), tiny by design (compaction): pong (ball/paddle events), invaders (rows of events + a pilot), slots (**weights**: the reel odds are a weight table, so tuning and fairness are data; payouts as chain events), the pet game (needs from the life op; a **faucet on its own test chain**: pets earn and spend through `ev_chain`, so the **subchain is visible**, with balances shown in the hub), a quick PvP tactics game (reuse the PvP code and the chess `elo.pdl`; ratings as rows).
* **One central editor:** a single editor window that opens any bundle's rows and event pages (candidates: `rpg-edit-hq` or `game-dev-hq` extended; the choice is the owner's, section 9.4). **Proof of "central":** switch to a game, change a rule row in the editor (e.g. paddle speed, slot odds), and the running game changes, with the change logged as a policy-style row; the same editor edits all five.
* **Evidence per slice (house rule):** a screenshot of the hub with the game, a state/ledger file, and the game's gate PASS; each game gets a negative gate case.
* **Build order:** (1) the hub shell + `game.pdl` + `game_check` on two existing games; (2) pong (simplest; proves a tick loop and the editor edit); (3) slots + the faucet subchain (weights, chain events, visible balance); (4) invaders; (5) the pet game on the life op; (6) the Elo tactics game with bots (the dataset generator, tier ladder of players: rules, Gemma 270M, API).
### 9.4 Open
Which editor is the central one; whether the hub is a taskbar window or a board inside rpg-space; whether a faucet game uses a real test chain per game or one shared test chain with per-game wallets; whether bots (Gemma-driven players) are in the first version of the PvP game or after the rules gate.
