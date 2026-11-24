# Opening a project as a BOOK in rpg-edit: event based, pages as sessions, playable through the Player (research + how-to, 2026-11-23)

Written after the owner said (2026-11-23): *"it should open in rpg-edit. do u see how projects can open in rpg-edit, be completely event based, and other projects can open. and pages will be sessions? and it's playable thru player? ... we will do it more in the future so research and write up hq-iq dox."* The first consumer is **hai-pal** (the embodied hai agents; hai repo book 25). The same recipe is for any later project.
Labels: **READ** = I read the file named (date given). **OBS** = I ran it on 2026-11-23 and saw the output. **DESIGN** = proposed, not built. **NOT VERIFIED** = nobody drove it live. Paths under the house root `44.xyz.01.00` unless they start with the hai repo name.

## 0. The answer in five lines (READ, then checked against files)
1. **A project opens in rpg-edit as a BOOK.** A book is a game's data folder `@.apps/<book>/` plus a pc-hq package `@.apps/<book>/<book>-pchq/` whose map folders are its PAGES. rpg-edit lists books in its Books dropdown and pages as desks.
2. **Book and session are the same thing; page and map are the same thing** (`18.pc-hq/PAGE-FILE.md`, owner standard). So "pages will be sessions" is the house's own model: a session folder holds a `session.pdl` naming the book and the open page; one page is one desk file; **a row in a page is one entity, and its `path` is that entity's pal folder** (so a hai agent, which is already a folder, is a pal row).
3. **Behaviour is event based.** Every rule is an op (a small C program), called by event pages (blueprintz, a folder of IR nodes compiled to pal) or by a verb; state is one append-only ledger row per action; the window is a projection. (Owner 2026-11-18, decision D92: no native rules, no exceptions.)
4. **Playable through the Player.** Player > Play runs the book's driver (the `GAME | driver` row of its `game.pdl`), which ticks while the editor's `running` switch is 1 and writes the feed the editor draws.
5. **Honest gap (OBS):** the generated demo book `chutes` fails the page-file check (section 4). Opening works by file convention; being a real citizen of the page file is the unfinished part. Build hai-pal on the conformant shape from the start.

## 1. The model in one page (READ: PAGE-FILE.md, GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md, 2026-11-23)
| Thing | What it is on disk |
|---|---|
| **Book** | `@.apps/<book>/` (game data: `game.pdl`, `state.pdl`, `verbs.pdl`, `rooms.pdl`, `layouts.pdl`, `nav.pdl`, `tuning.pdl`, `harness.pdl`, `rulesets/default/`, `events/`) + `@.apps/<book>/<book>-pchq/pieces/system/maps/<book>/game.pdl` (the page list: `n_desks`, `desk_N_id`) |
| **Session** | the same thing as a book at run time: `sessions/<s>/session.pdl` with `STATE \| name \| <book>` and `STATE \| active_desk \| <page>` (`&.widgits/book-page/ops/book_page_op.c`; address form `book:page`) |
| **Page** | one map: a folder under `maps/<book>/<page>/` (`map.txt`, `cells.txt/.rgba`, `events.txt`, `extrusion.pdl`, ...) and, for the page file, `desks/<page>.pdl` |
| **Entity** | one row of the page: `DESK \| name \| path \| x_px \| y_px \| cell_x \| cell_y \| glyph \| n`, `path` = the entity's pal folder; 2D and 3D read the same row |
| **Teleport** | an entity may carry `pages=<p1,p2,...>`: the pages it can take you to (design: a thin `book.pdl` with `BOOK`, `PAGE`, `ENTITY` rows, see GAMES-AS-BOOKS section 2; `book_check` is the BUILT half) |
| **Ledger** | `state/events.txt` or the game ledger: `EVENT \| round \| actor \| verb \| target \| at \| result`, one writer, replay reproduces the game (`02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`) |

## 2. How rpg-edit opens a book and how Play starts it (READ 2026-11-23; the live click NOT VERIFIED by me)
- **Books appear without a code change.** `@.apps/rpg-edit-hq/board/open_board.sh` links every `@.apps/*/*-pchq/pieces/system/maps/*/game.pdl` into the editor's maps at the next editor board relaunch. (A relaunch changes the owner's screen: ask first.)
- **Resolving a book and a page:** `@.apps/rpg-edit-hq/ops/edit_last.sh` resolves the book from `@.apps/<book>/rooms.pdl` (`ROOM \| page \| ...` rows); `@.apps/rpg-edit-hq/ops/edit_open.sh` answers `book=<name> page=<page>` for a desk. (The older docs name these under `board/`; they are in `ops/`.)
- **Play:** the shared core `&.widgits/_shared-lib/petslike_core.c` verb `play` (line about 1656) calls `book_driver()` (line about 212), which reads the book's `game.pdl`: `GAME | driver | <program> {house} {book} {editor}` is started detached, one instance. The driver publishes `state/game_ui.txt`, which the editor window draws as the game bar; `GAME | clock | pause` freezes the editor's world clock while the game is live.
- **State is declared:** `state.pdl` rows `FILE | name | kind | writer | regenerate_with` (LEDGER, PROJECTION, CONFIG, PREFERENCE); the `audit` verb fails an undeclared file.
- **Make a new book from args:** `game_new_op <house> new <name> --title T --kind ledger|realtime|editor` (guide: `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`). Use a SCRATCH house to try it, never the live house.

## 3. Event based, in the owner's sense (READ: EVENT-ARCHITECTURE-ONBOARDING, BLUEPRINT-EVENT-PAGES-GUIDE, 2026-11-23)
Four layers, one direction: (1) **when**: the clock fires named events from `time.pdl` rows; (2) **what happens**: an event page = a blueprint (`events/<name>/event_pkg/pages/page_N/` with `event.ir.pdl` as the only source, `condition.pdl` as the trigger, compiled `event.pal`, one wrapper per node, a C op doing the work); (3) **the record**: one append-only ledger; (4) **what you see**: the core writes `state/ui.txt`, the `.xhtpm` template binds `${key}`. A rule that changes state is a C op behind an event page or a verb; never a branch in a renderer or in `game_core.c` (skill `game-mechanics-are-events`, decision D92). The harness is the test, the events are the game; both, always (`02-architecture/PAL-VS-EVENTS.md`).

## 4. What I observed today (OBS, 2026-11-23)
- `book_check <pchq_root> --house <house>` on the generated book `@.apps/chutes/chutes-pchq`: **checks=14 pass=4 fail=2 skip=8, conformant=no.** A1 (no `pieces/display/open_book_page.txt`: the game has no open book/page record) and A2 (`source=(none)`: the board owns its own page and keeps private state instead of page-file rows) fail; A3 to A10 are skipped because they need a readable page file. B1 to B4 pass (`game.pdl`, unique valid desk ids, every desk has a `map.txt`, no orphan folders).
- `@.apps/chutes/` has **no `events/` folder**: the generated game's rules live in the shared engine `game_core` and its data (`rulesets/default/`), not in event pages. So today "a book opens in rpg-edit and plays" is true by file convention, and "completely event based" is the target, not what the generator ships. **Do not copy the generator's shape and call it event based.**
- All the named scripts and binaries exist (`open_board.sh`, `edit_open.sh`, `edit_last.sh`, `petslike_core.c`, `game_core.+x`, `game_new_op.+x`, `book_page_op.c`, `play_event.sh`, `book_check.+x`). **NOT VERIFIED:** the Books dropdown listing a new book in the live window and Player > Play starting its driver (the generator guide says the same).

## 5. Recipe: make any project a book that opens in rpg-edit (DESIGN, from sections 1 to 4)
1. **Name it and make the shell:** a book folder `@.apps/<book>/` with `game.pdl` (`GAME | name`, `title`, `kind`, `ruleset`, `driver`, `clock`), `state.pdl` (every file classified), `verbs.pdl`, `rooms.pdl` (one `ROOM` row per page), `harness.pdl` (shows the case in the Harness tab). Start from `game_new_op` in a scratch house, then keep only what you need.
2. **Make the pages real page files** (`source=desk`, entity rows), not a private board state; run `book_check` until A1 to A10 pass. This is what separates a book from a skin.
3. **Make every rule an op** (a C program with a header usage line and a harness case that can fail), a registry row, and **an event page** that calls it (edit it in blueprintz, so a person can see and change it). Numbers go in `RULE` rows.
4. **One ledger.** Rows only; replay must reproduce the world and refuse a hand-edited ledger; files beside it are projections.
5. **A driver** that ticks only while `running=1`, writes `state/game_ui.txt`, and decides nothing the ledger does not record.
6. **Prove it three ways:** the harness case (mutants must fail), `book_check`, and a real frame of the live window opened by Books > the book (open the PNG). Say which of these you did.

## 6. hai-pal on this model (DESIGN; the hai repo is `#.hai/harnecient-hai`)
| hai thing | becomes |
|---|---|
| a hai **session** (one chat ledger) | a **PAGE** of the book `hai-cli` (owner: "pages instead of sessions"); the page file holds who stands on it |
| a hai **agent** (a folder with corpus, persona, ledgers) | a **pal**: an entity row whose `path` is the agent folder; its `pages=` lists where it may teleport |
| **hai-cli** (main agent, not embodied) | the entity in slot 1; no body row on a page, global memory rows (hai book 25 section 7) |
| a **verb** (`say`, `teach`, `grade`, `move`, `mkdir`, `write`, `teleport`) | an op + a registry row + an event page; the row it writes is an `EVENT` row of the house ledger shape |
| the **world** (`ops/hai_world.c`, BUILT 2026-11-23, case `hai_world` 43 checks) | the rule set behind the movement verbs, but see the gaps below |
| the **window** | not a private window: the book opens in rpg-edit (board in the centre, chat and agents as overlays, pages on the left); the standalone `hai-cli` stays as the CLI-only way |
| **Play** | the driver runs the agents' loop; commands you type are verbs (self-directed goals come after) |
Both entry points must be proven: the standalone `hai-cli` and the book in rpg-edit open the SAME session (hai book 25 section 6).

## 7. Gaps between what exists and this model (honest list)
1. `hai_world` keeps its OWN ledger shape (`SPAWN|name|page|x|y`, `MOVE|name|dx|dy`, `REFUSED|...`) and its own page names. The house shape is `EVENT|round|actor|verb|target|at|result` with a resolver giving `EVRES` or `REJECT`. **Decision needed:** adopt the house row (then replay and the exporters read it) or keep mine and add a thin translation. Default recommendation: adopt the house row.
2. `hai_world` has **no registry row, no event page, no driver**; it is a rule set plus a frame writer. Those three are what make it a book.
3. Its pages are strings, not desk files: no `desks/<page>.pdl`, so `book_check` would fail A1/A2 as `chutes` does.
4. **Permissions and the sandbox for real folders are NOT built** (hai book 25 sections 4 and 5). No verb that writes to disk may ship before the escape cases pass.
5. The window canvas: a `<canvas>` fills its panel and shows `loading...` until `canvas_raw` names a frame; **a canvas hides the rest of the panel**, so chat and agents must be overlay rows over it (rpg-space's `canvas-overlay-left/right` + `ov-pin` rows). I learned this the hard way in the hai-cli window (it hid the chat).
6. A flex `<page>` (three columns) lost its centre panel after a click in the hai-cli window (2026-11-23, fixed by a plain sidebar + panel page, hai commit `d8a2c25`); copy rpg-space's page shape, not a custom flex page.

## 8. Design-choice test for "hai-pal as a book in rpg-edit" (AGENTS.md rule; the decision-log row is OWED, that file has other agents' uncommitted changes)
1. **One authority per fact?** Yes: the ledger; the page file and frames are projections. 2. **Modular?** Yes: rules are ops, wiring is data (`game.pdl`, `verbs.pdl`, event pages), no hai code in shared files. 3. **Scalable (the 10th consumer)?** Yes: the next project repeats section 5; nothing hai-specific enters the editor. 4. **Long term?** Matches the games-as-books pivot and D92. 5. **House shapes + cost?** pdl data, C ops, event pages, a harness that can fail; the cost is that the standalone hai window and the book are two front ends to keep identical (a parity case, as with the web demo).
**Recommendation:** build hai-pal as a book from the start (not a private window), keep the standalone `hai-cli` for CLI users, and prove both with one shared session.

## 9. Next steps (each with a check that can fail; nothing is claimed before it runs)
1. Decide the row shape (section 7 item 1); if adopted, change `hai_world` and its case in one commit.
2. Make `@.apps/hai-pal/` (scratch house first): `game.pdl`, `state.pdl`, `verbs.pdl`, `rooms.pdl`, a page file per page, one event page per verb; `book_check` must pass A1 to A10 and B1 to B4.
3. A driver; then open it from Books and Play, photograph it, open the PNG.
4. Overlays (pages left, agents and chat right) over the canvas, copied from rpg-space.
5. Then permissions, the sandbox, learned commands (hai book 25 build order).

## 10. First slice opened in the live editor (OBS 2026-11-24)
**What was built:** `@.apps/hai-pal/` (house commit 09357edfd): `game.pdl` (no `driver` row, so Play starts nothing: `book_driver()` returns 0), `rooms.pdl` (ROOM rows home and garden), `state.pdl`, `verbs.pdl`, `ledger/world.ledger`, and the package `hai-pal-pchq/pieces/system/maps/hai-pal/` with `game.pdl` (two desks) and a desk folder per page written by `hai_world desk <ledger> <page> <desk_dir> <atlas.rgba>` (hai repo, case `hai_world`, 62 checks, 5 mutants). **Observed:** `edit_open.sh <house> hai-pal home` and `garden` answered `book=hai-pal page=... pages=2`; a bad page exited 2. `book_check` on the package: 4 pass, 2 fail, 8 skip (B1 to B4 pass; A1 and A2 fail exactly as for the generated `chutes`): the page-file conformance is still NOT done.
**On screen (photographed, opened):** after `sh @.apps/rpg-edit-hq/button.sh run`, the Books dropdown (header cell 2) listed **hai-pal** (item 37) with the other books; picking it gave the header `book:hai-pal page:home` and the 3D board drew the room (parquet floor, cobble wall border, the two-cell inner block) with a figure on it and the room in the minimap. A leftover Teario-Play popup (the saved session's game) covered the centre.
**Traps hit driving the real window (each cost a run):**
- Typed digits through the relay (`KEY_PRESSED: 50`, `13`) did nothing for the header cell, and `37` then Enter chose the WRONG book (centroid). Use `MOUSE_EVENT: 1 <x> <y> 1` and `0` through `#.desktop/entity_menu_history/<board_window.pid>.txt`, with the pointer moved onto the window first.
- The FIRST click on a cell or row only focuses it (`[>]` appears); click a SECOND time to open or activate it. Photograph between clicks to know which state you are in.
- The launch itself rewrote the saved `@.apps/rpg-edit-hq/state/last_book.txt` (from `tsots/map071` to `teario-play/play`); record the file (copy + hash) BEFORE a launch and restore it afterwards.
- A shell loop of `xwininfo -id` over every window can hang for 30 s while the engine starts; photograph the known window id instead.
**Next:** a driver (a `GAME | driver` row) so Play runs the agents, event pages per verb, and `source=desk` page-file rows so `book_check` A1 to A10 pass.

## 11. OWNER RULE (2026-11-24): opening a project in rpg-edit switches the side bars to the relevant ones, never the palettes
- **Rule (owner's words, "no need for palettes"):** when a project (a book) is opened in rpg-edit, the side bars change by themselves to the ones that project needs: a game book shows ITS panels (the pet game: pets, wallet options, the party; hai-pal: pets), not the tile palettes. The palettes belong to the editing job, not to every book.
- **Seen as the failure (RAN, 2026-11-24, tear-on):** the left bar opened on the palettes list. One mis-aimed tap of mine opened the Chemicals+Compounds tile grid, and the bar then showed 48 empty rows full of nav numbers, which ended up in two tutorial videos. Restored with the panel's own back action: `@.apps/rpg-edit-hq/ops/+x/pal_side.+x <house> back` (state file `@.apps/rpg-edit-hq/board/state/pal.txt`: `pal_is_tiles`, `pal_can_back`, `pal_title`; the `< back` item is `sp-back` in `board/pchq-board.xhtpm`). `pal_is_list=1` with `pal_title=palettes` is the restored state.
- **How it should work (DESIGN, NOT BUILT):** a book names its side bars as DATA (a `GAME | sides | left=<panel> right=<panel>` row in its `game.pdl`, the grammar of the existing `GAME | driver` and `GAME | clock` rows); the editor reads it on `CONFIRM_START_MAP`, writes `side_left`, `side_pal` in `board/state/sides.txt`, and restores the previous choice when the book is closed (preference, never game state). Design-choice answers before building: one authority (the book row), modular (no per-book C in the editor), scalable (the 10th book is one row), preference not ledger.
- **For tear-on specifically (BUILT 2026-11-24 night, SEEN live, harness `tear_on` 88):** while Play runs, the driver feed sets `side_pal=` (palettes hidden), `side_left=1`, `n_party` and `party_<i>_label/_id/_cls` (the pets, the selected one marked), `coins`, `nick_line`, `stat_line`, `stat_line2` (the precedent of `deskopoly_driver`: the last vars file, `state/game_ui.txt`, wins over the editor's own keys and the editor values return when the feed drops `gm_on=`). This is the PLAY-time half only; the book-OPEN half (a `GAME | sides` row read on `CONFIRM_START_MAP`) is still not built, so between opening the book and pressing Play the bar is whatever it was. Clicking a party row runs the editor's `select`, which does nothing for a tear-on pet: choose a pet with Next pet. Lines are kept under about 20 characters, the bar is narrow.
- **For anyone filming:** photograph the board AFTER setting the bars; crop to the board area only if the bar cannot be set (what the 2026-11-24 night re-cut of both tear-on videos did, section 10 of the how-to).
