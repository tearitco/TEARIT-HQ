# Backlog and inferred decisions (2026-10-07)

Written by claude. The owner said: "use common sense and taste to infer those answers", so every answer below is **my inference, not the owner's word**. Each is marked `INFERRED` and stays revisable; a later owner statement overrides it. Reasons are one line each.

## A. Inferred decisions
| # | question (doc) | inferred answer | why |
|---|---|---|---|
| 1 | Snapshot storage (clock 7.6) | shared **content-addressed store** (identical files stored once) | many checkpoints of mostly-unchanged trees; cheap on a weak machine |
| 2 | Auto checkpoints (clock 7.4) | play-test: ring of **20**; normal play: **16 manual slots** + optional daily auto (keep 7) | matches the 16 existing slots, bounded disk |
| 3 | Catch-up cap (clock 3.3) | cap **400** occurrences, collapse overflow into one flagged "caught up N" row | never skip silently, never stall |
| 4 | Events move time backwards | **no**; rewind only by restoring a checkpoint | keeps ledgers honest |
| 5 | Unknown pc-hq map id | **refuse and log an error** instead of silently loading flat | silent fallback hides mistakes; change goes in alpha |
| 6 | Level index | derived from each map's `game.pdl`; `maps.pdl` is checked, not read by the engine | safest, already done in alpha |
| 7 | `test_walls` loaded in live pc-hq | leave the live world alone; it is runtime state | not mine to change |
| 8 | Clock window vs menu 154 | calendar **window opens from cell 15**; keep the menu until the window works | no regression |
| 9 | First needs entity / ticks | fresh minimal "digipet" entity; **manual End Turn and auto timer both** | smallest testable; both modes share rows |
| 10 | Sandbox first system / realism / scale / travel | Earth, Moon, Mars; **gravity only first**; toy scale with true ratios in an info row; **instant travel first**, cost later | smallest visible loop |
| 11 | Biomes / noise / voxel gravity | ~8 earthlike biomes; **value noise first**; flat-per-chunk gravity first; real-time 20 ticks/s, **1 real second = 1 game minute** default | cheap, tunable |
| 12 | DSR setup | 4 citizens per civ, sprites; 1 hotel per civ; hotel rent goes to the hotel business; shared market + FX; seed fixed per scenario; war off; attendance hybrid | owner answers + cost |
| 13 | Ratings | pairwise Elo averaged for FFA; one rating per game kind; agents in the same pool but flagged; casual sessions write a result row, no rating change | standard and simple |
| 14 | Play economy | faucet 1000 millicones/hour, daily cap 5000; pot off by default; rake 5%; FFA split 50/30/20; fake ads; play-chain coins never convert to cones | conservative, inflation-safe |
| 15 | Votes | called by the ruler or by 10% of shareholders/citizens; war needs a vote in elected governments; first view = adjacency heatmap | cheap and exact |
| 16 | Cones block-0 reward bug | **do not change the real chain**; document; fix only on test/user chains | changing derivation would change existing balances |
| 17 | Inbox watcher escrow validation | must be done **before** any peer play; local-only until then | forgery risk |
| 18 | Mars gravity 3.73 vs 3.71 | keep mean-radius value; show equatorial in the info row | both are real, label them |
| 19 | CC-BY-SA | reference-in-place internally; a `CREDITS` file before any distributed build | reviewer's point, correct |
| 20 | Merging alpha/staging into live, pushing alpha | **not done by me**; owner's call (AGENTS.md) | house rule |

## B. The backlog (grouped; status: B=built in alpha, D=design only, P=in progress, ?=unverified)
**1. Foundation (do first; everything depends on it)**
1. Clock: harness over existing `lc_clock`, recurrence (`repeat=` units), catch-up, ledgered/watchdogged event runs. D -> **starting**
2. Snapshot store + restore + clock state in checkpoints (needed for rewind and play-test). D -> **starting**
3. Clock commands (rate/pause/advance), chaining, master clock, Time setup + lock rules, calendar window. D
4. Needs step 1: digipet entity hunger loop on the clock, play/save/load, feed. D
5. `place_clock_tick`/cursor op and `schedule.pdl` per place. D
**2. Worlds and planets**
6. pc-hq: unknown-map refusal, desktop load path (`CONFIRM_START_MAP` from desktop), `default` level planet, mark kinds. B(partial: index+harness)
7. Physics nodes: more nodes (temperature, pressure, boiling, weather, travel cost), wire into entity menus. B(step 2: gravity/orbit/escape)
8. Solar sandbox: GUI check, orbit motion, link to pc-hq `default`, Moon/Mars pages with terrain. B(step 1)
9. Real-time clock source + spherical-gravity movement; chunked voxel planet; noise biomes; raymarch performance measurement. D (Grok lane)
**3. Content and crafting**
10. Canvas-Craft as the single `RECIPE` source (read manager first); mutaclysm commands as events; gathering/farming/water. D
**4. Economy and social**
11. DSR sim (clock, buildings, citizens as sprites, routines, economy via WSR ops, schools, growth, war last). D
12. Voting/ties/network maps; synchronized elections. D
13. Play economy: play-money ledger, faucet/pot/quests-with-rewards, fake ads, play chain. D (chain/escrow/faucet B)
14. Sessions/seats/Elo/lobbies; finish TSC_ELO winner + Elo write-back proof. D
15. Harness store and scoring. D
**5. Attrition/learning track**
16. TEARIT review window; halo auto-promote fix (owner decision pending); word banks for entities (retroactive, hand-scored); school years and report cards. D
**6. Housekeeping**
17. Live verification of taskbar rows (tomom, Concept Bank) and dsr-test in the desk switcher; book-stack verse in the live window. ?
18. Remove stale `build_db_hq.sh`; run `build.sh` itself for the chain ops; inbox watcher escrow validation. D
19. Merge/push alpha and staging (owner), `CREDITS` file, THIRD-PARTY assets note. D
20. Update memory notes for the new lanes.

## C. Order I am following
1 -> 2 -> 3 -> 5 -> 4 (clock + snapshots first because rewind, play-test, needs, schedules, DSR and the sandbox all sit on them), then 6, 7, 8, then content and economy. Each item: design check, alpha build, pal harness shown able to fail, report, commit on `claude-alpha`.

## D. Lanes (owner, 2026-10-07)
- **Grok:** the **CDDA and Mineclonia games** (survival/voxel content: gathering, farming, mining, water, the CDDA-style survival loop, the Mineclonia-asset planet). The existing Grok handoff docs cover it. Owner also said earlier "forget the grok commits, no one wants or trusts them": so claude **does not merge or cherry-pick from `origin/grok`**; Grok's work reaches claude only when the owner says so.
- **claude:** **`dsr-test`** (the DSR page: clock, entities, stores/banks/castles/populations, routines, economy, schools, growth, politics) plus the shared blocks DSR needs (clock, snapshots, needs loop, chain/escrow). Items 6-9 and 10 of the backlog (pc-hq worlds beyond what DSR needs, voxel planet, CDDA/Mineclonia content, Canvas-Craft recipes) are **not claude's focus now**.

## E. Added 2026-10-07 (later)
21. **Taskbar clock display from a daemon-written .txt, source chosen by pdl or game input** (`CLOCK-AS-THE-PLAY-SPINE-DESIGN.md` section 11): lc_clock `display_*.txt` writer + `datetime_source` key + taskbar read (shared file, owner review).
22. **Game conductor entities** (`GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md`): Eden v0 in progress in alpha; **dsr-test conductor** next; compare ways of running games (conductor vs Player-menu play hook vs game.pdl).
23. **Player-start path for one game end to end** (toy.pdl, spawn/install, play hook, beta relay check) per `ROADMAP-AND-STATE-OF-PLAY-2026-10-07.md`.
