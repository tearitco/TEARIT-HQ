# Grand roadmap checklist (one page, by feature, done vs not done)

Written 2026-11-04. This is the CHECKLIST view. The narrative front door is `../ROADMAP.md` (customers / investors / developers); the per-game milestone tables with proof columns are `../16.game/GAMES-ROADMAP.md`; the day-by-day proof labels for the editor work are `../09-appendix/HANDOFF-INDEX-2026-11-04.md`. This page does not replace them: it lists every feature area in one place, says whether each row is done, and points to the spec that holds the detail.

## How to read it

- `[x]` done AND proven: a harness or a live run passed, evidence named. `[~]` partly done, the gap is written on the row. `[ ]` not done.
- Evidence tags: **OBS** = I ran it on 2026-11-04 (harness name and count, or a screenshot / state file). **DOC** = an older doc says so and I did NOT re-run it today; treat it as a claim until re-run. **NONE** = nothing yet.
- A `[x]` with a scratch harness only is still "scratch": it says nothing about the live window unless the row says so. Every harness named here is under `44.xyz.01.00/&.widgits/_shared-lib/harness/` (`prisc <name>.pal`, or `ops/+x/playthrough_op.+x run play/phases/<name>.phases.pdl`).
- Owner decisions are marked **OWNER**. Nothing in a quarantined book is released without one.

## A. Editor platform (`@.apps/rpg-edit-hq`)

- [x] Chat pane with three modes (edit = pet chat, h-ai = the API chat that edits this book, network = IRC rooms); h-ai is the saved default (`layout.pdl`, commit `0721c2820`). OBS: screenshot of the right pane 2026-11-04 after `chatmode hai`. Gap: a fresh window open landing on h-ai was not re-tested, and the Settings preference `sidebar_start` may override it.
- [x] Player tab says `Player: playing` / `Player: stopped` (editor family, game-dev-hq, pet-house, pet-trainer). OBS: live screenshot and stop/play flip in the editor; the other three by binary/script text only.
- [x] In: ON = play (turn based, clock paused, each act = `TURN` row), In: OFF = demo (clock runs live). OBS: `play/phases/turn_clock_rpg_edit_hq.phases.pdl` 3 phases (acts advance 5 min, a 10-min event fires once, In: OFF act does not move the clock). Gaps: acts were `buy` not an arrow `step`; no resident clock daemon in scratch.
- [ ] Play mode starts the game clock at 00:00 / resets unless the game is reset (OWNER's stated rule). NONE: not in the code (`clock_day0` is written once at install).
- [x] Pets speak on the game clock (opt-in, shipped OFF); the controlled pet is silent in play mode only. OBS: `pet_say_clock.pal` 19; live: `clock advance 1h` produced 4 posts, restored byte for byte.
- [x] Pet-post guard (kill switch, caps, hold, approve / veto, audit). OBS: `pet_post.pal` 36; live approve/veto done earlier.
- [x] Two-machine IRC (debil, Mac). OBS: `irc_two_machine.pal` 19. Gap: `~/phase2-net` left on both machines (OWNER to clean).
- [x] Network pane controls (`netroom`, `netguard`), pane scrolling by wheel. Gap: PageUp/PageDown relay keys had no effect on the chat pane.
- [x] Placer, mouse alignment, dropdown thumb, lamps at dusk, blank-page size test. OBS: `live_placer_verb`, `live_place_and_move`, `live_mouse_align`, `live_mouse_calibrate`, `live_dropdown_thumb`, `live_lamps_at_dusk`, `blank_page_size` (all gated by `NEEDENV | LIVE_OK`).
- [x] Legend writer (`legend_page_op`) incl. live load. OBS: `legend_page.pal` 19, `live_legend_page.pal` 13.
- [ ] 2D / ASCII parity of the editor's views. NONE: never checked (owner rule: 3D must have 2D parity).
- [ ] `cpu_guard` hygiene sweep loop run live (only `trim_tail` was tested).
- [ ] Owner-pending live items: `show_pointer=1` left in `#.desktop/hq_ui.pdl`; stray Mac palnet node; three `.hai.bak` files under `events/phase_dawn`; the pre-modified `play_rpg_{dust,space}_planet_map.pdl`.

## B. Early-survivor slice (quarantined in the editor's own book, `books/ledger-lab/`)

Design: `../16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md` (has the same checklist with as-built notes). All ops refuse a folder without a `QUARANTINE` file. **OWNER** lifts quarantine.

- [x] Item ledger: append-only hash chain, mint only from allowed sources, no negative balances, per-id conservation, compaction, size and x/y/z limit test. OBS: `item_ledger.pal` 39 (+ size test 43).
- [x] Step 1 smash / needs / eat / drink / wield / light. OBS: `survivor_slice.pal` 44.
- [x] Step 2 books / skills / read / practice / craft. OBS: `survivor_craft.pal` 37.
- [x] Needs follow the GAME clock, shipped OFF. OBS: `needs_clock.pal` 15 PASS + mutation run; LIVE 2026-11-04 (on a backup, restored): the live clock daemon fired `needs_tick` 6 times for each of 2 actors in 1 game hour. The live run FOUND A BUG: six 10-minute ticks gave hunger 6 / thirst 12 / fatigue 0 instead of 10 / 15 / 5 (integer division dropped the remainder). Fixed by carrying the remainder in `state/needs_rem.txt`; checked against the old binary (6|12|0 vs 10|15|5); `survivor_slice` 44 and `survivor_craft` 37 still PASS. Gap: the schedule rows are NOT installed on the owner's clock after the test (restored).
- [x] Step 3 deterministic map writer (CDDA house chunk, DF 8x8x12 column). OBS: `gen_map.pal` 21. Gaps: no MCL chunks, no generation on stepping onto a house, no loot pickup verb, chunk not loaded into the editor.
- [x] Step 4 flow / rain / fire. OBS: `world_common.pal` 38 + mutation run. Gaps: one room only, no MCL water. Clock wiring: see the next row.
- [x] Flow / rain / fire follow the GAME clock, shipped OFF (`world_clock.pdl` `ENABLED | 0`; `time.pdl` rows `world_flow` `world_rain` `world_fire`). OBS: `world_clock.pal` 14 + mutation run; opt-in, ENABLED 0 and an unquarantined book change nothing. LIVE 2026-11-04 (on a backup, restored): the live clock daemon fired flow 12, fire 12 and rain 6 times in 1 game hour; fire fuel 20 -> 2 (= 12 + 6), water spread to 3 cells with the volume sum still 3, the container filled to its cap, the dirt cell got wet. Gap: the schedule rows are NOT installed on the owner's clock after the test (restored).
- [x] Step 5 MCL bench, 3x3 craft, furnace, bed + respawn, creeper burst. OBS: `mcl_bench.pal` 49 + mutation run. Gaps: not run from the clock or a player action; no night spawn; the respawn position file is not read by any real spawn yet.
- [x] Step 6 shop slice: coins are ledger items (`coin.play`, named after the exchange-hq unit `play`), `buy` / `sell` are atomic ledger trades, no swap verb (cdda / df / mcl stay three ids; across ids = two trades through coins), every item has sell < buy so a loop loses coins. OBS: `shop.pal` 41 + mutation run (4 failures as they must). Spec: `../16.game/SHOP-SLICE-SPEC-2026-11-04.md`. Gaps: not connected to the editor Shop tab (that one is a separate, older coin counter in `house.txt`), not to exchange-hq (paper ledger), no real customer, no restock or dynamic prices. RELEASED to the editor (see the next-but-one row): `Coin shop ▾` tab in the editor, live click proven.
- [~] Release of quarantine: OWNER DECIDED 2026-11-04: release ONE caller, the editor's shop. Done for that caller only: `books/shop/` (copy of the item and price data + a `RELEASE` file naming `shop_op`; `shop_op` and `item_ledger_op` accept it, the latter only when the env `ITEM_LEDGER_CALLER` is `shop_op`), the editor gets a `Coin shop ▾` tab and an `lshop` verb. Lab ledger `check` before the release: `CHECK OK rows=18 items=22`. OBS: `shop.pal` 50 PASS incl. a release-gate case (the released folder works for shop_op; `item_ledger_op` run directly, `survivor_op`, `world_common_op`, `mcl_op`, `gen_map_op` all exit 4; a RELEASE naming another caller or no marker at all is refused); LIVE: a real click on `buy cdda.rag 4c` in the editor window made `player` pay 4 coins and receive a rag, ledger `check` OK (then the shop book was reset to its owner-seeded baseline). Everything else stays quarantined (`books/ledger-lab/`). The editor has ONE tab `Coin shop <coins>c ▾` (7 item rows) and a Buy / Sell / Close popup (owner rework 2026-11-04; `coin_shop_ui.pal` 34 PASS + live screenshots); the player's coins are in the tab label and the popup. Not checked: whether the layout studio could own the popup (text wrap does not grow the row height). Gap: the soft guard is not a security boundary (any process can set the env); the furniture Shop tab and its coin counter are unchanged and separate.
- [~] Tear-it co source pages LOCATED 2026-11-04: the editor's floating islands = `@.apps/rpg-edit-hq/rooms.pdl` row `ROOM | start | Islands | ... | 56 | 17` (painted by the island code in `ops/rpg_edit_hq.c`, design `../16.game/EDIT-ISLANDS-DESIGN.md`, whose own header still says design-only: stale); the multi-planet pages = `@.apps/rpg-space/planets.pdl` and `@.apps/rpg-dust/planets.pdl` (maker / cata / cubia / hell, each its own seed), the spaceship skiff = `skiff_1` rows in `@.apps/rpg-space/space.pdl` (CRAFT and WCRAFT), page events `events/planet` and `events/newplanet`, harness tables `cases/play_rpg_{space,dust}_planet_map.pdl`. NOT done: turning them into tear-it co sources (nothing was copied or changed).

## C. CDDA (`cdda-hq`) - rules grok, infrastructure claude

Status below is the `GAMES-ROADMAP.md` status of 2026-10-13 (DOC, not re-run today).

- [ ] C0 cdda-hq spec (menus, movement, stats, inventory, crafting, stairs, vehicles, NPCs, enemies, mass building, map generation). DOC: designing.
- [ ] C1 one room with CDDA ids (UltiCa 32 px tiles). DOC: planned.
- [ ] C2 building with cellar, floors and a stair (first z build). DOC: planned.
- [ ] C3 a car as a part-cell grid. DOC: planned.
- [ ] C4 menus, stats, inventory, crafting on db-hq rows and events-hq pages. DOC: planned. Partial overlap: step 2 craft is in the lab book, not in cdda-hq.
- [ ] C5 NPC / enemy FSM with logged decisions. DOC: planned.
- [ ] C6 scripted cdda playthrough. DOC: planned. (A `play/phases` playthrough table style exists for the editor.)

## D. Dwarf Fortress style (`df-hq`) - grok

- [ ] D1 designation becomes a job row; one mover walks to the stand tile. DOC: planned.
- [ ] D2 several movers under a per-tick path budget. DOC: planned.
- [ ] D3 stockpiles and a save shape (`meta/map/items/units`), save / reload equal state. DOC: planned.
- [ ] D4 z-levels. DOC: planned (needs the shared z-cell format).
- [ ] D5 scripted df playthrough. DOC: planned.
- [~] DF column generation with mineral ids written up front (digging reveals): the writer exists in the lab book (OBS: `gen_map.pal`), no digging verb and no df-hq consumer.

## E. Minecraft style (`mcl-hq`)

- [x] MCL recipes with a 9-cell grid exist in `@.apps/mcl-hq/recipes.pdl` (2 rows). DOC. The 2x2 vs 3x3 + bench rule was "not checked yet" there; it IS checked in the lab book's `mcl_op` (OBS: `mcl_bench.pal` 49), not yet in mcl-hq.
- [ ] Port the bench / furnace / bed / burst rules from the lab book into `mcl-hq` (needs the quarantine decision). NONE.
- [ ] Biome / height / trees / ore / sea-level water volume generation, new chunks when the player walks off the loaded set. NONE.
- [ ] Redstone, mobs beyond the creeper, a second z world: out of this slice.

## F. GTA style, vehicles, space (`gta-hq`, `rpg-space`, `rpg-dust`)

- [x] rpg-space / rpg-dust playable with life layer, ambient asteroids, tick cost file; both gates 83 PASS. DOC (2026-10-27 roadmap). Helm flight, collision, mining, remnants: `16.game/RPG-SPACE-*.md`.
- [ ] gta-hq milestones. DOC: designing.
- [ ] Ships and vehicles as entities with relative positions; draw vehicle pieces that load but are not drawn. DOC: designing.
- [ ] "Micro" scenes inside a ship's voxels; deep simulation per entity opt-in. DOC: planned.

## G. AI

- [x] h-ai API chat in the editor: locate session, context file, Groq ask with a tool whitelist, propose, `/apply` `/no`, recompiles event pages from whitelisted nodes. OBS: the pane showed the applied proposal 2026-11-04; first delegated task (Ember says good morning at dawn) done end to end in commit `fb225e512`.
- [~] Harness as game brain: judge built, **no caller exists**. DOC. `../17.ai/HARNESS-AS-GAME-BRAIN.md`.
- [ ] Pets drive the same input path as a human, learn with house models and harnesses. DOC: designing.
- [ ] Promotion of a high-weight step into a concept-bank entry / training example / default plan. DOC: designing. Before touching chat or promotion-ledger code read `ROBOT-CHAT-BLUEPRINT.md` and the latest `2do.md` (an earlier session invented a disconnected mechanism by skipping this).
- [ ] Describe -> score -> store loop with a rejectable store. DOC: designing (`../17.ai/AI.md`).
- Standing rule: free models and a local model server only; Ollama calls go to the Mac. Never loop to a rate limit.

## H. Chain, store, economy

- [x] `ev_chain`, `ev_store`, 9 registry commands tested by hand (free `get` only, test chains only, unsigned). DOC.
- [ ] Money and trust layer, signatures, courts, creator loop. DOC: medium term in `../16.game/ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md`.
- [x] Shop slice for the survivor games (B step 6), coins on the item ledger. OBS: `shop.pal` 41.
- [ ] Bridge the ledger coin `coin.<unit>` to exchange-hq units (cones, mined, play). exchange-hq is a PAPER ledger today (balances unchecked, no chain coins move; READ in `exchange_manager.c`), so a bridge needs its settlement layer first. It never swaps game items either: no cross-id item exchange exists anywhere yet.

## I. Network, social, cross-machine

- [x] Same-machine and two-machine IRC (A above).
- [ ] General cross-machine networking (`palnet_peer.c` hardcodes 127.0.0.1). DOC: a plan only.
- [ ] Browser fully Chrome-replaceable (owner directive 2026-10-20). DOC: building (`../08-roadmap/2026-10-20-CHROME-REPLACEMENT-SIZING.md`, `NB-JS-ENGINE-ROADMAP.md`).
- [ ] Windows and macOS (macOS via XQuartz today; Windows thin shims). DOC: planned (`OPEN-ITEMS.md` item 2).

## I2. Views, layouts and the xelector look (owner vision 2026-11-04; doc `../18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md`)

- [ ] The "views" vision: see views live in a greater editor and chain them with RPG-Maker / blueprintz style scripting (Xcode / MVC shape). VISION, not built; the doc maps each idea to the house piece that exists.
- [~] Layout studio (`@.apps/layout-studio`): layers 1-2 (validator + catalog) built 2026-10-09; templates, `layout_op`, scaffolds and the editor window NOT built. OBS 2026-11-04: `layout_check` on the editor board = 44 errors (pre-existing, "key not published"); not investigated why. The coin popup's 4 extra errors were fixed (publish every key always).
- [~] Make `layout_check` green on the editor board: answered 2026-11-05, the 44 were a checker INPUT limit (pass every `board/state/*.txt` with repeated `--ui`: 19 left). The 19 = taskbar-published `load_*`, conditional `pal_armed`/`time_label`, and unused menu slots `page_post_*`/`book_post_*` (data-driven from `toolbar.pdl`, hidden by `show=` when empty; NOT dead, my first reading was wrong). Open: teach `layout_check` that `show=` slots may be absent (a checker change), or accept the 19.
- [x] Wrap-aware text rows for overlays (2026-11-05): class `ov-wraptext` on an overlay `<text>`; rows grow to the wrapped height (cap 8), the popup grows, rows below move; opt-in. OBS live frame: a 92-character receipt wrapped to 4 lines with no overlap. Headless/text mirror still 1 row; no harness case.
- [x] Xelector look (2026-11-05): lighter / thinner green while it CONTROLS an entity, the old saturated 6 px green while free; one pure text-include `_shared-lib/xelector_look.c` + `view.pdl` rows `xelector_free_*` / `xelector_ctl_*` (defaults free `40,255,80` 6 px, controlling `150,255,170` 3 px, owner can change them as data); the 3D HUD `poss` now follows `possessed_id`. OBS: `pieces_2d_parity.pal` 58 PASS incl. a negative renderer that must fail. 3D: OBSERVED live 2026-11-05: free box exactly (40,255,80), controlling exactly (150,255,170), HUD `poss` matches; 3D changes colour only (no thickness knob). Open: what counts as controlling for an NPC the player does not own. 2026-11-05 fix: `possessed_id=none` counts as free (59 PASS).
- [ ] An onboarding guide for the event architecture (four layers: clock `common:` events, event pages / choice popups, S1 ledger events, board projection). The pieces exist in `02-architecture/` and `16.game/`; no page connects them. Not written.

## J. Housekeeping the owner decides

- [ ] `grok` and `main` branches are synced to claude only on request (last sync commit `183048f53`, before this session's later commits).
- [ ] Python: the baseline in `harness/py_baseline.txt` may only shrink; nothing new was added.
- [ ] `README.md` has someone else's uncommitted edit (a `21.drive-doc-fix` line); I did not commit it.

---

# Technical spec library (what to read, and what is still unwritten)

Each row: in-house spec (path in this book) | external reference (the URL returned HTTP 200 on 2026-11-04; its content was NOT read) | spec still to write.

| Area | In-house specs | External technical references | Spec still to write |
|---|---|---|---|
| CDDA data and rules | `../16.game/RULES-cdda-hq.md`, `../08-roadmap/design-docs/CDDA-DF-SUBAGENT-BUILD-2026-10-12.md`, `../08-roadmap/design-docs/CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md`, `../16.game/Z-CELL-FORMAT.md`, `../16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md` | https://docs.cataclysmdda.org/ , https://github.com/CleverRaven/Cataclysm-DDA/blob/master/doc/JSON/JSON_INFO.md | A CDDA JSON import map: terrain / furniture / item / recipe ids to our three item ids and tile ids, with the subset we accept and the fields we ignore. Also a `furniture.pdl` drop-table spec (the lab book's smash rows) tied to CDDA `bash` data. |
| Dwarf Fortress style | `../16.game/RULES-df-hq.md`, same subagent doc and handoff, `../16.game/GAMES-ROADMAP.md` D1-D5 | https://dwarffortresswiki.org/index.php/Raw_file , https://dwarffortresswiki.org/index.php/Fluid | Job and designation row format; the per-tick path budget; the save shape `meta/map/items/units`; a fluid model note (our flow is 1 volume per call, sum conserved; DF's pressure model is out of scope and should be said so). |
| Minecraft style | `../16.game/RULES-mcl-hq.md`, `../16.game/HANDOFF-2026-10-28-MCL-BOOK-TO-GROK.md`, `../16.game/GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md` | https://minecraft.wiki/w/Crafting , https://minecraft.wiki/w/Explosion , https://minecraft.wiki/w/Bed | A chunk and biome generation spec (seeded, file output, byte-identical, same gate as `gen_map_op`); a mob night-spawn page spec (clock wakes a mob page; the creeper page calls `burst` once). |
| Engine, z, events | `../16.game/Z-CELL-FORMAT.md`, `../16.game/S1-EVENT-LEDGER.md`, `../16.game/GAME-MECHANICS-SPRINT.md`, `../16.game/EVENT-VERB-PAGES-DESIGN.md`, `../16.game/PETSLIKE-FRAMEWORK-PLAN.md` | (house-specific) | `todo:walk / dig / build / craft` bodies for the S1 ledger (today they return `todo:<verb>`); how the lab-book ops become ledger verbs without a second inventory writer. |
| AI side | `../17.ai/AI.md`, `../17.ai/HARNESS-AS-GAME-BRAIN.md`, `../17.ai/AI-BACKENDS-DISAMBIGUATION.md`, `../17.ai/API-REGISTRY-AND-CAPS.md`, `../08-roadmap/design-docs/H-AI-LAB-DESIGN.md`, `../16.game/HANDOFF-2026-10-27-AI-SIDE-COMPANION.md`, `../16.game/AI-SIDE-ANSWERS-BLOCK-A-2026-10-28.md` | Voyager (skill library from a game loop): https://arxiv.org/abs/2305.16291 ; Generative Agents (memory + planning for simulated actors): https://arxiv.org/abs/2304.03442 | A pet-caller interface spec: what an actor may propose to the harness, what the judge scores, how a weight change is recorded and promoted. The harness judge exists; no caller does. |
| Chain, store, economy | `../16.game/CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md`, `../08-roadmap/design-docs/PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md`, `../16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md`, `../16.game/SHOP-SLICE-SPEC-2026-11-04.md`, `&.hq-apps/exchange-hq/ops/exchange_manager.c` (header = the order-book spec), `../16.game/WALLET-CONVENTIONS-AND-OPEN-CALLS-2026-11-02.md`, `../16.game/PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` | (house-specific) | DONE for the slice: `../16.game/SHOP-SLICE-SPEC-2026-11-04.md` (price rows, spread rule, atomic trade, cross-id = two trades through coins). Still to write: the coin <-> exchange-hq unit bridge (needs exchange settlement) and dynamic prices. |
| Network and chat | `../08-roadmap/design-docs/IRC-FORUM-CHAIN-HQ-WINDOWS.md`, `../08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md`, `../08-roadmap/design-docs/HARNECIENT-H-AI-RELAY.md` | https://modern.ircdocs.horse/ , https://www.rfc-editor.org/rfc/rfc2812 | A cross-machine peer spec (replace the hardcoded 127.0.0.1 in `palnet_peer.c`, discovery, auth) and a rate / abuse spec for pets speaking on a public room (the guard's caps are the start). |
| Harness and proof | `harness/README.md` (in the house), `../06-testing/HARNESS-AUTHORING-GUIDE.md`, `../06-testing/GAME-PLAYTHROUGH-HARNESSES.md`, `../10-user-docs/PRESENTATION-VIDEO-PIPELINE.md` | (house-specific) | A one-page "negative test" recipe (mutation run: change an expectation, the case must fail) so every new case carries one. |

Missing-spec rule: when a row above says "still to write", write it BEFORE building the feature, and put the spec path in the checklist row.
