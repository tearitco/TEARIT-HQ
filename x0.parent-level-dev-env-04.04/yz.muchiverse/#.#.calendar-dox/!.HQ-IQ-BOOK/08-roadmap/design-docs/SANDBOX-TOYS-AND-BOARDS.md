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

## 6. Known gaps
- The daemon still reads the house-wide canvas click file, not the click file of its own board session (a click on another board can select a character); to be moved to the session file.
- The right-click context menu of a second pc-hq instance still reads the house-wide ctx file.
- The standalone dsr-rpg window and the board both open the same scene; there is no toy that opens both at once.
