# Roadmap and state of play (2026-10-07): where everything is, what works, what is not wired

Written by claude after the owner asked: "where is it (pages? toys?), can it start from player start, and has all our previous work and documentation been indexed in a roadmap?" This replaces the earlier `WEEK-2026-10-07-DESIGN-INDEX.md` as the single index (that file is kept, and is stale: it predates the builds below). Legend: **B** built and harness-passing in alpha; **D** design doc only; **L** live (in the owner's running tree); **?** not verified live.

## 1. Honest answer on "can it start from Player start?"
**No, not yet, and nothing built this week has been run from the live desktop.** Everything built lives in the **alpha branch** (`claude-alpha`, pushed to origin, **not merged into live**), as **template files** under `&.widgits/...`. Verified by harness on scratch roots only (headless, `sh -c` of the same menu action strings). Specifically for the Footrace Fu race (and the ring board and digipet):
- **Where it lives:** `44.xyz.01.00/&.widgits/footrace-fu/` (data, 182 event pages, ops, menu rows in `entity/meta.pdl`), harness `footrace_fu_race` (707/0). It is **not under a page, not a toy, not installed on any desk**.
- **Toys dropdown:** the taskbar's Toys cell lists any directory under the house root, `@.apps`, `&.widgits` or `&.hq-apps` that contains a **`toy.pdl`** (opt-in by file presence; `META|title`, `META|launch`). `&.widgits/footrace-fu/` has **no `toy.pdl`**, and the live taskbar runs from the live tree where this code does not exist yet.
- **Player start (taskbar "1.play: ON/OFF"):** it only writes `#.desktop/khtpm_play_mode.state.txt`. **Nothing starts a game, spawns an entity or starts a clock from it.** The digipet build confirmed the missing hook ("no play-to-clock hook").
- **Pages/desks:** a game entity is placed on a desk by `desk_copy_op`-style install or a `DESK` row; the Footrace Fu entity has only a by-hand install note (README).
**What would make it start from Player start (new work, in order):** (1) a `toy.pdl` so it appears in Toys; (2) an install/spawn step that places the game entity on a desk (rehearsed in beta with a backup, never on live data first); (3) the play hook: Play ON reads `game.pdl` (`GAME | start` entity, `SETUP` rows), starts the main clock, and runs the game's New Game page; (4) a live check through the relay on a beta copy. All designed in `DSR-AS-PHYSICAL-WSR-PLAN.md` and `GAME-SETUP-PDL-DESIGN.md`; none built.

## 2. State of play
| area | status | where | proof |
|---|---|---|---|
| Clock: recurrence, catch-up, commands, chaining/master, restore check | **B** | alpha `&.widgits/livedesk-clock/ops/lc_clock.c` | lc_clock_base 27, recur 54, cmds 103, restore 27 |
| Snapshot store + restore | **B** | alpha `&.widgits/_shared-lib/ops/game_snapshot_op.c` | game_snapshot 118 |
| Chain: multichain, faucet, escrow, daily mining cap | **B** | alpha `041.pal-chain/ops/` | chain_escrow_faucet 132 |
| Physics nodes (gravity/orbit/escape) | **B** | alpha `&.widgits/physics-nodes/` | phys_nodes 31 |
| Solar sandbox (system + Earth/Moon/Mars pages) | **B** (templates) | alpha `&.widgits/solar-sandbox/` | solar_sandbox 41 |
| pc-hq levels index fix + harness | **B** | alpha `maps.pdl`, `pchq_levels` | pchq_levels 99 |
| Digipet hunger loop on the clock | **B** | alpha `&.widgits/digipet/` | digipet_needs 111 |
| DSR step 1 (state, 30-day deterministic economy) | **B** | alpha `&.widgits/dsr-sim/` | dsr_sim_step1 156 |
| Ring board engine (neutral) | **B** | alpha `&.widgits/ring-board-rmmv/` | ring_board_core 321 |
| Footrace Fu race mode v0 | **B** | alpha `&.widgits/footrace-fu/` | footrace_fu_race 707 |
| tomom-hq, concept-bank-hq, desk_copy_op, dsr-test page | **L**/B | live (tomom-hq, dsr-test data); alpha (concept-bank-hq, desk_copy_op) | see earlier nights |
| Real save/load slots (`game_slot_op` v2), play-start hook, quiesced load | **D** | none | designed: SAVE-SLOTS-DESIGN, CLOCK doc 7, DSR-AS-PHYSICAL-WSR-PLAN |
| Everything else (DSR game, hotels, schools, politics, war, play economy, auction, needs beyond digipet, word banks, essence/clay, Booty Land, B.O.T.S., TSOTS, voxel planet, recipes) | **D** | design docs (index below) | none |
**Nothing is live-verified (no GUI run, no relay run on a real desk) for any alpha build.** Merging alpha/staging into live is the owner's call (AGENTS.md); alpha and `claude-staging-opencode` are pushed to origin.

## 3. Documentation index (all in `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/` unless noted; pushed on `claude`)
**Meta:** this file; `BACKLOG-AND-INFERRED-DECISIONS-2026-10-07.md` (20 inferred decisions + 20-item backlog + lanes); `GAME-BUILDING-BLOCKS-AND-TOOLS-DESIGN.md` (blocks, no plugins, noise biomes, Canvas-Craft as recipe source); `RPGMAKER-PRIMITIVES-FOR-EVERYTHING-DESIGN.md` (rule: entity-first, player-first, RPG Maker events; digipet build report); `STAGING-ENVIRONMENTS.md`; `WEEK-2026-10-07-DESIGN-INDEX.md` (older index).
**Games:** `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md` (the conductor button that runs a game; Eden experiment; a dsr-test conductor is the planned second one), `OWN-GAMES-REFERENCE-SUITE-DESIGN.md` (Footrace Fu, Booty Land, B.O.T.S., TSC, TSOTS; owner answers; Asa/Ava found); `MONOPOLY-AS-RMMV-EVENTS-DESIGN.md` (file name legacy; now the neutral ring-board core + build report); `DSR-ENTITY-GAME-DESIGN.md`, `DSR-SIMULATION-DESIGN.md` (+ build report), `DSR-AS-PHYSICAL-WSR-PLAN.md`, `DSR-POLITICS-SOCIAL-AND-NETWORK-MAPS-DESIGN.md`; `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`; `PLAY-ECONOMY-POT-FAUCETS-DESIGN.md`; `PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md`; `AUCTION-SCREEN-DESIGN.md`; `HARNESS-STORE-AND-SCORING-DESIGN.md`; `GAME-SETUP-PDL-DESIGN.md`, `PLAY-MODES-AND-MAP-ACCESS-DESIGN.md`, `SAVE-SLOTS-DESIGN.md`, `TASKS-AS-EVENT-DATA-DESIGN.md`.
**Clock, time, worlds:** `CLOCK-AS-THE-PLAY-SPINE-DESIGN.md` (with build reports); `SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md`; `REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md`; `PCHQ-LEVELS-DESKTOP-SYNC-REVIEW-PACKET.md`, `PCHQ-LEVELS-REVIEW-CORRECTIONS.md`; `pc-hq-INDEX.md`.
**Entities, learning, survival:** `ENTITY-NEEDS-AND-CARE-DESIGN.md` (+ digipet comparison), `ENTITY-SCHOOL-YEARS-DESIGN.md`, `ENTITY-WORD-BANK-DESIGN.md`, `HARNESS-BEHAVIOR-BANK-DESIGN.md`, `ENTITY-ESSENCE-GAS-AND-LIQUID-DESIGN.md` (gas, liquid, clay, decompose, CDDA-style taking apart), `MUTACLYSM-AS-EVENTS-DESIGN.md`, `SURVIVAL-GATHERING-FARMING-MINING-WATER-HANDOFF.md` (Grok lane), `TOMOM-HQ-DESIGN.md`, `GRAVEYARD-GHOSTS-DESIGN.md`, `XELECTOR-CURSWORD-POSSESSION-DESIGN.md`, `HAI-ROBOTS-PHONES-SERVER-DESIGN.md`, `OPS-BANK-DICTIONARY-DESIGN.md`, `USER-DATA-BRANCHES-DESIGN.md`.
**Nights** (`#.#.calendar-dox/1-1.HARNECIENT.SMOL/`): NIGHT_30 attrition model, 31 test that teaches, 32 turning the dials, 33 auction screen, 34 school year, 35 school is a place, 36 daily round. (Kilo's NIGHT_32_THE_MACHINE_THAT_CAN_WRITE collides with ours: renumber one.)
**Robot workforce (2026-10-07):** `ROBOT-WORKFORCE-GAMEPLAN-AND-PLAYBOOK.md` (+ `.html` in `XO/13.phymoji-engine/x/`): the one place that joins headstone, ghosts, souls, phones, server, tomom and the Joint contract into phases with exit tests; NIGHT 37 `NIGHT_37_THE_ROBOT_WORKFORCE.txt` (also reports the 2026-10-07 pull).
**Daily log:** `12.calendar/2026-10-06/2do.md` (entries 7n-fix .. 7n-fix18; **7n-fix19+ not added**: this stretch's later builds are indexed here instead).
**External review copies:** `XO/13.phymoji-engine/` (packet, corrections, backlog, design-doc copies; the XO copy of the clock/backlog docs is **older than the repo**: refresh before the next review).
**Reference material I read but did not write:** `#.ref/digipet-compairison/` (digipet vs house), the owner's game rules under `Piecemark-IT/中.SP_00.00/` (see OWN-GAMES doc).

## 4. Known gaps in documentation (to fix)
2do entries after 7n-fix18 are missing (this file stands in); the XO copies lag; the memory index lacks lanes for the build wave; NIGHT numbering collision; `WEEK-2026-10-07-DESIGN-INDEX.md` stale. I will fix these next (refresh XO copies, add memory notes) unless told otherwise.

## 5. Next steps (proposed order, claude's lane: dsr-test and the shared blocks it needs)
1. **Make one game start from Player start end to end**: `toy.pdl` + install/spawn + play hook (start clock, run New Game page) + relay check on a **beta copy**. Pick the smallest game: the **digipet** (already needs-loop + clock), then Footrace Fu.
2. `game_slot_op` v2 on the snapshot store + quiesced load.
3. DSR: entity-dir WSR ops, menu rows, load/save in play; then growth and politics.
4. Booty Land / B.O.T.S. tables (placeholders), TSC proofs; Bible-collecting TSOTS once the owner confirms Asa/Ava and the rules.

## 6. Scale and delegation (added 2026-10-07 with the robot workforce playbook)
- **Unit of scale:** quests cleared per day by the fleet, bounded by four budgets (free quota, CPU, review attention, merge safety). Not features per manager-day. Observed bound today: OpenRouter free = 50 requests/day per account (about 10 attempts/day); Groq limits not measured yet.
- **Delegability tag for every roadmap area:** 🧮 D (deterministic, anyone), 🔧 W (scoped + harness-able, free cloud worker), 🧑‍💼 M (shape-deciding, manager). No area is scheduled for delegation without a first quest and a locked harness.
- **Manager's job:** specify, lock the harness, verify, merge. Implementation moves down the tiers as per-skill grades are earned (Joint contract).
- **Order of work:** P0 fix sensors (HORN error reporting, quest packet) -> P1 first ghost runner -> P2 board + phones live -> P3 pending/judge -> P4 specialize -> P5 students (tomom) -> P6 IRL (Eden simulator) -> P7 network console. Details and exit tests: the playbook sections 10 and 15; first quests Q010-Q018 in its appendix.
- **Pull on 2026-10-07:** `opencode` (network browser forms, worker canvas/webgl, TOM layer fix) and `claude-staging-opencode` (night-32 materials) merged into `claude` after a scratch rehearsal (0 conflicts, 0 deletions, no user data); `debian`/`debian-clean` (299 user-data files each, older than the installed 2,425-file archive) and `grok` deliberately not merged; `attrition`/`claude-kilo` code already resolved to the staging side. Built, not exercised.


## 7. Knowledge domains: materials, compounds, anatomy (the bootstrapping path; added 2026-10-07, owner question)
**Owner's question:** do the grades and skills include materials, chemical-compound strings and anatomy, as things an entity can learn with tuned weights, and can the house build this out itself ("bootstrapping, as allowed")? **Honest state (corrected the same day after the owner pointed at the palettes): the raw material already exists as ASSETS; what does not exist is the labeling, scoring and hidden-layer on top of it.** My first draft of this section said no element or compound data existed; that was wrong, because I had only searched a few file types. What is on disk (checked 2026-10-07):
| asset | where (under `44.xyz.01.00/`) | what it holds (verified) |
|---|---|---|
| Elements | `#.ref/menu/palletes/elements]new=RECIPEZ+]z2.txt`, symbols in `&.widgits/palettes/ops/elements_palette_manager.c`, Drude colors in `chem-viz.txt` | 118 elements in Z order with proton / neutron counts (so the mass NUMBER is computable), canonical symbols, a periodic-table picker window (`palettes-elements.xhtpm`) |
| Compounds | `#.ref/menu/palletes/chemistry_tiles.csv` | 50 compounds: emoji, name, formula (Unicode subscripts), category number, hint. **All 50 formulas parse and use only real element symbols** (checked) |
| Compound properties | `chemistry_tiles_expanded.csv` | same 50 + color, state, melting point, boiling point, density, toxicity, reactivity, icon tile, animation frames; **only 12 of 50 rows are filled** |
| Organic / MCAT list | `mcat_compounds.csv` | 36 rows (name, formula, type, functional groups, relevance); 19 have a plain formula, the rest are names, amino acids or structural strings |
| Combination play | `little-alchemy-mockup.html`, `satisfactory-crafting.png` | element combination and crafting-recipe references |
| LOD voxel design | `chem-viz.txt` | element macro-voxel -> protons / neutrons / electrons / quarks zoom, with a CSV schema |
| Tile sets | `&.widgits/palettes/` (`pallets.pdl`): emojis, CDDA (`shared/CDDA-ASSET-SOURCE-LOCATION.pdl`, terrain / furniture / items / monsters), Mineclonia (`shared/MINECLONIA-ASSET-SOURCE-LOCATION.pdl`, `mcl_core` ...), RPG Maker, Piececraft blocks, Dwarf Fortress, Tiled, OHR | named texture folders = material and creature names with images (assets live outside the zip, license notes in the source-location files) |
| Mechanisms | concept bank, hidden layer v0, review gates, `entity_grade`, `curriculum.pdl`, `skillbook.pdl`, scoring ledger, free-worker pipeline, quartermaster | built and tested |
Owner direction (2026-10-07): *"we are literally gonna use that stuff and emojis"* and *"label it, score it, hidden layer it; we will do image (Stable Diffusion) and word generation with it too"*, with an X11-HQ window for it. Everything below is the plan; the labeling run is section 7.6.

### 7.1 The principle that makes bootstrapping safe: verifiable vs judged knowledge
| kind | examples | who can grade it | consequence |
|---|---|---|---|
| **Verifiable by code** | a formula string (`H2O`, `Ca(OH)2`): grammar, element symbols, atom counts; molar mass computed from the element table; atom conservation in a reaction; a body-part tree being acyclic with every parent existing | a deterministic op (no model, no person) | the exam is self-grading, so a model may propose freely; this is where the loop can run by itself and where auto-promotion can first be justified |
| **Judged** | properties ("salt dissolves in water"), function of a body part, how a material relates to the masters (energy / force / motion), "what is this good for" | a person reviewing against a gold set | model proposes, person approves; never auto-promoted |
The root of trust is a **hash-locked ground truth** (the element table from the house recipe file). Nothing a model says can change it; masses are never taken from a model.

### 7.2 Data model (reuse, do not invent)
- `elements.pdl`: `ELEMENT | Z | symbol | name | protons | neutrons | color`. **Derived from the house recipe file and the manager's symbol list (already on disk), not typed by a model**; locked by sha256 like a quest harness. Mass number = protons + neutrons (an integer approximation; a real atomic-mass column can be added by a person later).
- Compound / material nodes are **spokes** of the concept bank with extra rows: `FORMULA | H2O` and a stated `MASS | 18.015`. A deterministic `formula_lint` op parses the string, checks every symbol exists in `elements.pdl`, counts are positive integers, parentheses balance, and recomputes the mass (tolerance set in the file); a proposed row that fails is rejected before any person sees it.
- Anatomy: `PART | name | part_of=<name> | system=<name> | function="..."`. Deterministic checks: `part_of` resolves, the tree is acyclic, no part is its own ancestor, names unique. The facts (function, relations) are judged and reviewed.
- Masters: today only energy / force / motion exist. These domains need at most two more hubs, **`matter`** (substance and composition) and **`structure`** (part-of / connected-to). A new master is an owner decision (hubs are few by design); spokes then point at them exactly like `mass -> force` does today.
- Weights: each node's association to the masters is a hand-reviewed starting weight (the hidden layer v0 method: model proposes numbered rows, `assoc_lint` judges, a review row approves, the word table is rebuilt). Tuning later goes through `joint_tune` (bounded, ledgered, autonomy 0).

### 7.3 How it builds itself out, and what "as allowed" means
The loop is the one already built, repeated on a bigger corpus; **every arrow below is an existing op or one small new op**:
1. A skill (`study_chemistry`, `study_anatomy`, costing MP, gated by grade and level) makes an entity or a worker propose rows: *Watch -> DESCRIBE -> candidate*.
2. **Deterministic lint** (`formula_lint`, `part_tree_lint`, `assoc_lint`) rejects malformed or false-by-computation rows. Cost of a bad row is zero attention.
3. **Exams are generated by code** from the locked tables (`exam_make`: seeded questions such as "molar mass of Ca(OH)2", "which of these formulas is valid", "what is the parent of the femur in the tree"); answers are graded by code (`exam_grade`) into `FEEDBACK` rows (`layer=curriculum`, `concept=chemistry`). The same rows feed the report card, so a model or entity that answers well *earns* grade and level; one that answers badly is visibly graded down. No person is needed for the verifiable part.
4. Judged rows wait in `status=candidate` for a person's review (`REVIEW` rows, overruleable). Approved rows rebuild the hidden-layer word table.
5. Higher grades unlock `author_new_node` (propose a new spoke), so the corpus can grow from inside, still as candidates.
**Allowed means:** autonomy 0 by default; candidates only until reviewed; the ground truth is read-only to models; bounded weight moves with a ledger; free models only, within the quartermaster's limits; grades above elementary need a person's signature (`approval=person`); and **auto-promotion is justified only for verifiable domains** (chemistry formulas, anatomy structure), only at grade >= associate_bachelor, only with score >= 0.90 over >= 20 exam rows, never for judged knowledge. Anatomy here is for game and education content, not medical advice.

### 7.4 Phases (each with a locked harness; the deterministic ones are good free-worker pilots)
| phase | deliverable | verdict by | delegable to a free worker |
|---|---|---|---|
| P0 | owner decisions: add masters `matter` and `structure`? element source; which grades unlock which skills | owner | no |
| P1 | `elements.pdl` (hand-sourced, locked) + `formula_lint` (grammar, symbols, mass) | harness with valid/invalid formulas | yes (pure parsing; the op, not the data) |
| P2 | `exam_make` / `exam_grade`: seeded, deterministic, FEEDBACK rows | harness (same seed = same exam byte for byte) | yes |
| P3 | first 30 compounds and materials proposed by a free model, linted, reviewed, weights set, hidden layer rebuilt | lint + review | proposals yes, review no |
| P4 | anatomy v0: ~40 parts, `part_tree_lint`, review of functions | lint + review | lint yes, facts no |
| P5 | skills `study_chemistry` / `study_anatomy` in the skillbook; exams wired to the report card; entities take them | `entity_grade` harness + a scratch entity | yes |
| P6 | link Eden items to materials (clay, grain get a `FORMULA`/material row) so crafting rules can read composition | Eden harness stays green | owner decision first |
| P7 | bootstrapping: `author_new_node` at higher grades, the lessons bank and answer bank so the same question is never paid for twice | quartermaster + ledger | partly |

### 7.5 What this plan does not claim
No domain data exists yet; the exams, the lints and the two new masters are unbuilt; a model's chemistry or anatomy claims are never trusted without the lint or a review; the grade numbers in `curriculum.pdl` are first guesses; and the whole thing stays small on purpose (a few hundred rows) until the loop has produced graded evidence for it.

### 7.6 Label, score, hidden-layer: the first run, and the HQ window
- **Label:** every compound row gets a deterministic label block computed by code from the locked table (`formula_ok`, `atoms`, `mass_number`, `elements`), plus its emoji and (where present) state / melting / boiling point / density; a free model proposes `energy / force / motion` association weights per compound (the hidden-layer v0 method), judged by `assoc_lint`, reviewed, then trained into the word table.
- **Score:** the deterministic checks are exams (one FEEDBACK row per check, `layer=curriculum`, `concept=chemistry`), and the model's proposal is scored in the delegation bank like every other attempt.
- **Image and word generation (planned, not built, not verified on this machine):** a labeled node yields an image prompt from its emoji, name, state, color and tile name, and a word prompt from the hidden-layer word table; Stable Diffusion needs a local model and GPU or a free API, neither of which has been checked here, so it is a later phase. Generated images enter as candidates like any other row.
- **The X11-HQ window (`knowledge-hq`):** a khtpm HQ app (layout `.chtpm` + css + one compiled manager, nav-numbered, relay-drivable) that browses the labeled nodes (elements, compounds, materials, later anatomy), shows each card (emoji, formula, label block, scores, hidden-layer vector), and lets a person approve / adjust / reject proposed rows by writing `REVIEW` rows. It reads the same files the ops write; it holds no logic of its own.
