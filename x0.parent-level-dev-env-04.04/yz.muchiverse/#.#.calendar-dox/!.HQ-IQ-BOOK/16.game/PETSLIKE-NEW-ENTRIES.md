# "+ new" in the page / book menus of every pets-like board

Owner 2026-10-21: "page/book dropdowns dont have new, see how our lyfedesk tb has these ... all of our projects 'petslike' need this", then "do the new's first, new +pet would go under a 'pals' tb". Written by the claude engineer.

## What LyfeDesk does (the model)

The LyfeDesk taskbar has `+new-desk` in its Desk popup and `new` in File (`#.desktop/livedesk_taskbar.pdl`: `desk_menu_action_2_label = +new-desk`, cmd `livedesk:new-desk`). A new desk is EMPTY, is listed at once and the view switches to it. The boards copy that: a "+ new ..." row above "- cancel -" in the dropdown, which makes an empty thing, lists it and goes there.

## Two families of boards

| Family | Boards | Book | Page | "+ new" is |
|---|---|---|---|---|
| A. pc-hq maps (shared script `@.apps/piececraft-hq/ops/pchq_board_action.sh`) | rpg-edit-hq, piececraft-hq, layout-studio sandbox | File menu = a map folder under `pieces/system/maps/` | Desk menu = a desk (page) folder listed in the book's `game.pdl` | verbs `newbook`, `newpage` |
| B. rpg-pet clones (own C op, rooms in `rooms.pdl`) | rpg-pet, df-hq, tcg-rpgce, dsr-rpg, mcl-hq, cdda-hq, civ-rpgce, gta-hq | Pets / Cast menu = the party | Rooms menu = a ROOM row | verb `newroom` of the app's own op |

Not given a "+ new page": pet-trainer (its pages are three fixed views: room, manage, world, nothing to add) and game-dev-hq (its Book / Page menus hold no rows in the board template yet). The Pets / Cast menus have no "+ new" until "pals" exists (below).

## Family A: how it works

- `real_pchq()` reads the viewer session's `pieces/system/real_project_root.txt`, so the script acts on THE BOARD'S OWN game. Before 2026-10-21 the `desk`, `file-hq` and `load-map` verbs wrote to piececraft-hq's inbox even for another project's board: the Desk rows of rpg-edit-hq never switched its page.
- `newpage`: copies the current page folder to `page_N` (atlas, legend, extrusion), empties it with `blank_page`, appends `desk_K_id/label` and bumps `n_desks` in the book's `game.pdl`, then writes `CONFIRM_SET_DESK:page_N` to the game's `widget_cmds/inbox.txt`.
- `newbook`: same into `maps/book_N/start`, a `game.pdl` derived from the source book (label, one desk), then `CONFIRM_START_MAP:book_N`.
- `blank_page` keeps size, atlas and legend and fills every cell with the page's most common cell: map.txt glyph rows with the commonest glyph (piececraft `f`, edit board `_`), maker `cells.txt` rows with the commonest token, events emptied. A blank edit page is therefore a flat black slab (token 0,0,0 has no picture): a nicer default floor is open.
- A board that generates its own pages (rpg-edit-hq's `rpg_edit.c`) must not erase added pages: it now keeps every extra `desk_N_id` it finds in `game.pdl` when it rewrites the file at start.
- Evidence (2026-10-21, live board): newpage -> tab `page:page_2`, HUD `edit:page_2`, slab on screen; newbook -> `book:book_2`, `book_2:start`. Test folders removed.

## Family B: how it works

`<app> newroom` (verb in each clone's `verb()`): refuses at 8 rooms (the loader's `nroom < 8`), picks the first free `room_N`, appends `ROOM | room_N | Room K | 1 | 2` (an interior with floor tile Inside_A5 1,2) to the app's `rooms.pdl`, reloads and does `goto`. The row sits above "- cancel -" in the Rooms menu of the board and of the standalone window. Run on all eight: rooms +1 each, "watching the Room N" (or "<pet> is now in the Room N" when INT is on).
Not done: furnishing, doors to the new room (a ROOM with no DOOR row can only be reached from the menu), and the outdoor / overworld kind.

## "+ new pet" under a "pals" tab: research, plan

What a pet IS (rpg-pet, same in the clones):

- The party is read from the house RPG Maker DB (`&.widgits/db-hq/data`): `system.pdl` `SYSTEM | name | RpgPetParty` followed by member rows, and one `ACTOR` block per pet in `actors.pdl` (id, name, nickname, profile, mhp, mmp, atk, def, mat, mdf, agi, luk, character `Sheet:index`). `party.pdl` only chooses the picture (sheet and index 0-7).
- Each pet gets a stable `entity_uid` (32 hex, `state/uid_<id>.txt`, made on first use), its wallet identity is `e` + the first 24 hex (CHAIN-ECONOMY-DESIGN.md). No chain wallet is created yet.
- The count is fixed in code: `#define NP 6` sizes `party[]`, `others[]` and the per-pet state files.
- rpg-edit-hq has eight pets hard-coded in `rpg_edit.c`.

Built (2026-10-21), as a smaller step than the plan first wrote down:

1. A **Pals** toolbar tab (right after Pets / Cast) with `+ new pal`, `- remove last pal`, `- cancel -`, in the board and the standalone window of rpg-pet and the seven clones.
2. `newpal` verb: appends `PAL | pal_N | Pal N | <monster picture index>` to the app's own `pals.pdl` and selects it. `load_party()` reads `pals.pdl` after the DB members: default stats (40 hp, 20 mp, 10 atk / def / agi / luk, 8 mat / mdf), the picture sheet of the first pet. `delpal` drops the last row. The entity uid (and so the wallet identity) is made by the existing code on first use (`state/uid_pal_N.txt`).
3. `NP` 6 -> 8 (the 8 Monster.png pictures). The pets arrays and per-pet loops follow `NP`; the saved state of the original six is untouched. At 8 the verb answers "no more pals (the house holds 8 pets)".
4. **Decision: app-local `pals.pdl`, not the house DB.** The plan wanted a new ACTOR in `&.widgits/db-hq/data`, but that DB is shared by db-hq and every app; a board button creating house-wide records is a bigger decision than "+ new". Moving a pal into the DB (ACTOR id, `RpgPetParty` member row) is a later step, with a "remove" that can find what the board made.
5. Not done: rpg-edit-hq (its eight pets are code in `rpg_edit.c`; a `pals.txt` read by the daemon is the way), a pal with its own picture choice (it takes the next monster index, the sheet of the first pet), voices, chat greetings and relations for pals beyond the defaults, pet-trainer.
Evidence: rpg-pet and the seven clones rebuilt; `newpal` x2, `delpal` x2 on each (pals.pdl grew to 2 rows then emptied; "watching Pal 1 / Pal 2"); rpg-pet board window showed the Pals tab and "chat - Pal 1" with the pal's stats. The Pals dropdown rows themselves were not seen opened (a click on the tab only moved nav focus in my run).
