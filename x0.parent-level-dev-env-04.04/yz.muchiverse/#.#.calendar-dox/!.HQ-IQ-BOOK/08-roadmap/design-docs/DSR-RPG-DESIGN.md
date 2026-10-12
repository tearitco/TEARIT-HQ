# dsr-rpg: a carbon copy of rpg-pet turned into a two-city world (design, 2026-10-11)

Status: DESIGN. Nothing is built. Written by claude from the owner's words and from reading the real rpg-pet files (read-only). Builds on `DSR-WSR-PETS-BRIDGE-DESIGN.md` (the DSR desk of 16 building entities, link registry, one authority per fact) and on `@.apps/rpg-pet/TWO-CODEBASES.md` (the old window vs the pc-hq board window vs the one daemon).

## 1. The owner's ask (2026-10-11, verbatim in spirit)
- A **carbon copy** of rpg-pet, then turned into something else, called **`dsr-rpg`**.
- The six pets become the **main characters: soldier, chef, clown, king, queen, fairy**, and "change their db entries also".
- **More entities: chickens, horses, pigs.**
- **Many maps: 16 = 2 cities of 8** (per city: 1 castle, 2 banks, 3 stores, 1 school, 1 hotel), plus **one main map** like the "dsr-dev screen" with the **2 cities side by side**. All the buildings of a city are on ONE screen and have **doors that lead to their inside room**, like the rpg-pet board rooms. The difference from rpg-pet: the inside rooms are all connected by one **outside open area, "the city"**.
- Owner asks: "do you need to make a design doc first?" Answer: yes, this doc. It ends with open questions that have a recommended answer each.

## 2. What exists today (read, not assumed)
- rpg-pet = one C program `ops/rpg_pet.c` (~1000 lines: verb mode, resident daemon, scene painter) + data in `.pdl` files (`party.pdl`, `rooms.pdl`, `catalog.pdl`, `voices.pdl`, `time.pdl`, `toy.pdl`, `keybinds.pdl`, `tuning.pdl`) + events-hq pages (`events/`: play, stop, int_on, int_off, greet_<pet>, reply_<pet>, door_<room>_<n>, phase_*) + two front ends (old window `rpg-pet.xhtpm`, pc-hq board window `board/pchq-board.xhtpm`). Cast: a trainer/hero (Harold) and six monsters (Blip, Fang, Gruk, Draxa, Gargo, Bonz); rooms: bedroom, living room, outside (3 rooms; the Shop is a menu, not a room)
- **Doors today** are already `.pdl` rows in `rooms.pdl` (`DOOR | room | tx | dest | ax | ay | autow`, `Door {room, tx, dest, ax, ay, autow}` in C): a door sits on the wall row of a 13x8 room at column `tx`; walking into it moves you to room `dest` at `(ax, ay)`; outdoor rooms draw a path strip from the door down. So **a door is always a top-wall tile of a 13x8 room**. A city of 16 buildings needs doors anywhere on a larger map (section 6).
- **Scene size**: frame 660x430, grid 13x8, tile 48 px (`FW FH COLS ROWS T` in `rpg_pet.c`). The board's canvas box measured on the live window is about 624x384 (the frame is cropped to it); the 3D atlas already uses 24 px tiles (the 48 px sheets downscaled).
- **The DSR page today**: no `dsr` book on the pc-hq board; the "DSR page" is a desk of 16 free-floating pals in the owner's livedesk (`dsr_<kind>_<id>`: castle a/b, bank a1..b2, store a1..b4, population a/b). That is the "dsr-dev screen": two clusters side by side. **Live user data: read-only, never written** (house rule, xyzfs/users).
- **The dsr-dev desk (read-only look, 2026-10-12) already matches the owner's list.** Its icons: castle `🏰`, bank `🏦`, store `🏪`, **school `🏫`** (the entity named `store_a4` / `store_b4`), **hotel `🏨`** (the entity named `population_a` / `population_b`). So per city: castle, 2 banks, 3 stores, 1 school, 1 hotel = 8, and 16 in all; only the entity NAMES are older than the icons. The layout is a clean grid, x/80 = column, y/80 = row (pixels in the desk file):
  | | col 10 / 20 | col 11 / 21 | col 12 / 22 | col 13 / 23 |
  |---|---|---|---|---|
  | **row 1** | castle a / b | bank 1 | bank 2 | store 1 |
  | **row 3** | store 2 | store 3 | school (`store_x4`) | hotel (`population_x`) |
  City A is columns 10-13, city B columns 20-23: **six empty columns (14-19) lie between them**, and row 2 and everything below row 3 is empty. That open space is the "city" the owner wants kept free for more buildings.

## 3. Principle: copy, then diverge; do not link
House rule (memory `no header-link split`, `text-includes-not-the-standard`): no shared header + link between the two games. So **dsr-rpg is a real copy**: its own folder, its own binary built from its own copy of the C file, its own state. rpg-pet is not touched. The cost of that choice is drift (a fix in one is not in the other); the mitigation is that numbers live in `.pdl` (`tuning.pdl`) and the two-front-end doc exists. Do not "improve" rpg-pet while copying.

## 4. Naming and layout of the copy
| rpg-pet | dsr-rpg |
|---|---|
| `@.apps/rpg-pet/` | `@.apps/dsr-rpg/` |
| `ops/rpg_pet.c` -> `rpg_pet.+x` | `ops/dsr_rpg.c` -> `dsr_rpg.+x` |
| `rpg-pet.xhtpm`, `board/pchq-board.xhtpm` | `dsr-rpg.xhtpm`, `board/pchq-board.xhtpm` (window titles, ids, paths renamed) |
| state in `@.apps/rpg-pet/state` | `@.apps/dsr-rpg/state` (own flags: `int`, `running`, own clock root) |
| pc-hq book maps `.../maps/rpg-pet/<desk>` | `.../maps/dsr-rpg/<desk>` (own map ids, never `dsr`: that name is reserved for the DSR/WSR bridge book) |
| voices, greetings, replies | per new character |
Everything that names `rpg-pet` / `rpg_pet` (paths, flags, window class names, the proc list entry, `button.sh`, the daemon pid/process pattern in proc-mon) is renamed in one pass; a `grep -r "rpg.pet"` over the copy must return nothing but the history note in the README. Both games must run side by side without sharing a single state file (checked by running both and diffing which files each writes). **Prerequisite for the BOARD windows (found 2026-10-12): only one pc-hq board can exist today** (single-instance launcher, one shared `@.apps/piececraft-hq` host package); the owner decided pc-hq must run separate sessions on the same binary, like the p2p apps: `PCHQ-SESSIONS-DESIGN.md` (S1-S4) comes first, then dsr-rpg phase 0 runs on its own instance `@.apps/dsr-rpg/pchq/`.

## 5. Cast: pets -> six main characters
- Characters: **soldier, chef, clown, king, queen, fairy**. In the copy they take the six party slots: same mechanics as the pets (selectable, INT-controllable, walk on their own, chat, greet, reply, voices).
- **Data changes** (data only): `party.pdl` (ids, names, profile line, sheet, stats), the DB actor rows the daemon reads (`&.widgits/db-hq/data/actors.pdl` / `system.pdl`: **new rows with new ids** such as `dsr_soldier`; the six existing rpg-pet rows are not edited, they are in use by the other game), `voices.pdl` (one voice + language each), `events/greet_<id>` and `events/reply_<id>` (6 + 6 pages, converted the same way the rpg-pet ones were), `toy.pdl`.
- **Sprites**: needs six RPG Maker character sheets that read as soldier / chef / clown / king / queen / fairy. **Not yet checked** that the asset library has all six; if not, the nearest sheet is used and the gap is listed (verify in phase 1, do not invent art).
- The trainer/hero slot: open (question 1).

## 6. Maps: the city and 16 interiors
**17 map screens**: `city` (the main map) + 16 interiors (per city: castle, bank 1, bank 2, store 1, store 2, store 3, school, hotel). Ids: `a_castle`, `a_bank1`, `a_bank2`, `a_store1..3`, `a_school`, `a_hotel`, same for `b_`.

**The city map ("Outside")** is the dsr-dev screen turned into ground: two clusters of 8 buildings (A left, B right) with **the same relative layout as the desk** (columns 10-13 and 20-23, rows 1 and 3, six open columns between) and **plenty of free ground left** for more buildings, roads, fields and the animals' pastures (owner 2026-10-12: "dsr-rpg outside should have the 16 buildings and plenty of space for more, like the livedesk version dsr-dev"). Because the open space must stay open, the map is **bigger than one window**: recommended **64 x 28 tiles at 24 px (1536 x 672)** with a **scrolling camera that follows the selected character** (RPG Maker style), and a bigger window just shows more of it; the 3D atlas already uses 24 px tiles. Building positions are read from the desk layout (the table in section 2), so the later bridge to the real DSR/WSR entities (`LINK | entity_uid | piece_id | kind | desk | x | y`) is a position lookup, not a redraw. Each building is a 3 x 3-tile footprint with its door on the bottom edge; the grid step of one desk column = 4 tiles.

**The interiors** stay 13 x 8 at 48 px, same painter and same furniture catalog as rpg-pet; each gets a distinct layout (castle: throne + guard posts; bank: counter and vault door; store: shelves and a till, the Shop menu lives here; school: desks and a blackboard; hotel: lobby, desk and stairs). Their one door leads back to the city at that building's door tile.

**Engine change needed (small code change, then data)**: doors are data already but fixed to the top-wall row (`tx` only). The city needs a `y` too: `DOOR | room | x | y | dest room | arrive x | arrive y` anywhere on a map (old rows keep working with y = the wall row), and a per-room `grid cols/rows/tile` so the city can be 26 x 16 @ 24 and rooms 13 x 8 @ 48. Both go into `rooms.pdl` / a new `cities.pdl` as data; the painter and the walk code read the per-room size instead of the `COLS ROWS T` constants. This is the single biggest piece of work and is what the carbon copy exists to allow without risking rpg-pet.

`cities.pdl` (new), one row per building:
`BUILDING | id | city a/b | kind | display name | door x | door y | interior room | arrive x | arrive y`

## 7. The people, animals, fairies and bad guys (owner 2026-10-12: "the new sprites will play out there: soldiers, clowns etc., not monsters: people, animals, fairies, bad guys / evil guys")
Four kinds of figure live on the city map, none of them rpg-pet's monsters:
1. **The cast (people)**: soldier, chef, clown, king, queen, fairy (section 5): selectable, INT-controllable, chat, voices.
2. **Animals**: chickens, horses, pigs (below).
3. **Fairies** (ambient, small, fly over buildings and fields; decoration with a sparkle, no collision), the cast's own fairy is a character, these are extras.
4. **Bad guys / evil guys**: wanderers marked `evil` in the data (`critters.pdl` kind column / an `enemies.pdl` of their own, reading the RPG Maker enemy rows of the DB if suitable). v1 only places and walks them and lets the owner see them; **what happens when a character meets one (fight, flee, steal from a bank) is a separate design**: the events guide lists Battle Processing as not started, and the bank/store entities belong to the later WSR bridge.
All four are data rows (`critters.pdl`: `CRITTER | id | kind | area | count | sheet | index | evil`) so adding a fifth kind later is data, not code.

## 7b. Animals: chickens, horses, pigs
A new entity class **critter**: not selectable, no chat, no INT control, wanders by a small AI (random walk limited to its area, idles, optional sound), drawn from RPG Maker animal/character sheets. Data in `critters.pdl` (`CRITTER | id | kind | area | count | sheet | index`), a handful per city (chickens near the school/stores, pigs and horses in the fields at the city edge). Engine change: one more list next to `others[]`, the same walk code with a bump rule (critters do not enter buildings); they must obey the viewer gate and CPU rules (no timers when no window, step pace from `tuning.pdl`). Whether they are ownable / mineable / sellable like the pets of the chain economy is out of scope for now (question 5).

## 8. What is reused unchanged
INT (the In: tab runs the `int` verb, roguelike on / auto off, arrows move the chosen character through the xelector, a click on a character selects it), the clock and sky (`lc_clock`, day phases), the board window and its 2D styles, events-hq pages, the viewer gate and `tuning.pdl`, `hq-cpu-safety.md` rules, the harness approach (pal + cases, never new `.sh`).

## 9. Phases, each ends with visible evidence (house rule: PNG + click path)
0. **Carbon copy renamed.** `dsr-rpg` runs and looks identical to rpg-pet (including the **db ▾** tab: db-hq, blueprintz, sql-hq, added to the rpg-pet board 2026-10-11, see `BLUEPRINTZ-DESIGN.md`); both open side by side; no shared state file. Evidence: two screenshots + the file-write diff.
1. **Cast swap.** Six characters with DB rows, voices, greet/reply pages, sprites (or a named gap). Evidence: the pets list shows the six; INT moves each; a greeting is heard.
2. **Interiors.** 16 rooms as data in the existing 13 x 8 model, reachable through a temporary hub. Evidence: a screenshot of each.
3. **The city.** Per-room grid size + free-placed doors, the 26 x 16 city, 16 doors, walk in and out of every building. Evidence: a walk through all 16 doors, with the arrival tile checked.
4. **Critters.** Chickens, horses, pigs wander; CPU measured with all four stat fields, with and without a window.
5. **(later, separate design)** the bridge to the real DSR/WSR entities by `entity_uid`.

## 10. Risks
- Drift between the two games (accepted, see section 3).
- A second resident daemon: it must obey the same viewer gate; measured in phase 0 (no window = about 1% target).
- The pc-hq board session names maps by id; a name clash with the existing rpg-pet maps or the future `dsr` book would corrupt a book: use `dsr-rpg` ids only.
- 26 x 16 in 3D (a larger painted map than any rpg-pet room): check the 3D frame rate before promising 3D for the city; 2D first.
- Art gaps (six sheets, animals): list them, do not fake them.
- The shared DB: new rows only, never edit the existing actor rows.

## 11. Open questions (a recommended answer each; "go" = take all the recommendations)
1. **Cast control.** Do the six all work like the pets (select + INT), and is there a player/trainer character too (a Harold equivalent)? *Recommend:* all six selectable; no separate hero at first, the first selected character is the player.
2. **Same six in both cities?** *Recommend:* one cast of six that walks the whole map (they can enter any building), animals and later NPCs differ per city.
3. **School and hotel vs the older desk.** RESOLVED 2026-10-12 by reading the desk: the entities `store_x4` and `population_x` already carry the school and hotel icons, so nothing is dropped; school and hotel become kinds in the later link registry.
4. **Building positions.** RESOLVED by the owner 2026-10-12: like the livedesk dsr-dev desk (table in section 2), with plenty of room for more; positions are kept in `cities.pdl`.
4b. **City size and camera.** *Recommend:* 64 x 28 tiles at 24 px, scrolling camera following the selected character (so the open space stays open). The alternative, everything on one fixed screen at 26 x 16, leaves no room to grow.
4c. **Bad guys.** v1 = placed and walking only, no fighting yet. *Recommend: yes.*
5. **Critters.** Wander-only decoration, or ownable / tradeable? *Recommend:* wander-only now.
6. **Names.** App folder `@.apps/dsr-rpg`, binary `dsr_rpg.+x`, window title "dsr-rpg". *Recommend:* yes.
7. **Order.** Phases 0 to 5 as above, each committed on branch `claude` with evidence. *Recommend:* yes, starting with phase 0 right after you say go.
