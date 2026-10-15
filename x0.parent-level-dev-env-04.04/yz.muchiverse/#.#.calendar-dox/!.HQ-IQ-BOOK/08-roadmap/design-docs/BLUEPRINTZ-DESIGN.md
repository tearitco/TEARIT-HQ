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
- **B1 read-only tree**: book > pages > events from disk (rpg-pet is the first test book: 3 rooms, ~30 events). **2026-10-12 later:** `&.hq-apps/blueprintz/ops/bp_tree.sh` fills the `BP-TREE` block in `blueprintz.xhtpm` from `event.ir.pdl` under dsr-rpg, cdda-hq, df-hq, and gta-hq (12 names each). The window shows that list after it is opened again. Still read-only. B2–B5 are unchanged.
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

## 10. gta-hq, cdda-hq, df-hq and the `.pal` macro map (2026-10-12, claude; for Grok to confirm)
**What changed.** The owner's new line (handoff doc section 0): wire gta-hq / cdda-hq / df-hq events to events-hq and give blueprintz "a macro map in the Unreal-blueprint sense: visual scripting whose underlying code is `.pal`". Section 3 above says `event.ir.pdl` stays the one event format.
**Reconciling the two (proposal, with the evidence).** They already agree one level down: an event's `event.pal` is **compiled from** `event.ir.pdl` (first line of `@.apps/dsr-rpg/events/play/event_pkg/pages/page_1/event.pal`, *verified*: `# event.pal - COMPILED from event.ir.pdl (tsots_event_ir.py, shape of khtpm_events_hq_manager.c output)`). So the "underlying code is `.pal`" is true today at the compiled layer. Recommendation: **the node graph edits the IR (one source of truth), the compile step emits the `.pal`**, and the graph shows both: the IR node and the `.pal` lines it compiles to (read-only), so the owner can see the code under each block. Only if a block cannot be expressed in the IR is a raw `.pal` block needed, and then it should be stored as an IR node of type `pal` (`NODE | id=N type=pal | <lines>`), not as a second format. Not verified: how many registry commands the compiler covers; check `tsots_event_ir.py` before promising this.
**Books and pages for the three games** (what blueprintz must show). dsr-rpg/cdda-hq/df-hq copies carry their own app-local event packages (`@.apps/<game>/events/*/event_pkg`, ~25 in each scaffold, 424 files), the clock rows in `time.pdl`, and DB rows. The scaffolds' event commands are separate registry entries (`cdda_hq_verb`, `df_hq_verb`, `gta_hq_verb`, ...), so `bp_sources.pdl` per game lists that game's own events. **Z-levels change what a "page" is**: in dsr-rpg a page = a room reached by a door; in cdda-hq (one map, z levels, handoff section 3 P2) a page should be a **z-level or a region of the single map**, and a door is an ordinary cell, so the page-to-page links of the graph view become stair/ladder links. Decide this before the graph view (B5).
**gta-hq specifics.** Vehicles and people are brixel assemblies (`BRIXELS-DESIGN.md`): their "does" sentences are events on the pieces (door open, windshield broken, engine on), so blueprintz needs a view **per piece type**, not only per map event. Events needed first: enter/exit vehicle, drive step, traffic light phase (clock rows), crash. Existing command stubs for GTA are "movement + transfer + play_se" only (`TILESETS-EVENTS-AND-GAME-CLONES.md`).
**Phase impact.** B1 (read-only tree) can start on dsr-rpg and the three scaffolds now (they have the same event layout); B5 (graph) waits for the page decision above. Evidence rule unchanged: each phase ends with a screenshot and a round trip.

## 11. Refinement 2026-10-14 (owner's words; this section overrides sections 3 and 9 where they differ)
**Owner, 2026-10-14:** "the blueprint is an event page of event pages. It is supposed to show the entire book. It compiles all the events in a book and its pages and common events and plugins. All the code as one code file compiled like an event loop for the game. It calls the 'events on page'. It can be used to modify entity events or common events as an alternative to doing it by entity. It is like an IDE for the book:page events." Concept associations: shared (the compiler, so every editor gets them), per event for now.

**What changes in this design**
1. **Blueprintz has a compiled output: `book.pal`.** Sections 3 and 5 described a viewer/editor only, with compiling left to each event. The blueprint is now also the whole-book program: one generated `.pal` for the book that works as the game's event loop and calls each page's events (events of the page, the common events, the enabled plugins), in a defined order. **Not built. Nothing of it exists yet.**
2. **`book.pal` is generated, never hand-edited and never the source.** Section 3's rule stands: `event.ir.pdl` stays the one source of truth. Every edit made in blueprintz is an IR edit through the existing events-hq ops; `book.pal` is rebuilt from the IR (the same IR > `event.pal` > `cmd_N.sh` > `mr_*` chain, one level up). The IDE comparison holds: source is the IR, build output is the pal.
3. **Plugins are part of the book.** The plugin design (`PLUGINS-ARCHITECTURE-SCOPING.md`, 2026-08-25, design only, no code) says: per-project list, each plugin toggleable on/off, applied in a defined order, the unit is `.pal` script that triggers compiled ops. Blueprintz compiles the enabled list of the loaded book into `book.pal` and shows it in the tree. Because that design is scoping only, the plugin part of `book.pal` is a later phase and depends on the plugin manager existing first.
4. **Two ways to edit, one result.** By entity (events-hq on one entity) or by book (blueprintz). Both write the same IR files. Section 9 question 2 is answered: blueprintz absorbs events-hq's editing role in the end, but the op path stays single.
5. **Concept associations (new).** Every event may carry concept-bank links: `META | concept | <node> | source=hand|rule|model`. Decisions: shared (stored with the IR and read by the compiler, not by one editor), per event for now (per page or per command later). Blueprintz groups the book by concept, `book.pal` can list the concepts of each event, and the concept bank gets its first real data from it. Rules: a link must resolve to a real bank node (a validator rejects an unknown node, with a negative test); model-suggested links are not trusted until they beat the rule baseline on held-out events.

**What was checked in the code (read, not run, 2026-10-14)**
- Readers of `event.ir.pdl` select only `NODE` rows (`khtpm_events_hq_manager.c` lines 441 and 852; `ee_compose_frame.c` line 132), so an added `META` row is ignored by the compile and the display paths read.
- events-hq edit and delete (lines about 1140-1200) copy every non-matching row forward, so an added `META | concept` row survives them; add (line about 1233) appends. **Hazard found, reproduced and fixed (2026-10-15):** those edit and delete paths buffered the whole file in `char all[8192]`. On a scratch copy of a 110-row, 11,016-byte IR, the unmodified manager lost 29 rows on one `delete:1` (81 rows left, file cut at 8,164 bytes) with no message. The fix is a growing buffer (`ir_buf_add` in `khtpm_events_hq_manager.c`); the fixed build on the same input kept 109 of 109 rows after delete and after `edit:50`, and kept `META | concept` rows at the top and at the end of the file. Measured on scratch copies only; no live window and no real event was touched; the running events-hq manager still has the old binary until it is rebuilt and relaunched.
- Run (scratch manager process on a scratch package, relay file not used): `delete:` and `edit:` with `META | concept` rows present, old and fixed build. Not yet run: the events-hq window itself, event-editor, event-ez, and the `tsots_event_ir.py` converter with such a row. Still a limit: `compile_page` reads at most 128 NODE rows (`MAX_IR_NODES`) and says nothing when it stops; no real file has more than 61.
- The IR in the house today (counted 2026-10-15, sessions folders excluded): **3,662** `event.ir.pdl` files with 12,838 NODE rows in 52 command types. 2,487 files are the imported tsots maps under piececraft-hq; 109 are rpg-pet's own events; the game scaffolds (dsr-rpg, cdda-hq, df-hq, gta-hq, mcl-hq, tcg-rpgce, civ-rpgce) have about 105 each. Most common types: comment (2,692), if/end (2,554 each), show_text (1,160), show_choices (773), choice_to_switch (770), control_self_switch (563). Few are hand-written. (The first version of this section said 109 files; that was rpg-pet only, a counting mistake of mine, corrected here.)
- Size limits seen in real files: the largest IR is 8,494 bytes (61 NODE rows, `piececraft-hq/.../maps/tsots/map1004/ev/4`), so **one real file already exceeds the 8,192-byte edit buffer**. No file has more than 128 NODE rows; `compile_page` stops reading at 128 (`MAX_IR_NODES`) without saying so.

**Phases, amended**
- B0 shell: done. B1 read-only tree: done for dsr-rpg, cdda-hq, df-hq, gta-hq (list only).
- **B1c (new):** concept rows in the IR plus the validator; the "events for concept X" query; fix the 8 KiB buffer first.
- B2-B4 as before. **B4b (new):** compile the loaded book to `book.pal` (events and common events only), run it against one book (rpg-pet), compare its behaviour with the per-event runs.
- B5 graph as before. **B6 (new):** plugins in `book.pal`, after the plugin manager exists.

**Open questions (recommended answer)**
1. Does `book.pal` replace the per-event `event.pal` at run time, or sit beside it? *Recommend: beside it at first (the per-event files keep working), replace only after B4b shows the same behaviour.*
2. Order of calls in the loop: page events by trigger type (autorun, parallel, touch, action) as in RPG Maker, common events by switch. *Recommend: copy RPG Maker's order and write it down before building.*
3. Concept granularity beyond per event, and who creates the game nodes (door, greeting, trade, travel, time, the command types). *Recommend: hand-authored nodes, per event now.*

## 12. Blueprintz as the debugger: page view, Play, Pause, Step (owner 2026-10-15; design, nothing built)
**Owner, 2026-10-15:** blueprintz belongs under **player** as well, because it is about stepping through the visual code, like a debugger. It should show **all events on the page**; pressing **Play** runs the page; an event like Change Gold is **waiting for its trigger**, so it waits. "We will have a play, and pause, and step through, for debugging." Decision recorded: the window keeps its db-dropdown entry (book editor, sections 1-11) and gains a player-menu entry (debugger); one window, two entry points. Editing the player menu rows is an owner-owned config: confirm before changing it when this is built.

### 12.1 What exists today (read from the files, 2026-10-15; not run unless stated)
- **Play already exists as a desktop-wide switch.** `#.desktop/khtpm_play_mode.state.txt` holds `mode=on` (optionally a second line `playtest=1`: the game runs and editing is allowed). The player cell of the taskbar toggles it (`khtpm_taskbar_manager.c`, "1.play: ON/OFF"); pc-hq's `pchq_board_action.sh` has the sub-actions toggle, start, playtest, stop, synch, save:N, load:N. Older readers test only the first line, so extra lines are safe for them.
- **Triggers only fire while `mode=on`.** `tsots_touch_event.sh` (on-touch, event-touch, player-touch) exits at once unless `mode=on`; edit mode (play off) opens the entity's context menu instead.
- **One choke point runs events: `play_event.sh <pkg> [house] [trigger]`.** It picks the highest-numbered page whose `condition.pdl` trigger equals the trigger passed (default `on-click`) and runs its `event.pal`. It is called by the touch script, by `rpg_pet.c`, by the pals' `Play` method (`cursword/meta.pdl`) and by the events-hq Play button (harness `test_h8_play_button.sh`, 2026-08-25, checks a gold change). `rpg_pet.c` plays its events and the clock's `common:<event>` pages through it (read). `doom_event.sh` and `chess_event.sh` do not mention it, but that needs care: they are **verb executors** that event commands call (2,557 event `cmd_N.sh` files call them), so those runs are already inside `play_event.sh`. The gap is the **direct callers** (see 12.8).
- **Triggers in the house** (all `condition.pdl`, sessions excluded): on-click 2,634; on-touch 87; parallel 65; Autorun at least once (`greet_player`); plus the rpg-pet clock/time triggers. **Waiting is the normal state** of an event.
- **Pause and Step do not exist.** The old inert play/pause rows were removed 2026-09-14 (comment in the taskbar manager); `play_event.sh` has no step, trace or breakpoint (grep). The mode file is on/off only.
- **Per-event ledgers exist** for rpg-pet (`events/<name>/master_ledger.txt`, `history.txt`, e.g. `Play event page_1 (trigger=clock) for day_tick`), so a run history is partly recorded already.
- **A compiled event is simple.** For the Change Gold events: `exec cmd_1.sh`, `exec cmd_2.sh`, `halt`. Events with if/else compile to labels and jumps (not read in this pass), so command stepping must handle those.

### 12.2 The first example
`m8_redhorned` (template at `_.monads/_.muchi-pet/entities/m8_redhorned`): page 1, trigger `on-click`, two Change Gold commands (+10, +25), and the entity has a `gold.txt`, so the effect is a number. A live copy sits in the owner's data (`xyzfs/users/<owner>/home/livedesk/pals/cursword/inventory/m8_redhorned`): **owner data, read only for us, never edited or staged**. It is an inventory item of the pal `cursword`; I did not establish that it is placed on a map page. The demo therefore runs on a **scratch page** holding a copy of `m8_redhorned` and of `greet_player` (common event: Autorun, Change Gold +1 and +777, and two rows with `id=4`, one empty: an IR defect blueprintz should flag).

### 12.3 Behaviour
- **Page view**: one row per event on the selected page: name, position, trigger, condition, **state**. States: `waiting (on-click)`, `queued` (trigger arrived while paused), `running (cmd 2/3)`, `done`, `error`. Values the event changes (gold) are shown beside the row, before and after.
- **Play** = the existing switch (`mode=on`), the same one the player menu uses; Autorun events run at once, parallel events loop, the rest go to `waiting`.
- **Pause** (new): triggers that arrive are not run and not dropped; they are appended to a queue and shown as `queued`.
- **Step event** (new): run the oldest queued event, then stay paused.
- **Step command** (new): run exactly one command of the event that is mid-run, show the values after it.
- **Fire** (per row): send the trigger a click would send, for a waiting event. A debugger convenience; it goes through the same `play_event.sh`.
- **Stop** = `mode=off`, as the player menu's stop.

### 12.4 Where the new state lives (marker style, append-only, no new format)
- `#.desktop/blueprintz_debug.txt`: `paused=1` or missing. **Missing file = run exactly as today**; nothing changes for anyone who is not using blueprintz. This is the default-safe rule and gets a regression case.
- `&.hq-apps/blueprintz/state/queue.txt` (append-only: `QUEUED | ts | pkg | page | trigger`) and `trace.txt` (append-only: `START`, `CMD n`, `END`, `ERR` rows per run). The window's change detection is the growth of these files, not mtime.
- The gate sits in `play_event.sh`, the one choke point: if `paused=1`, append to the queue and exit instead of running. Step runs the queue entry with the gate bypassed.
- Command stepping: **decided by the owner 2026-10-15: use the prisc VM** (see 12.8), not a per-command shell loop.

### 12.5 Phases (each ends with a screenshot or state-file evidence and a negative test)
- **D0 read-only page view, visualize and flag only** (owner: "just visualize. flag problems"): events, triggers, `waiting` state, from the files, plus the problem flags of 12.8. Evidence: screenshot, and each flag proven on a known-bad copy and absent on a good one. Estimate: one focused session (a guess).
- **D1 trace**: `play_event.sh` appends START/CMD/END rows; Change Gold shows its before/after gold. Regression: with no debug file, the events behave and the output is the same as before.
- **D2 pause and queue**: with `paused=1` a relay click on the scratch entity leaves gold unchanged and adds a queue row; negative: remove the file and the same click changes gold by 35.
- **D3 step event** and **Fire**.
- **D4 step command in the prisc VM** (hook in the main loop, 12.8); **D5** breakpoints and watch values.
- Player-menu rows (pause, step) and the player entry for the window: last, after confirming with the owner.

### 12.6 Risks and limits
- `play_event.sh` is shared by many callers; the gate must be tiny and default to run. Callers that do not go through it are not covered until checked.
- The relay is not real hardware input; a click that works through the relay can still fail with real focus.
- `play_event.sh` is shell; window logic stays C or pal, so the page view and queue handling are C (manager) and only the one-line gate lives in the script.
- `compile_page` reads at most 128 command rows and says nothing when it stops (see section 11).
- Pause covers events started by triggers; it does not freeze the rest of the game (clock, daemons).

### 12.7 Questions for the owner (recommended answer)
1. Does Pause also hold the world clock, or only events? *Recommend: only events at first; the clock is a separate daemon.*
2. Should Fire be in the first version? *Recommend: yes.*
3. Where do the player-menu rows go (after "play", before "reset")? *Recommend: ask when built, since the menu is yours.*

### 12.8 Owner decisions and findings, second pass (2026-10-15)
**Decisions (owner):** (1) step with the **prisc VM**; (2) first build is **just visualize, and flag problems**; (3) `m8_redhorned` is an inventory item of `cursword`, and it can still fire its event through cursword: exactly the kind of path to **suss out**; (4) **all event firing should go through `play_event.sh`**, including doom and chess. Recorded here; nothing is built.

**The one-door rule (proposed wording for the owner to approve):** every firing of an event page goes through events-hq's `play_event.sh`; scripts like `doom_event.sh` and `chess_event.sh` are verb executors that an event's commands call, and nothing else calls them. Reason: pause, trace, step, page conditions and the run history all live at that door.

**How cursword fires it (read, not run):**
- cursword's `Play` method (`cursword/meta.pdl` line 13, also the seed and `_.monads/_.cursword`) runs `$1/_.monads/_.muchi-pet/ops/play_event.sh`. That file is a 1,066-byte **compatibility shim** (2026-08-29) that `exec`s the real `&.widgits/events-hq/ops/play_event.sh` (10,849 bytes). So a gate placed in the real script covers cursword. Good: one place.
- **Flag 1 (found):** the live copy of `m8_redhorned` (owner data, read only) has a `Play` object whose action is an **absolute path into another checkout** (`.../NNEST-11.17/...`). That directory exists on this machine but the script path inside it does not, so that Play object points at nothing. Only one `objects.pdl`/`meta.pdl` carries such a path (grep count 1). Not fixed (owner data); blueprintz should flag it.
- Other shim users (read): `m6_golddeity`'s `objects.pdl`, `_.monads/_.cursword`, and two test harnesses hard-code the shim path. They work only because the shim exists; keep it.
- `m8_redhorned` page 2 has trigger `pp6`, which matches nothing the game sends (flag it as an unused trigger, not an error); an `event.pal` sits loose in its `pages/` folder.

**Direct callers that skip the door (read, not run):**
| Caller | What it calls | Note |
|---|---|---|
| `piececraft-hq/ops/pc_entity_ctx.sh` (lines 78, 101-117) | `doom_event.sh` / `chess_event.sh` with a verb | play-mode entity menu rows call the verb directly |
| `piececraft-hq/ops/pc_menu_input.c` (lines 428-431) | `doom_event.sh <cmd>` then `kill_event` | same, from C |
| `piececraft-hq/ops/pchq_board_action.sh` (lines 260-289) | `start_game`, `stop_game`, `save_slot`, `load_slot` | session verbs (title screen, saves) |
| `@.apps/chess/open.sh` (line 10) | `chess_event.sh start_standard` | launcher |
Not decided: which of these should become an event page fired at `play_event.sh` and which are session verbs that are not events at all (new game, save, load). Recommend: audit table first (this one), then convert one row at a time, each with a relay case and a negative test (the old path must no longer change state).

**Stepping in the prisc VM (read):** `_shared-lib/system/prisc+x.c` (1,619 lines) has no step, trace or breakpoint support (only a commented-out debug line). Its run loop is `int pc = 0; while (pc < inst_count) { Inst i = program[pc]; ... }`, so a hook at the top of that loop is the natural place: when a step file is named in the environment, write `pc`, the op and the registers to a state file, then wait for a step marker (append-only file growth, not mtime) before executing the instruction. Open points: (a) there are several copies of `prisc+x` in the house (per-project `system/` folders, `world-manager/system/prisc+x`), so it is unknown which binary runs events; the hook goes in the shared source and each copy needs the rebuild; (b) the default must be no hook, with a regression case; (c) `exec cmd_N.sh` is one VM instruction, so one VM step runs a whole shell wrapper: that is the right size for "step command", and a finer "step instruction" view is available for jumps.

**Problem flags for D0 (each is a rule that reads files; each gets a known-bad and a known-good case):**
1. duplicate `id=` in one `event.ir.pdl` (`greet_player`); 2. NODE with empty params where the registry requires some (`greet_player` id 4); 3. command type not in `event_commands.registry.pdl`; 4. `event.pal` older than its IR by content (recompile needed), judged by content, not mtime; 5. page with a trigger no sender uses (`pp6`); 6. page without a `condition.pdl` or without `cmd_N.sh` for a NODE; 7. object or method action that is an absolute path to another checkout, or points at a missing script (`m8_redhorned` Play); 8. more than 128 NODE rows (compile stops silently); 9. IR larger than the old 8 KiB limit (info only, fixed); 10. stray files in `pages/`.

### 12.9 Flags: scan, fixes and findings (2026-10-15, owner: "fix the flags first")
**Scan** (one-off read-only script over all 3,662 IR files outside sessions; the D0 flag rules will be C in the manager, this only sized the problem). Counts are files; "tracked" means in git.
| Flag | Found | Action |
|---|---|---|
| F1 duplicate ids | 1 (`common_events/greet_player`, tracked) | **Fixed**: removed the empty duplicate row, recompiled with the current compiler (pal now `cmd_2, cmd_3, cmd_4, halt`; it had `cmd_4` twice). Wrappers `cmd_2/3/4.sh` were regenerated in today's template (`MUCHI_TARGET_ENT` override). Stale `cmd_5/6/8/10.sh` remain in that folder (not removed). |
| F2 empty params | 3 (1 tracked = greet_player; 2 in owner data, `tax_robot`) | greet_player fixed; owner data left. |
| F3 type not in registry | `civ_rpgce_verb` 9, `tcg_rpgce_verb` 9 (tracked); `ret` 294 (6 tracked; generated pet-trainer state); `transfer_desk` 2 (untracked); a few legacy-format files | **Fixed** civ/tcg: two registry blocks added (same shape as `mcl_hq_verb`). `ret`, `transfer_desk`: not fixed; need a decision whether they are compiler-internal types the registry should list. |
| F6a page without `condition.pdl` | 10 (6 tracked) | Not fixed. Real consequence: `play_event.sh` skips a page with no `condition.pdl`, so such a page can never run. Tracked ones are legacy-format `pets/` and `asa-&-ava/` pieces and the empty `m6_golddeity` page. |
| F6b NODE without any `cmd_N.sh` | 1,743 (38 tracked) | Not fixed; needs a closer look (tsots imports may compile differently). |
| F7 wrapper targets a missing binary | `m8_redhorned` template: `cmd_1/2.sh` ran `../../ops/+x/mr_change_gold.+x`, which no longer exists (only `mr_monster_extract.exe` is left there) | **Fixed**: recompiled both commands. **Evidence (real prisc VM in a scratch house):** recompiled wrappers take `inventory.txt` `qolq` 35 to 70 (+10, +25); the old wrappers from git HEAD leave it at 35 (control). |
| F7 absolute path in a compiled pal | 731 of 6,501 `event.pal` contain a `"/home/` path (696 in piececraft-hq, 4 each in the scaffolds and rpg-pet); 2 point into the old `NNEST_CLEAN_PARENT` checkout | **Not fixed**: the compiler/converter bakes the checkout path into `ecall` lines; the fix belongs there (anchor like the wrappers do). Moving or renaming the checkout breaks these events. |
| F7 object/method path | `m8_redhorned` live Play (old checkout) and `Menu`/`Events (ez)` rows (two more old-checkout paths) | Play **fixed** (see 12.10). Menu and Events (ez) rows in the live copy **not fixed**: the permission layer refused the second edit of the owner's file; the tracked template got them (house-rooted). |
| F10 stray files in a page dir | 4 (all tracked) | No fix. Rule correction: `text_N.txt` (show_text storage) is legitimate and must be whitelisted; `piece.pdl`/`state.txt` are the legacy `asa-&-ava` piece format. |
| F8 more than 128 rows | 0 | Documented only. |

**Findings that change the design (read and run on scratch copies):**
1. **Play fires more than the entity.** Running cursword's Play command on a scratch copy printed `ran entity page m8copy`, then `ran common event nested_inner` and `ran common event palcraft_sign_onclick`: `play_event.sh` also fires every common event whose trigger matches. It wrote to the real tracked ledgers of those common events (`master_ledger.txt`, `history.txt`, `messages.txt`); I restored them with `git restore` on exactly those paths. **A test of Play must run in a scratch house, never the real one**, and the page view must list the common events a trigger will also fire.
2. **The gold number to watch is `inventory.txt` `qolq`.** The VM run changes `inventory.txt`; `play_event.sh` copies it to `gold.txt` afterwards.
3. **`[Prisc Error] Could not open ops file: .../system/default_op.txt` is noise** for these events: it appears in the working run too. `default_op.txt` lives one level up from `system/`; the script runs the VM from `system/`. Harmless today, but any op defined only in that file would be missing.
4. **Tests that touch the real house dirty tracked files.** Same lesson as the single-shared-checkout rule: use a scratch house.

### 12.10 The shim and m8_redhorned (owner 2026-10-15: "get rid of?" / "update to be in house")
- **Why the shim existed:** per-entity data files hard-coded the old path `_.monads/_.muchi-pet/ops/play_event.sh` (2026-08-29 move); the shim forwarded to the real script so those files kept working.
- **Removed.** All 11 references were repointed to `&.widgits/events-hq/ops/play_event.sh` (house-rooted through `$1`, `&amp;` in `.chtpm`): seed and monad cursword `meta.pdl` + `menu.chtpm`, `m6_golddeity` and `m8_redhorned` `objects.pdl` (tracked), the owner's live cursword `meta.pdl` + `menu.chtpm`, the live `m8_redhorned` `objects.pdl`, and two test harnesses under `xyzfs/users` (path variable only). Each changed line count checked (1 or 2 per file); originals kept with checksums outside the repo for this session. Then `git rm` of the shim. No references remain (history logs excluded).
- **Not yet verified on screen:** the cursword Play menu item itself (a menu popup); the string was run as the dispatcher runs it, on a scratch copy.
- **Owner data touched** (path strings only, with the owner's instruction for `m8_redhorned` and for the shim): live `m8_redhorned/objects.pdl` Play row; live cursword `meta.pdl`, `menu.chtpm`; two harness scripts. **Still pointing at the old checkout in the live `m8_redhorned`:** the `Menu` and `Events (ez)` rows (targets missing). The permission layer refused that edit; the exact two replacement lines are the ones now in the tracked template (`sh -c 'exec "$1/_.monads/_.muchi-pet/ops/open_rp_menu.sh" "$0"'` and `... open_event_ez.sh" "$0" "$1"'`). Needs the owner's go-ahead or their own edit.

### 12.11 The VM does not step (checked, owner: "it should be able to step")
`_shared-lib/system/prisc+x.c` is the canonical source (the wsr-pal, pal-chain, lpns and hai-horn build scripts compile their own copies from it). Events run the binary `101.mutaclsym*/system/prisc+x` (two such directories exist; `play_event.sh` takes the first). It has no step, trace or breakpoint; it reads only `PRISC_ACTIVE_TARGET`, `PRISC_KEY`, `PRISC_PROJECT_ID`, `PRISC_PROJECT_ROOT`. Adding the hook in the canonical source and rebuilding the copies is the work of phase D4 (design in 12.8).
