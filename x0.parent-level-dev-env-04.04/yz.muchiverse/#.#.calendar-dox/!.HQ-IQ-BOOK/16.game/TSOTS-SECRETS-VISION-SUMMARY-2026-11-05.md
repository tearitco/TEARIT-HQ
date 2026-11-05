# TSOTS SECRETS: what the owner's old design notes say (summary, 2026-11-05)

Source (read in full by Claude on 2026-11-05, copied read-only to scratch, originals untouched): the owner's Desktop folder `~/Desktop/🤖️🪤️🏠️/xv.👨🏽‍🚀️RMMV+sec]linux<newline>🐧️]0002/TSOTS SECRETS]0003/` (the folder name contains a real newline: use `find ... -name 'TSOTS SECRETS*'`, never type the path). 54 files, 2.0 MB. Companion docs: `CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md` (what to build from this) and `Z-CELL-FORMAT.md` (stairs on paper).

**How to read this.** These are the owner's early design notes for "The Sword of the Spirit" (TSOTS), written around 2015-2019 for RPG Maker MV: part script comments, part pasted chat logs. They are a vision, not a spec and not working code. Where a note is a stated rule I write it plainly; where it is a wish or a joke I say so. Dates and names changed over time (the company is "tsots" in one copy and "chimochio" in the newer copy of the same notes: 35 vs 44 in the index below). Several notes contain crude or adult material and mentions of drugs, sex and slavery as game mechanics; this summary paraphrases neutrally and flags them in section 9 for the owner to decide what carries into the university.

## 1. What is in the folder (index)
| Files | What they are |
|---|---|
| `TsotsNotesGlobal` (2 copies, differ only in the company name) | story/setting notes: chimochio, the fox, mission arc, turns, the lyfebook |
| `Tsots Classical Peices.js` (2 near-identical copies), `PrototypePeice.js`, `Tsots Lucky.js` | the classic board game: piece list, AI brains, jobs; the last two are mostly pasted RPG Maker engine code with a few TSOTS lines |
| `Territorymode.js`, `Tsots Online mode.js`, `TSOTSlobbys .js`, `Tsots online datamanager.js`, `TsotsMarketManagers.js` | territory builder, online/expanse modes, lobbies and matching, accounts and stores, the economy |
| `SkillMethods.js`, `tsotsitems.js`, `tilelyfe.js`, `effect.js` / `tsoteffect.js`, `aibrain.js`, `TsotsGAMEMANAGERglobal.js` | skills and menus, items, tile properties, per-turn body effects, the per-piece "brain", the game-manager rules |
| `Tsots story mode.js`, `Tstos Story mode Peices.js`, `The Sword of the Spirit Game Script.js` | story beats 1-20, the mode list |
| `Tsots LyfeBook.js`, `JSPEACES LANGUAGE.js`, `Sketch book.js`, `lyfebookspiritangularclient.js`, `clientsideLYFEBOOKPROFILES...js`, `clientsidereasonreapertype...js` | the lyfebook computer, the drag-and-drop language, the drawing minigame, the social profile side |
| `mychara.txt`, `notesMyChara+-.swift` | "my chara" and the BLOCKFLIP 3D / brick notes |
| `RealGameCode` | how to start writing real code (tiny plugins, moves, turns, hunger first) |
| `TSOTSrpg_core/managers/objects/scenes/sprites/windows.js`, `FB REsume.js` (byte-identical to `rpg_objects`), `main.js`, `plugins.js`, `console boot.js`, `server.js`, `package.json`, `GAMEVARIABLES.js` | stock RPG Maker MV engine files, a chat-room server sample, a debug console plugin, and edits to variable handling. Nothing to port |
| images | the Ten Commandments tablets titled "The Sixth Commandment" (murder: the crime system's root), a friendly ghost, two logos |

## 2. The core idea: an MMO board game, played in turns
- TSOTS is "an MMO board game": everything, even online and even in 3D later, is **turns**. A player has a time bar to move; when everyone has moved or been skipped, **the game hour passes** and all independent actions run (plants grow, electricity flows, AI and clowns act, prices change, day and night turn). No real-time lag; a fast hacker out-turns a slow browser.
- Classic mode is a chess-like board: 22-piece armies, timed games (5, 10, 20, 40 minutes, an hour), chess-style rating, 1v1, 2v2, 4v4 with two boards hooked together, or a 4-player free-for-all where teammates share one army (one moves, the other advises). **The "classic board4 plys" map (map071, 42x52) is this board** (see the companion doc).
- Classic AI is deliberately "dumb over power": it does not out-think the player, it gets stronger stats by difficulty rating (like weight training), the rating is shown to the player, and the player's army is constant. A real chess-like engine is a later goal. Sol's AI incinerates clowns the moment they turn; Lucky's AI lets them turn and uses them.
- Hidden move ranges on purpose (like chess or poker); a flashlight-style reveal is allowed. Night: pieces are not visible unless close; tired 15% faster at night.

## 3. Pieces: the actors and their body
- Everything on the board is a **piece**: inanimate building blocks, animate things (plant, animal, human, spirit, undead), machines (vehicle, robot, mech). Humans have a brain (stress, hunger), a body, and a **lyfebook** (their inventory and wallet).
- The classic roster (named pieces, each with a stat line `st hp weight soul pow int loy char luck money beaut`): Lucky, Sol, Gem, Princess Stella, Smokey, Death, the queen's guard (alpha sinder), Loki, banker, captain, maid/nun, chef, merchant, king and queen of Asgard, clown, sniper clone, clone, robot, race car, CEO, hacker, lawyer, mech, animals (cacajao monkey, bat, fairy, cat, dog), "God Delusion", the Nihilator (steals the opponent's methods, like Ditto or Kirby).
- Per-turn body: stomach and hunger, waste (a waste piece radiates bad vibes and sickness until cleaners move it), stress (sleep, luxury items and "clown entertainment" relieve it), energy sets move range, sleep (3 turns) and rest, illness, bones (broken limits move), oxygen (no air: die at end of turn), vampire (blood only). Items held reduce mobility; dirty or fragile when used.
- Stats: int (learning), speed (move, evasion), lucky, strength, charisma (makes others doubt their loyalty), **loyalty** (a measure of whether a piece can be captured, bought or turned; a disloyal clone becomes a clown below ~30%), faith, beauty, mood, **crime** (kept per piece; see section 6).
- A piece can go rogue and act once per turn without costing you a move (clowns do this a lot); KO below 10% health, dead below 0%, killing may be murder (a heavy crime penalty).

## 4. Skills, jobs, magic (the skill tree the owner now wants)
- Menus are user-built: the player drags skills onto the piece's menu; skills are "methods tacked onto a piece" and a piece holds only so many (like Pokemon). Basic actions: move, attack (customizable attack methods), defend (or **preempt**: 50% chance to negate an adjacent attacker), throw (about 3 tiles), climb, skill (jobs, at most 4, each with sub-skills), spirit.
- **Jobs have levels** and are taught by teachers (the higher the teacher, the better; a new piece is bad at a new skill and it often backfires while the learned stat climbs a curve; computers, law and rockets climb slower). Jobs listed: cleaner, seducer, locksmith, cook/chef (named dishes), assembler/builder, martial artist, soldier, hacker, electrician, diplomat, teacher/professor, artist, musician, actor, captain, lawyer (then judge), clergy, healer (pharmacy), merchant, banker, engineer, warrior.
- **Robots and vehicles** get slots from parts (an arm adds throw, two arms add climb, wheels add locomotion); robots can take any job without an asterisk (the human or spiritual ones).
- "Tear it" magic: elements are wind/oxygen, fire/explosions, electricity, water, healing, nihilist, plus praise (morale) and pray (refresh, soul farming). Artists are the mage class: they draw spells with a soul pen in the lyfebook (see 7). Spirit abilities unlock from "inspiration XP" earned by talking, reading and sketching. A black "nothing ball" soul-drain power is a story reward.
- There is **no MP number** in the notes: costs are energy, stress, money, time ("wait time instead of mp", estimated before you start) and soul. The owner's new MP-drain idea is therefore new work (companion doc).

## 5. Economy: shops, farms, mines, markets
- Items form a **commodity** layer (fuel, steel, wood, brick, plastic, "redmetal" mars metal, "redcake" explosives) and a **luxury** layer (toilet, fridge, bed, mirror, weights, tuxedo, spacesuit, scuba gear, server, medicine...). Seeds grow grain, vegetable, fruit or drugs; chickens and eggs; horses are the generic cattle (milk, meat, soap byproduct); chefs combine foods (meat+bread=burger).
- A **market manager** floats every price per game hour from rarity, availability and the average player's money, starting near the board-game prices; it also prices every tile of land, keeps a pool of contracts and legal stats, and feeds a stock ticker and news. Merchants must be IN their shop for it to open; their prices vary; a shop is restocked by the distributor for a delivery fee; selling too much to the computer bank pays less and less, which pushes you to build more merchants. In classic games no real items are taken, and games can only be bet on before the start.
- Territory mode buildings: barracks, courthouse (it "actually has use"), neighborhood, clone factory, farm, bank (the banker must stay in it or its functions stop), shop, factory, library, training dummies, gas station / superstore.
- Property owners can set a **visit price** (a day pass) and sell land with everything built on it.
- Finance: loans, bank failure (a low-level banker can lose your money silently), stocks (fuel, clone, tech, food, soul), bonds, and **soul** as a valuable currency (harvested by killing; "ripping" a good piece's soul after 1000 turns costs no evil points).
- Real-money side (accounts, DLC, auctions, a "soul server" subscription, user stores) is a business plan, not a game rule.

## 6. Law, crime, courts, prison (the university's justice system)
- **Crime is held by the individual piece**; kill the piece and the crime vanishes (execution as justice). Attacking the piece that just attacked you earns only 25-50% of the normal crime points. Crime is like GTA stars, "way more".
- **Lawyers** make and block laws (drugs, alcohol, assault, murder, environment), sue, defend (self-defense lets a lawyer shoot anyone that turn without crime points), prosecute (a hacker plus a lawyer can prosecute hacking), collect fines (maybe with bodyguards), write contracts and tribute demands; lawyers can become **judges** who can heal crime points or legalize an act. Repeat offenses raise the fines.
- Territories **police themselves**: the notes say "no arrest method is good, too complicated", you may only levy fines, and there are no prisons, but you could build a place to house captured pieces (pieces are captured through loyalty). In expanse mode, guards with crime readers stand at the entrances only, with a limited patrol radius, so nobody can camp and block new arrivals. A text-based "spirit court" is imagined for disputes.
- **The owner's new vision differs in one point**: a dungeon where any entity serves time for breaking laws set by courts and enforced by officers. The old notes decided against arrests; the new plan reverses that decision, so it needs an explicit design (companion doc).

## 7. The lyfebook, art, programming
- The **lyfebook** is a handheld computer every piece or spirit needs (price joke 166.66). It stores the piece's data, inventory, wallet, maps and drawings; it must be powered. Hacking is real: a hacker can edit enemy or own variables (money, crime, loyalty) through it; **scripture** is a deliberately dumbed-down JavaScript where you buy loop iterations (limits 1, 3, 5, very expensive 10), numbers cost memory (bigger costs more, add/subtract cheaper than multiply/divide), and an infinite loop or too much power overloads and explodes your wiring.
- **Electricity and plumbing are tile layers**: wires, switches, buttons (a button works only while stood on), logic gates (NOT, AND, NAND, OR, NOR, XOR; flip-flops for high-level hackers), lasers, grounders (1-10) against blown circuits, solar and fuel generators, lights (a lit tile is visible at night), fridges (a warehouse that needs power; food spoils in 4 turns otherwise). Plumbing waters plants and cleans waste.
- **Drawing is magic**: the sketchbook scores strokes, area and time; complex drawings become collectible cards or flags that yield tax money; a hidden "AI drawer" nudges you; a drag-and-drop bubble language and a music studio are planned (music scored for harmony can do damage in battle).

## 8. Modes, places and the story
- Modes: story (20 beats from cleaning a battleship to the Mars mission, with a dojo, a gym, a second body "BRAND NEW SOUL", a coloring-book art beat, a water-deal negotiation beat, and free mode after the end), classic (local or online, rated), **arena** (pvp; a customizable arena; a pvp death can mean the piece is captured or dead in real life and must be recaptured in story mode), **territory** (build a dungeon/base, others attack it, tax, invade; up to 8 players attached), **expanse** (huge open server lands with no rules, up to 10 per instance, specialized regions: ocean, desert and mountains, outer space, the moon, Mars, the space station "ss"), **non-pvp peace mode** (cities, houses, politics, schools and campuses, hotels, sports betting, racing, clothing design, film and music), and 1-player story as "a private extension of online".
- The **dojo / arena** on the station: 1v1 to royal rumble, spectators bet at a bookie (odds from the AI balance), tap out or get knocked out, train blocking and speed, keep pieces fit.
- Peace-mode **schools, hotels, hotels as save points, houses, warehouses, rented storage, shipping** are named; the hub (hotel or station) is where saves live.
- Vertical space: tile heights make tiles climbable and cause drop damage; **from Beat 4 on levels matter: above you cannot be attacked and cannot attack, except on stairs; guns and spirit attacks can hit high and low, physical attacks cannot; planes pass through leveled tiles; no real height simulation "that is way too much work"**. Carrying a lot makes you fall; fall damage grows with tiles fallen; skates go faster; stairs climb faster with empty hands. Castles and palaces are multi-storey structures entered like ships.
- Tile properties: selectable, maneuverable, attackable, enterable (door), occupied, oxygen, lit, ground/air/ocean/space, locked (keys by random number; locksmiths), resource yield (25% plus skill), real-estate owner and value, cleanliness.

## 9. Items to review with the owner before they carry into the university
Sexual content and "sex as stress relief" mechanics; drug items and addiction; "slaves" used for auctioned strong pieces and for farm labor; a "vampire AIDS" disease idea; one note uses a slur in code. The notes are the owner's own early drafts: nothing here has been decided for the new game. This summary does not carry any of it forward.

## 10. Where each theme already exists in the house (do not rebuild)
| Theme | House doc / code |
|---|---|
| pieces as one-uid entities, Master Pal List | `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md`, `piece_mint` |
| tech trees and research pipeline | `TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md`, `CIV-RPGCE-DESIGN.md` |
| courts, contracts, bank, fed | `CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md` (same folder) |
| jail / civs / modes | `GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`, `ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md` |
| arena, village | `../08-roadmap/design-docs/PET-VILLAGE-GAME-DESIGN-2026-10-09.md`, `../08-roadmap/design-docs/DSR-WSR-PETS-BRIDGE-DESIGN.md` |
| stairs, levels | `Z-CELL-FORMAT.md` (paper), `EDIT-ISLANDS-DESIGN.md` (heights, cliffs) |
| the shop slice | `SHOP-SLICE-SPEC-2026-11-04.md` (coin shop, editor) |
