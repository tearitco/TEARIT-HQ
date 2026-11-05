---
name: tsots-game-map
description: How the TSOTS / True Swords Clash / Tear-it co plans map onto the house software and the 98 imported RPG Maker maps (map071 = the 4-player classic board): the TSOTS game family and where its local code and rules live (TSC_ELO, +.TSOTS-ALPHA-OMEGA, AOW, FFU, B.O.T.S.), which lore element maps to which house function (possession, ledgers, sessions/seats/ratings, palnet_peer), the RMMV tricks the owner used, the public Twitter promise and the roadmap to keep it. Use BEFORE touching TSC_ELO, any rated-PvP, Elo, bot, lobby, Bible-level or map-to-level work.
---

# TSOTS, the maps and the house: working map (2026-11-05)

Long forms: book `#.#.calendar-dox/!.HQ-IQ-BOOK/22.lore-ip/TSOTS-GAME-FAMILY-AND-LEVEL-MAP-2026-11-05.md` (tables below in full), `08-roadmap/ROADMAP-TEAR-IT-PROMISE-AND-TSOTS-2026-11-05.md` (the roadmap), `08-roadmap/design-docs/GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md` (the rating pattern), `22.lore-ip/` (lore; skill `soul-pen-lore`). House paths below are from `44.xyz.01.00/`.

## The public promise (SAID; the account is Tear-it co on X, about 30 loyal followers)
"coming soon: てるてる : single & multi player AI and human automation ecosystem : online Elo-rated PvP games with RPG Maker!  ~ RPGツクール製の、Eloレーティング対応オンラインPvPゲーム（シングル＆マルチプレイ）！📌". Four clauses: **A single and multi player, B AI and human automation, C online Elo-rated PvP, D made with RPG Maker.** Rule: **no clause is claimed publicly until it has a receipt** (harness case or recorded run). Until then say "coming soon".

## The game family (READ `+.TSOTS-ALPHA-OMEGA/`)
- **True Swords Clash (TSC)**: `@.apps/TSC_ELO`; rules `#.RULES/True_Swords_Clash_Rules.txt`; 2 players, HP 100, Mana from ordering 2 to 10 Bible verses, 11 Miracles (cost 0 to 10), 4 statuses; **the first rated game** (owner's order). `ops/tsc_elo.c` = Elo (`resolve`, `get`, `update`; ratings file `player|rating|wins|losses|draws|games`, new player 1000, K=32 in the pattern doc; chess elsewhere says 1200: decision D15).
- **TSOTS-OG** (`TSOTS-OG+01.00`): classic game app, single player vs computer with difficulty rising by level; `NO_GL=1` runs without the GL window.
- **TSOMOJIO**: Bible characters + merchants on a chess-like board, 1v1 or 2v2.
- **AOW / Booty Land** (`#.RULES/AOW.Booty_Rules.txt`): 4 players, ~25x25, capital + mech/truck/jet/cruiser/sub, 1 action per turn, coin-flip combat. **FFU / Footrace Fu = Time Fu** (`ff-fu-IQ-a29-RULES`): 2 to 4 players, 5 ninjas, race/duel/Buffet. **B.O.T.S.** (`BOTS_Rules.txt`): 8x8, 32 pieces, blood tax. **Owner 2026-11-05: AOW and FFU bots come later (roadmap M8), after True Swords Clash.**
- All of these are **middle house** (terminal + GL mirror): the owner's rule is to convert to the x11-hq window shell (`20.middlehouse/middle-house-conversion.txt`; TSC_ELO is group B, never launched in that pass).

## The roadmap in one screen (details in the roadmap doc)
M0 decisions and legal (D13 what "made with RPG Maker" means; rename "Diagonal Alley"; RMMV art license; "cursword"; Bible text = public domain) → **M1 True Swords Clash complete offline** (duel to a winner, Elo write-back, HvH/HvC/CvC, x11-hq window) → **M2 shared session/seat/rating layer + rated bots** → **M3 online** (`palnet_peer` is same-machine: **127.0.0.1 hardcoded at `@.apps/TSC_ELO/ops/palnet_peer.c` lines 138 and 166**; signed identity; lobby ledger) → M4 client, JP/EN, distribution → **M5 public alpha = the promise** → M6 TSOTS Bible layer (good/evil, possess, capture, verse record) → M7 chess-like board on `map071` → M8 AOW and FFU bots (+ B.O.T.S.) → M9 Centroid University (the arena is the same rated infrastructure). Receipts table in the roadmap doc.

## The maps (98 imported RPG Maker maps)
`@.apps/piececraft-hq/pieces/system/maps/tsots/map001..map106` (each: `map.txt`, `cells.*`, `layers.txt`, `events.txt`, `extrusion.pdl`, `map.painted.png`); `game.pdl` lists them. **`map071` classic board4 plys (42x52, 4 players)**; 016/017/063 other classic boards; 019/054/055/056 territory; 064/065/066/023 expanse and SS online; 003/077/045/010/067 Afghanistan, 006/046 rabbit hole, 047/049/008/012 launch-saucer-hangar-"out of this world", 009/011 gym and dojo, 014/048 final battles; Mars 030-036/041/095/104-106; lunar 073/082/085/089/088; SS 068 (God's temple), 069, 078, 079, 040, 042, 076; trade 027 library, 028 finance, 081 item shop, 084 forge, 103 mech garage, 085. **No map is named as a Bible level**: Bible levels are new maps to make. Gallery for the owner: XO `31.Nov/nov-5/0.tsots-map-gallery.html`. Only `map071`'s picture has been viewed by an agent.

## Lore → house function (what to reuse; do not rebuild)
- **Possess a Bible character / cursword**: xelector possession (Enter, Tab and click select, `9`, Esc-first, lock to hero), `possessed_id` is the one authority; harness `possess_enter`, `select_follows_possession`.
- **Missions and levels**: event pages (pal) + blueprint viewer + `rpg-edit-hq`; maps from `piececraft-hq`.
- **Economy**: coin ledger `coin.play`, `item_ledger_op`, `shop_op` (one released caller: the editor coin shop; the rest of the lab book stays quarantined).
- **PvP/Elo/lobbies/seats**: the PATTERN doc; seat kinds `human|agent|peer|spectator`; agents use the human command path.
- **AI bots**: `HARNESS-AS-GAME-BRAIN` (judge built, no caller); free models only; Ollama calls go to the Mac.
- **Stairs/levels**: `Z-CELL-FORMAT` (paper), decision D12 proposed. **Arena/tournaments/party battles**: M9.
- Not built: mana/MP, loyalty-capture, crime/courts, lyfebook, soul-pen drawing magic, **vehicle crafting/chop shops (design: `16.game/VEHICLE-CRAFTING-AND-MODDING-DESIGN-2026-11-05.md`, roadmap M10), vehicle physics + proving grounds (M11, `VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md`), gyms/zoos/museums (M12, `GYMS-ZOOS-MUSEUMS-IN-GAME-DOCS-2026-11-05.md`); AI roadmap: `17.ai/AI-ROADMAP-MICRO-AI-AND-IN-HOUSE-MODELS-2026-11-05.md`**.

## RMMV secrets (READ TSOTS SECRETS folder; summary)
The core `rpg_*.js` are replaced by owner copies loaded as plugins (`TSOTSrpg_*`); `GAMEVARIABLES.js` overrides `Game_Variables.setValue` and the interpreter commands 201 (Transfer Player), 285 (Get Location Info), 122 (Control Variables; operand 4 = `eval`, an injection hazard); `tilelyfe.js` plans RMMV **regions** as a meta map builder; script-call functions read `$gamePlayer` and `$gameMap.events()`. The house's real bridge is the **map import** (RMMV map → `map.txt`/`cells`/`events`/`extrusion.pdl`). The house renders RMMV art in its own engine: **check the RPG Maker license before shipping**.

## Standing cautions
Verify before trusting: `TSC_ELO/prog-report-pvp.md` claims a live P2P duel; it was **not re-run** by an agent this session. Heavy work through `cpu_gate`; never loop to a free-API quota. Language rule: C, pal, pdl; no Python, no new `.sh`; a JS RPG Maker plugin client is an **owner decision** (D13). Cross-machine and signed ratings come before any "online" claim.
