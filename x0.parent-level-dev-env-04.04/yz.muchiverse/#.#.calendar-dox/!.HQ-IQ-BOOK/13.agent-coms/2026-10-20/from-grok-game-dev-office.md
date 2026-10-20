# game-dev office, notes while building

Written while the office picture is being added. The week rules stay the ones in `16.game/GAME-DEV-HQ-DESIGN.md`.

## What was asked

Stick to Game Dev Story, and use this house's layouts and pictures. Not Kairosoft's art, and not the stock-and-bonds CLI under the share drive.

## How I am reading Game Dev Story

A small studio, one week at a time. You hire a specialist, train one stat, or rest. A contract is two words: a genre and a type. People stand in the office and get assigned to the one game in progress. The game moves proposal, production, debug, review. Ship turns the five scores into rank, sales, and fame. Consoles, sequels, and the hall of fame wait.

The picture in that game is the office, with the menus over it. The menu window we already have is only the menus.

## What I am using from this house

- Tiles already on disk: `Inside_A5` for the floor and the wall, `Inside_B` for desks. The desk cell is the same one mcl-hq's catalog calls `table_white` (`Inside_B` column 1, row 12).
- Character sheets already worn by the other books: Rex `People3` index 6, Pip `People2` index 6, Gus `People1` index 6. A new hire uses `People3` index 0, the same sheet slot mcl-hq gives Orin.
- The board book shape the other games already export: `map.png` for the 2D room, `events.txt` rows `x y r g b sheet index dir pattern letter`, and a 16×24 frame file the 3D view extrudes. Book id `game-dev-hq`, one desk `office`.

## What I am not doing

- Not copying `mcl_hq.c`. That file is the whole pet house (walk, clock, shop, mine).
- Not calling `pc_generate_chunk`.
- Not loading Spacefox, UltiCa, or Mineclonia into this room. Those paints belong to df, cdda, and mcl. This room is the RPG Maker interior those books already use for a building inside.
- Not opening a board window in this pass. Export writes the files. The launcher is `board/open_board.sh`.

## Tools

- Read the design page, `mcl-hq/party.pdl`, `mcl-hq/catalog.pdl`, and the `export3d` comments in `mcl_hq.c` for the file names the board already understands.
- `ls` on `#.NNEST_ASSETS/rmmv-www-img/tilesets` and `characters` to confirm the PNGs exist.
- gcc via `ops/build_game_dev_hq.sh`. stb_image is the same text include the other daemons use.
- A verb run: `game_dev_hq.+x exportbook`, then check `map.png` size and one `events.txt` row.

## Steps

1. This note.
2. `party.pdl` and `rooms.pdl` for the office.
3. `ops/office_book.c` paints one room and writes the book under `game-dev-hq-pchq/pieces/system/maps/game-dev-hq/office/`.
4. The daemon calls that after each verb. `exportbook` prints the path.
5. `board/open_board.sh` points `PCHQ_PKG` at that folder and asks the board to open the book.
6. Design page: the office is that room. The tabs stay the Game Dev Story commands.

## What the export showed

`sh ops/build_game_dev_hq.sh` then `game_dev_hq.+x exportbook`.

`map.png` is 50628 bytes at `game-dev-hq-pchq/pieces/system/maps/game-dev-hq/office/map.png`. Glyph map is a wall strip, then four `f` desks. `events.txt` has four sprites facing up (dir 8): Rex `People3` 6, Pip `People2` 6, Gus `People1` 6, Ada `People3` 0. Ada is in the file because an earlier hire was still in `staff.pdl`. The first color sample for Rex was a clear pixel, so the sampler now takes the first opaque pixel.

The board window was not opened. `board/open_board.sh` is the launcher. I have not watched the canvas.

## Window after the later passes

The live window is `button.sh run`, not a separate board. Top bar is INT, Book, Page. The left pin holds cash, fame, and the four menus Hire, Train, Contract, Advertise. Those open in the sidebar. Names are footer buttons only. A `!Door1` sits on the office floor. A week-ending verb sends the staff out. The resident brings them back. Design page `16.game/GAME-DEV-HQ-DESIGN.md` matches this.
