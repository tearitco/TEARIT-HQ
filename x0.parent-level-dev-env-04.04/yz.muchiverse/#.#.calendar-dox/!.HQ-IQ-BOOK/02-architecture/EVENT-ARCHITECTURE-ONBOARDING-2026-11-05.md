# Event architecture: start here (onboarding guide, 2026-11-05)

For a new agent or the owner. This page does NOT repeat the detailed docs; it tells you which layer you are in, where a new thing goes, and the traps. Labels: **DOC** = from the doc named, **OBS** = I ran or read it in the 2026-11-04/05 session (path given). Where a path is relative it is under the house root `44.xyz.01.00`.

## 1. Four layers, one direction (things flow down; never skip a layer)

| layer | what it is | the file you edit | the doc |
|---|---|---|---|
| 1. When | the clock fires named events; `common:<event>` rows | `@.apps/<app>/time.pdl` (AT rows), opt-in `survivor_clock.pdl`, `world_clock.pdl`, `net_say.pdl` (all ship `ENABLED \| 0`) | `16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md` "Common events" |
| 2. What happens | event pages (a blueprint: IR nodes, trigger, compiled `event.pal`, one wrapper per command, a C op does the work); verb pages give a game's buttons the same shape | `@.apps/<app>/events/<name>/event_pkg/pages/page_N/` (edit with blueprintz, or the IR) | `02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md`, `16.game/EVENT-VERB-PAGES-DESIGN.md` |
| 3. The record | one append-only ledger per game: `EVENT\|ts\|actor\|verb\|target\|at` -> resolver -> `EVRES` / `REJECT` (verbs give_item take walk dig build craft); items go through the item ledger (hash chain, atomic `trade`) | `state/events.txt`, `books/<book>/` | `16.game/S1-EVENT-LEDGER.md`, `16.game/SHOP-SLICE-SPEC-2026-11-04.md` |
| 4. What you see | the core writes `state/ui.txt` after every verb; the `.xhtpm` template binds `${key}`; popups are overlay rows | `board/pchq-board.xhtpm` + css | `02-architecture/XHTPM-WINDOW-BUILD-GUIDE.md`, `18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md` |

Pal vs events (which one is the TEST and which is the GAME): `02-architecture/PAL-VS-EVENTS.md`. Events cannot replace the harness; a harness alone cannot be played by a person. Both, always.

## 2. One traced path (OBS: built and run 2026-11-04, scratch + one live backup run)

`time.pdl` AT row -> `common:needs_tick` (or `world_flow`, `world_rain`, `world_fire`, `pet_say`) -> shared core `clock_event` in `&.widgits/_shared-lib/petslike_core.c` -> only if the opt-in file says `ENABLED | 1` -> the op (`survivor_op` etc.) changes state files -> the core rewrites `state/ui.txt` -> the template redraws. A player's click takes the same last three steps through a verb (`rpg_edit_hq.+x fire lshop buy`) and, for items, the item ledger. Evidence: harness `needs_clock`, `world_clock`, `pet_say_clock`, `coin_shop_ui`, and the play tables `turn_clock_rpg_edit_hq`, `lamps_clock_*` (`&.widgits/_shared-lib/harness/README.md`).

## 3. Where does my new thing go? (decision list)

1. It happens on a schedule -> layer 1 (a `time.pdl` AT row calling a `common:` event) and ship it OFF.
2. It is a rule that reads/changes game state -> a C op behind an event page (layer 2) or a verb; never a new per-game renderer branch.
3. It must be remembered or traded -> the ledger (layer 3). Items: the item ledger only; coins are the item `coin.play`. Exchange-hq is a paper currency order book, not for game items.
4. It must be shown -> publish keys in the core's ui writer (layer 4) and bind them in the template. Popups: copy the rpg-space helm `ask-ov` pattern.
5. Need to prove it -> a pal harness case + a mutation run that must FAIL, then a live frame. No Python, no new `.sh` (AGENTS.md).

## 4. Traps (each was hit; paths are proof)

- **Publish every key a template uses, even empty.** A key published only while a popup is open makes `layout_check` report an error (the coin popup, 48 -> 44). Check with ALL state files: `layout_check board/pchq-board.xhtpm --ui state/ui.txt --ui board/state/ui.txt --ui board/state/sides.txt --ui board/state/pal.txt --ui board/state/prefs_ui.txt` (one `--ui` gave 44 errors, all of them gives 19; the 19 are taskbar-published, conditional, or data-driven menu slots from `toolbar.pdl`).
- **Menu slots (`book_post_N_*`, `page_post_N_*`) come from `toolbar.pdl` SECTION rows**; the key names are built, so grep finds no writer. Not dead.
- **Overlay text rows are one row high** unless the row has class `ov-wraptext` (2026-11-05); otherwise a long line overlaps the next row.
- **Tab dropdowns scroll** when they would run off the screen (`dropdown_max_rows` in `#.desktop/hq_ui.pdl`); there are no nested submenus inside a board. Split the list or use a popup.
- **Quarantine:** ops that are locked exit 4 without `QUARANTINE`; a `RELEASE` file opens one book for one named caller. Only the owner lifts it (today: `shop_op` for the editor coin shop).
- **A rebuilt binary does nothing for a running window or engine** until it is relaunched; relaunching leaks the old engine (stop it by exact pid). Test state you change on the live desk must be restored and verified by hash.
- **Data values that mean "nobody":** `possessed_id` can be `none`, `-`, `.` or empty (the xelector look had a bug from this).
- Paths with `&` and `@` need quotes; `git commit -- <paths>` takes the whole file; never `pkill -f` in a tool shell.

## 5. Not written yet (honest gaps)

No single diagram of all four layers on one page; the item ledger and the S1 event ledger are two different ledgers (items vs events) and no doc says how a trade shows up as an `EVRES` row (I did not check whether it does); the owner's "Xcode views / blueprintz" visual editor (`18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md`) is a vision, not a tool.
