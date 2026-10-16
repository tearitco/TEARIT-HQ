# Sandbox games: one data set, several interfaces ("toys") (explained 2026-10-12)

Status: EXPLANATION of what exists today (rpg-pet and dsr-rpg) plus the rule for the next sandboxes. Written by claude after the owner asked why `dsr-rpg` and `dsr-rpg board` are two toys entries.

## 1. What the toys menu is
The taskbar's toys menu lists one entry per `@.apps/<dir>/toy.pdl` (`SECTION | title | <name>`, `SECTION | launch | button.sh`). An entry is a **way to open something**, not a game. A game can have several entries.

## 2. Why rpg-pet and dsr-rpg each have TWO entries
| toys entry | folder | what it opens | interface |
|---|---|---|---|
| `dsr-rpg` | `@.apps/dsr-rpg/` | the standalone window (`button.sh run`) | a small khtpm window that paints the scene itself; the original "toy" shape |
| `dsr-rpg board` | `@.apps/dsr-rpg-board/` (a thin launcher) | the same game inside a **pc-hq board window** (`@.apps/dsr-rpg/board/open_board.sh`) | the pc-hq engine: 2D and 3D views, camera, xelector, placer, the db tab (db-hq / blueprintz / sql-hq), the entities bar, the book/page tabs |

Both entries run on **one data set and one daemon**: `@.apps/dsr-rpg/` holds the rooms, party, catalog, event pages and `state/`; the single resident daemon `dsr_rpg.+x` walks the characters and writes the files both windows read. The two windows are only two views of the same world. A developer picks the interface that suits the job: the small window to watch or play fast, the board to inspect, edit and debug (3D, db, events). This is deliberate: this is a **dev A/B sandbox**.

The board entry is its own folder (`dsr-rpg-board`) because a toys entry is a directory with a `toy.pdl`; the folder holds only `toy.pdl` and a `button.sh` that calls the board launcher. All real files stay in `@.apps/dsr-rpg/board/`.

## 3. The board runs on its own pc-hq instance
`dsr-rpg board` does not share rpg-pet's pc-hq world. It sets `PCHQ_PKG=@.apps/dsr-rpg/dsr-rpg-pchq` (the DATA root: world, hero, xelector, maps, engine sessions; the programs stay in `@.apps/piececraft-hq`). That is why it opens **next to** the rpg-pet board instead of replacing it. See `PCHQ-SESSIONS-DESIGN.md`. The instance folder name is unique on purpose (board-viewer scopes its engine session by it).

## 4. "book:dsr-rpg  page:bank" and the old "pets" label
The top bar reads `book:<name>` and `page:<room>`. For this board the **book is `dsr-rpg`** (its own book of maps in its own instance, `maps/dsr-rpg/<room>`) and the **page is the room** you are looking at (`city`, `castle`, `bank`, `store`, `school`, `hotel`). So `book:dsr-rpg page:bank` is the correct pair: bank is a page of the dsr-rpg book, not a book.
The word **"pets"** that was still on the board (left bar title, footer toggle, the first tab) was a **stale label from the carbon copy of rpg-pet**; the cast is people now. It was renamed **Cast** (`board/pchq-board.xhtpm`). Only labels changed; ids and actions are the same. Some internal names still say pet (`pet_says`, `party.pdl PET` rows, `state/pet_<id>.txt`); they are identifiers and are renamed only when it pays off.

## 5. The rule for the next sandboxes (GTA-like, Dwarf Fortress-like, CDDA-like)
- **One game = one data app** (`@.apps/<game>/`): its rooms/maps, party/actors (as RPG Maker DB rows), event pages, tuning, one resident daemon. The data never knows which window is open.
- **Interfaces are separate toys entries over that data**: the small window, the pc-hq board (its own `<game>-pchq` instance), later others (ascii, a map editor, a spectator view). Add one by adding a folder with a `toy.pdl` and a launcher; do not fork the data.
- **Each board gets its own instance and its own book** so several games and several boards run at once (rpg-pet, dsr-rpg, then the new ones).
- **World scale is data**: rooms declare their own size, tile size and kind (`ROOM ... over | cols | rows | tile_px | biome`), so a city overmap, a fortress level or a roguelike map are rows in `rooms.pdl`, not new C. Ground uses RPG Maker autotiles (kind x 48 + shape from the 8 neighbours; verified on the tsots maps).
- The viewer gate stays: a daemon does heavy work only while a window for it exists.

## 6. Building a new sandbox (checklist; the later games the owner named: GTA-like, Dwarf Fortress-like, CDDA-like)
These are the owner's stated direction, not built. What each needs from the same machinery, and what is new:
1. **Data app** `@.apps/<game>/`: `rooms.pdl` (levels; kinds: normal room, outdoor, `over` overmap biome; sizes per room), `party.pdl` + RPG Maker DB actors (`&.widgits/db-hq/data`: ACTOR rows, a party in system.pdl), `catalog.pdl` (items), `events/` packages (greetings, doors, time phases, later jobs), `time.pdl` / `tuning.pdl`, one resident daemon (start from `dsr_rpg.c`; it is data driven).
2. **Interfaces** as toys entries: the small window, the pc-hq board (`<game>-pchq` instance via `PCHQ_PKG`, `pchq_instance_init.sh`), later others.
3. **Pictures**: RPG Maker MV tiles and characters from `#.NNEST_ASSETS/rmmv-www-img` (emoji overlays are not planned for now). Ground = autotiles (kind x 48 + shape from 8 neighbours, verified on the tsots maps; `draw_auto()` in `dsr_rpg.c`, tables in `tile_autotile.c`).
4. **Scale by genre**: GTA-like = a large overmap (`over`, many rooms/areas, vehicles as movers); Dwarf Fortress-like = many z-levels of one map, jobs and designations as data, hundreds of movers (the per-room pathing is breadth-first and will need budgets); CDDA-like = a tile world with items, stats and turn time (INT/rogue time already exists). Each is mostly rows and rules in data; engine work (scrolling, z-levels, many movers) is added once in the shared daemon code, not per game.
5. **Evidence per phase** as everywhere in the house: build, run, screenshot, CPU measured (the viewer gate keeps an idle game near zero).
Per-room geometry (cols, rows, tile px, trigger row) is what made the overmap possible; z-levels and scrolling are the next engine steps and are NOT built.

## 7. Known gaps
- Fixed 2026-10-12: the daemon read the house-wide canvas click file (a click on another board could select a character): it now reads its own board session's `canvas_click.txt`. The right-click menu of a second instance read the house-wide ctx file: `pc_canvas_rclick.sh` / `pc_entity_ctx.sh` now take the instance data root from `PCHQ_PKG` and the ctx dir from `<instance>/pieces/system/ctx_dir.txt`. Evidence: two boards' click files differ and are written separately; the daemon selecting a character from its own file and the second instance's right-click menu were NOT exercised on screen yet.
- The standalone dsr-rpg window and the board both open the same scene; there is no toy that opens both at once.
- Still house-wide: `pc_synch_request.txt` (4 readers) and the instance's chunks/maps are not yet game-specific.
