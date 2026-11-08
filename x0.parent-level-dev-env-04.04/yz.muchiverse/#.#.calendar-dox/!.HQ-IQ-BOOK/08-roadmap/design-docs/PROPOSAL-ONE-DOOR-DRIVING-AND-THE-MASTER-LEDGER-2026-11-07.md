# 🚪📒 One door: the way we drive a harness is the way a pet, an event or an AI should drive (D49, 2026-11-07)

💌 *For the owner. Written by Claude Sonnet 5.5 on 2026-11-07, after you asked: is it understood that driving a harness is the same way AI, events and pets should drive (for maximum learning and reuse); was that in the docs, house and roadmap; how do other games do it; how should we; and then added that the job queue is meant to be a **master ledger**, like how `WSR_PAL-PREFERED` works.*
🏷️ **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it · **DOC** my reasoning, not run · **EXT** from an outside page I searched (a search result, not read in full).
📎 Companion to D48 (`PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md`) and the nuance brief. **Nothing here is built.**

---

## 1. 🎯 Short answers
1. **Do I understand it?** Yes: *whatever drives the game, a person's click, an event page, a harness, a pet's own step, an AI, does it through the same door: one verb, appended as one row, resolved by the game, recorded in a ledger.* That is what makes everything observable, replayable and learnable, and it is what a harness already does.
2. **Was it communicated in the docs, house and roadmap?** **Yes, in at least five places (section 2).** But **my D48 proposal never stated it as the principle**, and it **missed the master-ledger vision** and the `WSR_PAL-PREFERED` decision chassis. Both are corrected here.
3. **How do other games do it?** Section 5: Dwarf Fortress (jobs from designations, claimed by idle workers), F.E.A.R. (a tiny state machine plus a planner over actions), RTS lockstep (every change is a command). Your architecture is closest to Dwarf Fortress plus lockstep, in plain text.
4. **How should we?** Section 6: intent as rows in one master ledger per game, a **decision slot with tiers** (rule, weights, LLM, human) like `WSR_PAL-PREFERED`, results as game events, and the two numbers (`can`, `does`) deciding which job a free pet claims.

---

## 2. 📚 It was already the vision (READ, with the exact words)
| Where | What it says |
|---|---|
| `16.game/GAME-MECHANICS-SPRINT.md` | "Rules are rows. A screen appends an event. The daemon resolves the tail. **The same verb is the menu click, the job, and the actor's own next step.**" |
| `17.ai/HARNESS-AS-GAME-BRAIN.md` §1, §6 | "Anything that wants to PLAY the game, a person, a pet, the `auto` conductor, has the same problem." And: "`16.game`'s rule stands: **driving the game means appending one command the game already knows.**" The harness does it on a scratch copy; adopting a plan live is "a separate act" through the normal relay or event route |
| `08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` §6, §6b | "**every entity on the desk … operates by WRITING its own intended action to a shared master ledger** … A real game-manager process reads the ledger and EXECUTES each entity's action … entities never mutate shared state directly, they only ever declare intent." And §6b: DSR has a **dual front door**, toy and desk, "onto the exact same backend state" |
| `WSR_PAL-PREFERED/LLM-HOUSE-PILOT.md` | a small local model pilots "the entire WSR house … through a **single file-mediated event bus**" (the relay). "No external APIs, no shims, no Python. C ops + PAL loops only." |
| `WSR_PAL-PREFERED/docs/DRIVING.md`, the khtpm house standard (AGENTS and skill) | drive windows through the **per-pid relay file**, "exactly like a human keypress"; xdotool last |
| `16.game/GAME-MECHANICS-TRAINABLE-EVENTS-2026-11-06.md` (D47) | "A mechanic is done when the verb exists, a harness phase can fail, and a bank seed can be found. The page is what the UI fires." |
**The pattern:** one verb vocabulary, many front doors (click, page, script, pet, model), one ledger of what happened.

---

## 3. 🧩 What exists in code today (OBS)
### 3a. `WSR_PAL-PREFERED` (the sibling you pointed at)
* **Every actor is a piece with a state and an op per state.** `corp_decide` is "the 'deciding' state's op": it reads `decision_mode`, picks **buy / sell / hold** into `pending_action`, then advances the piece from `deciding` to `trading`. A government has `gov_decide` (raise tax / cut spending / hold): "same 5-tier chassis". This is **an FSM per entity, driven by ops**, not one big loop.
* **One decision slot, several fillers.** `decision_mode` 1 is a **weighted rule** (a real port of the original's price formula, not a stub); 3 is **a small LLM** (gemma, one-shot, URL and model overridable); 4 is **a human**: the piece "parks in `deciding` until a human queues a choice" through `corp_set_human_decision`. The human and the AI write **the same `pending_action`**, and the same next state executes it. (I did not read what modes 0 and 2 are; the file names five tiers.)
* **`day_loop.c`** ticks the whole economy daily so it moves without the human pressing End Turn.
* **A behavior catalog with GOAP-shaped rows:** `behaviors/real.behaviors` lists **13 behaviors** (`navigate_to`, `buy_stock`, `end_turn`, …), each with `op`, `pal`, `params`, **`pre`**, **`post`**, **`cost`**. **No planner reads it** (OBS: nothing in `ops/` is named for planning, and the op the rows name, `wsr_fsm_driver.+x`, is **not in `ops/`**). It is a design artifact, honest about being unused.
* Status per the roadmap answers (2026-09): Phase 1, the shareholder registry with cent-exact dividend maths, verified 2026-09-29; Phase 2 in progress; DSR paused until the inventory substrate exists.

### 3b. The master ledger (DSR design, desk proof)
* Row grammar from the roadmap: **`timestamp|turn|actor|action_type|details`**, appended by `desktop_ledger_append()`, executed by a manager op (same pattern as the desktop trigger watcher).
* **OBS:** the house file `#.desktop/master_ledger.txt` has **42 rows, all `touched_npc` by `cursword`** (a castle touch). So the *mechanism is proven for desk touches*; the **economy side was never built on it** (the older `MarS.StreetRace.wsr…/data/master_ledger.txt` is empty, 0 lines).

### 3c. Four related ledgers, four grammars
| Ledger | Row | Role |
|---|---|---|
| **S1 event ledger** (game, `events.txt`) | `EVENT | time | actor | verb | target | at | result` | **what happened**; the game-state authority |
| **master ledger** (desk, DSR design) | `timestamp|turn|actor|action_type|details` | **intent**: what an entity wants done |
| **df-hq `jobs.txt`** | `DESIGNATE|ts|room|x|y`, `CLAIM|…|actor|…`, `DONE|…` | **a job queue**, per game, per kind (23 live rows) |
| **decision ledger** (`decisions.txt`) | `DEC|unix|stamp|actor|state|action|target|outcome` | **what a pet chose and what came of it** |
These four are **the same idea in four dialects**. Nobody wrote that down, which is part of why the pets and the harness never met.

### 3d. Pets in the shared core
`ai_step` keeps or picks a random target, walks one BFS step and greets a neighbour (D48 gap G9). One selected character is driven by input; the rest run `ai_step`. df-hq adds the job rule above.

---

### 3e. 🔺 The Venn diagram: where harness, events and pets actually overlap (added after your correction: "not just to learn, also to act")
You are right that learning alone is not the vision: **the same driver must also *act*.** Checked against `EVENT-VERB-PAGES-DESIGN.md` sections 1, 8 to 10 (READ) and the pet code (OBS):
| Pair | Overlap today | Evidence |
|---|---|---|
| **Harness ↔ events** | **Yes, and it acts.** A harness phase table edits a verb page through the real events-hq manager (`BP edit`), then clicks the same button; the game's behaviour changes (game-dev-hq: the contract page runs `advance`; rpg-pvp-hq: the shop button scavenges), and `BP delete` falls back to the plain verb. RAN 2026-10-20, `passed=16 failed=0` and `PASS 4.6 s` | `EVENT-VERB-PAGES-DESIGN.md` §10 |
| **Events ↔ verbs** | **Yes** for games with generated pages (verbgen from `verbs.pdl`; mcl-hq has `events/mine`, `craft`, `goto`, ...). Page runs `mcl_hq.+x 'mine'` | `@.apps/mcl-hq/events/mine/.../cmd_1.sh` |
| **Pets ↔ harness** | **Learning only, and thin.** The harness judges phase tables in scratch; the pet never runs them. The pet's own step is C | D48 G1, G9 |
| **Pets ↔ events** | **None for acting.** `ai_step` moves a pet directly; it does not call a verb or play a page. mcl-hq's `walk`/`door` DEC rows are labels written after the step. df-hq's dig job is C inside the step (`PET_SECTION` 36) | `petslike_core.c`, `mcl_hq.c`, `df_hq.c` |
| **Player's arrow keys ↔ events** | **Bypass.** The design doc itself says "INT key bindings still bypass the pages" | `EVENT-VERB-PAGES-DESIGN.md` §10 |
**So the diagram fails exactly where the pets and the keys are.** Harness, events and verbs form a working overlap (proven); pets sit outside it. My earlier line "one door is already the rule" (section 7) is true for verbs and pages, **not** for pet steps or arrow keys. D48 said a pet plan would be "executed live with the same keys", but never said the pet must **act through the verb and the page**, so the harness's proven edit-and-play path never reached it. That is the miss.
**Consequence for the design (DOC):** the pet's step should become *choose a job, then fire the verb through the page* (the path `BP edit` already proves), with the DEC row written by that path instead of labelled afterwards. Then the same page is what a click fires, what a harness edits and plays, and what a pet fires, and an edit in blueprintz changes the pet's behaviour too. That is S3 and S4 below, and it should be stated as their acceptance test: **edit the page and the pet's behaviour changes**, the same proof V7 gave the button.

## 4. 🔍 How I read "the job queue is a master ledger like WSR_PREFERRED" (DOC, please correct me)
* **A job is not a df-hq file; it is an intent row in the master ledger.** Anyone with a front door writes it: a person (`DESIGNATE`), a blueprint stamp, **the D40 manager distilling the master blueprint to employees**, a harness, an AI.
* **Actors claim and execute through the same verbs** a person would click (the "one door" rule), and the **result is a game event** (a `DIG`, a `PLACE` row in the S1 ledger).
* **A manager op, not each entity, resolves the ledger** (WSR: "entities never mutate shared state directly").
* **The same slot is filled by different drivers** (WSR's tiers): a rule, learned weights, a model, or **you playing**: that is the answer to "driver or mini FSM?": **both, as one slot with tiers.**
* **Why that is the maximum-learning shape:** every driver leaves the same rows, so a harness proves a step list in scratch, a pet runs it live, a human can overrule it, and one counted weight set learns from all of them.

---

## 5. 🌍 How other games do it (EXT: from the searches I ran today; verify before quoting)
| Game or field | How it drives actors | Likeness to ours |
|---|---|---|
| **Dwarf Fortress** (wiki, DFHack docs) | **Jobs are created by designations, zones, workshop tasks and manager work orders; an idle dwarf with the matching labour enabled takes the job.** A **labour is a permission switch**, not a preference. **Needs (eating, sleeping, drinking) appear as jobs automatically.** Designation priority 1 to 9 exists for some jobs (an anecdote, not an official spec). DFHack `autolabor` assigns labours by skill | **Closest to df-hq's `DESIGNATE`/`CLAIM`/`DONE`.** A labour switch is our `can` filter; the manager work order is the D40 manager |
| **F.E.A.R.** (Jeff Orkin, GDC 2006 "Three States and a Plan") | a finite state machine with **only three states**, plus **A\* planning over actions with preconditions and effects** (GOAP, adapted from STRIPS) | The shape of the `.behaviors` rows (`pre`, `post`, `cost`) is GOAP-like; the planner is the part we do not have |
| **RTS lockstep** (an indie developer write-up, and two engine design docs; unreviewed) | **only player commands are exchanged** after a synced start; all state changes go through commands; **a replay is the start state plus the commands**; a periodic state checksum catches desync; an explicit **idle** command exists | Exactly "one door": our harness `p5_replay` (a fresh game fed the same event ledger replays to the same state) is the same test. The sources also note rewinding backward is hard and needs checkpoints |
| **ML game environments** (Gym-style APIs, MineRL) | my search **did not confirm** whether the agent's action space equals what a human performs, so I am **not claiming** that | Same aim, unverified |
**Where we differ on purpose:** plain-text append-only ledgers with byte cursors (never mtime), no always-running engine per entity, ops by fork/exec, human promotion of anything learned.

---

## 6. 🏗️ How we should do it, by our architecture ideals (DOC recommendation)
Using the standing design-choice test (one authority per fact; modular; scalable to the tenth consumer; long term; house shapes; cost):
**P1. One door (a rule, not a build).** *Every driver changes the game only by appending the same kind of row through a verb the game already knows.* A person's click, an event page, a harness `DO` row, a pet's step, an AI proposal. Nothing writes game state any other way. (Already the rule; restate it where D48 and D47 meet.)
**P2. Intent in one master ledger per game, results in the S1 ledger, choices in the DEC ledger.** Three roles, **one row grammar** (the master ledger's `timestamp|turn|actor|action_type|details`, or the S1 row if the owner prefers one authority; see decision 1). Jobs are `action_type = job_designate | job_claim | job_done`. df-hq's `jobs.txt` is the first dialect to migrate (its rows carry a timestamp, a room and cell, and on `CLAIM` an actor, in a different order and without a turn column, so the mapping is real work, not a rename).
**P3. A decision slot with tiers, like the WSR chassis.** Each actor has a state and a `pending_action`. **Tier 0 random** (today's pet), **1 weighted** (`can` filters, `does` picks, D48), **2 learned or planned** (later, a GOAP over the `.behaviors` rows), **3 a model** (gemma on the Mac, free), **4 a human driver** (you take over any pet, which the house already allows). All write the same `pending_action`; the same verb executes. Tiers are data (`decision_mode`), so a pet can be promoted or demoted without code. *(WSR's own numbers are 1 weighted, 3 LLM, 4 human; I did not read its 0 and 2, so tiers 0 and 2 here are my proposal.)*
**P4. A behavior catalog as the planner's input.** `.behaviors`-style rows (`op`, `pre`, `post`, `cost`) per game, **generated from or checked against `verbs.pdl` and the `BRAIN` rows** (one authority). The harness phase table is a *sequence of behaviors*; the same table is judged in scratch, then executed live.
**P5. One manager op resolves the master ledger** (the D40 manager writes jobs; a free pet claims by tier; the verbs append results). Registered process, byte-cursor, capped files, one heavy job at a time.
**P6. Learning stays two numbers and human promotion.** `can` from harness runs, `does` from live outcomes, never merged; weights rank and choose, never promote.
**What this changes in D48:** P0 and P1 stand. The "pet action layer" (P3 there) becomes **"a job claim through the decision slot"**, and the first dialect is df-hq's queue, **not a new FSM**. I have updated D48's section 4.8 to point here.

### Staged order (smallest provable step first)
| Step | What | Proof that can fail |
|---|---|---|
| **S0** | D48 P0: find why mcl-hq's bank is empty | non-empty weights equal the hand count (walk 456/493 = 0.925, door 19/20 = 0.950) |
| **S1** | D48 P1: `BRAIN` rows and a lint | made-up phase, action and verb each fail |
| **S2** | **Write the row-grammar decision** (decision 1) and a one-page mapping of the four dialects | a harness case that parses a df-hq `jobs.txt` row and its master-ledger equivalent to the same fields |
| **S3** | Tier slot (`decision_mode`) for mcl-hq pets, and the pet **fires its chosen verb through the verb page** | tier 0 reproduces today's walk behaviour in a scratch run; **editing the page with `BP edit` changes what the pet does** (the V7 proof, applied to a pet) |
| **S4** | `JOB` rows for mcl-hq `mine craft place`, claimed by tier | a deliberately wrong job is refused and never touches the live ledger |
| **S5** | Manager op resolves intent rows | a removed bridge job leaves D40's side island unreachable (Grok's existing check) |
| **S6** | `.behaviors` catalog for mcl-hq and a planner as tier 2 | the plan equals the hand-written phase table for the same goal |
S2 to S6 each need their own short design page before code; none starts without your go.

---

## 7. ✅ Was it all communicated? An honest check
| Topic | In the docs | In my D48 proposal | Status now |
|---|---|---|---|
| One door: same driving path for person, harness, pet, AI | **Yes** as a vision (section 2); **in code only for verbs and pages**, not pet steps or arrow keys (3e) | partly, **not stated as the principle, and it never required pets to act through verbs and pages** | stated here (P1, 3e) |
| Master ledger as the job queue | **Yes** (TEST-GAMES §6) | **No**: I proposed a per-game `jobs.txt` generalisation | corrected (P2; D48 4.8 updated) |
| WSR decision chassis (a state per entity, a decision slot, human and AI fill the same slot) | in code and comments, thin in docs | **No** | described (3a, P3) |
| Behavior catalog with pre/post/cost | exists as data, unused | **No** (I said no GOAP exists, true, but missed the data) | described (3a, P4) |
| How other games do it | no | **No** | section 5 (EXT, verify) |
| What is built vs not | partly | yes | unchanged: **nothing here is built** |

---

## 8. ❓ Decisions for you (none blocks S0 and S1)
1. **One row grammar: which?** (a) the master ledger's `timestamp|turn|actor|action_type|details` for intent, with S1 `EVENT` rows for results; or (b) one S1-style row for everything. I recommend (a): it is what DSR and WSR already specify, and it keeps *intent* and *what happened* as different facts, which is what lets a manager refuse an intent without corrupting history.
2. **Where does a game's master ledger live?** The house one is `#.desktop/master_ledger.txt`; a game would get its own under its `state/`. I recommend per game (a pet world's jobs should not be in the desktop's file).
3. **Tiers as data (`decision_mode`)?** I recommend yes, default tier 0, so nothing changes until a pet is promoted.
4. **May a human take over any pet's slot** (tier 4)? The house already lets you control one character; this makes it the same path as the AI's.

---

## 9. 🔗 External references (for other AI agents)
**Repo** (house `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`; the WSR project is `x0.parent-level-dev-env-04.04/yz.muchiverse/WSR_PAL-PREFERED`; BOOK = `…/#.#.calendar-dox/!.HQ-IQ-BOOK`)
* `BOOK/16.game/GAME-MECHANICS-SPRINT.md` · `BOOK/17.ai/HARNESS-AS-GAME-BRAIN.md` · `BOOK/08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` (§6, §6b) · `BOOK/RUSSIAN_DOLL_HOUSE_DESIGN/AGENT_ROADMAP_ANSWERS.md` (§3) · `BOOK/17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md`
* `WSR_PAL-PREFERED/ops/{corp_decide,gov_decide,corp_set_human_decision,day_loop}.c` · `WSR_PAL-PREFERED/behaviors/real.behaviors` · `WSR_PAL-PREFERED/docs/DRIVING.md` · `WSR_PAL-PREFERED/LLM-HOUSE-PILOT.md`
* `44.xyz.01.00/#.desktop/master_ledger.txt` · `@.apps/df-hq/ops/df_hq.c` (lines 91, 240, 314) and `@.apps/df-hq/state/jobs.txt` · `&.widgits/_shared-lib/petslike_core.c` (`ai_step`)
* D45 to D48 in `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`
**Outside (EXT; searched 2026-11-07, not read in full)**
* Dwarf Fortress labour and jobs: https://dwarffortresswiki.org/index.php/DF2014:Labor · https://dwarffortresswiki.org/index.php/DF2014:Job · DFHack autolabor https://dfhack.readthedocs.io/en/latest/docs/tools/autolabor.html
* Orkin, "Three States and a Plan: The AI of F.E.A.R." (GDC 2006): https://gdcvault.com/play/1013282/Three-States-and-a-Plan · his GOAP page http://alumni.media.mit.edu/~jorkin/goap.html (named in a search result, not opened)
* Lockstep and command-based replays (illustrative, not authoritative): https://www.gamingonlinux.com/2017/12/rewrite-the-past-in-airships-conquer-the-skies-with-the-new-replay-feature · https://forgottenhistory.github.io/Archon-Engine/Docs/Engine/multiplayer-architecture.html · https://iron-curtain-engine.github.io/iron-curtain-design-docs/netcode/protocol.html

*Nothing here was sent to an outside service. The lore stays local.* 💛
