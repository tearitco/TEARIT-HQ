# 🤝 Handoff and colab: Claude to Grok, for TEARITIZATION (2026-11-23)

From Claude (Sonnet 5.5, branch `claude`) to Grok (branch `grok`), at the owner's request. The owner wants a real colab, not a relay: you take work that fits what you have already touched (cities, CDDA-shaped parts, DF, Minecraft-style modes), I take work that fits what I touched this week, and each of us checks the other's evidence. Labels: **READ** (from a doc or commit I opened) · **RAN** (I ran it) · **MY VIEW** (opinion, argue with it) · **DESIGN** (nothing built).
House `H` = `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`. BOOK = `.../#.#.calendar-dox/!.HQ-IQ-BOOK`. This file is also in BOOK `13.agent-coms/GROK/` on my branch; **read the XO copy** (`XO/31.Nov/nov-23/5.*`), your worktree may not have mine.

## 1. 🎯 The goal in one paragraph (owner, 2026-11-23)
**TEARITIZATION** = a "Civ-SNES"-style clone (Sid Meier's Civilization on the SNES) built with EVENTS, playable by MULTIPLE AGENTS + a HUMAN, with WORLD GENERATION, working in BOTH 2D and 3D. Read first: BOOK `16.game/TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md` (my design, 13 sections, all DESIGN, 8 owner decisions, a staged plan S0 to S6 with a check that can fail at each stage). The owner says `@.apps/civ-rpgce` is **likely a false start**: do not build on it (its 2026-10-14 design doc had useful ideas: settlers, huts, founding cities, fog).

## 2. 🧭 What you last experienced (READ from your branch and docs) and why it matters here
- **Cities from open data:** Centon (`@.apps/centon`, 168x254, 10 m), the bay slice (163x216, 20 m), the OSM/Overpass puller and painter (`rta_city_op`, KIND rows, `board_city.pdl`), the Battery grid, Jerusalem/Giza boxes cached in `#.NNEST_ASSETS/rta-sources/INVENTORY.pdl`. **For TEARITIZATION this is your strongest asset: you already know how a map becomes ledger-friendly cells.**
- **Cars as CDDA-shaped parts, driven by a living entity with an FSM:** engine/motor, fuel store, tires, steering; "drive works only when the parts allow it" (your 2026-11-22 notes). I built the living-entity layer on it this week (RTA: enter/exit/eat/sleep/drive/part/walk as ledger rows on Blueprintz event pages; D103, D108, D113, D116, D120). **For Civ: units, buildings and tools are parts-with-state and a unit's job is an FSM step. Same shape.**
- **DF and Minecraft modes:** your 2026-11-10 handoff (`HANDOFF-2026-11-10-DF-MINECRAFT-EVENTS-META-HARNESSES-AND-CENTROID.md`), the skyland/centroid lane (`ONBOARD-2026-11-10-SKYLAND-AND-CREW.md`), `bp_op gen` / `crew_op raise`, `mech_new_op`. CDDA: `cdda-hq`, `df-hq`, `mcl-hq`, `rpg-dust` modes (skins of `petslike_core.c`, rules in C hooks today).
- **Your own weaknesses list** (`WEAKNESSES-2026-11-11.md`, written by your lane; it is good, keep following it): a passing harness is not the thing on screen; open the PNG; do not move the camera to hide a miss; say plainly what was written by hand versus built by the op; measure our own processes before blaming the browser; commit by explicit path from your worktree.

## 3. 💪 Strengths and weaknesses (MY VIEW, argue with it)
| | Grok | Claude (me) |
|---|---|---|
| Strengths | Open-data to map pipelines; wide reading of the old game skins (CDDA, DF, MC); long-form handoffs with BUILT/PROPOSED/NOT READ labels (I checked: followed); fast scaffolding of new apps from data templates; knows the cities painter and its traps (bridges, junctions, sidewalks) | Harness-first work with sabotages that must fail; verifying other agents' claims by re-running; the event/op shape (D92) and Blueprintz page seeding (`bp_edit_op`); the In: ON + possession + keys proof path and the 2D/3D desk projection recipe (vox, rta-board); commit hygiene in a shared checkout (`commit_scoped`, private index) |
| Weaknesses I observed | Counts that prove a scaffold, not a game (three cases were one 102-line template, all 28); a rule number hard-coded in C (stars cap 5); docs that disagree about "the next step"; history of burning tokens re-deriving things (your own WEAKNESSES file) | I missed a one-step cursor lag in a frame I described, until the owner pointed at it; I made a report and a night class and pushed them before the owner was ready; I lean on subagent reports (some stopped early twice; one said "done" and had built nothing); my private-index commits left working files stale; heavy use of context; quota ends my sessions |
| What each should NOT do alone | Declare something playable without a photograph and a human-path take | Invent art, lore or open-data pipelines; trust a count I did not run |

## 4. 🗂️ Lanes (so we do not step on each other)
- **You:** branch `grok`, worktree `/home/no/Desktop/github/work/NNEST-12.00/.kilo/worktrees/grok`, commit with `H/&.widgits/_shared-lib/ops/+x/commit_scoped.+x <repo> --branch grok --message-file F --trailer '...' -- <explicit paths>`. Never commit on `claude`. Your files: map/city/world-gen ops and data (`@.apps/rta*` painter pieces you own, a new `@.apps/tearit-*` map app), CDDA/DF/MC "what to steal" docs.
- **Me:** branch `claude`. My files: turn loop and units/cities as ledger folds, seats and agents, the board/projection and proof recordings, `vox_op` and the verify checkpoint.
- **Shared, append-only by section:** the decision log, `harness/README.md`, `event_commands.registry.pdl`. Add your own row; never rewrite another agent's. Take the next free D number (last: D121). `game_core.c` and `bv_*` are **not yours and not mine to touch casually**: ask first.
- **Merges** are the owner's call (or mine when asked). Until then neither of us copies the other's commits. The owner pushes/approves; **do not push `user/*`; no one pushes without the owner saying so** (I pushed docs early today and was corrected).
- **Talk:** you write `13.agent-coms/GROK/QUESTIONS-FOR-CLAUDE.md`, I answer in `ANSWERS-FROM-CLAUDE.md` (the existing protocol). Put anything the owner must see in XO, named `<pre>.<name>`.

## 5. 📋 Tasks for you (each ends with evidence the owner can open)
**G0 (read, one hour, no building):** the guidance doc above, `D92`, `STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, the presentations standard sections 10 and 11, the vox/rta-board shape in `16.game/MINECRAFT-EVENTS-HANDOFF-...` sections 16-20 and `16.game/presentations/RTA-BOARD-2D-3D-VIDEO-2026-11-22.md`.
**G1 WORLD GENERATION (S0 of the plan, harness first, DESIGN to build):** a seeded op `tearit_map_op gen <seed> <w> <h>` that writes a small map (start 16x16, then 32x32) as DATA rows: terrain classes (water, grass, forest, hill, mountain, desert), rivers/coast, resource tiles, huts, start positions, using KIND-style rows and priority rules like your painter (and bridge-over-water as data, which I fixed in `rta_city_op`: D108). Then a **desk projection op** writing `cells.txt`, `cells.marker` (size grows on each write), `map.png`, `events.txt` exactly like `vox_op desk` / `rta_board_op` so the same pc-hq board shows it in **2D (key 0) and 3D**. Checks that can FAIL (sabotage, restore, cmp): same seed gives the same bytes; a different seed differs; every start position is walkable and connected to at least N land tiles (a flood-fill check); water never overwrites a bridge; deleted projection regenerates identical bytes; the 2D picture and the 3D heights are one description (parity). Numbers (sea level, forest chance) are RULE rows, never C.
**G2 TEST-PLAY THE WORLD (S0 evidence):** open the board through `cpu_gate`, **In: ON, possess a settler/token, keys only** (recipe: BOOK presentations standard section 11; `relay_drive click 60 48` on a NEW launch; Esc releases possession first, then In), walk it, photograph 2D and 3D before/after, **read the HUD `pos` in every frame you cite** (I missed a lag by not doing that). Report what a human would hate (unreadable 3D, tiles too small, stuck keys).
**G3 "WHAT TO STEAL" (docs only, one page each, with READ/RAN labels):** from your CDDA experience: parts, fuel as an item count that goes down (not minted), items/inventory as a ledger fold; from DF: work orders and jobs as FSM steps, "eat and sleep are the same verbs as a job step" (your own 2026-11-22 answer), multi-entity management UI; from Minecraft/mcl-hq/rpg-dust: dig/place verbs on a grid, chunking, what broke. For each: which TEARITIZATION piece it maps to (units doing jobs, city production, terrain editing, fog), what we should NOT copy (code, text, art: no CDDA/DF/MC code or content).
**G4 (stretch, only after G1 and G2 are green):** the settler `found-city` rule as an op + an event page seeded with `bp_edit_op` (S1), on the map from G1. I will take S2 (seats and turns) in parallel.

## 6. 📋 Tasks for me (so you know what to lean on)
- The **turn loop and ledger folds** (TURN rows, units, cities, gold) and the **two-seat loop** (human by keys + one hai-style agent, a driver change is a row): S1-S2.
- The **vox verify checkpoint** (O(n^2): 0.42 s at 1,007 rows, 9.9 s at 5,035; observed once). A long world ledger needs it first.
- **Proof videos** in the owner's standard, **Blueprintz scenes** per event page, **verifying your counts** by re-running them (I will cite which I ran).
- Fix what your map hits in the shared board code (`bv_*`) when you report it with a harness scene.

## 7. ✅ How we check each other (the colab rule)
1. Every claim needs a **harness count I can re-run** AND, for anything visible, a **photograph/video the owner can open**. "Playable" is not said before a human-path take exists.
2. When you hand me a result, send: the command, the expected count, the commit id. I re-run it and reply with my observed count. I do the same for you in reverse: send me mine, you re-run and disagree out loud.
3. A passing case that cannot fail is a finding. Show the sabotage.
4. Honest labels: BUILT, PROPOSED, NOT READ, NOT PROVEN. Say what was done by hand (a hand-drawn grid versus an op's output).
5. If you are stuck for more than one long turn, write what you tried and stop; do not loop.

## 8. ⚠️ Traps (all learned this week, RAN)
- `In:` never turned ON on copied boards: the shared projector bound a stale session (D117); on a NEW launch a click gives `In: ON` (a click possesses `hero_01` first: press Esc once to release before moving the cursor to your token).
- The possession cursor trails a moved token unless the engine takes one `events.txt` snapshot per frame (D119; rebuild the board binary and relaunch to get it).
- ONE board window at a time (two share one 3D daemon). Back up and restore live state (tar + sha256), including `#.desktop/pchq_board_view.txt`.
- A private-index commit leaves your working file stale: merge your lines into it too.
- `game_core.c`/`bv_*` may carry other agents' uncommitted `axis=side` hunks: never commit them; check `git show HEAD:<full path> | grep -c axis=side` is 0.
- No Python. No new `.sh`. Heavy work through `cpu_gate`, niced, one job at a time (Chrome loads the machine).

## 9. 🙋 What the owner has to decide first (from the guidance doc section 9)
Map scale; simultaneous vs turn-based; how many AI seats; which free model tier (free models only); unit art (placeholders now; our own art comes later from the diffusion pipeline through hai); where it lives (a new app, not `civ-rpgce`); and the verify-checkpoint priority. Ask the owner, not me.

## 10. 🔗 External references
Repo `github.com/tearitco/TEARIT-HQ`. Branches `claude` and `grok`. BOOK: `16.game/TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D87, D92, D101-D121), `16.game/CIV-RPGCE-DESIGN.md` (ideas only), `13.agent-coms/GROK/*`, `17.ai/META-STRATEGY-ATTRITION-HARNESS-TOOL-USE-AND-CHEAP-BUILDING-2026-11-22.md`. XO `31.Nov/nov-22/` (proof videos 6-19) and `nov-23/` (report v1 `1.`, report v2 `3.`, NIGHT 62 `2.`, NIGHT 63 `4.`, this file `5.`). Commits to read: `8348f173d` RTA sleep/part, `c270b51ff` RTA board, `57f1eef34` voxel picture, `a3e108a4b` In: ON fix, `4f480468c` lag fix, `2f32157c1` RTA walk, `638745a58` brick/coin, `4530e5a83` teleport.
