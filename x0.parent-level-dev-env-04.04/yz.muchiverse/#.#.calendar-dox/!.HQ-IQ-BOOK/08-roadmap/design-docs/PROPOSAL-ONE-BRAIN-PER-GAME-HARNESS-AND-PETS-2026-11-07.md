# 🧠🐾 PROPOSAL: one brain per game, shared by the harnesses and the pets (D48, 2026-11-07)

💌 *For the owner. Written by Claude Sonnet 5.5 on 2026-11-07, after you asked: "will the pets building their version in live reuse the harnesses? if not we misdesigned them".*
🏷️ **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it today · **DOC** my reasoning, not run. **Nothing in sections 4 to 9 is built.** This is a proposal for you to accept, change or refuse.

---

## 1. 🎯 Verdict in five lines
1. **Not misdesigned.** The reuse was designed on 2026-10-17 (`17.ai/HARNESS-AS-GAME-BRAIN.md`): same verbs, same bank file shape, same weight formula.
2. **But the loop was never closed.** Its own section 4 says: no caller exists, promotion is undesigned, no planner reads the weights, and the harness only plays a *scratch* copy.
3. **What I found today makes it concrete (OBS):** the pets' own bank in mcl-hq is **empty** (509 live decision rows, 0 non-empty weight files), and those rows are only `walk` (491) and `door` (18). **No pet has ever decided to mine, craft or place**, which are exactly the verbs the campus run (D38) needs.
4. **The proposal:** one action vocabulary per game as the single authority; **two numbers per action, not one** (`can` from the harness, `does` from live play); a small caller so a pet's plan is judged in scratch and then executed live with the same keys; the material gate named as the skin step.
5. **Timing: early enough, not perfect.** No pet-build history exists to migrate and no caller exists to break, but one more naming scheme (Grok's centroid parts) and my own material gate landed without being mapped. Section 8.

---

## 2. 🧱 What exists today (three banks that look alike and are not connected)
| # | Bank | Writer → file | Evidence it counts | Reader today |
|---|---|---|---|---|
| 1 | **Harness phase bank** | `playthrough_op ledger` → `harness_bank_op update` → `&.widgits/_shared-lib/harness/play/bank/live/<game>__<phase>.behavior.pdl` (`COUNTS reward punish cursor`, `WEIGHT`) | a phase PASS or FAIL **in a scratch copy** (`/tmp/hc_*`, a fresh game) | `harness_bank_op find` (ranks), nothing else. Decision D45: `find` does not run the winner |
| 2 | **Decision bank** | the game appends `DEC|unix|stamp|actor|state|action|target|outcome` to `<app>/state/decisions.txt`; `dec_op bank` → `<app>/state/bank/dec_<game>_<action>.behavior.pdl` | one reward per row whose outcome is not blocked/miss/refused, one punish per row whose outcome is, **in the live game** | one reader: `dec_op` computes `pick_pct = 45 + (0.5 − weight("walk")) × 60`, clamped 15..85 (hard-coded, not a .pdl policy) |
| 3 | **Concept bank, look table, gate** | masters/spokes (`concept-bank/data`), `look_assoc.*` + `look_words.txt`, `material_concepts.pdl`, studio user layer `state/studio_bias.pdl` | hand-set and reviewed weights (agent-reviewed under your 2026-11-06 delegation) | `gate_op` (readout and studio edits); nothing in the game reads it yet. Grok's own note: "the campus stamp does not read the gate" |

**Same shape, same Laplace formula** `(reward+1)/(reward+punish+2)` in banks 1 and 2 (READ `harness_bank_op.c`, `dec_op.c`), so one reader (`harness_bank_op find`) can already rank both. That is why the unification is cheap. The three differ in **what the count means**, and that difference is the whole design problem (section 4.2).

**Lanes (READ `harness_bank_op.c`):** `dev` = developer runs, kept as the corpus an AI learns *how to develop* from; `live` = "runs made for the AI/game itself … the only lane a pet or game should read weights from". Default lane: `dev` for `update`, `live` for `find`. Every ordinary gate run (including all of mine and Grok's suites) is a dev run, so **the live lane is empty by construction** until an actor writes to it.

---

## 3. 🔎 The seam, stated precisely (each item labelled)
| # | Gap | Evidence |
|---|---|---|
| G1 | **No caller.** No pet, tomom run or `auto` conductor calls `playthrough_op run` | READ `HARNESS-AS-GAME-BRAIN.md` §4.1 (2026-10-17; I did not find a caller today either, DOC) |
| G2 | **Vocabulary mismatch.** Harness phase ids (`p1_mine`, `p2_craft`, `p3_place` for mcl-hq) vs pet DEC action keys (`pick`, `walk`, `door`, `dig`, `chase`, `strike`). They never name the same thing | OBS: mcl-hq phases (`mcl_hq.phases.pdl`) and its 509 DEC rows: 491 `walk`, 18 `door`, **0 mine/craft/place**. Grok's `centroid_parts_op` adds a *third* granularity: stamps `floor, wall, door, roof, bridge` |
| G3 | **The pets' bank is empty.** `state/bank/` has `dec_cursor.txt` (0 bytes) and two weight files (0 bytes) next to a 509-row ledger | OBS. **Cause not established.** Hypothesis (DOC): `dec_bank()` runs "every 200 decisions" from a `static int since` counter, but each game verb is a fresh process, so the counter never reaches 200; the digest then runs only on the `review` verb. Must be checked first (step P0) |
| G4 | **Scratch only.** The harness judges a *fresh* game, which answers "can this step list work at all", not "is it good now" | READ §4.5 and `playthrough_op.c` ("never touches a live game") |
| G5 | **One reader, one action.** Only `walk` feeds back (`pick_pct`) | READ `dec_op.c` |
| G6 | **The skin step has no reader.** The stamp places voxels without asking the material gate; no harness fails when the material name is wrong | READ Grok's `WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md` ("Not yet") |
| G7 | **D45 to D47 count in bank 1 only.** Verb pages, phases and the campus phrases all rank through the harness bank. I found no mention of `decisions.txt` or `dec_op` in D45, D46, D47 or their two pages, so the live evidence (bank 2) has no place in them yet | READ D45 to D47, `GAME-MECHANICS-TRAINABLE-EVENTS-2026-11-06.md`, `SELF-CODING-HARNESS-PLANS-2026-11-06.md` |
| G8 | **A stated rule to reconcile.** D47: "A higher weight does not auto-fire the page in the live house. A person, or a phase table a person shipped, chooses the attempt. Rank is a suggestion." Pets already choose on a weight for `walk` (`pick_pct`). The two have to be drawn apart (section 4.2, decision 5) | READ `GAME-MECHANICS-TRAINABLE-EVENTS-2026-11-06.md` and `dec_op.c` |

---

## 4. 🏗️ The proposal

### 4.1 One action vocabulary per game: the `verbs.pdl` that D47 already made the manifest
**Revised after reading D45 to D47 (Grok, 2026-11-06):** `@.apps/<game>/verbs.pdl` is already the per-game manifest of verbs (`VERB | name | page | kind | args | description`, read by `verbgen`; mcl-hq has 44 verbs, including `mine`, `craft`, `walk`). A new `actions.pdl` would be a **second authority for the same fact**, so this proposal adds **one row type to the same file** instead:
```
BRAIN | dec_key | verb  | phases   | ok_outcomes | bad_outcomes | note
BRAIN | walk    | walk  | -        | ok          | blocked      | exists today in DEC rows
BRAIN | door    | -     | -        | ok          | blocked      | engine-internal (a step that crossed a door); no verb page
BRAIN | mine    | mine  | p1_mine  | ok          | refused      | new for the campus run
BRAIN | craft   | craft | p2_craft | ok          | refused      | new
BRAIN | place   | place | p3_place | ok          | refused,taken| new (the verb name is checked against the VERB rows)
```
* **Safe for the existing tool (READ `verbgen.c` lines 43 and 44):** it parses only `GAME` and `VERB` rows; other rows are skipped. A `verbgen check` run after adding the rows must still agree (a step in P1).
* **Existing keys keep their names** (`walk door pick dig chase strike`), so no ledger, bank file name or phase id changes and nothing is migrated.
* `phases` maps harness phase ids to the action they exercise; `-` marks actions with no phase and phases that are not actions (`p4_saveload`, `p5_replay`) are listed as `phase_only` in a one-line `BRAIN | - | - | p4_saveload | ...` row.
* **One authority per fact:** the verb pages (D47), the harness phase tables, the DEC writers and the ranker all read this one file. A **lint case** fails when (a) a PHASE id in `play/phases/<game>.phases.pdl` maps to nothing and is not `phase_only`, (b) a DEC row from a scratch run carries an action with no `BRAIN` row, or (c) a `BRAIN` row names a verb that has no `VERB` row. Negative test: a made-up phase, a made-up action and a made-up verb each fail.
* Centroid stamps (`floor wall door roof bridge`) are **sequences of actions**, not actions: `bridge = place × N`. They stay a harness/blueprint concept (`centroid_parts_op`), never a DEC key.

### 4.2 Two numbers per action, not one (the key design choice)
| Number | Means | Source | Formula |
|---|---|---|---|
| **can** | "this step list **works from scratch**" (feasibility) | harness `live`-lane phase weights for the phases mapped to the action | `min` over the mapped phases of `(pass+1)/(runs+2)`; no evidence = 0.5 |
| **does** | "this choice **worked when a pet made it in the live game**" (utility) | the decision bank (`dec_op weight`) | `(reward+1)/(reward+punish+2)`; no evidence = 0.5 |

**Choice rule:** among actions with `can ≥ can_min` (default 0.5, so *no evidence passes*, and one failing harness run drops an action below the line), pick by `does`, as a weighted random pick whose floor and ceiling come from `.pdl` **policy rows** (replacing today's hard-coded 15..85).
**Why not one merged count (rejected, DOC):** a scratch pass proves feasibility from a *fresh* game; a live outcome proves the choice was good in *this* world. Merging lets forty scratch passes drown five live `blocked` rows, and makes the number mean nothing. Two numbers keep the harness as a **validity filter** and live play as the **preference**, which is also how a person plays: first "can I?", then "should I?".

**Boundary with D47 (needs your decision 5):** D47 forbids a weight from auto-firing a *verb page* for a person (the UI, the player's actions, a shipped phase table). This proposal keeps that: the automatic choice applies **only to a pet's own autonomous choice among its enumerated actions**, exactly the class `walk` already is today through `pick_pct`. A weight never fires a page, never moves the player, never edits a bank row, and never promotes anything. If you want pets to be suggestion-only too, the `does` and `can` numbers still rank and are shown, and a person or a shipped table picks.

### 4.3 The ranker is one new verb on the existing op (no new process)
`dec_op rank <bank_dir> <harness_bank_dir> <verbs.pdl> <game> [action...]` prints `action | can | does | pick`, sorted, from files only; deterministic; exit 2 on a bad file. `dec_op` already parses both bank shapes. **No daemon, no new binary.**

### 4.4 The caller: a pet's plan = a phase table, judged in scratch, executed live
1. **Goal** in words (from the blueprint row, e.g. "stone wall at 11,9").
2. `harness_bank_op find <play bank> <words>` ranks known phase tables; the pet writes a proposal from **enumerated verbs only** (contract of `HARNESS-AS-GAME-BRAIN.md` §5; a phase with no expectation is refused).
3. `playthrough_op run` with **`HARNESS_BANK_LANE=live`** (set by the caller, so a pet's proposal lands in the live lane and developer runs stay `dev`): the table runs in `/tmp/hc_*`, never touching the live game.
4. If it passes, the pet executes **the same verbs live, each as one appended command the game already knows** (the standing rule: no second play button), writing DEC rows with the **same action keys**.
5. The next digest updates `does`; the next proposal sees both numbers.
* **Cost (OBS from the doc, not re-measured):** a run is about 1.5 s on a quiet machine and 17 s measured under load 13, mostly the `gcc -O0` of the game. So it is **per plan, not per tick**: at most one proposal per pet per game hour, one at a time through `cpu_gate`, and a **cache** (skip the scratch run when the table hash and the verb binary's content hash are unchanged and the last run passed; hashes, **never mtime**).
* **Processes (house rule):** the planner is a short fork/exec child of the pet step, **registered** through `proc_reg_op`; it exits when done; no `setsid nohup`. Idle cost: zero.

### 4.5 "Good now": a read-only live snapshot for the scratch run (design only)
A new `INIT snapshot=<n>` row copies the **last n KB of the live `events.txt`** (cap 256 KB) into the scratch house, read-only, so a proposal is judged against the real occupied cells. Without it, "can I place a wall at 11,9" is judged on an empty world. This closes G4 and is the biggest single piece of work; it can follow the rest.

### 4.6 The material gate becomes the skin step (closes G6)
A campus event page runs `gate_op gate … <words from the blueprint row>` and writes the top material name to a file; the stamp reads that file for the skin. A harness **fails when the name is wrong** (the pin already exists: "stone wall" → `stone` in Grok's `campus_look`, 9/0). This is Grok's own "not yet" condition made into a test. The `place` DEC row carries the target as `wall@x,y`; the material is a **skin** of the same action, not a new action.

### 4.7 Promotion stays human (unchanged)
A high `does` or `can` ranks and chooses; it **never writes the concept bank**. Turning a weight into a bank entry or a default plan stays the attrition path (candidate EDIT, validator, a person promotes; `AUTO-PROMOTION-RULE.md` stays a stub). tomom stays the later learner, not a dependency.

---

## 5. ⚙️ Technical details
**Files**
| File | Kind | Cap and guard |
|---|---|---|
| `BRAIN` rows in `@.apps/<game>/verbs.pdl` | tracked data (existing file) | lint case; about 10 to 40 rows |
| `play/proposed/<actor>-<n>.phases.pdl` | git-ignored runtime | 20 per actor, oldest rotated; row in `RUNTIME-FILE-GROWTH-STANDARD.md` |
| `play/results/proposal_cache.txt` | git-ignored runtime | 200 rows, marker-style append, trimmed whole |
| `state/decisions.txt`, `state/bank/*` | existing | existing 2 MiB rotation (`DEC_MAX_BYTES`); the cursor is a byte offset, never mtime |
| policy rows (`pick floor/ceiling/can_min`) | tracked `.pdl` | replaces the hard-coded 45/60/15..85 flagged in `dec_op.c`'s own header |

**Compatibility:** every existing key, file name, phase id and ledger row format stays. New keys (`mine craft place`) have **no history to migrate**.
**Determinism:** `rank` and `find` are pure functions of files; the only randomness is the pet's weighted pick, seeded from the game clock like the existing pick.
**Evidence hygiene:** `can` reads only the `live` lane; every developer suite stays in `dev` and cannot move a pet.
**No Python, no new `.sh`:** op verb + `.pdl` + a pal harness case, per the house rule.

---

## 6. 🆚 How this differs
| Today | Proposed |
|---|---|
| Three banks, three vocabularies | **One vocabulary** (the existing `verbs.pdl`, plus `BRAIN` rows) that all three read |
| One number (a Laplace weight) per bank | **Two numbers** with different meanings, combined by a rule, never merged |
| Harness and pets never exchange anything | Harness = validity filter, live DEC = preference, **same action keys** |
| Pets only feed `walk` back | Every mapped action feeds back; policy in `.pdl` |
| The gate is a readout | The gate **names the skin** and a harness fails on a wrong name |
| Scratch judges a fresh world | Optional read-only live snapshot judges the real one |
| Not tomom, not attrition | Same safety rails; this is the **runtime choice loop**, attrition remains the offline, human-reviewed promotion path |

---

## 7. ✅ Build order, smallest provable step first (DOC)
| Step | What | Proof that can fail |
|---|---|---|
| **P0** | **Find why mcl-hq's bank is empty.** Run `dec_op bank` by hand on the 509 rows in a scratch copy; check the 200-row counter hypothesis | non-empty weights equal the hand count I took today from `decisions.txt` (OBS: walk 455 ok / 36 blocked, door 18 ok, so walk weight = 456/493 = 0.925 and door weight = 19/20 = 0.950); then fix the trigger |
| **P1** | `BRAIN` rows in mcl-hq `verbs.pdl` + lint case (`verbgen check` must still pass) | a made-up phase and a made-up DEC action each fail |
| **P2** | `dec_op rank` + policy rows | pinned output for fixed files; a failing harness phase drops the action below `can_min`; `can_min` change flips the pick |
| **P3** | mcl-hq pet planner for `mine craft place` (new DEC keys), proposal in scratch, execute live | scratch run passes; live rows carry the same keys; a deliberately wrong proposal is refused and never touches the live ledger |
| **P4** | gate event page + stamp reads the material | harness fails when the name is wrong (negative pinned) |
| **P5** | `INIT snapshot` for "good now" | a wall on an occupied cell fails in scratch only with the snapshot |
| **P6** | repeat for df-hq (`dig`) and cdda-hq (`strike`) | same lint and pins per game |
Each step is heavy-work-gated (`cpu_gate`, one at a time, niced) and reports its idle and active cost with a browser open.

---

## 8. ⏱️ Was this a good time to catch it, or too late? (honest)
**Good, because (all OBS or READ):**
1. **No history to migrate.** mcl-hq's 509 live rows are `walk` and `door` only; `mine craft place` have never been decided by a pet. df-hq has 196 and cdda-hq 162 rows (OBS: both contain only `walk` and `door`; the `dig`, `chase` and `strike` keys exist in the code but not in these ledgers). New keys start clean.
2. **No caller to break.** `HARNESS-AS-GAME-BRAIN.md` §4.1: nothing calls `playthrough_op run`. Changing its contract costs nothing yet.
3. **D40 left a window.** The centroid test page is *prebuilt*; the pets raising it is "a later run". No pet-build decisions exist to be mis-keyed.
4. **The ledgers are tiny** (mcl-hq's is 21 KB against a 2 MiB rotation), so if a re-key were ever needed it can re-read from byte 0.
5. **The pieces exist** (same file shape, same formula, `find`, lanes, `dec_op`), so this is wiring, not invention.

**Not perfect, because:**
1. **D38 proposes a new phase table on `playthrough_op` and its text does not mention the decision ledger** (OBS: no mention; I do not know what its author read), **and I (Claude) wrote the material gate with its own user layer and `bias` rows without mapping it to the other two banks.** The first is a gap I found, the second is my own miss; I am naming both.
2. **A third granularity landed** (Grok's centroid stamps, 29 checks in `centroid/parts.pal`). It is easy to map (4.1), harder with every added table.
3. **G3 means the pets' loop has been silently open for weeks** (empty bank beside a full ledger). That is an existing bug, not a design flaw, but it shows nobody was reading the loop's output.

**It would have been too late if:** a pet run had already written ad-hoc action names; if the ledger had rotated into the archive (re-keying then means re-reading archives); if more phase tables had hardened names; or if pets already ran proposals in the `dev` lane (counts must never be mixed, by rule). None of those has happened.

---

## 9. ❓ Decisions I need from you (none blocks P0 and P1)
1. **Two numbers (`can` and `does`), or one merged weight?** I recommend two (4.2).
2. **`can_min` default 0.5** (no evidence passes) or stricter (a pet may not try an action the harness has never passed)? I recommend 0.5 first.
3. **How often may a pet run a scratch proposal?** I proposed one per pet per game hour, one at a time, with a cache. CPU on the weak machine is the limit.
4. **Priority:** P0 and P1 first (small: find the empty bank, add the BRAIN rows), then P2, before any pet-build code. OK?
5. **The D47 boundary (section 4.2):** may a pet choose its own *actions* by `can` and `does` automatically (as `walk` does today), with a weight never firing a page for a person? Or suggestion-only everywhere? I recommend automatic for a pet's own actions only.

---

## 10. 🔗 External references (for other AI agents)
**Repo** (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`; BOOK = `…/#.#.calendar-dox/!.HQ-IQ-BOOK`)
- `BOOK/17.ai/HARNESS-AS-GAME-BRAIN.md` (the original design; §4 = what was never built)
- `BOOK/16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md` · `BOOK/06-testing/GAME-PLAYTHROUGH-HARNESSES.md` · `BOOK/08-roadmap/design-docs/HARNESS-BEHAVIOR-BANK-DESIGN.md`
- `BOOK/16.game/WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md` (Grok) · `BOOK/16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md` · decisions D38, D39, D40, D45 in `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`
- `&.widgits/_shared-lib/ops/dec_op.c` · `&.widgits/_shared-lib/harness/ops/{playthrough_op,harness_bank_op}.c` · `&.widgits/_shared-lib/harness/play/phases/mcl_hq.phases.pdl`
- `@.apps/mcl-hq/ops/mcl_hq.c` (`dec_row`, `dec_log`, `dec_bank`) · `@.apps/mcl-hq/state/{decisions.txt,bank/}`
- `@.apps/generator-hq/ops/gate_op.c` · `events/gen_material/material_concepts.pdl` · `&.widgits/_shared-lib/harness/cases/campus_look.pdl`
- `BOOK/08-roadmap/design-docs/GROK-HANDOFF-TO-CLAUDE-CENTROID-2026-11-06.md`

**Outside**
- Laplace's rule of succession (the `(r+1)/(r+p+2)` estimate): https://en.wikipedia.org/wiki/Rule_of_succession
- Utility-based AI and behaviour selection in games (the "can I / should I" split): https://en.wikipedia.org/wiki/Utility_system
- Dwarf Fortress job and labour priorities (the reference the owner named for the pets' life loop): https://dwarffortresswiki.org/index.php/DF2014:Labor

*Nothing here was sent to an outside service. The lore stays local.* 💛
