# Visual games plan: Harvest Moon + Moonlighter, then Civilization / Monopoly / WU / GDS / WSR, all visual

2026-10-11, claude (Sonnet 5.5), from the owner's direction. **Plan only, nothing here is built.** Items marked **[ASSUMPTION]** are my guesses and must be confirmed by the owner (section 7).

## 1. The idea in one paragraph
rpg-pet is the first complete example of a "visual game window": a khtpm window with a 2D default view and an optional 3D view, a data-driven map (rooms, doors, scenery, items), entities that move on their own, an append-only event ledger, the house clock, rogue/live time modes, Play/Stop, save slots, chat, hotbar, a minimap. Every game below needs the same skeleton. So the order is: **finish the shared skeleton in rpg-pet, extract it, then each game is mostly data (a book) plus its own rules op.** Pets can later PLAY these games through the same relay a human uses (ecosystem soul, memory note), which is why every action must be a verb the relay can send.

## 2. What exists today (verified)
- rpg-pet: window + C op + ledgers + clock + INT + turn/live modes + Play/Stop + saves + chat; export of rooms as a pc-hq book (TSOTS format) with sprites; camera/xelector follow the selected entity (2026-10-11). The in-window 3D view and the minimap are the next two items (handoff A-C).
- pc-hq books: TSOTS (98 desks, full 3D set), Doom, chess, `cdda_sample`, `mineclonia_sample`; the clone-game brief `GROK-BRIEF-CLONE-GAME-BOOKS-2026-10-09.md` (dwarf fortress, cdda, mineclonia, civ book skeletons: data only, no rules).
- `@.apps/civ-txt/CIV_TXT_DESIGN.md` (civ design), WSR in `WSR_PAL-PREFERED/` (50 companies, 7 governments, order book, ledgers; bonds broken, see `opencode-oct10.md`), exchange-hq + auction-hq (new, harnessed), pet chain economy design (`CHAIN-ECONOMY-DESIGN.md`).
- Assets: RPG Maker MV sheets (tiles, characters, monsters, parallaxes), mineclonia and cdda tilesets in `#.NNEST_ASSETS`. Rule: nothing but vetted/free assets.

## 3. Shared skeleton (phase 0: do once, in rpg-pet, before any new game)
1. **In-window 3D (2D default, 3D toggle)**: mirror the board-viewer frame into the game window's canvas (own size handoff, own engine session so the pc-hq window is NOT needed), POV keys 1-4 / 0 / q e r t w a s d c v f forwarded from INT. Camera + xelector follow the selected entity (done as scaffolding).
2. **HUD**: minimap of the current map + thumbnails of the others (`hud.pdl`, toggles), like pc-hq's HUD.
3. **Generic game shell**: pull the reusable parts out of `rpg_pet.c` into the house's sharing style (text-include for pure logic, op + IPC for stateful; no header+link split): ledger replay, flag files, clock gate (rogue/live, Play/Stop), save/load slots, key reader (INT relay), chat/verb dispatcher, `follow_*`/export to a book, test harness (pal cases). Each game then supplies: `rooms/maps.pdl`, `catalog.pdl`, `rules` code, `time.pdl`.
4. **Regression harness** for the shell (pets walk, chat tail, event runner, save/load): cheap agents can write these cases.
Proof for phase 0: rpg-pet window shows 3D and 2D, with before/after PNGs and the click path.

## 4. Group A: life/shop sims (Harvest Moon, Moonlighter)  -> built on the pet world
Both are top-down tile worlds with a day/night clock, an inventory, NPCs and money: exactly what rpg-pet already is. The pets become the companions/workers.
**Harvest Moon-style farm (`farm` book)**
- Maps: farm (plots), house interior, village, shop, mine/forest (reuse RPG Maker Outside/Inside tiles; TSOTS interiors as layout corpus).
- Rules (`farm_rules`): plots with a crop ledger (`PLANT|t|plot|crop`, `WATER`, `GROW` fired by the clock `AT` events, `HARVEST`), seasons from the house clock, weather, stamina (= the MP/skill system from the skills plan), selling at a shop price table, relationships with villagers (the existing `MEET`/relations ledger), festivals as clock events.
- Verbs: till, plant, water, harvest, sell, buy, talk, sleep (advances the clock to morning; turn mode makes this natural).
- Pets: pets can water/harvest (a pet as a worker with skills), earning into the pet wallet.
**Moonlighter-style shop + dungeon (`moonlight` book)**
- Day = run the shop (price items, customers = events that walk in and pick items; reaction ledger), night = dungeon crawl: generated rooms (use TSOTS dungeon desks as the corpus), enemies = the RPG Maker DB enemies/troops already in `db-hq`, loot into the shop inventory. Turn (rogue) mode for dungeon fights, live mode for the shop day: the rogue | live toggle exists for exactly this.
- Rules: item catalog with value/rarity, shop price-feedback (customer mood), dungeon room generator (desk generator in C, seeded), simple combat using DB stats/skills.
Order: farm first (smaller rules), then moonlight (reuses farm items/shop + adds combat).

## 5. Group B: strategy and business sims (Civilization, Monopoly, WU, GDS, WSR)  -> board/management views
These are "state and rules first, picture second", so the visual job is a **map or board view plus panels**, using the same book + window machinery.
- **Civilization (`civ` book)**: world map (tile grid, terrain from existing tilesets), cities as events, units as sprite events, turn mode only. Rules from `civ-txt/CIV_TXT_DESIGN.md` (research, production, borders). Visual: 2D map default, 3D optional (the engine already extrudes terrain heights). Panels: research, reports.
- **Monopoly (`monopoly` book)**: a board ring of 40 desks or one big desk; tokens = sprites; dice, property ledger (`BUY|t|player|tile|price`, `RENT`, `MORTGAGE`), cards. Natural tie to WSR/exchange/auction: auctions already exist (auction-hq); wallets/pets are players. Turn mode only. Smallest rules of the group, a good first board game to prove the pattern.
- **WSR visual**: the market game as places: a city map with company buildings and government halls, a stock-exchange room (order book as a wall display), bank; panels for portfolio/bonds. First deliverable is the exchange-window market tab (`opencode-oct10.md` item 5); the map is the later skin. Bonds must be fixed first (pegged to government lending).
- **GDS = Game Dev Story, a business and R&D sim (owner confirmed 2026-10-11)**: a studio interior (RPG Maker office tiles), staff as characters at desks, a project pipeline (idea -> design -> build -> test -> release) as clock events, R&D that unlocks options, sales via the market. Strong fit with rogue/live time (live with a speed control). Built on WU (below).
- **WU = the owner's 2025 modular simulation prototype** (folder `sim.wu` on the external drive, `/media/no/b7ced73c-.../home/jbez/Desktop/^.SHARE]/.../SIM]NOW!]NOS]a0/.../sim.wu...d13]`; 672 KB, April-May 2025): C ops `4.setup_player`, `5.sim` (school/bank/factory), `6.ai`, `7.hash`, `game_ai`, `player_ai`; notes say: players go back and forth in rounds, AI players ("customers") are plugged in through an `ai=ngn` argument, control can be handed from AI to human and back at any time (hotkey idea), spectators take over free slots, "life/biz has a lot more dimensions than sim", and "dont worry about kairosoft/wsr, I will do that as TASK2/3" - i.e. Game Dev Story and WSR were meant to sit on this core. I read only the notes and the file listing, NOT the C code. Treat it as the sim/player/AI core for the business games (GDS, WSR, Monopoly-style board play): a new agent should read `dox/player_ai]b1.txt` and the C ops before designing the core.
Common to the group: each rule set is an op run by the shared shell, ledgers are append-only, every move is a verb (so pets/agents can play), and each game has a pal harness that proves a full round conserves the totals (cash, units, tiles), the same standard as the exchange/auction harnesses.

## 6. Order of work, owners, proof
| # | Item | Lane | Proof |
|---|---|---|---|
| 0 | In-window 3D + POV keys, minimap, shell extraction, shell harness | claude / Opus (renderer, camera) + cheap agents for harness cases | PNG of 2D and 3D in the game window; harness verdicts |
| 1 | Monopoly (smallest rules, pattern proof for board games) | cheap agents (data + rules op) with a locked harness; claude reviews | a full 20-turn game by script, totals conserved; PNG of the board |
| 2 | Farm (Harvest Moon) | claude + cheap agents (maps/catalog data) | a game day with crop growth ledger; PNGs |
| 3 | WSR visual: market tab + bonds fixed + city/exchange map | opencode lane (economy) + claude (window) | harness: bond issue conserves cash; PNG |
| 4 | Moonlighter (shop + dungeon) | claude + agents | shop day + one dungeon run; PNGs |
| 5 | Civilization | agents on rules, claude on map view | 3-turn game, borders and research change; PNG |
| 6 | GDS studio | agents | a project through the pipeline; PNG |
| 7 | WU | after the owner explains it | - |
Every item ends with a before/after PNG the owner can see plus the click path (owner rule). Free models only; heavy builds under `nice -n 15 ionice -c3` (weak CPU: only run 3D while a 3D view is showing).

## 7. Questions for the owner (do not guess)
1. ~~What is WU / GDS?~~ ANSWERED 2026-10-11: GDS = Game Dev Story (business and R&D sim); WU = the 2025 modular sim prototype (section 5).
2. **Order**: is the order in section 6 right, or do you want Harvest Moon / Civilization first?
3. **One window per game, or a "game hub"** window that lists the books (like pc-hq's File menu) and opens each? (I lean toward a hub that launches the same shell with a different book.)
4. **3D in the game window**: should each game window run its own board-viewer engine session (heavier, independent of pc-hq), or reuse one shared session (lighter, but only one 3D game open at a time)? I lean toward its own session started only while View is 3D.
5. **Art**: RPG Maker tiles for everything, or add the mineclonia/cdda tilesets for some (dungeon, city)? Any asset you do not want used?
6. **Pets as players**: should pets be able to play these games (through the relay) from the first version, or after the human version works?
7. **Real-time or turn-based per game**: my assumption: farm = live with a day clock, Moonlighter = live (shop) + rogue (dungeon), civ/monopoly = turns only, GDS and WSR = live with a speed control.
