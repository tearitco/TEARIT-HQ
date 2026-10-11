# Layout Studio v2: layouts + RPG Maker style events, authored visually, run as pal (design)

2026-10-11, claude (Sonnet 5.5), from the owner's direction. **Design only, nothing here is built.** Facts marked (verified) were read in the repo today; everything else is a proposal. Supersedes nothing: v1 (`LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`, `@.apps/layout-studio/README.md`) stays the data/audit layer this sits on.

## 1. The goal in one paragraph
One studio where a person (or a cheap agent) builds a game window the way the house already builds them: an `.xhtpm` layout (panels, **pinned side bars**, bottom-bar cells, canvas) plus **events** in the RPG Maker sense (Show Text, Show Choices, Change Gold, Transfer, conditions, move routes) that compile to **pal** and run. The sidebars and windows are entities: they open from the studio, can be collapsed into the bottom toolbar, and every game (rpg-pet first) is "a book + a layout + events", not a new C program.

## 2. What exists today (verified)
- **Layout data layer (v1)**: `layout_check` validator + generated `CATALOG.md`, five templates, `layout_flow` (check / navmap / press: drives a window by its real nav numbers through the relay), `pet_gen`. README: "Not an editor": no palette, no drag and drop, no preview.
- **Pinned side bars (2026-10-11)**: `canvas-overlay-left|right` + `ov-pin` in `khtpm_core_render.c`, width from css or `side_style.pdl`, `side_toggle` op, bottom-bar cells. Verified in the pc-hq sandbox board; not yet on livedesk (hotbar-desk is a separate `vars-positioned` window), the real pc-hq board, or rpg-pet. Spec: `KHTPM-DOCK-ELEMENT-SPEC-2026-10-11.md` (revised).
- **RPG Maker events already exist as ops (events-hq)**: `&.widgits/events-hq/ops/mr_*.c` are the event commands, one compiled op each: `mr_show_text`, `mr_show_choices`, `mr_scrolling_text`, `mr_input_number`, `mr_select_item`, `mr_change_gold`, `mr_actor_string`, `mr_character`, `mr_move_to_entity`, `mr_transfer_desk`, `mr_world`, `mr_clock_common`, `mr_read_receipt`; plus `common_events_manager.c`, `play_event.sh`, `event_page_to_pal.sh` (exports an existing page as its own pal). Authoring: an entity's `event_pkg/` holds pages; `event.ir.pdl` is the node list (`NODE | id=1 type=show_text | text=...`) and `event.pal` is the lowered form (`show_text "..."`, `ret`; the pal calls `exec cmd_N.sh` which runs the op). The compile chain (verified in `mr_show_text.c`): event-ez authors a page -> compiles to `event.pal` -> `play_event.sh` / a Play click runs `prisc+x` -> blocking exec -> the `mr_*` op. Pal itself is RISC-V assembly run by `prisc+x`, so "Show Text" is a pal instruction that calls an op; it is not a scripting-language statement.
- **rpg-pet does NOT use that pipeline.** It uses RPG Maker only for art, tilesets, actors/monsters (DB rows) and the TSOTS maps. Its logic is custom: `time.pdl` `AT` rows fire `common:<name>` events, doors are `world:` events, all run by `rpg_pet.c`'s own event-runner mode.
- Unread (do before building): the event-editor harness `%.harnesses/event-editor+desktop/`, `EVENT-MODULARITY-AND-BUILD-SPEED.md`, `IN-GAME-LAYOUTS-PLAN.md` (I could not find it at the path I tried), `asa&ava-design-plan.md` (behavior trees compiled to pal).

## 3. Shape of the studio
Three views over the same files, no hidden format:
1. **Layout view**: tree of the `.xhtpm` (window > page > panel / sidebar / row / item / canvas / pinned bars / footer cells) on the left, a live preview of the real window in the middle (the real renderer, headless, as `layout_check`/`layout_flow` already drive it), properties on the right (class, width from css/pdl, `show=` var, action). Edits are text edits of the file (the renderer already reparses live), validated by `layout_check` on every change. A bar's open/closed state is a property (var + bottom cell), not code.
2. **Events view**: pages of an event as a vertical command list like RPG Maker's (Show Text, Show Choices, Conditional Branch, Change Gold, Transfer, Wait, Move Route, Common Event, Script = a pal snippet). Each command row is one `mr_*` op with its fields; the list IS `event.ir.pdl`; "Compile" writes `event.pal`; "Play" runs `play_event.sh` and shows the receipts (`mr_read_receipt`). New commands are new `mr_*` ops (house rule: C or pal, one binary per verb, no shell for game logic).
3. **Book view**: the maps/rooms of a game book (`maps/<book>/<desk>/`: map.txt, map.png, events.txt, bar.txt, ascii_legend.txt) with events placed on cells (the xelector places them, as pc-hq already does), each placed event pointing at an `event_pkg` page.
Game = book + layout + events + `time.pdl` (clock events) + a rules op where needed. rpg-pet becomes the first game expressed this way; Monopoly/farm/etc. follow (`VISUAL-GAMES-PLAN-2026-10-11.md`).

## 4. Making it usable by weak agents (the meta goal)
- Every studio action is also a CLI verb (`layout_op`, `event_op`) so an agent can do it without the GUI; the GUI only calls them.
- Every file the studio writes passes `layout_check` / an event checker before it lands; the error text carries the fix (v1 style).
- Locked pal harness cases per feature (cases pdl + case/verdict ops): "layout with two pinned bars: canvas width = window - bars", "Show Text event produces the SHOW_TEXT_FILE relay line", "Transfer moves the entity to the desk".

## 5. Order of work (each ends with a PNG + click path, owner rule)
| # | Item | Proof |
|---|---|---|
| 0 | Read the unread docs above; write the real command inventory (RPG Maker MV codes vs existing `mr_*` ops: which are missing) | a table in this doc |
| 1 | Pinned bars on livedesk + pc-hq board + rpg-pet; add `ov-pin` / `canvas-overlay-left` to `layout_check` and CATALOG | PNGs, `layout_check --drift` clean |
| 2 | rpg-pet as a board window using the bars; its events re-expressed as event pages (`time.pdl` AT rows -> common events; doors -> Transfer) running through `play_event.sh`; keep the old window beside it for comparison | same behaviour in both windows, PNGs |
| 3 | TSOTS importer: RMMV `MapNNN.json` event pages -> `event.ir.pdl` pages (codes map 1:1 to `mr_*` ops where one exists; unknown codes become a visible "TODO <code>" node, never dropped silently). The converter is a one-off, so Python is allowed (house rule: only one-off converters) | counts: events imported, codes mapped / unmapped, observed |
| 4 | Studio layout view (tree + live preview + properties) | PNG of editing a bar width and the view shrinking |
| 5 | Studio events view (command list, compile, play) | a Show Text + Show Choices event authored with clicks, played, receipts shown |
| 6 | Book view with event placement | place an event on a rpg-pet cell, walk onto it, it runs |

## 6. Open questions for the owner
1. Is `event-ez` (the authoring UI named in `mr_show_text.c`) the editor to extend, or the `event-editor` harness, or a new view inside the studio? (I only saw references.)
2. "Script" command: a raw pal snippet, or only the fixed `mr_*` ops?
3. Should side bars be listed as entities in the book view (so a game can ship its default bars), or stay layout-only?
4. Import scope for TSOTS: all 94 maps' events, or only the maps rpg-pet uses?
