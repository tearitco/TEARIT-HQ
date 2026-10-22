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

1. **The header menus are data now, in the LyfeDesk taskbar's shape** (owner: "they should have .pdl for headers", "should be similar to lyfedesk"). Each pets-like window has a `toolbar.pdl` written like `#.desktop/livedesk_taskbar.pdl`: `SECTION | <cell>_menu_<N>_label` + `_cmd` rows, rows staged around a list the op builds with `<cell>_menu_pre_<N>_*` / `_post_<N>_*` (the taskbar's `pals_menu_pre` / `pals_menu_post`), fixed tabs as `tab_<N>_cell` / `tab_<N>_label` (the taskbar's `strip_btn_<N>_label`). `_cmd`: `op:<verb>` = the app op's own verb (the taskbar's `livedesk:<name>`), empty = just close, anything else = a shell string as written (the taskbar's bare-string form). The shared reader `&.widgits/_shared-lib/petslike_toolbar.c` (text-include, prefix `ptb_`) publishes `n_tab`, `tb_N_*`, `n_item` / `it_N_*`, `n_pre` / `pre_N_*`, `n_post` / `post_N_*`; the template repeats over them (a pre repeat before the pets rows, a post repeat after the rooms rows). A new tab or row is a line in `toolbar.pdl`. The pets (book) and rooms (page) rows stay fed by the op's own data; `+ new room` and `- cancel -` of the Rooms menu are `page_menu_post_*` rows.
2. **Pals** is such a tab: `+ new pal`, `- remove last pal`, `- cancel -` (rpg-pet and the seven clones). `newpal` appends `PAL | pal_N | Pal N | <picture>` to the app's `pals.pdl` and selects it; `delpal` drops the last; `load_party()` reads `pals.pdl` after the DB members; `NP` 6 -> 8. App-local on purpose (a board button does not write the house DB). The LyfeDesk top bar also has a `pals` button next to `palettes` (`strip_btn_2`, `livedesk:pals`): that lists the HOUSE pal library (`xyzfs/users/<uuid>/home/livedesk/pals`, 53 entities: asa, ava, cursword, terumon_001_ember ...). The two are different things (library entities that live on desks vs a game's roster); connecting them (a roster row that names a library pal and takes its sprite.csv) is the next step.
3. **rpg-edit-hq roster is data**: `party.pdl` (`PET | name | art id | letter | r | g | b | spawn x | spawn y`, `DOOR | name`) read at start; up to 16 pets. Names, colours, art ids, spawn cells and the door pet were arrays in `rpg_edit.c`. The left bar is a `repeat` over `n_pet`. Its Menu has `+ new pal` (appends a PET row, live).
4. Evidence: rpg-pet board window re-parsed to the data-driven tab bar (Pals tab present), the Pals dropdown opened with its three rows, `+ new pal` activated from that menu (pals.pdl got `pal_1`, the window showed "chat - Pal 1", HP 40 MP 20, a green box in the bedroom); rpg-edit-hq: a ninth `PET | Test` row appeared in the left bar, the chat and the world after a restart; `newpal` through the action form made `Pal 9` live. The seven clones were rebuilt and `newpal`/`delpal` run on each the day before; after the toolbar.pdl change only rpg-pet was run in a window.
5. Not done: the other tabs (Menu, Player, Shop, Time, db) are still typed in the templates (move them into toolbar.pdl the same way); pet-trainer and game-dev-hq; a library-backed roster; voices and greetings for pals.

## LyfeDesk vs the x11-hq windows: which does it better (owner: "if u see something x11-hq window does better, let me know")

- Menus from a pdl: only the LyfeDesk taskbar does it. No x11-hq window (db-hq, co-lab-hai, chat-hai, chain-hq, blueprintz ...) defines a menu in a pdl; they all do list-from-vars: a manager publishes `n_x`, `x_N_label` and the template `repeat`s over them. That is the mechanism the pets-like windows use for the pets and rooms, and it is what the fixed rows ride on now, so there is no second mechanism.
- Where the taskbar is better and now copied: flat `<cell>_menu_<N>_*` keys, `pre` / `post` staging around a scanned list (the rows go before / after the list with no ordering hack in the template), and the `_cmd` grammar (internal verb / shell string).
- Where the windows do better: a row can be shown or hidden by `show="${var}"` (a row that only exists in some state), and the dropdown rows get their icon and nav number for free. The taskbar's `_cmd` has no such gate; a `_show` key would be the matching pdl feature (not built).
- Where the taskbar is worse and the windows should not copy it: the taskbar parses its pdl inside a ~5000-line C manager with 81 `strcmp` dispatch branches (TASKBAR-MENUS-DATA-DRIVEN.md section 1); a window's verbs are the op's own `verb()` table, one function per app.
- Open duplication: `read_key_value()` (taskbar manager), `pgr_pdl_value` (khtpm_page_rows.c) and `ptb_` each read `SECTION | KEY | VALUE` rows. One shared pdl reader in `_shared-lib` would end that; I did not touch the taskbar manager to get there.

## The house convention against hardcoding (answer to "what is the typical std")

- Data goes in `.pdl` files: `SECTION | KEY | VALUE` header, `|`-separated rows, `#` comments. Examples that already exist: `party.pdl` (the roster), `rooms.pdl`, `tuning.pdl`, `keybinds.pdl` (KEY / VERB / AXIS rows), `side_style.pdl`, the taskbar's `livedesk_taskbar.pdl` (`strip_btn_N_label`, `pals_menu_pre_N_*` rows around the scanned list: TASKBAR-MENUS-DATA-DRIVEN.md).
- A game's own `party.pdl` is its only roster (BET-RPG-DESIGN.md, CHAR-CREATE-DESIGN.md).
- Code that two or more apps need is a text-included `&.widgits/_shared-lib/<name>.c` with a short prefix and static functions (pgr_, mw_, ptb_); stateful code is an op process. Never a header plus a link step (memory: no header-link split).
- Paths come from the running binary's location or the session record (`real_project_root.txt`), never a typed absolute path.
- What is still typed in C is a default or a fallback only, and a missing file must degrade (rpg_edit.c: no PET rows = an island with no pets, said on stderr).
