# 🤝📦 Final nov-7 handoff: one vocabulary, pets that act, and what is next (2026-11-07)

💌 *For the owner and for any AI agent picking this up. Written by Claude Sonnet 5.5 on 2026-11-07. Plain words first; evidence labels: **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it · **DOC** my reasoning, not run · **EXT** outside page, searched not read in full.*

---

## 1. 🎯 Goal in one paragraph (SAID)
Everything that plays a game (your click, an event page, a harness, a pet's own step, an AI) uses **one vocabulary** (the same verb names, the same row grammar) and goes through **one door** (append a row through a verb the game knows, via its page). A harness must **act** through that door, not only judge; a pet must **act** through it too, so that **editing a page changes what the pet does**. First pet actions you named: **mine / place, walk, eat, farm, survival stuff**. Later: **2D and 3D Monopoly-style games that 4 pets play**, so the whole chain can be tested end to end.

## 2. ✅ What I did today (all committed on branch `claude`, pushed)
| Commit | What |
|---|---|
| `9d81ed3f3`, `8fb444d13` | D48 proposal "one brain per game" and its revision |
| `e9dc4418e` | nuance check + slide brief |
| `14394223c` | pets' three control paths; df-hq job queue as the pattern |
| `722a3a24f` | D49 one door + master ledger (WSR_PAL-PREFERED) |
| `c62335b17` | correction: harness↔events↔verbs act (proven), pets and arrow keys sit outside |
| this handoff | written last; see the git log for its commit |
**Built: nothing in code except one read-only experiment (S0 below).** Everything else is design, labelled as such.

## 3. 🔬 S0 result (OBS, the first step of the plan)
Question: why is mcl-hq's behaviour bank empty?
* `@.apps/mcl-hq/state/decisions.txt` has **509 DEC rows** (last written 29 Oct). The live bank files `state/bank/dec_mcl-hq_{walk,door}.behavior.pdl` are **0 bytes, dated 21 Oct**.
* **Hand count from the ledger:** walk 455 ok + 36 blocked = 491; door 18 ok of 18. (Same numbers I corrected in D48.)
* I ran the shared op **on a scratch copy only** (`dec_op bank <copy> <scratch bank> mcl-hq`, niced). Result: `rows=509`, **walk reward=455 punish=36 WEIGHT 0.9249; door reward=18 punish=0 WEIGHT 0.9500; pick_pct 20.** These equal the hand count (455/491 = 0.9266 raw, Laplace (455+1)/(491+2) = 0.9249; (18+1)/(18+2) = 0.9500).
* **Conclusion:** the digest op is correct; **it simply never ran on this data in the live folder.** Live files were not touched.
* **Likely cause (DOC, unproven):** `mcl_hq.c` line 327 digests only when `static int since` reaches 200, and verbs run as fresh processes (rpg-space board skill), so the counter restarts at 0 each time and never gets there. I did not check which process wrote the 509 rows or whether a resident daemon calls the logger.
* **To prove (next, S0b):** find the writer process; show the counter resets; fix by keying the digest to the ledger byte cursor (it already has one in `dec_cursor.txt`) instead of an in-memory count; negative-test by feeding 199 rows (no digest) and 201 (digest) in a scratch house.
* Scratch evidence: the session scratchpad folder `s0/` (will not survive; the numbers above are the record).

## 4. 🧰 Pet actions you asked for: what exists today (OBS, manifest names only)
I counted verb **names** in each game's `verbs.pdl`; I did not read the verb bodies.
| Action | mcl-hq | df-hq | cdda-hq | rpg-pet, gta-hq, dsr-rpg, civ-rpgce | Note |
|---|---|---|---|---|---|
| **walk** | yes | yes | yes | yes | all five; pet's own step walks in C (not through the verb) |
| **place** | yes | yes | yes | yes | |
| **mine** | **yes** | no (`dig` instead) | no | no | df-hq `dig` is also the job-queue verb |
| **craft** | yes | no | yes | no | |
| **eat / drink / sleep / feed** | **no verb** | no | no | no | hunger exists as a **life tick** in mcl-hq (`mcl_hq.c` lines 495 to 498: "hunger, starving, death, respawn"); eating appears only in the chat reply table (`petslike_core.c` line 706). I did **not** read the life tick to see if it feeds anyone |
| **farm / plant / harvest** | **no verb** | no | no | no | not found in any manifest |
**So:** `mine`, `place`, `walk`, `craft` have verbs today; **`eat` and `farm` do not exist as verbs** and must be **added to the shared vocabulary first** (S1b below), with `verbs.pdl` rows, a game implementation, a harness phase that can fail, and a `BRAIN` row each.

## 5. 🧭 The plan, in order (each step ends with a proof that can fail)
| Step | What | Size | Proof |
|---|---|---|---|
| **S0** | digest op checked on scratch copy | **done** (section 3) | weights equal hand count |
| **S0b** | find why the live bank never digests; fix with a byte cursor | small | 199 rows: no digest; 201: digest |
| **S1** | `BRAIN | dec_key | verb | phases | ok_outcomes | bad_outcomes | note` rows in each `verbs.pdl` + a lint | small | made-up phase, action or verb each fail the lint |
| **S1b** | add **`eat`, `farm` (plant, harvest)** and any survival verb to the vocabulary: manifest row, implementation, harness phase, BRAIN row | medium, per verb | the harness phase fails when the verb is removed |
| **S2** | one-page mapping of the four ledger dialects (S1 event, master ledger, df-hq jobs, DEC) | small, docs + one parse case | one case parses both a jobs row and its master-ledger equivalent |
| **S3** | tier slot (`decision_mode`) + **pet fires its chosen verb through the verb page** | LARGE, design page first | `BP edit` on the page changes what the pet does (V7 proof, applied to a pet) |
| **S4** | `JOB` rows for `mine place craft eat farm`, claimed by tier | LARGE, design page first | a deliberately wrong job is refused; never touches live ledger |
| **S5** | manager op resolves intent rows (D40 manager writes jobs) | medium | a removed bridge job leaves the side island unreachable |
| **S6** | `.behaviors` catalog + planner as tier 2 | LARGE | the plan equals the hand-written phase table |
| **S7** | **Monopoly-style board game, 2D then 3D, 4 pets as players** (section 6) | LARGE | four pet drivers finish a game in a scratch house; the harness replays it |
Rules for every step: scratch house under `/tmp`-style scratch, never the live house; compile and engine runs through `cpu_gate` one at a time, niced; commit by explicit pathspec; no Python; every new runtime file gets a cap and a row in `RUNTIME-FILE-GROWTH-STANDARD.md`.

## 6. 🎲 Timeline item you added: Monopoly-style games that 4 pets play (SAID; design is DOC)
**Why it is a good test (DOC):** it forces every part of the chain at once: shared vocabulary (roll, move, buy, pay rent, build, trade), one door (each player's turn is rows), a decision slot (4 pets, tiers 0 to 3, optionally 1 human), a master ledger (bank, deeds, rent), a harness that judges and replays a whole game, and 2D/3D parity (one scene description, two projections; the standing rule).
**What exists (OBS/READ):** no game. `@.apps/deskopoly=FFU/deskopoly=ffu.txt` is a 31-line notes file (an earlier idea: "can hai play monopoly … its monopoly fsm of its ai bank is biased / linked 2 playing monopoly desk", and WSR with business names and R&D). The FFU rules document has a "Time-Fu: Buffet (Monopoly-Style Variant)" appendix (`+.TSOTS-ALPHA-OMEGA/#.RULES/ff-fu-IQ-a29-RULES…txt`, Appendix A, line ~127; I read only the heading). The DSR economy (banks, loans, stocks) and `WSR_PAL-PREFERED` (cent-exact dividend math, verified 2026-09-29) are the likely money engine (READ in `AGENT_ROADMAP_ANSWERS.md` §3). I did not read `bet-rpg` or `tcg-rpgce` design docs; they may overlap.
**Proposed shape (DOC):**
* A new petslike skin or a board-game game folder with a `verbs.pdl`: `roll`, `move`, `buy`, `pay`, `build`, `trade`, `end_turn`, plus `BRAIN` rows. Board = rooms or a ring of tiles; money = the master ledger (intent rows) and S1 events (what happened).
* **4 players = 4 pets** with the decision slot; optionally one human takes over any slot (tier 4).
* **Order: 2D first** (cheap, testable headless), **then 3D from the same scene description** (the 3D parity rule), never a second render path.
* **Test:** a harness phase table plays a full game headless in a scratch house, with a fixed dice seed: same seed, same ledger (replay test). A second case plays with weights on and shows decisions differ only when weights differ.
* **Cost:** a headless game is a few hundred rows; the 3D run is the heavy part (one engine at a time).
**Open questions for you:** which rules (classic property, or the FFU Buffet variant)? Does money live on the WSR/DSR engine or a small private ledger first? Are the 4 pets the existing six DB pets? Not decided; recommend: classic rules, a private ledger first, existing pets.

## 7. ⏳ Open items
* **Your unfinished message:** "i also wanted to make a" (cut off). Not acted on. Please resend the rest.
* **Decisions already made:** two numbers `can` and `does` never merged; `can_min` = 0.5.
* **Decisions pending (D48, D49):** pet scratch proposal frequency (and how long a game hour is in real minutes); D47 boundary (may a pet's own actions be chosen by weight automatically; I recommend yes for a pet's own actions only, never pages or the player); the master-ledger row grammar (I recommend the `timestamp|turn|actor|action_type|details` shape for intent plus S1 `EVENT` rows for results); where a game's master ledger lives (recommend per game); tiers as data; human takeover of a pet's slot.
* **Not verified:** which process wrote the 509 DEC rows; why df-hq's decision log has no `dig` rows although its jobs file shows finished digs; the code that uses `rpg-edit-hq/doors.fsm.pdl`; the V7 results were not re-run today (they are from 2026-10-20, per the design doc).
* **Queued from earlier, not started:** image-bank measurement trial on debil; M2 expert mix and wiring the gate to recipes; typed-words box in the studio; remaining sheets (Inside, Dungeon, World, sci-fi B and C, A1 water); Chinese labels; select by typed coordinates and mouse drag; 2D/3D toggle in the studio; optional replacement of the `system(gcc)` wart in `harness_case_op.c`.

## 8. 🔗 Where everything is
**Repo** (BOOK = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK`; house = `…/yz.muchiverse/44.xyz.01.00`)
* `BOOK/08-roadmap/design-docs/PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md` (D48) · `…NUANCE-CHECK-AND-SLIDE-BRIEF…` · `PROPOSAL-ONE-DOOR-DRIVING-AND-THE-MASTER-LEDGER-2026-11-07.md` (D49)
* `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D45 to D49) · `BOOK/16.game/EVENT-VERB-PAGES-DESIGN.md` (sections 8 to 10) · `BOOK/17.ai/HARNESS-AS-GAME-BRAIN.md` · `BOOK/16.game/GAME-MECHANICS-SPRINT.md`
* `WSR_PAL-PREFERED/` (sibling project) · `house/&.widgits/_shared-lib/petslike_core.c` (`ai_step`) · `house/@.apps/mcl-hq/{ops/mcl_hq.c,verbs.pdl,state/decisions.txt}` · `house/&.widgits/_shared-lib/ops/dec_op.c` · `house/@.apps/df-hq/state/jobs.txt` · `house/@.apps/deskopoly=FFU/`
* XO copies: `XO/31.Nov/nov-7/` (`0.`, `1.`, `2.`, `3.` documents, md and html)
**Outside (EXT; searched, not read in full):** Dwarf Fortress jobs https://dwarffortresswiki.org/index.php/DF2014:Job · F.E.A.R. GOAP https://gdcvault.com/play/1013282/Three-States-and-a-Plan · lockstep and command replays https://www.gamingonlinux.com/2017/12/rewrite-the-past-in-airships-conquer-the-skies-with-the-new-replay-feature

## 9. 🚦 First thing the next session does
1. Read this file, then D49 section 3e and 6.
2. Do **S0b** (small, provable), then **S1**; ask before anything marked LARGE.
3. Do not touch live `state/` files; verify by scratch copy; report only numbers you observed.


## 10. 🚀 Pass 1 (started after the handoff above; written BEFORE the work, so you know what we started and why)
**Your words (SAID, 2026-11-07):** "get as much foundational stuff done in pass 1, even for the monopoly game; I want to open it in rpg-edit so I can view its 'known mechanics' through our rpg-edit editor/player and see if we can use its hard state to improve our UX/UI functionality."
**How I read it (DOC, correct me):**
* **Known mechanics** = what the game *is*, as data you can list: its verbs (`verbs.pdl`), the `BRAIN` rows that name which verbs a pet may choose, the rule tables (board tiles, prices, rents, starting money), and the names of the state files each verb reads and writes.
* **Hard state** = the files that *fully determine* the game at any moment (the append-only event ledger plus a few small state files), so that nothing hides in memory. If the whole game is hard state, **any window can show it** and a replay of the ledger reproduces it.
* **UX goal:** rpg-edit (the editor/player) should be able to *show* a game's mechanics and state from those files alone, and we learn which panels, lists and menus we can generate from data instead of hand-building. Monopoly is a good probe: small rule set, many players, money, turns.
**What rpg-edit-hq is today (READ, not run):** a petslike skin with its own rooms, party and 36 verbs (`verbs.pdl`, `rpg_edit.c`); the pivot doc `16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md` says it is **not yet an editor of other games**; its item E, "rpg-edit opens any book; export as a game", is **not done**. Games are meant to be **books** (`game.pdl` + one folder per page), checked by `book_check`.
**Pass 1 scope (what I will try, each with a proof that can fail; anything not reached will be written as NOT DONE in section 11 below):**
| # | Piece | Proof |
|---|---|---|
| 1 | **`deskopoly` game as data**: `game.pdl` (a book), `verbs.pdl` with `BRAIN` rows, `board.pdl` (tiles, prices, rents), `rules.pdl` (money, seed) | `book_check` on the folder; lint refuses a made-up verb in a `BRAIN` row |
| 2 | **A small C rules op** (`roll move buy pay build trade end_turn`) over an append-only ledger, seeded dice, 2 to 4 players | same seed gives the same ledger; replay of the ledger reproduces state byte for byte |
| 3 | **4 pets play a whole game headless in a scratch house** through a pal harness | game ends; money is conserved (sum of cash plus bank equals the start total); a deliberately illegal move is refused |
| 4 | **A `mechanics` projection**: one verb that writes the known mechanics and hard-state map as a plain file (verbs, reads, writes, state keys, current values) | file equals the manifests; a mutated manifest changes it |
| 5 | **S0b/S1 foundations** for the shared vocabulary: `BRAIN` row grammar and a lint, applied first to `deskopoly` (and mcl-hq only if the verb generator ignores unknown rows) | the lint fails on a made-up phase, action or verb |
**Explicitly NOT in pass 1 (so nobody assumes):** opening the game inside the rpg-edit window (needs the pivot's item E, a renderer and window task of its own), 3D, the live house, any change to the shared pet core (`ai_step`), and any change to a live `state/` file. If the projection in piece 4 is enough for rpg-edit to read, section 11 says so; otherwise it says what is missing.
**House rules I will follow:** scratch house only, compiles through `cpu_gate` niced one at a time, no Python, harness = pal + case + C, commits by explicit pathspec on branch `claude`, every number reported is one I observed.


## 11. ✅ Pass 1 results (written after the work; commit `ba16a8ce6`)
| # | Piece | Status | Evidence (OBS, scratch only) |
|---|---|---|---|
| 1 | `deskopoly` as data: `board.pdl` (16 tiles), `rules.pdl`, `verbs.pdl` with VERB, FLOW, BRAIN rows | **built** | harness case `deskopoly`: `passed=44 failed=0`, run under three house prisc VMs |
| 2 | rules op `deskopoly.c`: roll, buy, skip, end_turn, autoplay, status, replay, check, mechanics | **built** | seed 7: 334 events, 40 rounds, winner fenn, money sum 6000 = 1500 x 4, replay hash `128baad3c4609438`; the same seed gives an identical ledger (`cmp`) and seed 8 does not |
| 3 | 4 pets play a whole game headless | **built** (four styles stand in for pets, not the real pet core) | autoplay ends; a hand-edited rent is refused on replay (line 22); refusals append nothing; harsh rules (150 cash) seed 2: 1 bankruptcy, 2 blocked buys, sum still 600 |
| 4 | `mechanics` projection | **built** | 77 plain rows: verbs, flows, BRAIN rows, players, tiles and replayed state; a verb with no FLOW row is named `GAP` (negative test passes) |
| 5 | `brain_lint` for D48 BRAIN rows | **built**, applied to `deskopoly` only | clean on the real manifest (`brain_rows=4 fail=0`); fails a made-up verb, a duplicate key, an empty ok list, an overlapping outcome, a phase that is no CASE |
**A real bug found by the negative tests:** my first `brain_lint` used nested `strtok`, so the "outcome is both ok and bad" check silently passed a bad row. The test that must fail did not fail; I fixed it with `strtok_r` and the test now passes. (A trap already listed in the rpg-space skill.)
**Pets and the decision bank:** the game writes the same `DEC|unix|stamp|actor|state|action|target|outcome` rows the bank already digests. `dec_op` gave buy `0.9286` = (12+1)/(12+0+2) and skip `0.6667` = (1+1)/(1+0+2), equal to the hand count. That is the "same vocabulary" claim shown on a second game, not only argued.
**Footprint (house rule):** no resident process, no timer; one short process per verb; files bounded by one game (about 60 KB); a row was added to `RUNTIME-FILE-GROWTH-STANDARD.md`. `cpu_lint`: 571 hits in the house, none in the new files.

### 11b. What this gives rpg-edit and the UX question (DOC)
* **Readable today:** `state/mechanics.txt` is plain pipe rows an editor panel can list: verbs (with the data flow), which verbs a pet may choose and which harness cases judge them, rules, players, every tile, and the live state. Nothing in it is hand-written; it is a projection of three data files plus the ledger.
* **UX lessons the probe already shows:** (1) a list of verbs with `reads` and `writes` is enough to draw a data-flow panel with no extra code; (2) the `GAP` rows are a free "undocumented mechanic" warning an editor can show in red; (3) if the whole state is a ledger, an **undo / step-back / replay-to-here** control costs one replay and no new storage; (4) a "who can choose this verb, and how often did it work" column comes straight from the BRAIN row plus the bank weight.
* **NOT done, and why:** the game does **not** open inside the rpg-edit window yet. rpg-edit-hq is a petslike skin, and the pivot's item E ("rpg-edit opens any book") is not built; it needs a window and renderer task of its own. `book_check` was not run: it needs a pc-hq folder (`pieces/display/open_book_page.txt`, a map book), which `deskopoly` does not have. 2D board art, 3D, event pages for the verbs, the real pet core as the players, and `BRAIN` rows for mcl-hq are also not done.
* **Cheapest next step to open it in rpg-edit (DOC):** give `deskopoly` a pc-hq book whose page rows are the 4 pets and the 16 tiles (the PAGE-FILE standard: one row per entity), so `book_check` can score it and the existing board renders it; then add one "Mechanics" tab in rpg-edit-hq that reads `state/mechanics.txt`. Size: medium.

## 12. 🏛️ Your question: did we cover rewriting the old Centroid building harnesses? (answered 2026-11-07)
**Short answer: no. No document I found says whether they are rewritten, how, or why.** (I searched `16.game` and `08-roadmap` for "rewrite / replace / retire / redo" near harness and centroid: nothing.) What the docs do say (READ, `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`): **centroid-test** loads the University as a finished file and the pets do **not** build it; "a second attempt edits the phase table. It does not edit the pets by hand"; and **later** a manager is shown the master blueprint, breaks it into jobs for employees, and passes **only if the employees' placed columns match the prebuilt volume** ("not part of centroid-test").
**What exists (OBS: names and check counts only; I did not run or read the bodies):** `centroid_test` (5 checks, runs `centroid_prebuild_op`), `centroid_book` (11; nine of its steps run a shell script, the existing `edit_last.sh`), `centroid_clear` (6), `centroid_parts` (32, the tapered island and room stamper), `campus_look` (9, the look gate), and `pet_build` (15, rooms built as EVENTS: `build_room` appends ROOM/DOOR/CELL rows and each DOOR becomes a TELEPORT event page).
**My recommendation (DOC, not decided):** do **not** rewrite them. Why:
1. They test **files and generators** (the loader, the stamp, the clear, the look gate), not who does the building. They still pass or fail for the reasons they were written, so rewriting would only lose proof.
2. The thing the docs say comes **later** is a **different run**: the manager and employees *build*. By the house rule that is a **new phase table**, not an edit of these.
3. The one place they must connect to the new vocabulary is that run: employees must build through the **same verbs and pages** (`place`, `mine`, per D49), so that "placed columns match the prebuilt volume" compares **rows of the same kind**. `pet_build` is the closest existing piece (building as events), but nothing has pets *choosing* to build yet.
4. One small cleanup, optional and later: `centroid_book` leans on a shell script; under the no-new-`.sh` rule that is a candidate to become a small C op, **not** a rewrite of the case.
**What I did not verify:** whether the prebuild output and the pets' `place` rows share a row format (that decides if the later comparison is trivial or needs a converter).

*Nothing was sent to an outside service. The lore stays local.* 💛
