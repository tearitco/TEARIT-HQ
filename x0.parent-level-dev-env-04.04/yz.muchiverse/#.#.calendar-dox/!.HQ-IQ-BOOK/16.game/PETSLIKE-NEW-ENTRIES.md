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

Built (2026-10-21):

1. **The header menus are data now** (owner: "they should have .pdl for headers ... reusing / sharing parts better"). Each pets-like window has a `toolbar.pdl` (`TAB | id | label`, `ITEM | tab id | label | verb`). The shared reader `&.widgits/_shared-lib/petslike_toolbar.c` (text-include, prefix `ptb_`, pure file I/O: the house convention for code with 2+ consumers) turns it into `ui.txt` lines (`n_tab`, `tb_N_id/label`, `n_item`, `it_N_tab/label/act`); the template repeats over them (one `repeat` at the tab bar, one in the rail). A new tab or row is a line in `toolbar.pdl`, no template edit. The pets and rooms rows stay fed by the op's own data (party / `rooms.pdl`); `+ new room` and `- cancel -` of the Rooms menu are ITEM rows now, so the static rows I typed into eight templates the day before are gone.
2. **Pals** is such a tab: `+ new pal`, `- remove last pal`, `- cancel -` (rpg-pet and the seven clones). `newpal` appends `PAL | pal_N | Pal N | <picture>` to the app's `pals.pdl` and selects it; `delpal` drops the last; `load_party()` reads `pals.pdl` after the DB members; `NP` 6 -> 8. App-local on purpose (a board button does not write the house DB). The LyfeDesk top bar also has a `pals` button next to `palettes` (`strip_btn_2`, `livedesk:pals`): that lists the HOUSE pal library (`xyzfs/users/<uuid>/home/livedesk/pals`, 53 entities: asa, ava, cursword, terumon_001_ember ...). The two are different things (library entities that live on desks vs a game's roster); connecting them (a roster row that names a library pal and takes its sprite.csv) is the next step.
3. **rpg-edit-hq roster is data**: `party.pdl` (`PET | name | art id | letter | r | g | b | spawn x | spawn y`, `DOOR | name`) read at start; up to 16 pets. Names, colours, art ids, spawn cells and the door pet were arrays in `rpg_edit.c`. The left bar is a `repeat` over `n_pet`. Its Menu has `+ new pal` (appends a PET row, live).
4. Evidence: rpg-pet board window re-parsed to the data-driven tab bar (Pals tab present), the Pals dropdown opened with its three rows, `+ new pal` activated from that menu (pals.pdl got `pal_1`, the window showed "chat - Pal 1", HP 40 MP 20, a green box in the bedroom); rpg-edit-hq: a ninth `PET | Test` row appeared in the left bar, the chat and the world after a restart; `newpal` through the action form made `Pal 9` live. The seven clones were rebuilt and `newpal`/`delpal` run on each the day before; after the toolbar.pdl change only rpg-pet was run in a window.
5. Not done: the other tabs (Menu, Player, Shop, Time, db) are still typed in the templates (move them into toolbar.pdl the same way); pet-trainer and game-dev-hq; a library-backed roster; voices and greetings for pals.

## The house convention against hardcoding (answer to "what is the typical std")

- Data goes in `.pdl` files: `SECTION | KEY | VALUE` header, `|`-separated rows, `#` comments. Examples that already exist: `party.pdl` (the roster), `rooms.pdl`, `tuning.pdl`, `keybinds.pdl` (KEY / VERB / AXIS rows), `side_style.pdl`, the taskbar's `livedesk_taskbar.pdl` (`strip_btn_N_label`, `pals_menu_pre_N_*` rows around the scanned list: TASKBAR-MENUS-DATA-DRIVEN.md).
- A game's own `party.pdl` is its only roster (BET-RPG-DESIGN.md, CHAR-CREATE-DESIGN.md).
- Code that two or more apps need is a text-included `&.widgits/_shared-lib/<name>.c` with a short prefix and static functions (pgr_, mw_, ptb_); stateful code is an op process. Never a header plus a link step (memory: no header-link split).
- Paths come from the running binary's location or the session record (`real_project_root.txt`), never a typed absolute path.
- What is still typed in C is a default or a fallback only, and a missing file must degrade (rpg_edit.c: no PET rows = an island with no pets, said on stderr).
