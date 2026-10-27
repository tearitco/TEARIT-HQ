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

## 6. What stays unclear (asked in chat; answers will replace these)
See the questions at the end of the chat turn that produced this file and the open list in `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` sections 22.4 and 23.7.

## 7. Sources read for this synthesis
`RPG-DUST-DESIGN.md` (sections 1, 5, 6.1), `ROADMAP-AND-STATE-OF-PLAY-2026-10-07.md` section 7, `^.hai-horn/skillbook.pdl` and `curriculum.pdl`, `HARNESS-AS-GAME-BRAIN.md`, `DECISION-LEDGER-AND-DAILY-REVIEW.md`, `AI-BACKENDS-DISAMBIGUATION.md`, `AUTO-AND-AUTO-STUDIO-DESIGN.md`, `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md`, the owner's memory notes (opt-in fidelity, attrition model, ecosystem soul, delegation flywheel, run-alongside-users), `AGENTS.md`, and today's sessions. **Not read in full:** the rest of the rpg-dust designs (sections 7-10), `PALCRAFT-DESIGN.md`, `LLMUD-HACK.md`, `A-TEARIT-IS-ALL-YOU-NEED.md`, the terumon spec beyond its head.
