# How to build "m" (move mode) and party reorder: handoff for the next agent (written 2026-11-24 by claude)

Read first: `HAI-AND-PALS-REUSE-AND-COMPATIBILITY-GUIDE-2026-11-24.md` (the contract; section 2 is the audit, section 7 the C/JS method) and the hai repo `book/27-master-and-slave-pals-the-rpg-space-shape-design-2026-11-24.md` (sections 5, 6, 9, 10). Labels: **BUILT** (ran, with evidence), **READ** (file read today), **DESIGN**, **NOT VERIFIED**, **NOT BUILT**. Line numbers are from 2026-11-24 and drift: search by the quoted names.

## 1. What the owner decided (his words, 2026-11-24)
- The party is an ORDERED list; **whoever is first is the master**, the others follow. "Its supposed to be about rearrange/order." The master has meta control of the interface and of the other bots.
- Reordering UI: on the row marked `>` (focused) press **`m`** to enter MOVE MODE (marker `↕`); the **arrow keys (up, down)** then move that row; **Enter or Esc leaves**. **`^` stays the interact-mode lock; no rename to `#`** (that would touch the renderer, the taskbar parser, wraith-alpha and TPMOS parsers; held, cost unmeasured). It must be generic: ANY ordered list (the party, a TSC sort list), not only pals.

## 2. What already exists (so you do not rebuild it)
- **The verb, in C and JavaScript (BUILT):** `REORDER|name|slot` (1-based; slot 1 = master) in `#.hai/harnecient-hai/ops/hai_world.c` and `docs/world.js`, plus `hai_world master <ledger>` and `hai_world party <ledger>`. Refusals are rows: `no such agent`, `slot out of range`, `already in that slot`, `missing arguments`. Case `cases/hai_world_reorder.pdl` (134 checks, 7 mutants) and the differential case `cases/hai_compat.pdl` (205) prove C and JS agree. Ledger of the book: `@.apps/hai-pal/ledger/world.ledger`.
- **The left bar reads a book's own party (BUILT, live):** `&.widgits/_shared-lib/petslike_core.c` `load_party()` (house commits 8980d72c0 and 8873ffed9): if the open book ships `@.apps/<book>/db/system.pdl` with a `BookParty` group, the pets bar lists `Party member1` (the first pal, hero slot) then the `BookParty` members, in that order. hai-pal has `@.apps/hai-pal/db/system.pdl` (hai, pip). Proof and the traps are in hai book 27 section 10.
- **NOT BUILT:** (a) the `m` key and move mode in the renderer; (b) an op that turns a reorder into a ledger row AND rewrites the db file; (c) the harness cases below.

## 3. The one authority (do not create a second)
The ORDER lives in the **ledger** (`REORDER` rows). `db/system.pdl` is a **PROJECTION** of it, rewritten by a named op after every successful reorder (guide section 2 item 7; `NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY`). Never edit `system.pdl` by hand to change the order, and never read it back as input to the ledger.

## 4. Where the key handling lives (READ, `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`, 24,825 lines)
- **A per-element key action already exists and is the precedent:** `backspace_action` (parse at `strcmp(name, "backspace_action")` near line 1431; Elem field declared in `khtpm_render_core.c` under `&.widgits/_shared-lib/`; dispatched near line 13985 inside the key handler: `if (focused->backspace_action[0]) { dispatch_no_quit(focused->backspace_action); return; }`). Build `move_action` the same way: an attribute on `<item>`, e.g. `move_action="'${HOUSE}/@.apps/hai-pal/ops/+x/party_move.+x' ${party.id}"`, called with a direction appended.
- **The arrow keys** are handled by `if (ks == XK_Up || ks == XK_Left)` and `XK_Down || XK_Right` (near lines 13995 and 14012): they walk the `>` cursor. Move mode must intercept them BEFORE that walk, only while `g_move_mode` is set and the focused element has a `move_action`.
- **Esc** has several branches (near 11647, 11858, 11874, 13637, 13650): the move mode's Esc must be handled first, and must not fall through to leave "In" mode (the owner's rule: Esc backs out ONE level, memory `project-esc-backs-out-one-level`).
- **The `>` / `^` marker** is drawn by the cursor-prefix code (`elem_cursor_prefix`, header comment near line 4539; `g_focus_nav` near 4276; `[>]` = focused only, `[^]` = armed). Show `[↕]` (or `[m]`) while the mode is on. NOT VERIFIED: that the glyph exists in the khtpm font; test with an ASCII fallback `[m]`.
- **Incremental reparse:** `khtpm_reparse_diff.c` (shared) copies element fields when the page is re-parsed. **A new Elem field must be added there too** or `move_action` silently stops working after the first redraw (this is the trap that bites new attributes). Read `khtpm-incremental-reparse-design` in memory first.
- **Document the attribute** in `XHTPM-PARSER-REFERENCE.md` (HQ-IQ 02-architecture) in the same commit.

## 5. The build plan, in order (each step: a case that can fail, then the real window)
1. **The op** `@.apps/hai-pal/ops/party_move.c` (C, no shell): `party_move <house> <book> <name> up|down`: replay the book's `ledger/world.ledger` (call the same rules as `hai_world`, text-include or exec it; do NOT copy the rules), append `REORDER|name|slot` through the rule or a `REFUSED` row, then rewrite `<book>/db/system.pdl` (`Party member1` = slot 1, `BookParty member1..n` = slots 2..n; max 6 members per the house limit; keep the header comment) atomically (temp file then rename), then touch nothing else. Exit 0 ok, 3 refused. Print one line. Add a `COMMAND` row to `#.ref/menu/event_commands.registry.pdl` and an event page if Blueprintz must show it (guide contract rule 1).
2. **Case `party_move`** (harness, scratch house under /tmp; copy `@.apps/hai-pal` only): moves the first pal down, asserts ledger row, `system.pdl` bytes (exact text), refusal at the ends, replay gives the same order, deleting `system.pdl` and re-running the projection verb gives identical bytes (the four tests of NO-GHOST-DATA), and mutants (off-by-one slot, swapped direction, member limit).
3. **The renderer change** (SHARED CODE: ask first; generic; no hai-specific word in it): add the `move_action` Elem field + parse + reparse_diff copy; add `g_move_mode`; `m` toggles it only when the focused element has `move_action`; Up/Down in the mode call `dispatch_no_quit(move_action + " up|down")` then re-focus the moved row (the row's nav number changes: re-find it by element id, NOT by number); Enter/Esc clear the mode. A page with no `move_action` must behave byte-identically (prove it: run the existing renderer cases and a frame diff of two untouched windows).
4. **The xhtpm row**: in `@.apps/rpg-edit-hq/board/pchq-board.xhtpm` the pets bar item `id="pt${party.#}"` (lines ~122-124) gets the `move_action` attribute. NOTE: that is the editor's own file, shared by every book; the attribute is harmless to books whose party has no ledger, so make `party_move` exit 0 with "not reorderable" for a book without `ledger/world.ledger`, and test that.
5. **Real window evidence:** drive it like a person (skill `lyfedesk-taskbar-driving`, `relay_drive`): focus a pal row, press `m`, press the down arrow, photograph (the order in the bar AND the master change: row 1 is the master), press Esc, photograph. Open the PNGs. Restore the owner's live state by hash afterwards.

## 6. Test spec (write these as harness cases; every one must be able to fail)
| # | Check | How it fails |
|---|---|---|
| T1 | `party_move` writes `REORDER` and the exact `system.pdl` | change the projection format |
| T2 | refusals at the ends and for an unknown pal leave `system.pdl` unchanged | drop a bound check |
| T3 | delete `system.pdl`, run the projection verb: identical bytes | write a non-deterministic field |
| T4 | the left bar lists the new order after a move (scratch house, delete `ui.txt` first as a positive control) | read `last_book` instead of the board's book |
| T5 | a page with no `move_action`: `m` does nothing, arrows walk as before | let `m` always enter the mode |
| T6 | Esc in move mode leaves the mode and stays In | fall through to the In-mode Esc |
| T7 | the moved row keeps focus (by id) after the re-parse | focus by number |
| T8 | a new `move_action` survives an incremental reparse | forget `khtpm_reparse_diff.c` |
| T9 | live window frame: order changed, master changed (opened by eye) | n/a (human check) |

## 7. Traps already paid for (do not pay again)
- Driving the real window: typed digits through the relay chose the WRONG book once; use `MOUSE_EVENT` clicks with the pointer moved onto the window, and the FIRST click only focuses (click twice). Details in `16.game/OPEN-A-PROJECT-AS-A-BOOK-IN-RPG-EDIT-...-2026-11-23.md` section 10.
- A test that reads a file the program did not just write proves nothing: delete `ui.txt` first (a stale copy fooled me twice on 2026-11-24).
- The editor's party group is deliberately a NONEXISTENT name (`party_row | RpgEditNoDbParty` in `@.apps/rpg-edit-hq/toy.pdl`) with a fixed hero: it never reads the DB party. That is why the book party uses its own `BookParty` group.
- The harness `EXPECT_HAS` reads only the first ~8 KB of a file: use `RUN | grep -q` plus `EXPECT_EXIT` on big files.
- Heavy work: compile and render under `cpu_gate` (one heavy job at a time; another agent's job can hold the lock: wait, never bypass); never wrap `button.sh reset` in it; the live editor runs the editor binary per click, so a rebuilt `rpg_edit_hq.+x` takes effect on the next action (backup the old one first: `/home/no/hai-live-backups/2026-11-24/rpg_edit_hq.+x.orig`).
- The renderer is shared by every window: keep window-size and layout mutations idempotent (memory `khtpm-shared-layout-caution`).

## 8. What is NOT known (ask or measure, do not assume)
The font glyph for `↕`; whether the dock/bottom-bar rows also need `move_action`; how many parsers read `^` as the lock (counted by file, not traced: renderer and taskbar 4 files, wraith-alpha 1, TPMOS 0 as a char literal); whether the editor's Play mode keeps the pets bar in the same order after a move (the bar is re-read per action: expected, not observed).

## 9. State left on the owner's machine (2026-11-24)
rpg-edit is OPEN on the owner's screen with hai-pal and the pets bar showing; the editor binary was replaced by the tested build (backup path above); `@.apps/rpg-edit-hq/state/last_book.txt` was changed by launches (original `book=tsots page=map071`, md5 c9e7c469747e, backup in the same folder). Restore only when the owner says.
