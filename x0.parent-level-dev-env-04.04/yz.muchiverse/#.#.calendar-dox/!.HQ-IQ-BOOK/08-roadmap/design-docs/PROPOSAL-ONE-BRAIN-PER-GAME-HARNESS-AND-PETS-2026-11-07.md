# 🧠🐾 PROPOSAL: one brain per game, shared by the harnesses and the pets (D48, 2026-11-07)

💌 *For the owner. Written by Claude Sonnet 5.5 on 2026-11-07, after you asked: "will the pets building their version in live reuse the harnesses? if not we misdesigned them".*
🏷️ **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it today · **DOC** my reasoning, not run. **Nothing in sections 4 to 9 is built.** This is a proposal for you to accept, change or refuse.

---

## 0. 📖 How to read this (plain words, in case a term is unfamiliar)
| Word | What it means here |
|---|---|
| **harness** | a test that plays the game's own verbs in a throwaway copy and writes PASS or FAIL. Not the game itself |
| **scratch / lane** | scratch = the throwaway copy under `/tmp/hc_*`; the lane (`dev` or `live`) says whose runs a count came from, so a developer's test runs never steer a pet |
| **verb** | one thing the game can do (`mine`, `craft`, `walk`). A C function that appends one row to the game's ledger |
| **event page** | a small file the UI runs when a button or tile fires; it just calls the verb (D47: "the page is what the UI fires") |
| **op** | a small compiled C program run by fork/exec (no shell) that does one job |
| **pal** | the house's tiny assembly runner; a harness `.pal` is three lines: run the case, print the verdict, halt |
| **pdl** | plain-text data in `KEY | a | b` rows: case files, phase tables, banks, vocabularies |
| **bank** | a `.pdl` entry with keywords and a **weight** `(reward+1)/(reward+punish+2)`: a counted success rate that starts at 0.5 when nothing has been seen |
| **DEC row** | one line a pet writes each time it makes a choice: who, what it was doing, which action, what came of it |
| **blueprint** | a plan the game is checked against (for the campus, the prebuilt volume file); the harness checks it, it is not made of it |

**The proposal in one story:** a pet wants to put up a wall. Today it just does or does not, and nobody learns anything. Proposed: it first asks "does a plan like this work at all?" (a harness run in the throwaway copy), then does it for real, and what happens is written down in the same words the harness used, so next time both facts are known: *can it* and *did it work when I tried*.

---

## 1. 🎯 Verdict in five lines
1. **Not misdesigned.** The reuse was designed on 2026-10-17 (`17.ai/HARNESS-AS-GAME-BRAIN.md`): same verbs, same bank file shape, same weight formula.
2. **But the loop was never closed.** Its own section 4 says: no caller exists, promotion is undesigned, no planner reads the weights, and the harness only plays a *scratch* copy.
3. **What I found today makes it concrete (OBS):** the pets' own bank in mcl-hq is **empty** (509 live decision rows, 0 non-empty weight files), and those rows are only `walk` (491) and `door` (18). **No pet has ever decided to mine, craft or place**, which are exactly the verbs the campus run (D38) needs.
4. **The proposal:** one action vocabulary per game as the single authority; **two numbers per action, not one** (`can` from the harness, `does` from live play); a small caller so a pet's plan is judged in scratch and then executed live with the same keys; the material gate named as the skin step.
5. **Timing: early enough, not perfect.** No pet-build history exists to migrate and no caller exists to break, but one more naming scheme (Grok's centroid parts) and my own material gate landed without being mapped. Section 8.


**Decided by the owner on 2026-11-07:** two numbers (`can` and `does`, never merged) and `can_min` = 0.5. Still open, with the reasons written out in section 9: how often a pet may run a scratch proposal (3), what P0 and P1 are (4, explained; recommended first), and the D47 boundary (5).
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

## 2b. 🧰 What the harnesses (and this proposal) are made of: events, ops, blueprints, pal, `.sh`
You asked: do the harnesses use events, ops, blueprints; are they pal or still wrongly `.sh`; did the document say so? **It did not. Here is the answer, counted today (OBS).**
| Question | Answer |
|---|---|
| **Made of pal?** | **Yes.** The harness folder tracks **185 `.pal`** runners; the ones I opened are three lines (`exec harness_case_op`, `exec harness_verdict_op`, `halt`) The README's first line is your 2026-10-07 direction: "harnesses are pal + events, not sh, not C one-offs" |
| **Data?** | **232 `.pdl`** files: case files and phase tables (the attempts), banks, fixtures |
| **Ops?** | **21 `.c` files, 15 of them ops in `ops/`**, do the work pal cannot: `harness_case_op`, `harness_verdict_op`, `playthrough_op`, `harness_bank_op`, audits. Children are started by fork/exec |
| **Events?** | **Yes, two ways.** (1) The case runner has a `PLAY_PAGE` step that plays a real event page in the scratch house (`eat_page`, `mcl_touch_page`, `world_common` cases test pages). (2) The game verbs that a phase table calls are the same C verbs a page calls. But a phase table's `DO | mine | 15 | 4` row calls the **verb on the game binary directly**, not the page; the page is the UI path (D47). Checked in the code: READ `playthrough_op.c` lines 103 to 107 turn `DO | mine | 12 | 9` into `RUN | nice | -n | 15 | <the game binary> | mine | 12 | 9` |
| **Blueprints?** | **Harnesses are not made from blueprints; they check them.** Examples: `blueprintz_*.phases.pdl`, `game_dev_hq_blueprint`, `rpg_pvp_hq_blueprint`, and the campus volume file that `centroid-test` loads and tests. A blueprint is the thing a harness expects to be true |
| **Still `.sh`?** | **Almost none, and the four that exist are explainable (OBS, tracked files):** two are *test fixtures* that pretend to be a broken program (`fake_book_exit3.sh`, `fake_book_silent.sh`); one is *an event page's own script inside a fixture* (`cmd_1.sh`, the thing being tested); one is **`ops/build_harness_ops.sh`, a build script**, which is the house build convention (picked up by `compile-runner.sh`) and not a harness |
| **One real wart found** | `harness_case_op.c` builds `prisc` and `event_page_op` for a `PLAY_PAGE` step with **`system("nice -n 15 gcc ...")`** (two places, lines 242 and 243). That is a **shell-out from C**, against the house "fork/exec, no shell" habit that `playthrough_op` states for itself. It is an existing wart, not something this proposal introduced. I list it as optional step **P7** (replace with fork/exec) and I am not claiming the harness is shell-free |

**And this proposal itself (nothing new in a wrong shape):**
| Piece | Made of |
|---|---|
| `BRAIN` vocabulary rows | `.pdl` rows in the existing `verbs.pdl` |
| `rank` | a new **verb in the existing C op** `dec_op` (no new binary, no new `.sh`; its build line goes in the existing `build_dec_op.sh`) |
| Lint, pins, refusals | **pal + `.pdl` case files** run by the existing runner, each with a negative that must fail |
| Proposal phase tables | `.pdl` (data), written by the pet, git-ignored |
| The pet's planning step | **C inside the game's existing `ai_step`** (as today), starting `playthrough_op` by fork/exec, registered in the process registry |
| Event pages | **Only the UI path.** A pet does not go through a page; it calls the verb, exactly as `ai_step` does today. If you want pets to go through pages too, that is a different design and I would need to say so |
| Blueprints | **Checked, not generating.** The campus blueprint stays data the harness tests; the manager-learns-the-blueprint idea (D40) is a later run and untouched |

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


**Worked example (the numbers are illustrative, marked DOC; the file names and rows are the real formats):** pet `fenn` in mcl-hq wants a stone wall at 11,9.
1. **Goal words:** the blueprint row says `stone wall`. `harness_bank_op find play/bank stone wall place` ranks phase tables; `p3_place` comes first.
2. **Both numbers for `place`** (`dec_op rank`): `can` = weight of `p3_place` in the **live lane** = 0.50 (never run for a pet yet, so no evidence); `does` = 0.50 (no `place` rows yet). Both pass `can_min` 0.5.
3. **Proposal:** fenn writes `play/proposed/fenn-1.phases.pdl` with rows `DO | mine | 15 | 4`, `DO | place | stone | 11 | 9`, and expectations `SAY | placed stone ...`. A phase with no expectation is refused.
4. **Scratch run** (`playthrough_op run`, lane `live`): a throwaway house under `/tmp/hc_*`, the game compiled once, the rows played. Result PASS. The phase weight in the **live lane** becomes `(1+1)/(1+2) = 0.667`, so `can` rises.
5. **Live execution:** fenn calls the same two verbs on the real game. Each appends a normal game-ledger row, and the pet appends `DEC|…|fenn|goal|mine|15,4|ok` and `DEC|…|fenn|goal|place|wall@11,9|ok`.
6. **Digest** (`dec_op bank`) updates `dec_mcl-hq_place`: one reward, so `does` = `(1+1)/(1+2)` = 0.667.
7. **Next time** both numbers are known. If the cell had been occupied, the live row would say `refused`, `does` would fall, and `can` would be unchanged (the scratch world was empty: that is exactly the "good now" gap in 4.5).
The live game was touched only in step 5, by verbs it already knows; the harness never wrote to it.

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
| **P7** (optional) | replace the two `system("nice -n 15 gcc ...")` calls in `harness_case_op.c` (lines 242 and 243, the `PLAY_PAGE` step) with fork/exec | the `PLAY_PAGE` cases still pass (`eat_page` 17, `mcl_touch_page` 12, `world_common` 38 in the README) and `grep -c 'system(' ops/harness_case_op.c` prints 0; a deliberately broken compile path must make the case fail, not hang |
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

## 9. ❓ Your decisions, explained so you can answer
**Decided by you (2026-11-07):** **1. two numbers**, `can` and `does`, never merged. **2. `can_min` = 0.5** (an action with no harness evidence is allowed; one failing run drops it below the line). Both are now written into D48.

### 3. How often may a pet run a scratch proposal? (technical reason)
A scratch proposal is **not cheap** and that is the whole question. What one run does (READ `playthrough_op.c`, `HARNESS-AS-GAME-BRAIN.md` §5): copies the game into a throwaway house, **compiles the game with `gcc -O0` (about 1 s of CPU)**, plays the rows, judges, writes ledger rows, deletes the house. Measured on 2026-10-17 (not re-measured today): **about 1.5 s on a quiet machine, up to 17 s at load average 13**.
| Option | Cost | Verdict |
|---|---|---|
| per game tick (about 30 a second) | a compile per tick | impossible |
| per decision (a pet moves every step) | a compile per step | impossible; also pointless, `walk` needs no proof |
| **per plan** (when a pet starts a multi-step job such as "build a wall") | one run per job | **right unit** |
| per plan, **cached** | a run only when the table text or the game binary content changed or the last run failed (hash, never mtime) | **recommended** |
**Your rules already limit it:** the house "heavy work asks first" rule allows **one heavy job at a time** through `cpu_gate`, and `cpu_gate` answers exit 3 ("wait") when the machine is hot. So the real question is **what a pet does on a wait**: my proposal says it **keeps walking its existing plan, does not retry in a loop, and asks again after its current job ends**. A cap on top of that is optional: with five pets, "at most one new plan per game hour per pet" keeps the total at five runs per game hour. I need one thing from you: **how long is a game hour in real minutes** in the games you run (I did not look, and the number decides whether five runs an hour is nothing or a lot). My default if you do not mind: one plan per pet per game hour, one at a time, cached, never during a game launch.

### 4. What are P0 and P1? (the first two steps, and why they are safe)
* **P0, "find out why the pets' bank is empty."** Today mcl-hq has 509 recorded pet decisions and **no learned weights** (OBS). P0 copies that ledger to a scratch folder and runs the digest by hand. If it works, `walk` should come out at 0.925 and `door` at 0.950 (I computed both from the counts: 456/493 and 19/20). If it does, the maths is fine and the *trigger* is broken (my hypothesis: a counter that restarts with every process). Then I fix the trigger by a byte-count marker file, as the house rule prefers over counters. **Result you can see:** non-empty weight files, and a harness case that fails if they are empty again. **No pet behaves differently** except `walk`, whose chance to pick a new target starts to follow real numbers (the code for that already exists and is currently reading nothing).
* **P1, "add the shared vocabulary."** About six `BRAIN` rows in mcl-hq's `verbs.pdl` and a harness case (pal + pdl) that fails if a phase, action or verb name is made up. **Result you can see:** `verbgen check .` still passes and the new case passes 3 negative tests. **No game code changes at all.**
Both steps change nothing a player or pet does apart from the `walk` side effect above, which is why I want them first. I am **not** starting any pet-build code until you say so.

### 5. The D47 boundary: may pets choose actions by weight on their own? (plain explanation)
There are **three different ways something gets done in the game**:
| Who decides | Example | What D47 says about weights |
|---|---|---|
| **A person** | you click Craft in the window; an event page runs | a weight may **rank** things for you; it never presses the button |
| **A shipped phase table** | a harness or a playthrough a person wrote | a person or the table chooses; rank is a suggestion |
| **A pet on its own** | `fenn` decides where to walk next | **not mentioned by D47**; today it is already automatic for one action: `walk`'s chance to pick a new target is `45 + (0.5 − weight) × 60`, between 15% and 85% |
**The question:** extend that third row from `walk` to `mine`, `craft`, `place`?
* **Yes (my recommendation).** It is the same mechanism already running, only for more actions, and it is what "pets build the campus DF-style, trying more than one attempt" (D38) needs: a pet that never chooses has nothing to learn from.
* **What could go wrong:** a "rich get richer" loop. An action that rarely fails gets a high `does`, so it is picked more, so it collects more rewards. **Guards in the proposal:** the 15% to 85% floor and ceiling (a choice is never forced and never starved), the `can` filter (a failing harness run takes an action out), and the fact that a weight **cannot** fire a page, move the player, edit a bank row or promote anything.
* **If you say no (suggestion-only everywhere):** the numbers are still computed and shown, but pets pick by a fixed order or a person's shipped table. Simpler and safer, but then pets cannot learn to build better, which is the point of D38.
**What I need:** "automatic for a pet's own actions, never for pages or the player" (recommended), or "suggestion only". If you are unsure, say "recommended" and I will build only P0 and P1 first, which leaves this choice open.

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
