# TSOTS games, the 98 maps, house functions and the RMMV secrets (2026-11-05)

Labels: **READ** = in a named file; **OBS** = observed in the repo this session; **DOC** = my reasoning, not run. The long form behind the two local skills `tsots-game-map` and `soul-pen-lore` (`.claude/skills/`).

## 1. The TSOTS game family (READ: `44.xyz.01.00/+.TSOTS-ALPHA-OMEGA/`)
The owner's local TSOTS work lives in the **middle house** (the older terminal + GL-mirror app generation; `20.middlehouse/middle-house-conversion.txt`). Several acronyms appear; do not mix them:
| Name | Expansion (as written) | Where | What it is |
|---|---|---|---|
| **True Swords Clash (TSC)** | "True Swords Clash (The Sword of the Spirit)" | `@.apps/TSC_ELO` (387 files), rules `+.TSOTS-ALPHA-OMEGA/#.RULES/True_Swords_Clash_Rules.txt` | the **Elo game**: a 2-player turn duel, HP 100, Mana 0; Mana is earned by sorting 2 to 10 shuffled Bible verses into order; 11 Miracles (Hail Mary, Slash, Fire, Berserk, Heal, Judgement, Poison, Locust, Blessing, Angel, Holy Sword) with Mana costs 0 to 10; statuses Bleeding, Poison, Locust, Blessing; win by HP 0, forfeit allowed; HvH or HvC; source text usually the Bible, other public-domain texts allowed. The Elo op is `ops/tsc_elo.c` |
| **TSOTS-OG** | "TSOTS, This Order Still Stands" (middle-house list) | `+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00` | the classic game app; notes: single player vs the computer, difficulty shifting as the player levels up, optionally vs online "CORPUS AGENTS"; runs without the GL window with `NO_GL=1` |
| **TSOMOJIO / TSOMOJI** | | `TSOMOJIO/#.wussup+@io.txt` | the idea of Bible characters and merchants as pieces on a chess-like board (the "real tsomojio"); a board game helps data structures and widgets evolve |
| **B.O.T.S.** | Battle of the Sovereigns | `#.RULES/BOTS_Rules.txt` | 8x8, 32 pieces, blood-and-bullets economy on Mars; win by assassination (queen) or bankruptcy ("blood tax") |
| **AOW / Booty Land** | Armor of War | `#.RULES/AOW.Booty_Rules.txt` | 4 players, about 25x25 islands and water, capital + mech, truck, jet, cruiser, submarine; one action per turn; coin-flip combat; 1,200 "tokens" of plunder; win by conquest or wealth |
| **FFU / Footrace Fu (Time Fu)** | | `#.RULES/ff-fu-IQ-a29-RULES...txt` | 2 to 4 players, 5 time-bending ninjas, race on a grid or chutes-and-ladders board, duel and "Buffet" economy variants; set in the **Land of Time** (Soul Pen pillar 12) |
| **classic board / chimochio 4x4** | | `map071` (42x52, 4 players) | the chess-like board of the old notes (see `../16.game/TSOTS-SECRETS-VISION-SUMMARY-2026-11-05.md`) |
The owner's own order (READ `#.wussup+@o.txt`): **do TSOTS (True Swords Clash) first** ("bible game vs computer + mana and level up character ... single player or multiplayer, classic rules, nothing changed"); **then** take the sword into the Bible: Adam and Eve "Dwarf Fortress style", **do good or evil, gain influence over characters and possess them (possess the snake in the garden)**, treat TSOTS as chess with Bible characters and merchants on the board, 1v1 or 2v2; remember which verses a player did (achievements, personal lore, history); keep the terminal "in the shape of a phone". RPG Maker (rmmv) waits for the board version ("wait for rmmv? yes tbh").

## 2. The 98 imported maps by role (READ: `@.apps/piececraft-hq/pieces/system/maps/tsots/`, names from each `extrusion.pdl`)
Imported from the owner's RPG Maker MV project "The Sword of the Spirit". Sizes in tiles. **I viewed only `map071`'s painted image; roles below are from the map names and the old story notes (DOC).**
| Role | Maps (id name, size) |
|---|---|
| **Classic boards and menus** | 016 classic board MARS VERSION (42x53), 017 classic board (42x52), 063 Board game version (42x52), **071 classic board4 plys (42x52, 4 players)**, 052 CLASSIC MODE (17x13), 053 ONLINE APP, 057 ONLINE LOBBY (S) (24x25) |
| **Territory mode** | 019 Tearitory mode small (22x16), 054 Territory med (24x22), 055 territory mode large (30x26), 056 XL DLC ONLY (48x30) |
| **Expanse / online MMO** | 064 Ocean Expanse MMO (80x70), 065 SPaCe expance MMO (100x100), 066 Mars xpans MMO (66x66), 023 and 059 SS ONLINE (55x55) |
| **Story mode (old notes' beats)** | 003 Unlukky vision, 077 unlucky vision 2, 045 AFGANI TOWN, 010 empty afgani desert, 067 afgan hills (Afghanistan); 005 God mini game lvl (God Delusion); 006 RABBIT HOLE (91x36) and 046 Rabbit hole caves (Beat 4); 047 launch plataue, 049 inside saucer, 008 Lunar hangar cut scene, 012 OUT OF THIS WORLD (25x110, Beat 5); 009 mars gym for training and 011 dojo (Beat 7); 014 FINAL BATTLE (86x33), 048 mars final battle; 013 halo trauma |
| **Mars** | 030 mars, 031 landing hangar, 033 labs, 034 clone slums, 035 city square, 036 Red bone suburb, 041 balcony party, 095 mars roof floor, 104 ghetto outskirt guarded by robots, 105 slums shopping area, 106 RollerCoin Room |
| **Moon / lunar** | 073 lunar city (28x73), 082 lunar roof, 085 LUNAR BLDG FIRST FLOOR TRADING, 089 lunas roof floor, 088 wite nite |
| **Space station Solomon (SS)** | 068 Church on ss. GODS TEMPLE (42x58), 069 Circuit board room SS, 078 ss solomon luckies YARD, 079 sss mess hall, 040 The spirit server (56x50), 042 Console plug in test space and 076 Console2 |
| **Clones and labs** | 020 clone wing, 021 clone room 2, 022 clone bathrooms, 025 killing rooms ENGINE, 015 lab in zooo |
| **Town, trade, craft** (the University's shop quarter candidates) | 027 LIBRARY (49x45), 028 finance district (55x25), 081 Item Shop (13x15), 084 Forge (21x13), 103 mech garage, 026 guest rooms |
| **Spiritual / lore rooms** | 070 HOLY IMGINATION (75x60), 050 Book of life (and death), 058 The Tree of lyfe, 024 the nothing room, 051 THE NOTHING, 044 Celestias Wing, 061 HEART GEM |
| **Houses, palaces, misc** | 004 palace ext close (night), 032 illuminiti party palace, 037 home in castle, 038/039 home 2/3, 002 Grey OP base, 007 LUKY ROOM, 018 and 093 the radius rail right/left, 102 rail way motion right, 060 hall 1, 072 room right, 075 Basic, 080 Troop Event, 083 dtest, 001 Blank page |
| **Placeholders** (default 17x13 or unnamed) | 029, 043, 062, 074, 1001 to 1004 |
**No imported map is named as a Bible level** (no Eden, Ark, Exodus, Gospel names). The closest are 070, 050, 058, 068 and 005. The Bible levels are therefore **new maps to make** (consistent with the owner: "we will be making levels"). The Soul Pen places (Lunaria, SSOL, Mars colony) are largely covered by the lunar, Mars and SS rows.

## 3. How the lore maps onto house functions (OBS unless marked; the system names are the house's)
| Lore / game idea | House function that already exists | Status |
|---|---|---|
| **Possess a Bible character** (the snake in the garden); cursword good/evil | xelector **possession**: Enter possesses a pet or Harold, `possessed_id` is the one authority, Tab/click selection follows, `9` releases, Esc-first, "lock to hero"; harness `possess_enter`, `select_follows_possession` | built, proven in a scratch house and live (Enter, Esc) |
| Characters as **pals / pieces** | entities and pets with needs (`life.pdl`), the piece identity design (`PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN`, `piece_mint`) | built / designed |
| **Loyalty-capture**, crime per piece, jobs (old notes) | none as such | designing |
| **Missions and levels** | event pages (small pal programs) edited by the blueprint viewer and `rpg-edit-hq`; maps from `piececraft-hq` | built |
| **Mana / MP, miracles, attacks** | none; `life.pdl` has hp and hunger-like needs | missing (TSC has its own Mana in `TSC_ELO`) |
| **Economy, shops, souls as currency** | coin ledger `coin.play`, `item_ledger_op`, `shop_op` (the editor coin shop, one released caller), chain/bank/fed designs | partly built / designed |
| **Game hours, day and night** | `lc_clock` and the world clock | built |
| **PvP, Elo, lobbies, seats** | `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`, `TSC_ELO`, `palnet_peer`, `irc_two_machine` | partly built; same-machine P2P; no lobby |
| **AI bots as players** | agent seats (pattern), `HARNESS-AS-GAME-BRAIN`, pets drive the same input path, free models only | designing; the judge exists with no caller |
| **Lyfebook** (the handheld computer), scripture, hacking | the house's file-based windows and the network browser are the nearest | idea only |
| **Soul Pen drawing magic, Tear It Cards** | none (the editor places tiles; no drawing-to-spell) | idea only |
| **Stairs, levels** | `Z-CELL-FORMAT` (paper), decision D12 | proposed |
| **Arena, tournaments** | the PvP infrastructure above | M9 in the roadmap |

## 4. RMMV secrets (READ: the owner's TSOTS SECRETS folder; the "secrets" are how the owner bent RPG Maker MV)
- The six core files are **modified copies loaded as plugins**: `plugins.js` lists `TSOTSrpg_core`, `_managers`, `_objects`, `_scenes`, `_sprites` (and the console plugin), so the whole `rpg_*.js` runtime is replaced by the owner's copies (`TSOTSrpg_windows.js` is also present). `FB REsume.js` is byte-identical to `TSOTSrpg_objects.js`.
- **`GAMEVARIABLES.js`** overrides `Game_Variables.prototype.setValue` (floors numbers, requests a map refresh), `Game_Interpreter.prototype.command201` (Transfer Player: direct or by variables), `command285` (Get Location Info: terrain tag, event id, tile ids, region id) and `command122` (Control Variables, including **operand case 4 = `eval()` of a script string**, an injection hazard if the string ever comes from outside). The debug `console.log` lines in it contain slurs: do not copy that text.
- **`console boot.js`** is the stock "ShowConsoleOnBoot" plugin (opens the NW.js dev console in test mode).
- **`Tsots Script calls.js`** are the first "script call" functions: read `$gamePlayer.x/y`, set game variables, read `$gameMap.events()[n]`; a `peiceChoices` idea uses `$gameMessage.setChoices` for a piece's action menu (up to 8 choices in the notes); a move-range check as the sum of x and y.
- **`tilelyfe.js`**: use **RPG Maker regions** (numbered region ids painted on tiles, up to 255 in MV: DOC) as a **meta map builder** (a region id means a tile type; if the ids run out, an array of arrays); per-tile properties (selectable, enterable, oxygen, lit, locked, height, owner); a `Tilelyfe` file with time stamps per tile for save and load.
- **`PrototypePeice.js` / `Tsots Lucky.js`** are mostly pasted engine code with a few TSOTS lines (`peicemovelimit = 5` tied to energy and hunger).
- The notes' working order for real code: "start with every tiny thing: moves, turns, hunger; one piece working; tiny plugins that can be pieced together"; and **move descriptive comments out of the plugins folder**.
- House relevance (DOC): the **import path** (RMMV maps to `map.txt`, `cells.*`, `layers.txt`, `events.txt`, `extrusion.pdl`) is the real bridge; RMMV **events** become house event pages; RMMV **variables and switches** are `state/world/switches.txt`-style files; regions are a candidate for the height and collision data (decision D12).
- **License caution:** the house renders RPG Maker MV tile art in its own engine; check the RPG Maker EULA before shipping (roadmap decision D13).

## 5. Map atlas
Visual descriptions of each map (11 viewed, the rest from data), a book/setting guess per map, and the maps still to make (Venus, Metal Sun, hydroverse, Eden, FFU, my chara, Bible levels, University districts, digipet ranch, test maps): `MAP-ATLAS-VISUALS-LORE-AND-MAPS-TO-MAKE-2026-11-05.md`. Note: map040 (spirit server) and 068 (cathedral) are the most finished lore maps; 070 is blank.

## 6. Where the long documents are
`../08-roadmap/ROADMAP-TEAR-IT-PROMISE-AND-TSOTS-2026-11-05.md` (the roadmap), `SOUL-PEN-LORE-SUMMARY-2026-11-05.md` (lore), `TSOTS-DEFINITION-AND-PLAYER-LOOP.md` (the definition), `../16.game/CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md` (the University), `../16.game/TSOTS-SECRETS-VISION-SUMMARY-2026-11-05.md` (the old design notes).
