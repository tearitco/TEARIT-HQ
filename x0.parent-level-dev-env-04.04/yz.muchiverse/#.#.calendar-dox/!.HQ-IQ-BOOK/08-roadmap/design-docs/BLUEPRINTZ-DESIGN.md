# blueprintz: the visual events editor for a whole book and its pages (design, 2026-10-11)

Status: DESIGN + an EMPTY SHELL. What exists today: the window `&.hq-apps/blueprintz/blueprintz.xhtpm` (static, no module, no state), its launcher `&.hq-apps/blueprintz/button.sh` and `_.monads/_.livedesk-taskbar/ops/open_blueprintz.sh`, and the entry points: the **blueprintz** row in the taskbar's db dropdown (`#.desktop/livedesk_taskbar.pdl`, `db_menu_4`), and a **db ▾** tab (db-hq / blueprintz / sql-hq) on the rpg-pet board and on the generic pc-hq game window (`@.apps/piececraft-hq/pchq-board.xhtpm`, which replaced its "Db (todo)" row). Nothing else is built. Written by claude from the owner's words and from reading the real files.

## 1. The owner's ask (2026-10-11)
> a new button in the db dropdown, "blueprintz", that opens an empty events-hq like window ... the way to visually view and edit all events for the entire loaded book and pages; like db works. I will also add one in the tbs of the pc-hq games and will want to view the visual events scripting of the entire game somehow using "pages of pages" in the book. Make a design doc for that and put it on the roadmap as a todo.

So: **db-hq edits the database; events-hq edits one event; blueprintz is the missing level above them: all events of the whole loaded book, across all its pages.**

## 2. Vocabulary (what "book" and "pages" mean here, read from the real files)
- **Book** = a livedesk session / a pc-hq game: `sessions/<id>/session.pdl` (`STATE | name | <book>`, `active_desk`). The taskbar shows `book:<name>` and `page:<desk>`. In pc-hq a book is also a map set: `@.apps/piececraft-hq/pieces/system/maps/<book>/<desk>/`.
- **Page** = a desk of the book (a map screen, a room). A page holds placed things and **events** (map events: `events.txt` rows `x y ... charset index dir pattern`, plus event packages).
- **Event** = an events-hq package `event_pkg/` with **event pages** (`pages/page_N/`: `event.ir.pdl` = the command list as `NODE | id=N type=<command> | args`, `condition.pdl` = when this event page is active, `event.pal` = the compiled program, `cmd_N.sh`). So "pages of pages": **book > pages(desks) > events > event pages > commands**.
- **Commands** come from the live registry `#.ref/menu/event_commands.registry.pdl` (never hard-code a command list: `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`). **Switches / variables / common events** are shared state: `state/world/switches.txt`, variables, `101/102` common events.
- Two stores of events exist today, and blueprintz must read both: **desk events** (placed on a pc-hq page) and **app-local event packages** (e.g. rpg-pet's `@.apps/rpg-pet/events/{play,stop,int_on,greet_*,door_*,phase_*}/event_pkg`, the clock rows in `time.pdl`).

## 3. What blueprintz is (and is not)
- It **is** a book-level overview and editor: see every event of the loaded book on one screen, group by page, search, and see how they connect (which events set a switch, which read it, which call which common event, which door leads where). The name suggests a node-graph view (Unreal-style "blueprints"); that is the end state, not the first view.
- It **is not** a second event format. The `event.ir.pdl` files stay the one source of truth; edits go through the existing events-hq ops (`mr_*` ops, `evhq_action.sh`), never straight into a parallel copy. It **is not** new per-app C in `khtpm_core_render.c`: it follows the house standard (static template + projector module, like db-hq/sql-hq: `blueprintz.xhtpm` + `ops/blueprintz_projector.+x` writing `state/ui.txt`).
- events-hq stays the editor for ONE event; blueprintz opens it for single-event edits at first and may absorb it later (question 2).

## 4. Scope: which book is "the loaded book"
`button.sh <house>` opens it on the active livedesk book. Launched from a game's tab, it gets the game's book root as an argument (the board passes its own session / book), so the same window can show rpg-pet, dsr-rpg, chess, doom, ... one book at a time. An `bp_sources.pdl` per book lists where its events live (desk event dirs + app-local `events/` dirs), so a game with app-local events (rpg-pet, dsr-rpg) and a game with only map events are both covered without special cases in C.

## 5. Views (each phase adds one)
1. **Book tree** (left rail): book > pages, with event counts per page.
2. **Page event list** (main): the events of the selected page, one row each: name, position, trigger (action button / player touch / autorun / parallel), the switch condition, number of event pages, a one-line summary of the first commands.
3. **Event detail**: the command list of an event page as readable lines (the IR rendered with the registry's labels), its conditions, its switches.
4. **Cross-references**: for a switch/variable/common event: every event that writes or reads it; for an event: what triggers it. Computed by scanning the IR, never stored (one authority per fact).
5. **Edit**: rename, reorder, add/delete a command, change a condition, through the ops; every edit is an IR diff the window shows before it writes, with undo from the history file.
6. **Graph (the "blueprint")**: nodes = events/commands, edges = switch and common-event links and door links; page-to-page links form the "pages of pages" map (for dsr-rpg: the city and its 16 interiors).

## 6. Entry points
- Desktop taskbar: db dropdown > **blueprintz** (done, opens the empty shell).
- Every pc-hq game window: **db ▾** tab (done in `piececraft-hq/pchq-board.xhtpm`: db-hq, blueprintz, sql-hq; one file, every game).
- The rpg-pet board has the same tab (done, for parity); **dsr-rpg gets it from the carbon copy** (phase 0 of `DSR-RPG-DESIGN.md`).
- Later: a book-scoped launch from the tab (passes the game's book root), and a "edit this event in blueprintz" row on an event's context menu.

## 7. Phases (each ends with a screenshot and a round trip, per the evidence rule)
- **B0 shell** (done 2026-10-11): empty window + menu entries. Evidence: the window renders; the menu data is read by the taskbar.
- **B1 read-only tree**: book > pages > events from disk (rpg-pet is the first test book: 3 rooms, ~30 events).
- **B2 event detail** read-only; conditions and switches shown.
- **B3 cross-references** (who sets / reads this switch).
- **B4 editing through the ops** with a diff preview and undo; round trip: edit in blueprintz -> the `event.ir.pdl` diff is exactly the edit -> the game behaves (play the event).
- **B5 node graph** view, then book-scoped launch from the tabs, then dsr-rpg's city/interiors as the large test (17 pages).

## 8. Risks
- Two editors for one event (events-hq and blueprintz) drifting: mitigated by one IR and one op path.
- The IR may change (event format work is live in events-hq): read through the registry and the ops, do not parse commands by hand.
- Scale: dsr-rpg is ~17 pages x many events; the page list must read lazily (one page at a time).
- A window that grows with its content (the rpg board widened from 1074 to 1149 px when the db tab was added): watch tab-bar width on the small game windows.

## 9. Open questions (recommended answer each)
1. **Name and menu.** `blueprintz` as given. *Recommend: keep.*
2. **Relation to events-hq.** *Recommend:* blueprintz = book overview first and opens events-hq for single-event edits; absorb later if it earns it.
3. **First view: list or graph.** *Recommend:* list (phases B1 to B4), graph at B5.
4. **Which events count.** Desk events + app-local packages + common events + clock rows (`time.pdl`)? *Recommend:* yes, all four as sources in `bp_sources.pdl`.
5. **One book at a time or several.** *Recommend:* one at a time (the loaded / launching book).
6. **db tab contents in games.** db-hq, blueprintz, sql-hq (done). Scope db-hq to the game's own data later? *Recommend:* later.
