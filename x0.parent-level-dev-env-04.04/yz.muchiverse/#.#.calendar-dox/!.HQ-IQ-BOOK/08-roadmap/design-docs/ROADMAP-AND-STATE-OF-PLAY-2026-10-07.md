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

