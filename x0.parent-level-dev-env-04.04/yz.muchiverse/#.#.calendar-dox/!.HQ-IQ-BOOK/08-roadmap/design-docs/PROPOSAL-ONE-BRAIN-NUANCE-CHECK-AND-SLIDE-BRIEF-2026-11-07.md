# 🔍🧠 Nuance check and slide-deck brief: "one brain per game" (companion to D48, 2026-11-07)

💌 *For the owner, and for the agent who will make the visual deck. Written by Claude Sonnet 5.5 after you pasted another AI's summary and asked whether it misses the nuance of our vision and architecture.*
🏷️ **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it · **DOC** my reasoning, not run.
📎 Main document: `PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md` (decision D48). Read that for the detail; this page says what the summary drops and how to build a deck that does not overclaim.

---

## 1. 🎯 Short answer
**Yes, it misses a lot, and in two places it says something slightly false.** It is a fair one-line description of the *mechanism* (one vocabulary, two numbers, a phased plan). It leaves out **the finding, the honesty, the architecture rules, the vision and the open questions**, which is where most of the value is. And while checking it I found **something I missed in my own proposal** (section 3), which matters more than anything the summary left out.

---

## 2. 📋 The summary, sentence by sentence
| # | The summary says | Verdict | What is missing or wrong |
|---|---|---|---|
| 1 | "unify game harnesses and AI pet behaviors by establishing a single action vocabulary within existing configuration files" | ✅ mostly right | Two corrections. (a) "AI pet behaviors" suggests pets already act with a repertoire; **in the shared core and in mcl-hq a pet only wanders** (keep or pick a random target, walk one step, greet a neighbour; OBS `petslike_core.c`), while **df-hq adds one real rule, a dig job queue** (b) "configuration files": it is the game's own **verb manifest** (`verbs.pdl`, D47), which is why no second authority is created |
| 2 | "separating feasibility from utility into two distinct numerical metrics … validity filters … live play determines preferences" | ✅ right | Missing: they are **never merged** and why (a scratch run proves a step list works from a *fresh* world; a live outcome proves it was good in *this* world); both are the same counted success rate `(reward+1)/(reward+punish+2)`; in the proposal the pets' scratch runs feed only the **`live` lane**; **you decided this on 2026-11-07** (two numbers, minimum feasibility 0.5) |
| 3 | "evaluate the current state of decision ledgers, identify missing execution callers" | ⚠️ true but buries the headline | The headline is **an observed fact**: mcl-hq has 509 recorded pet decisions (walk 455 ok, walk 36 blocked, door 18 ok, nothing else) and **0 non-empty learned-weight files**. Also: the reuse was **already designed on 2026-10-17**, so the verdict to your question is "**not misdesigned, but the loop was never closed**"; and Grok's D45 to D47 count only through the harness, not through live play |
| 4 | "phased implementation plan starting with diagnostic investigations and schema linting" | ✅ right, thin | P0 (find why the bank is empty) and P1 (shared vocabulary + lint) **change no pet behaviour**, which is why they come first. The summary does not say that **P3 is large** (see section 3) or that every step has a **proof that can fail** |
| 5 | "enable autonomous agents to learn and build effectively without introducing new system binaries or breaking established architectural constraints" | ❌ overclaims | **Nothing is built**; the document is a proposal. "Build effectively" is the *goal*, not a result, and pets cannot build at all yet. "No new binaries" is true of the proposal (one new verb inside the existing `dec_op`) but the sentence hides **which constraints** matter (section 4.C) |

---

## 3. 🚨 Something I missed in my own proposal (found while checking this, then corrected again after your question)
**OBS, read in the code:** how a character gets its actions in this house, in three separate paths:
| Path | Who or what decides | Used for |
|---|---|---|
| **1. The pet's own step** (`ai_step`, `petslike_core.c` line 1022 on) | a **hard-coded rule chain in C**: keep or pick a **random** target, walk one BFS step toward it, greet a neighbour. An implicit two-state loop (`idle` or `has target`). **Not a data FSM**, not a person | every non-controlled pet, on a timer (INT off) or once per turn (INT on) |
| **2. A game's job rule inside that step** (`PET_SECTION` 36) | **df-hq only:** a marked cell in the room comes before wandering. A person writes `DESIGNATE`, a free pet writes `CLAIM`, digs, and `DONE` is appended, in `state/jobs.txt` (OBS: 23 live rows, all DESIGNATE and DONE). **mcl-hq has no such hook** | digging in df-hq |
| **3. The driver** (a person's keys or the relay: the `ctl` flag, the `control` verb, INT mode) | the **one selected character** is moved by input; every other character runs path 1. A scripted harness (`playthrough_op`) is a driver too: it calls verbs directly | you playing; harness runs |
mcl-hq's decision rows are **labelled after the step** from before-and-after state (`mcl_hq.c` lines 338 to 344), so `walk`, `door`, `pick` are labels, not choices. No FSM or GOAP is registered (`HARNESS-AS-GAME-BRAIN.md` §4.4); the only `.fsm.pdl` I found is `rpg-edit-hq/doors.fsm.pdl`, a door-transition table for the editor (header read only).

**What that changes:**
1. **In mcl-hq a pet cannot choose to mine, craft or place by any path except a person or a script driving it.** "A pet chooses among actions by `can` and `does`" needs a layer that does not exist there.
2. **The house already has the pattern for that layer: df-hq's job queue.** The recommendation (main document, new section 4.8, DOC) is to generalise it to `JOB` rows (`mine craft place`) that a person or, later, the D40 manager writes and a free pet claims, with `can` and `does` deciding which kind it claims first. That is "generalise one proven pattern into a second game", smaller than inventing pet decision-making, but still the largest step (P3).
3. **P0 to P2 still stand** (they need no new pet behaviour).
4. **Your decision 5** is now really "may the claiming rule use the numbers automatically?".
5. **Nine games share the core** (dsr-rpg, df-hq, cdda-hq, rpg-pet, civ-rpgce, rpg-edit-hq, tcg-rpgce, gta-hq, mcl-hq), so any change to path 1 is a change to all of them.

**Other things I did not check (stated so nobody assumes):** whether the pet step runs in a resident process or per command (it decides whether the "every 200 decisions" digest counter can ever fire); why df-hq's decision ledger has no `dig` rows although its jobs file shows completed digs (the jobs are older than the ledger, DOC, not verified); the other eight games' phase tables (I read mcl-hq's); how `petslike_core.c` skins differ; any multi-machine play (`palnet_peer` is 127.0.0.1 only); whether the `walk` formula's direction is intended (a higher walk success *lowers* the chance to pick a new target; not judged).

---

## 4. 🧩 The nuance the summary leaves out
### A. The verdict and the finding
* Your question was "if not, we misdesigned them". **Answer: not misdesigned; the loop was never closed** (design 2026-10-17; its own section 4 lists the missing caller, GOAP, promotion and live-state judging).
* **Observed:** 509 live decisions, 0 learned weights, no `mine`, `craft` or `place` ever. **Cause of the empty bank unproven** (hypothesis only).

### B. How the idea works (the parts a deck needs)
* **Three banks that look alike and are not connected:** the harness phase bank, the pets' decision bank, and the concept bank / look table / material gate.
* **Two numbers:** *can* = "this step list works from scratch" (from harness runs), *does* = "this choice worked when a pet made it live". Choice rule: filter by *can*, pick by *does*, with a floor and ceiling so nothing is forced or starved.
* **The harness never touches the live game.** It plays a throwaway copy; the pet then does the same verbs for real, each as one command the game already knows ("no second play button"). A read-only snapshot of live state for "is it good *now*" is proposed, not built.
* **Lanes:** developer test runs go to `dev`; only `live` can influence a pet.

### C. Architecture rules the proposal obeys (this is what "established constraints" means)
**One authority per fact** (the vocabulary lives in the existing `verbs.pdl`, not a new file) · **ops by fork/exec, no shell; pal runners; `.pdl` data** (no new `.sh`, no Python) · **no new binary** (one verb added to `dec_op`) · **human promotion**: a weight ranks and chooses, it never writes the concept bank or promotes itself (attrition path, `AUTO-PROMOTION-RULE` is a stub on purpose) · **heavy work one at a time through `cpu_gate`** on a weak machine shared with a browser · **registered processes, capped runtime files, cursors not mtimes** · **in-house only** (the lore is unpublished IP).
*Honest wart (OBS):* the harness runner shells out to `gcc` with `system()` in two places (`harness_case_op.c` lines 242 to 243); recorded as optional step P7. The harness is **not** claimed shell-free.

### D. The vision this sits inside
* **Centroid University** as one **voxel campus on floating islands** (D37), pages `test` and `live` in the editor (D43). **Test** is a prebuilt campus the harness checks (D40); **live** is the one you watch, with the RPG Maker / generated sheet toggle (backslash key).
* **Pets building the campus DF-style, gather then craft** is the *later* run (D38, D40), after the test page; a **manager learning the master blueprint and distilling it to employees** is later still.
* **Customer proof = a YouTube video of a real run frame** (D41): a verdict line without a frame is not a customer video. *The harness is also the product's proof material.*
* **The material gate** (`gate_op`, my D35 work, plus Grok's campus phrases) names the **skin** of a placed thing; "stone wall" → stone is already pinned by a passing test. The proposal makes that a test that fails on a wrong name.
* **tomom and attrition** stay the *offline, human-reviewed* promotion path; this proposal is the **runtime choice loop**, not a replacement.

### E. Honesty the summary drops
**Nothing in the proposal is built.** Labels (SAID / READ / OBS / DOC) are on every claim. Two named gaps: D38's text does not mention the decision ledger, and my own material gate was written without mapping the other two banks. The empty-bank cause is a hypothesis. P0 exists to test it.

### F. What is decided and what is open
**Decided (you, 2026-11-07):** two numbers; `can_min` = 0.5. **Open (explained in the main document, section 9):** how often a pet may run a scratch proposal (a run compiles the game: about 1.5 s quiet, 17 s loaded, measured 2026-10-17, not re-measured); that P0 and P1 go first (recommended); the boundary with D47 ("a weight does not auto-fire a page in the live house"). The main document also asks how long a game hour is in real minutes.

---

## 5. ✍️ A better one-paragraph summary (use this instead)
> A proposal, not yet built, to close a loop the house designed on 2026-10-17 but never connected. The game harnesses (which test verbs in a throwaway copy of the game) and the pets (which today mostly wander; df-hq pets also dig cells a person marked) count successes in separate banks with separate names. The proposal adds one shared vocabulary to each game's existing verb manifest and keeps two numbers per action: *can* (the harness proves a step list works from scratch) and *does* (live play shows it worked in the real world), never merged. An observed finding motivates it: mcl-hq has 509 recorded pet decisions but no learned weights, and no pet can yet choose to mine, craft or place. The first steps (find why the bank is empty, add the vocabulary) change no behaviour; the step that gives pets an action-choosing layer is large and needs its own design. Weights rank and choose but never promote anything: a person does.

---

## 6. 🖼️ Slide-deck brief (for the agent making the visual summary)
**Audience:** the owner first (a builder who wants honesty and decisions), possibly later shared with collaborators. **Tone:** personal, plain, emoji-friendly, but never overclaiming. **Length:** about 12 slides. **Every claim that is a number must carry its label (OBS / READ / DOC).**
| # | Slide | Key message | Visual idea |
|---|---|---|---|
| 1 | The question | "Will the pets reuse the harnesses? If not, we misdesigned them." | one bold question, two icons (🐾 and 🧪) |
| 2 | The verdict | Not misdesigned: designed 2026-10-17, loop never closed | a loop diagram with one broken arrow |
| 3 | Three banks, no bridge | harness bank, pets' bank, concept/gate bank look alike and don't connect | three boxes, dashed lines between them |
| 4 | What we observed | 509 decisions, 0 learned weights, only walk and door | a small bar chart: walk ok 455, walk blocked 36, door ok 18, mine/craft/place 0 |
| 5 | The surprise | In mcl-hq pets only wander; the logs label steps afterwards. df-hq already has a job queue (DESIGNATE, CLAIM, DONE) that digs | a pet icon with a random-target arrow, next to a small job-board with three cards |
| 6 | The idea | one vocabulary in the existing `verbs.pdl`, no new file | a table strip with BRAIN rows |
| 7 | Two numbers | *can* (works from scratch) and *does* (worked live): filter, then pick | a funnel then a dice, never merged |
| 8 | Scratch vs live | the harness never touches the live game; same verbs, then the real thing | two lanes: throwaway copy / live world |
| 9 | One story | a pet builds one wall: plan, scratch run, live run, numbers update | 5-step comic strip (use the worked example, mark numbers illustrative) |
| 10 | The rules it keeps | one authority, no shell, no new binary, human promotes, one heavy job at a time | a checklist with ticks |
| 11 | Steps P0 to P7 | small and safe first; P3 is large | a ladder with P3 drawn tall |
| 12 | Timing and open questions | early enough, not perfect; 3 decisions open | a timeline plus three question cards |
**Numbers the deck may use (with labels):** 509 rows; walk 455 ok / 36 blocked; door 18 ok; **0** non-empty weight files (OBS); 185 `.pal`, 232 `.pdl`, 21 `.c` of which 15 are ops, 4 tracked `.sh` (OBS); 9 games share the pet core (OBS); 1.5 s and 17 s per scratch run (**measured 2026-10-17, not re-measured**); the mcl-hq ledger is 21 KB against a 2 MiB rotation (OBS).
**Do NOT say:** that pets can build (in df-hq they dig marked cells; nothing builds with mine, craft or place), that anything is built, that the bank's emptiness is explained, that the harness is shell-free, that weights can promote anything, that tomom or an image model is involved, or that the campus run has started (it has not). **Do not publish or paste lore** (unpublished IP).
**Glossary slide (optional):** harness, scratch, lane, verb, event page, op, pal, pdl, bank, weight, DEC row, blueprint (plain-words table is section 0 of the main document).

---

## 7. 🔗 External references (for other AI agents)
**Repo** (house `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`; BOOK = `…/#.#.calendar-dox/!.HQ-IQ-BOOK`)
* `BOOK/08-roadmap/design-docs/PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md` (D48) · `BOOK/17.ai/HARNESS-AS-GAME-BRAIN.md` · `BOOK/16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md`
* `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D35, D36 to D48) · `BOOK/16.game/WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md` · `GAME-MECHANICS-TRAINABLE-EVENTS-2026-11-06.md` · `CENTROID-TEST-AND-LIVE-2026-11-06.md`
* `&.widgits/_shared-lib/petslike_core.c` (`ai_step`, line 1022) · `@.apps/mcl-hq/ops/mcl_hq.c` (lines 296 to 347) · `@.apps/mcl-hq/verbs.pdl` · `@.apps/mcl-hq/state/{decisions.txt,bank/}`
* `&.widgits/_shared-lib/ops/dec_op.c` · `&.widgits/_shared-lib/harness/ops/{playthrough_op,harness_bank_op,harness_case_op}.c` · `&.widgits/_shared-lib/harness/README.md`
**Outside:** Laplace's rule of succession https://en.wikipedia.org/wiki/Rule_of_succession · utility-based AI https://en.wikipedia.org/wiki/Utility_system

*Nothing here was sent to an outside service. The lore stays local.* 💛
