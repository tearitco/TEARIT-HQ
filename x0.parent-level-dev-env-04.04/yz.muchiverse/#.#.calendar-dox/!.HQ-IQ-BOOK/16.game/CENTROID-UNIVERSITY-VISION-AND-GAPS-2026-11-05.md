# Centroid University: the owner's vision, what exists, what is missing (2026-11-05)

> **Read first (owner definition, 2026-11-05): "TSOTS" means playing through the levels of the Bible, doing missions, and capturing Bible characters with the main character "cursword" to use as pals in battle and trade; Soul Pen lore picks up after Revelation, and the story mode uses most of the imported maps.** See `../22.lore-ip/TSOTS-DEFINITION-AND-PLAYER-LOOP.md` and `../22.lore-ip/SOUL-PEN-LORE-SUMMARY-2026-11-05.md`. The notes summarized below are the owner's earlier (2015-2019) game-design drafts and describe a different, board-game framing; where they conflict, the definition above wins.

Status: **vision and gap analysis. Nothing here is built or decided unless marked BUILT.** Labels: SAID = the owner said it on 2026-11-05; NOTES = the owner's old TSOTS notes say it (`TSOTS-SECRETS-VISION-SUMMARY-2026-11-05.md`, section numbers in brackets); OBS = observed in the repo this session; DOC = reasoned, not run. The target board is the **chimochio 4x4 "classic board4 plys"**, already imported as `map071` (42x52) in the TSOTS map set; a gallery of all 98 imported maps was exported for the owner to `XO/31.Nov/nov-5/0.tsots-map-gallery.html`.

## 1. What the owner wants (SAID, 2026-11-05)
1. Get as close as possible to building the **Centroid University**, a place with:
   - **Stairs and inclines**: characters walk up them like stairs, **without a jump**, while **collision stays in force**.
   - **Attack choices and animations**, and **magic with MP drain**.
   - **A skill tree / common tech tree for all entities and items** (weapons, armor).
   - **Physical shops**, not teleport shops: real buildings with physical inventory (or order-and-deliver). They buy and sell from **local farms, mines, smiths** etc.
   - **Hotels** and **schools** (you pay to learn skills, magic or tech).
   - **A dungeon** where any entity serves time when it breaks **laws set by the courts and enforced by officers**.
   - **An arena** for pvp between players and for official prize tournaments.
   - **Party battles**: party entities can be controlled, auto-fight, or hybrid.
2. Ultimately the **chimochio 4x4 board** (the 42x52 map, 4 players) playable.
3. A **map preview** to see the imported maps, and help finding the RPG Maker maps.

## 2. Where the RPG Maker maps are (answered)
OBS: no `Map###.json` exists in the repo. The maps were imported into **`44.xyz.01.00/@.apps/piececraft-hq/pieces/system/maps/tsots/`**: 98 folders `map001`..`map098`, each with `map.txt` (glyph grid), `cells.txt`/`cells.rgba`/`cells.png` (tile atlas), `layers.txt`, `events.txt`, `ev/`, `extrusion.pdl` (heights), `parallax.pdl`, `map.painted.png` (the picture; `map.png` is a link to it) and `bar.txt`. `game.pdl` lists them (`desk_71_id | map071`). `map071` META note: `classic board4 plys`; its painted picture is 1008x1248 px for 42x52 tiles of 24 px; `EXTRUDE W 3` (walls 3 high). The painted image shows the tiles only, not the pieces (the rows of faces in the owner's RPG Maker screenshot are events). The preview now exists as the gallery above; a viewer inside the house is a separate task (section 6).

## 3. Stairs and inclines: researched? (answered)
**Documented on paper, not built, and the exact requirement was not yet researched.**
- `Z-CELL-FORMAT.md` v1.1 (design, 2026-10-13): real z-levels, cells flagged `stair`, links `ZLINK | world | z | x | y | kind | to_z | to_x | to_y` with kinds `up`, `down`, `ladder` and **`ramp`** (x,y may move by at most 1 cell; z changes by exactly 1; both ends must be stair cells). Its own status line: nothing in any game reads it yet, no game loads it until the owner and grok approve v1.1.
- `EDIT-ISLANDS-DESIGN.md`: the only running height behavior: floor heights per glyph (`G` grass height 1, bridges height 1, cliffs from extruding cells), a wooden ramp as decor only.
- OBS: a code search of the game core and the editor skin found height only as **visual extrusion** (`extrusion.pdl`, `terrain_legend` height column) and `pos_z` for engine pieces in the air; I found **no rule that makes walking depend on height**. DOC: so inclines with collision need a rule that does not exist yet. (Not run: a live test of walking over an extruded cell.)
- NOTES [8]: from story Beat 4 "levels matter": above you cannot be attacked or attack except on stairs; guns and spirit attacks hit high and low, physical attacks do not; planes pass through; no real height simulation (too much work). The tile notes add: height makes a tile climbable, drop damage grows with tiles fallen, empty hands climb stairs faster.

### The design choice (the standing test, run now)
Question: how should "walk up an incline smoothly, no jump, with collision" work?
| Option | One authority per fact | Modular / scalable | Long term | House shape | Cost / risk |
|---|---|---|---|---|---|
| **A. Height per cell + step rule**: each cell has a height (the value the renderer already extrudes); a move is allowed if `abs(h_to - h_from) <= step_max`; stairs and ramps are runs of cells whose heights differ by at most `step_max`; the renderer interpolates the displayed z during the move; attacks compare heights (the notes' "above cannot be attacked except on stairs") | yes: ONE height field drives drawing, walking and attack | data in `.pdl`; works for every board and entity | cannot stack two floors on the same x,y | `.pdl` data + one shared rule in the core, harness-testable | small; touches the move rule once |
| **B. Discrete z-levels with ZLINKs** (the paper Z-CELL design) | yes, links name the transitions | scales to buildings with floors | exactly what the roadmap names | `.pdl` + a loader, harness `zcell_check` already exists | larger: a loader, per-level maps, camera; a ramp "moves z by exactly 1" which is a step, so a smooth look is still a renderer job |
| **C. Scripted slide (event page transfers the character along the stairs)** | no: the rule lives in each page | no | no | event pages | cheap, and it is the "jump" the owner does not want |
**Recommendation (DOC, owner decides):** start with **A** for the university's first ground-floor outdoors and stair-run buildings, and keep B for true stacked floors later; A's height field is the same value B would assign per level, so A does not block B. Deciding reason: one height value already exists in the data and in the 3D look, and a single move rule over it gives smooth stairs, collision (walls are simply cells higher than `step_max`) and the notes' attack rule with no new object type. To log: `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` as D12 **proposed, not built**.
First proof to build when approved: a harness case with a scratch board of one staircase: walk up (allowed, no jump frame), a cliff of 2 (blocked), diagonal across a step, a piece above vs below attacking, and the 2D and 3D projections agreeing (the standing "3D must have 2D parity" rule).

## 4. Gap table: the rest of the vision against what exists
| Wish | NOTES say | House has (OBS) | Gap |
|---|---|---|---|
| Attack choices and animations | attack = customizable placeable attack methods, defend/preempt, throw, climb; classic AI weighs weaknesses [3,4] | pets and the editor have verbs and the 3D engine draws pieces; no attack-choice menu or animation set was found | an **attack verb set + animation hook** (data rows), per entity |
| Magic with MP drain | no MP: costs are energy, stress, money, time, soul [4]; tear-it elements list | `life.pdl` (hp, hunger-like needs, death), coin ledger `coin.play` | a **mana/energy resource** per entity, a drain rule, spell rows; the owner's MP idea is new (needs a decision: reuse the notes' energy or add MP) |
| Skill tree / tech tree for all entities and items | jobs with levels, teachers, robot parts add slots, items have categories [4] | `TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md`, `CIV-RPGCE-DESIGN.md` (docs); entity learner design (grades by exams; memory `project-entity-learner-and-knowledge-domains`) | one **tree data format** shared by entities and items (weapons, armor); not built |
| Physical shops with physical inventory, buying from farms, mines, smiths | merchant must be inside; stock from the distributor; prices float per game hour [5] | the **coin shop** (one released caller, `SHOP-SLICE-SPEC-2026-11-04.md`, `item_ledger_op`, `shop_op`) is a ledger shop in the editor; farms/mines are ledger ideas | a shop **building** with stock as real piece items, supplier links (a farm piece produces, a smith crafts, a courier delivers), the order-and-deliver flow |
| Hotels | hotel as save hub; peace-mode houses [8] | none | a hotel building: rent a room, rest = energy recovery, save point |
| Schools: pay to learn skills, magic or tech | teachers teach jobs; learning starts bad and backfires, curve by subject [4] | entity learner design only | **school building + tuition + lessons that raise a skill along its curve** |
| Courts, laws, officers | crime per piece, lawyers, judges, territory police, fines; no prisons in the old notes [6] | contracts/bank/fed design; no law engine | **law table (data), crime ledger per entity, officer behavior, court procedure, sentences** |
| Dungeon (imprisonment) | the old notes say no arrest; the vision reverses it [6] | a dungeon-builder idea for territories [8] | a **jail zone + sentence timer + release**; must say what an officer may do and how a sentence ends |
| Arena, pvp, official tournaments | arena pvp, dojo with a bookie and spectators, rated classic matches, timed matches [2,8] | none running | arena board + entry, match rules, prize pool, spectators/bets (later) |
| Party battles: controlled, auto, hybrid | a classic AI exists as a design (brains: board scan, logistics, diplomacy, finance, legal; weighted play bank) [2] | the pets' autonomy loop and `fire` verbs, possession (Enter, Esc-first, Tab-select) | a **party roster + per-member control mode (manual / auto / hybrid)** and an AI policy format |
| The 4x4 board playable | 22-piece armies, hidden ranges, two boards hooked [2] | map071 imported; board-viewer renders imported maps | pieces placed from the map's events, turn order, the classic rules |

## 5. Suggested order (DOC: for the owner to confirm, smallest provable steps first)
1. **Map71 in view**: open `map071` in the board viewer and confirm it renders with its heights (proves the import, 1 session).
2. **Stairs (option A)** with the harness case above.
3. **Entity resources and the attack/magic verb set**: energy/MP, drain, 3 attacks, 1 spell, with animations as data.
4. **Skill tree format** (data) + one school lesson.
5. **Physical shop slice** (one shop building, one supplier, one delivery).
6. **Law slice**: 3 laws, crime ledger, an officer, a fine, then the dungeon sentence.
7. **Arena** board + party control modes.
Each step gets a harness case that can fail, a doc update here, and a decision-log entry (the design-choice test).

## 6. Questions for the owner (answer when convenient; none blocks the docs)
1. Stairs: is option A (height per cell + step rule) the direction, with true stacked floors later?
2. MP: keep the notes' "energy/stress/time/soul" costs, or add a numeric MP per entity?
3. Justice: which punishments does a court have (fine, dungeon time, exile, execution), and may the **player** be arrested, or only NPCs and AI?
4. Shops: should stock be real items on the shelf that a thief could take (physical), or a ledger the shop owns?
5. Map preview: is the HTML gallery enough, or do you want a viewer window (pan/zoom, heights) as a house tool?
6. Which notes carry into the university (section 9 of the summary lists the ones I did not carry)?

**Map atlas (2026-11-05):** `../22.lore-ip/MAP-ATLAS-VISUALS-LORE-AND-MAPS-TO-MAKE-2026-11-05.md` describes each imported map, says which book it fits (Soul Pen or Centroid), and lists the University districts that still have to be made.
