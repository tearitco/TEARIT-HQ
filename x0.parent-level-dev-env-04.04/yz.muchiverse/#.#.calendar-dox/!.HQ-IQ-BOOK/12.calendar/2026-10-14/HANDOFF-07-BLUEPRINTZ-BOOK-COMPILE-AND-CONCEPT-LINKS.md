# blueprintz as the IDE for the book, and concept links on events (2026-10-14)

Hand-off plus design summary. Full design with phases: BOOK `08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md`, **section 11** (this file points there; if they differ, section 11 wins). Written from the owner's words and from reading the real files. **Nothing in this file is built.**

## 1. The idea in the owner's words
The blueprint is an event page of event pages. It shows the entire book. It compiles all the events in a book and its pages, the common events and the plugins, as one code file that works like the game's event loop and calls the "events on page". It can modify entity events or common events as an alternative to editing by entity. It is an IDE for the book:page events. Separately: events should get concept-bank associations, because they are the most important pieces for automatic coding. Decisions given: the associations are shared (stored with the IR, read by the compiler), per event for now.

## 2. What exists (verified by reading files and counting)
- **Event format**: `event_pkg/pages/page_N/{condition.pdl, event.ir.pdl, event.pal, cmd_N.sh}`. IR rows: `NODE | id=N type=<command> | args`. Chain: IR > `event.pal` > `cmd_N.sh` > `mr_<command>.+x`. Source: `EVENTS-PAL-BUILDOUT-PLAN.md`.
- **109 `event.ir.pdl` files** in the house. Command types by count: comment 2,692; if/end 2,554 each; show_text 1,160; show_choices 773; choice_to_switch 770; control_self_switch 563; ret 294; the per-game `*_pet_says` and `*_verb` rows; about 30 types in all. Mostly imports and scaffolds, few hand-written.
- **Command registry**: `#.ref/menu/event_commands.registry.pdl` (1,268 lines). Do not hard-code command lists.
- **blueprintz**: window `&.hq-apps/blueprintz/` (B0 shell, B1 read-only list for dsr-rpg, cdda-hq, df-hq, gta-hq), design doc B0-B5.
- **Editors that write the IR**: events-hq (`khtpm_events_hq_manager.c`), event-editor, event-ez, plus the converter `tsots_event_ir.py`. Only events-hq's ops were read for this.
- **Plugins**: `PLUGINS-ARCHITECTURE-SCOPING.md` (2026-08-25): per-project list, toggle on/off, ordered, `.pal` that triggers ops. Scoping only, no code.
- **Concept bank**: `&.widgits/concept-bank/` (masters, spokes, physics and chem stubs, image words, an unwired promotion rule). It has no game, door or command nodes yet.

## 3. What is new in the design (section 11 of the BOOK doc)
1. Blueprintz also produces **`book.pal`**: one generated program for the book that calls page events, common events and enabled plugins in order. Generated from the IR, never the source.
2. Plugins are part of the book (depends on the plugin manager, which does not exist yet).
3. `META | concept | <node> | source=hand|rule|model` rows on events; a validator that rejects nodes missing from the bank; concept grouping in the book view; "events for concept X" query.

## 4. Why concepts on events
Automatic coding needs (a) retrieval of the closest existing events for a new one ("shopkeeper greets the player" finds `greeting` and `trade` events), (b) a typed vocabulary (the ~30 command types as nodes with usual parts), (c) a judge (the IR must parse, compile and run through the relay), and (d) scored outcomes for the bank's still-empty promotion ledger. Links give (a) and (b); (c) exists; (d) starts when generated events pass or fail.

## 5. Facts found while checking that the links are safe (read, not run)
- The readers I looked at take only `NODE` rows, so an added `META` row is ignored by them.
- events-hq edit and delete copy other rows forward, so a concept row survives; add appends.
- **Hazard**: edit and delete hold the whole IR in an 8,192-byte buffer. A larger IR is truncated silently on edit or delete. Fix before adding rows. Not reproduced.
- Not run: a live edit with a concept row present; event-editor, event-ez and the converter with such a row.

## 6. Next steps in order
1. Reproduce the 8 KiB truncation on a copy; fix it; negative-test.
2. Run one live events-hq edit on a scratch copy of an event that has a `META | concept` row; confirm the row and the compiled `event.pal` are right.
3. Hand-author the first game nodes in the bank (door, greeting, trade, travel, time, switch, the command types) and the validator.
4. Seed links by rule over the 109 events; read the results by eye for wrong matches; write the "events for concept X" query.
5. B4b: compile one book (rpg-pet) to `book.pal`; compare its behaviour with the per-event runs; keep both until they match.
6. Plugins after the plugin manager exists.

## 7. Open questions for the owner
- `book.pal`: beside the per-event pals first, or replace them? (recommend beside)
- Order of calls in the loop: copy RPG Maker's trigger order? (recommend yes, written down first)
- Who creates the game nodes? (recommend hand-authored)
- Concept links per page or per command later? (recommend later)

## 8. Honest limits
Everything in sections 3, 4 and 6 is a plan. Section 5 is a reading of source, not a run. The counts in section 2 were counted from files on disk on 2026-10-14. The owner has not reviewed the refinement text.
