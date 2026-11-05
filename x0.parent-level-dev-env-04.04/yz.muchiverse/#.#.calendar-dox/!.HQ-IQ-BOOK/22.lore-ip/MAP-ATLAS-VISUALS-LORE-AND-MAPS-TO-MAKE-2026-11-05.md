# Map atlas: what each imported map looks like, which book it fits, and which maps still have to be made (2026-11-05)

Labels: **VIEWED** = I looked at the painted picture (`map.painted.png`) in this session; **DATA** = from the map's own files (name, size, backdrop, events); **DOC** = my guess at the lore setting, not canon. Companion docs: `TSOTS-GAME-FAMILY-AND-LEVEL-MAP-2026-11-05.md` (maps by role, house functions), `SOUL-PEN-LORE-SUMMARY-2026-11-05.md`, `../16.game/CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md`, `../08-roadmap/ROADMAP-TEAR-IT-PROMISE-AND-TSOTS-2026-11-05.md`.

## 1. How to read this, and how much I actually saw
- The 98 imported RPG Maker maps live in `44.xyz.01.00/@.apps/piececraft-hq/pieces/system/maps/tsots/mapNNN/` (`map.painted.png` is the picture, 24 px per tile; `map.txt` the glyph grid; `events.txt` one row per event with its sprite sheet; `parallax.pdl` the backdrop).
- **I viewed 11 pictures:** 006, 012, 014, 017, 027, 030, 040, 064, 068, 070 and 071. Everything else is described from its **name, size, backdrop and event sprites** only. A row says VIEWED when the visual text is from a picture. The owner has seen all of them in RPG Maker; where the owner's memory differs from this table, the owner wins.
- Names are the owner's RPG Maker names (typos kept). **XVS = the expanse maps** (owner 2026-11-05): 064 Ocean, 065 Space, 066 Mars; in the lore XVS is also a series of seven worlds with the Hydroverse atlas (`BOOK-BY-BOOK-SUMMARIES-AND-WORLD-RULES-2026-11-05.md` section 1.5). "Book" is "Book" is **SP** (the Soul Pen story), **Centroid** (the University), **Board** (game-mode and online maps), **Meta** (placeholders, tests, spiritual rooms).

## 2. Surprises worth knowing (all VIEWED or counted from DATA)
1. **Names and pictures do not always match.** map006 "RABBIT HOLE" is a desert bazaar; map014 "FINAL BATTLE" is a large Mars-style industrial city; map012 "OUT OF THIS WORLD" is a tall, almost empty sky strip (a deliberate scrolling journey: dishes and one planet).
2. **Several maps are unpainted or nearly so:** map070 "HOLY IMGINATION" is completely blank white (0 events); map027 "LIBRARY" is a floor slab and three black panels on white (17 events exist, so the logic was authored but the art was not); map065 "SPaCe expance MMO" (100x100) is almost empty by design.
3. **Event-rich maps (a game was authored):** 071 (114 events), 009 (80), 016/017/063 (77-78), 076/042 (52/47 console test spaces), 023/059 (49-50), 030 (36).
4. **Unpopulated maps:** 47 of the 98 have 0 events (counted from `events.txt`) (rooms, towns and the Mars city are scenery only).
5. **Duplicates:** 023 and 059 (SS online), 016/017/063/071 (four classic-board variants), 042 and 076 (console spaces).
6. **Strong lore matches found in the sources:** the Afghan desert maps (006, 010, 045, 067) match the first chapter of the Living Testament, "The City Beneath the Dust", where Lucky and Astra enter a desert city beyond the mountains of Afghanistan to find a weapon sale, with the warlord Kaito (READ `!.SP_AS_BIBLE.../0.LT-SP-ch1.txt`). map040 "The spirit server" fits the Avanta.C / Q00 "digital holy spirit" lore. map068 (cathedral on the station, VIEWED) is already a finished Temple-quality map.

## 3. The 98 maps
Columns: id, owner's name, size in tiles, book, setting and visual, event sprite sheets, note. All settings are DOC guesses unless the owner says otherwise.

| id | name | size | book | setting / visual | event sprites | note |
|---|---|---|---|---|---|---|
| 001 | Blank page | 38x42 | Meta | test/blank stage | People2(3) People1(1) Evil(1)  | name says blank page, cloudy-sky backdrop, 6 events |
| 002 | 1. Grey OP base | 80x27 | SP | Grey OP base (military/black-ops base) | People2(1) Evil(1)  | big flat base map, 2 events |
| 003 | Unlukky vision | 51x47 | SP | "Unlucky vision" (a dream/vision of Lucky; 14 monster events) | Monster(14) Actor2(1)  | dark-space backdrop |
| 004 | march 2 the palace ext close (night) | 50x35 | SP | palace exterior at night | People4(5)  | 5 villager events |
| 005 | God mini game lvl | 25x26 | SP | "God mini game" level (crystal + two big monsters) | !Crystal(1) $BigMonster2(1) $BigMonster1(1)  | dark-space backdrop; Bible-adjacent |
| 006 | RABBIT HOLE | 91x36 | SP | Afghan desert bazaar town (VIEWED: sandy ground, brown plank buildings, ladders, produce stall, grey stone paths, bare sand east; NOT a rabbit hole despite the name) | Evil(2)  | matches Living Testament ch.1 "city beneath the dust" |
| 007 | LUKY ROOM | 33x27 | SP | Lucky's room | !Door2(1)  | starlit window backdrop |
| 008 | Lunar hangar cut scene | 74x38 | SP | Lunar hangar cut scene | People2(1)  | starlit backdrop, 1 event |
| 009 | mars gym for training | 42x53 | SP | Mars gym / training hall with 80 events (8 chests) | People1(8) !Chest(8) Actor3(6)  | mountains backdrop |
| 010 | empty afgani desert pan walkie talkie | 85x25 | SP | empty Afghan desert (walkie-talkie scene) | People4(3) Vehicle(1) Evil(1)  | 6 events incl. a vehicle |
| 011 | dojo | 44x30 | SP | dojo (8 trainee actors) | Actor1(8) People2(1) Actor3(1)  | Footrace Fu / ninja flavour is a guess |
| 012 | OUT OF THIS WORLD | 25x110 | SP | "Out of this world": a tall empty white sky column with ~14 satellite dishes, sparkles, a lone tower at the top and a small planet at the bottom (VIEWED; 600x2640 px) |  | a scrolling space-journey strip; 0 events |
| 013 | halo trauma | 43x22 | SP | "Halo trauma" scene | Actor2(1) Actor1(1)  | sea-of-clouds backdrop |
| 014 | FINAL BATTLE | 86x33 | SP | "Final battle": a large Mars-style industrial city/base (VIEWED: domes, solar panels, windmills, grey roads, tanks, a blue harbour at the south, lava ground and grey rock below, red walls around) | !Flame(4)  | reads as a Mars colony city, not a battlefield; 4 flame events |
| 015 | lab in zooo | 23x22 | SP | lab in the zoo (3 monsters) | Monster(3)  |  |
| 016 | classic board MARS VERSION | 42x53 | Board | classic board, Mars version | !Other1(7) People1(6) Actor1(6)  | 78 events |
| 017 | classic board | 42x52 | Board | classic board in a station (VIEWED: a casino/shop strip with neon signs along the top, a big pale-yellow square arena floor with a hazard-stripe line, black voids at the sides, yellow corridors on the right, a lower control room) | !Other1(7) People2(6) People1(6)  | 78 events; probably the "board in the SSOL station" |
| 018 | the radius rail right | 22x19 | SP | the radius rail (right) |  | rail, starlit |
| 019 | Tearitory mode - small | 22x16 | Board | Tearitory mode, small |  | 0 events |
| 020 | clone wing | 17x20 | SP | clone wing |  | 17x20 |
| 021 | clone room 2 | 17x13 | SP | clone room 2 |  |  |
| 022 | clone bathrooms | 17x13 | SP | clone bathrooms |  |  |
| 023 | SS ONLINE | 55x55 | Board | SS online (space-station hub, 49 events, 10 chests) | !Chest(10) Actor1(7) People3(6)  | duplicate of 059 |
| 024 | the nothing room | 35x28 | SP | the nothing room |  | Nihilator lore |
| 025 | killing rooms ENGINE | 33x24 | SP | killing rooms engine (clone souls power the engine) |  | sunset backdrop |
| 026 | guest rooms | 26x28 | Centroid | guest rooms | People4(1)  |  |
| 027 | LIBRARY | 49x45 | Centroid | library (VIEWED: mostly unpainted; one wooden floor slab with three black panels and a black block; white void) | People4(4) People2(4) Nature(4)  | 17 events exist; the picture is nearly empty, the layout is unfinished or the layers did not paint |
| 028 | finance district | 55x25 | Centroid | finance district (shops quarter candidate; Soul Bank?) |  | 0 events |
| 029 | MAP029 | 17x13 | Meta | placeholder |  |  |
| 030 | mars | 55x29 | SP | Mars surface (VIEWED: orange rocky terrain, lava/red rubble patch, rock heaps, a metal landing pad with a T mark and arrow strips, tilled-looking rows; 36 "other" events) | !Other1(36)  |  |
| 031 | mars city LANDING HANGAR | 32x20 | SP | Mars city landing hangar |  |  |
| 032 | illuminiti party palace | 43x28 | SP | illuminati party palace | People3(2) !Flame(1)  |  |
| 033 | mars labs | 33x18 | SP | Mars labs |  |  |
| 034 | mars clone slums | 49x25 | SP | Mars clone slums |  |  |
| 035 | mars city square | 49x42 | SP | Mars city square |  |  |
| 036 | Red bone suburb | 33x28 | SP | Red bone suburb |  |  |
| 037 | home in castle | 49x43 | SP | home in a castle | !Flame(5) People3(1)  | 6 events |
| 038 | home 2 | 17x13 | SP | home 2 |  | placeholder size |
| 039 | home 3 | 17x13 | SP | home 3 |  | placeholder size |
| 040 | The spirit server | 56x50 | SP | "The spirit server" (VIEWED: a grey-grating lab/server room with consoles, 11 glass cryo-or-server pods and a big green panel; the rest is an enormous glowing cyan-dot grid like a digital void, with unpainted white areas) | Vehicle(1) People4(1) People2(1)  | cyberspace/afterlife server; strong Avanta.C / Q00 fit |
| 041 | balcony mars party | 24x21 | SP | balcony at the Mars party | !Flame(2) Monster(1)  |  |
| 042 | Console plug in test space | 40x44 | SP | console plug-in test space (47 events) | People2(10) Evil(8) People1(6)  | test space |
| 043 | MAP043 | 17x13 | Meta | placeholder |  |  |
| 044 | Celestias Wing | 34x39 | Centroid | Celestia's wing |  | sunset backdrop |
| 045 | AFGANI TOWN | 56x32 | SP | Afghan town | Vehicle(3)  | 3 vehicle events |
| 046 | Rabbit hole caves | 77x58 | SP | rabbit-hole caves |  | 77x58, 0 events |
| 047 | launch plataue | 49x29 | SP | launch plateau |  | sunset |
| 048 | mars final battle | 17x13 | SP | Mars final battle (small) |  | placeholder size |
| 049 | inside saucer | 23x20 | SP | inside a saucer |  |  |
| 050 | Book of life(and death) | 26x17 | Meta | Book of Life (and Death) room | !Flame(1)  | Bible-adjacent |
| 051 | THE NOTHING | 17x13 | Meta | "The Nothing" |  |  |
| 052 | CLASSIC MODE | 17x13 | Board | CLASSIC MODE menu room |  | small |
| 053 | ONLINE APP | 17x13 | Board | ONLINE APP menu room |  | small |
| 054 | Territory med | 24x22 | Board | territory mode, medium |  |  |
| 055 | territory mode large | 30x26 | Board | territory mode, large |  |  |
| 056 | XL DLC ONLY | 48x30 | Board | XL DLC only (territory) |  |  |
| 057 | ONLINE LOBBY (S) | 24x25 | Board | ONLINE LOBBY (small) |  | a lobby map for the rated-PvP plan |
| 058 | The Tree of lyfe | 17x13 | Meta | The Tree of Life (small, 2 flame events) | !Flame(2)  | Bible/Eden-adjacent |
| 059 | SS ONLINE | 55x55 | Board | SS online (same as 023) | !Chest(10) Actor1(7) People3(6)  | duplicate |
| 060 | hall 1 | 38x13 | Centroid | hall 1 (a long corridor) |  |  |
| 061 | HEART GEM | 23x29 | Centroid | Heart Gem |  |  |
| 062 | MAP062 | 17x13 | Meta | placeholder (ocean backdrop) | People2(1) Monster(1)  |  |
| 063 | Board game version | 42x52 | Board | board game version | !Other1(7) People2(6) People1(6)  | 77 events |
| 064 | Ocean Expanse MMO | 80x70 | Board | Ocean expanse MMO (VIEWED: a huge blue sea with sand coasts west/north/east, a few islets, a diagonal line of rocks, a whirlpool patch, a cloud strip at the top and waves along the bottom/right edges) | Vehicle(5) !Flame(1)  | a natural boat proving ground |
| 065 | SPaCe expance MMO | 100x100 | Board | space expanse MMO (100x100, nearly empty) | !Flame(2)  |  |
| 066 | Mars xpans MMO | 66x66 | Board | Mars expanse MMO | !Door1(3)  |  |
| 067 | afgan hills | 120x38 | SP | Afghan hills (120x38) | !Flame(2)  |  |
| 068 | Church on ss.GODS TEMPLE | 42x58 | SP | God's temple: cathedral on the station (VIEWED: tall hall with an orange wooden floor, a pink-paved plaza, blue star-patterned glass tiles at the top, stained-glass windows, statues, a grey central path) | !Flame(2) !Door2(1)  | a Bible-level-quality map, reusable as a Temple |
| 069 | Circuit board room SS | 34x25 | SP | circuit-board room on the station | !Chest(1)  |  |
| 070 | HOLY IMGINATION | 75x60 | Meta | HOLY IMAGINATION (VIEWED: completely blank white, 75x60, 0 events) |  | an empty canvas waiting to be painted |
| 071 | classic board4 plys | 42x52 | Board | classic board, 4 players (VIEWED earlier: station board; 114 events) | People2(14) Actor3(12) Monster(10)  | the chimochio 4x4 |
| 072 | room right | 33x25 | SP | room right |  |  |
| 073 | lunar city | 28x73 | SP | lunar city (tall 28x73) | !Door1(1)  |  |
| 074 | MAP074 | 17x13 | Meta | placeholder |  |  |
| 075 | Basic | 18x16 | Meta | Basic (small test) | Monster(2) !Chest(1)  |  |
| 076 | Console2 | 40x44 | SP | Console 2 test space (52 events) | People2(10) Evil(8) People1(6)  | duplicate-ish of 042 |
| 077 | unlucky vision 2 | 49x26 | SP | unlucky vision 2 | Evil(3) !Door1(1)  |  |
| 078 | ss solomon luckies YARD | 38x25 | SP | Solomon / Lucky's yard on the station |  |  |
| 079 | sss mess hall | 47x28 | SP | mess hall on the station | !Flame(1) Actor3(1) Actor1(1)  |  |
| 080 | Troop Event | 19x25 | Meta | troop event room | People3(1) Actor1(1)  |  |
| 081 | Item Shop | 13x15 | Centroid | item shop | Actor1(3)  | 3 clerks |
| 082 | lunar roof | 36x33 | SP | lunar roof |  |  |
| 083 | dtest | 17x13 | Meta | "dtest" |  |  |
| 084 | Forge | 21x13 | Centroid | forge (4 chests) | !Chest(4) Actor3(1)  |  |
| 085 | LUNAR BLDG FIRST FLOOR TRAIDING | 49x33 | SP | lunar building first floor, trading | !Crystal(2) Actor3(1)  |  |
| 088 | wite nite | 31x13 | SP | "white night" |  |  |
| 089 | lunas roof floor | 45x34 | SP | lunar roof floor |  |  |
| 093 | the radius rail left | 22x22 | SP | the radius rail (left) |  |  |
| 095 | mars roof floor | 36x34 | SP | Mars roof floor |  |  |
| 1001 | Map 1001 | 18x16 | Meta | map 1001 (small test: 2 monsters, chest) | Monster(2) !Chest(1)  |  |
| 1002 | Map 1002 | 19x25 | Meta | map 1002 | People3(1) Actor1(1)  |  |
| 1003 | Map 1003 | 17x13 | Meta | map 1003 |  |  |
| 1004 | Map 1004 | 17x13 | Meta | map 1004 | Actor1(4) Monster(1)  |  |
| 102 | rail way motion right | 49x24 | SP | rail-way motion (right) |  |  |
| 103 | mech garage | 39x21 | Centroid | mech garage | People4(1)  |  |
| 104 | ghettto outskirt guarded by robots | 53x24 | SP | slum outskirts guarded by robots |  |  |
| 105 | slums shopping area | 25x22 | SP | slums shopping area |  |  |
| 106 | RollerCoin Room | 17x13 | Meta | RollerCoin room |  |  |

## 4. Maps that still have to be made
Existing maps cover **Soul Pen (Afghanistan, Mars, the Moon, the station), the Centroid shop/school fragments and the classic boards**. Everything below is **new**. Owner-named items are marked SAID; the rest are DOC proposals tied to a lore or roadmap need. Order is a suggestion; each map is made with the editor and house event pages, and (where it is a test space) gets harness lanes (`../16.game/GYMS-ZOOS-MUSEUMS-IN-GAME-DOCS-2026-11-05.md`).

### 4.1 Lore worlds (SAID by the owner unless marked)
| Map to make | Book | What it is for | Notes |
|---|---|---|---|
| **Venus** | SP DLC (book 6.7, Venus Fly Trap Phonk) | the Floating Outpost Alpha-Zeta "The Slag Drip" (48.2 km up, upper-sulfur deck, acid clouds, floating luxury cities above), the toxic surface hellscape, the mother Venus fly trap (Garden Mind) cave, crash site of the luxury transport, Metal Mafia raid zones | SOURCE FOUND 2026-11-05 (`VENUS.FLY.TRAP.Ph&U]DLC`: 50-page CYOA, 15 beats). Not a Lucky/Sol book; clone-survival Rimworld style; see `SOUL-PEN-LORE-SUMMARY-2026-11-05.md` |
| **Metal Sun / Icarus** | SP (pillar 10 and Superwar Ikkarus) | the Architect's ship-star; Eden lives inside it per the owner's theology | interior decks, bridge, engine core |
| **Hydroverse (XVS)** | SP / XVS | water worlds of the XVS expanse, navigated by [H hydrogen density, T thermal variance, P pressure]: Puffara-Kai (puffer-folk shield world), Venom-Spire (lion-fish aristocratic ice), Carchar-Industrial (shark-kin forge); the Angler Empire and the Unlit Tribe fight there | SOURCE FOUND: `101.XVS++[FLEXOPEDAI]00.05/XVS_PLANETS_ATLAS/HYDROVERSE_EXPANSION_ATLAS.md`; the XVS series has seven worlds (Aeterna Medusa, Detritus Prime, Crabfold, Nearth, Dark, Fungi, Lux). map064 Ocean Expanse is the nearest existing map |
| **Eden / Edin** | SP + Bible | the Garden: Adam and Eve, the snake (the first possession level in `#.wussup+@o.txt`), the Tree of Life (map058 is a tiny stub) | first Bible level candidate |
| **Footrace Fu / Land of Time (FFU)** | SP pillar 12 | ninja racing: night, alien, gold/red architecture, cherry blossoms, time crystals; track and chutes-and-ladders boards | the FFU rules exist (`+.TSOTS-ALPHA-OMEGA/#.RULES`); the dojo map011 is the nearest piece |
| **"my chara" home** (SAID) | SP pillar 0 (charanon) | the player's own character space/home | I assume this relates to the `my-chara-txt` app family: confirm |
| Angler Empyre | SP pillar 14 | the Angler Empire (deep-sea/space predators) | named in the lore; map not requested yet |
| **Mars: Circus Ville slums, HSG compound, The Temple** | SP (Red White & You zine) | the clone-slum and security map from the zine; map034 (Mars clone slums) and map104/105 are the nearest | from `Red_White_and_You.pdf` page 3 |
| **SS.Sol station: Engineering, Cargo Bays, Ratchet's Realm** | SP (zine) | ship-station decks; maps 068/069/078/079 are the nearest | "Don't leave the ship" |
| Atlantis Prime, Avanta.C, Glitch World, Planet X | SP | places named in the lore | map040 is the seed of Avanta.C |
| Tempt Island, Dustopedia, Muchi-pets village | SP pillars 16/18/17 | pet and micro-world levels | Dustopia is already a house design (`RPG-DUST-DESIGN`) |

### 4.2 Bible levels (TSOTS proper; owner answer pending on scope and order)
Garden of Eden, Noah's Ark and the Flood, Babel, the Exodus/Red Sea/Sinai, Jericho, a Temple (map068 can serve as the base), Bethlehem/Nazareth/Galilee (a lake: reuse the boat proving ground), Gethsemane/Calvary/the garden tomb, Patmos and the New Jerusalem (the bridge to the Soul Pen, which picks up after Revelation). Each needs its characters listed as capturable pals with an alignment (good/evil, "cursword").

### 4.3 Centroid University (see the vision doc)
The campus is on the 42x52 board of map071. Needed as **separate maps or districts**: Spiral Alley and the shop quarter (rename "Diagonal Alley" first), the Soul Bank, the library (map027 has the logic, needs the art), schools/classrooms, hotel, courthouse, **dungeon**, **arena** (rated PvP), a market square with physical shops, farm/mine/smith supply sites, and **stairs** (decision D12). The old maps 028 (finance), 081 (item shop), 084 (forge), 103 (mech garage), 026 (guest rooms), 060 (hall) are the existing pieces.

### 4.4 Digipet ranch (SAID 2026-11-05, see section 5)
A monster-rancher/digipet ranch and a **1v1 "mini 4x4" board** for digipets (a smaller map than map071), plus a **Bible school/study room** where digipets learn verses.

### 4.5 Engineering and test maps (roadmap M11 and M12)
Vacuum range, test basin (map064 is a ready-made open sea to start from), test track, airfield, mecha yard, weigh station (`../16.game/VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md`); the Concourse zoo/gym/museum (`../16.game/GYMS-ZOOS-MUSEUMS-IN-GAME-DOCS-2026-11-05.md`).

### 4.6 Paint what already exists (cheapest first)
map070 "Holy Imagination" (blank), map027 library (art), map065 space expanse (nearly empty), map012 (decide whether the near-empty strip is finished), the placeholders 029/043/062/074 and 1001-1004, and the unpopulated towns (events).

## 5. Open questions for the owner
1. Venus is answered by the Venus Fly Trap Phonk DLC (confirm it is the Venus you meant). The hydroverse is answered by the XVS atlas; where does the DLC (6.7) sit in the book order? 2. Is "my chara" the `my-chara` app's character home? 3. Which maps are Bible levels versus Soul Pen levels (the table is a first guess)? 4. Should map070 "Holy Imagination" become the lore/Concourse "museum" hall? 5. Do 023 and 059 get merged?

## 6. Owner answers 2026-11-05 (canon proposals adopted)
- **Soul Pen maps:** the Afghan, Mars, Moon and station maps. **Bible maps:** **068 = the Temple**, **058 = Eden's Tree**, **064 = the Ark and Galilee** (open sea), **012 = Babel or Jacob's ladder** (the tall sky strip). **070 "Holy Imagination" becomes the Concourse museum hall** (see the gyms/zoos/museums doc).
- **Merge 023 and 059** (SS online): keep one.
- **Pillars:** 15 now has a source (rb&u); 16 Tempt Island = a planned demo world; 17 Muchi-pets = the digipet ranch (section 4.4).
- **"my chara" home** = the player avatar's (cursword's) home space.
- **Map viewer: yes**, build on the existing gallery (`XO/31.Nov/nov-5/0.tsots-map-gallery.html`).
- Venus and the Hydroverse are answered by their sources (Venus DLC 6.7 as a "meanwhile" story during Hellride; Hydroverse is part of XVS). Section 5 questions 1 to 5 are therefore answered; the open ones left are which maps still need art and the Bible-level list per section 4.2 (order now fixed in `TSOTS-DEFINITION-AND-PLAYER-LOOP.md` section 6).
