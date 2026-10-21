# How to build an xhtpm HQ window the way blueprintz was built (guide for devs and future agents)

Written 2026-10-20 by the `claude` engineer (Sonnet 5.5), from building the blueprintz window and its edit panel (BLUEPRINTZ-DESIGN.md 12.16 and 12.22) and from the verb-page work (16.game/EVENT-VERB-PAGES-DESIGN.md). Every rule below is something that was done or hit in those files; the path of each is given so you can open the real thing. This guide does not replace `CENTROID_GOLD_STD.md`, `X11-HQ-APP-DESIGN-WISDOMS.md` or `RELAY-DRIVE.md`; read those first, this is the worked example that sits on top of them.

What the window edits (event pages, pal, prisc, C ops) is explained in BLUEPRINT-EVENT-PAGES-GUIDE.md.

Reference app: `44.xyz.01.00/&.hq-apps/blueprintz/` (`blueprintz.xhtpm`, `blueprintz.css`, `blueprintz_manager.c`, `ops/bp_req.c`, `ops/bp_edit_op.c`, `ops/build.sh`, `button.sh`).

Section 3 is the tool-panel look. Section 8 is the game look: the small pet window and the pc-hq board. A new game uses section 8. It does not restyle blueprintz.

## 1. The shape: three parts, no new renderer code

1. **A static template** (`blueprintz.xhtpm`) that never changes at run time. It only names slots: `${status_line}`, `${e_count}`, `<repeat count="${n_count}" bind="n">`, `action="${edit_action}"`.
2. **A manager** (`blueprintz_manager.c`), a separate compiled process the renderer forks from the `<module src=...>` tag. It owns all logic and publishes one flat file, `state/ui.txt` (`key=value` lines, written to a `.tmp` and renamed).
3. **A click side** (`ops/bp_req.c`), a tiny program that appends one line to `state/request.txt`. The manager reads that file by **size growth** (the marker rule), never by mtime.

The shared renderer (`khtpm_core_render.+x`) is not touched. If you feel you need a new tag, a new `g_is_<app>` branch or key handling in the renderer, stop: the house standing rule (see `khtpm-house-standards` skill) is that the existing vocabulary (`window / sidebar / panel / row / item / text / repeat / cli_io / footer`) is enough. blueprintz needed none.

Data flow: `click -> action string in ui.txt -> bp_req appends "VERB[:typed]" -> manager reads the new bytes -> does the work -> rewrites ui.txt -> renderer re-parses -> window shows it`.

## 2. Technical choices, and why

- **Actions are baked into `ui.txt`.** `e_0_action='<abs>/ops/+x/bp_req.+x' 'SEL:0'`. The template just says `action="${e.action}"`. The manager decides what a click means, so the template stays static and a new control is two lines in `write_ui()` plus one `<item>`.
- **Lists are `count` + indexed keys.** `n_count=2`, `n_0_label=`, `n_0_class=`, `n_0_action=` and `<repeat count="${n_count}" bind="n">` with `${n.label}`. The class is data too, so a selected row is `bp-node active` with no renderer support.
- **The renderer appends two arguments to every action**: `<house root> <app dir>`. So `bp_req` receives `VERB pkg house [typed]`. The typed value of a `<cli_io>` is argv[4]. (Found the hard way on rpg-pvp-hq, where `buy <key>` arrived as `buy <key> <path> <path>` and every window purchase was placed at 0,0. Engines strip trailing absolute-directory args.)
- **One write path.** The window never writes the thing it edits. Edits go `manager -> ops/bp_edit_op -> events-hq manager -> event.ir.pdl + compile`. The same op is what the headless harness (`BP | ...` rows in a phase table) calls, so a click and a saved harness row are the same edit. Any new editor you add to a window should be a thin caller of an op that a harness can also call.
- **Children by fork+exec with a time limit** (`run()` in the manager): never `popen`, never an unbounded wait. A hung child must not freeze the window (this is the prisc popen freeze lesson in memory).
- **State is plain text, append-only where it is a log.** `state/request.txt` (append), `state/ui.txt` (replace). No logging by the window itself.
- **Test-redirectable.** `state/target.pdl` (`TARGET | house | <path>`) and `state/bp_sources.pdl` (`EVENT | name | <event dir>`) point the window at a scratch house or a scratch copy of a package. Build every new window with such a switch from day one; it is what let the edit panel be tested without touching real games.
- **Validate at the edge, before writing.** Typed text becomes part of a single-quoted word in a generated shell script, so the manager refuses everything outside `[A-Za-z0-9_-.:,=| /]`, and refuses a command type that is not a `COMMAND` row of `#.ref/menu/event_commands.registry.pdl`. The registry check exists because a real test typed `17` into a still-armed field and the window accepted it (a page with no exec line, silently). Negative-test every refusal once.
- **Undo is an inverse-edit stack**, not a snapshot: edit -> edit back, add -> delete, delete -> append. Say the limits in the UI text and the doc (a deleted command comes back at the end). A diff preview is one status line `old -> new`; for a one-line-per-node format that is the whole diff.

## 3. Aesthetic choices (blueprintz.css)

The look is a dark tool panel, same family as `sfx-hai.css` / `database-window`. It is consistent on purpose: a new HQ window should feel like part of one desk.

| Role | Colour | Used for |
|---|---|---|
| window | `#14161b`, DejaVu Sans 11px | whole background |
| title | `#f5b642` bold on `#23262f` | the status line (amber = "state of the whole thing") |
| section head | `#7ec8ff` bold on `#1d2027` | "Commands of the selected event", "Problems ..." (blue = a heading) |
| quiet text | `#c8cdd8` on `#2a2d36` | hints, targets, messages |
| problem | `#ff9a9a` on `#3a1f22` | flags (red = something to fix) |
| list state | wait `#d0d0d4`, queued amber, running green `#7ee08a`, ran grey `#8b93a7` | event state classes `st-*` |
| selected | `.active` `#24384a` | the selected row |
| input | `.bp-edit` white on `#1d2027` | cli_io fields |

Layout rules used: a left **sidebar** list (what exists, click to select) and a right **panel** (what you can do to the selection, then problems, then trace); a **footer** line for the last message; toolbar **rows** of short verbs (Play, Stop, Step ...). Put the selection summary above the buttons, controls in the middle, evidence (flags, trace) below. Colour carries state, never decoration; text always says the state too ("waiting", "!! flags"), so nothing depends on colour alone. Size is set in CSS only (`window { width: 1180px; height: 780px; }`); layout stays in the shared renderer.

### Scrolling and small windows (added 2026-10-20, owner: "missing thumb scroll ... if window is too small")

A window the user can shrink must scroll. This is **not** renderer work: wrap every list that can outgrow the window in the generic `<scrolllist>` (it brings the scrollbar thumb, the up/down arrows as nav numbers, wheel and PgUp/PgDn; see XHTPM-PARSER-REFERENCE.md, `generic_sbar_register`). blueprintz had none, so at 856x404 the lists were cut off. What worked, tried and screenshot-checked:
- **Sidebar:** keep the heading `<text>` fixed, put the `<repeat>` of rows inside `<scrolllist>`.
- **Panel:** keep what must always be reachable fixed (status line, the button `<row class="toolbar">` rows); put the long, variable content (selection line, command list, fields, problems, trace) in one `<scrolllist>` below.
- **Do not put toolbar `<row>`s of several `<item>`s inside a `<scrolllist>`:** they vanished from the render (tried; reverted). Rows of single items, `<text>`, `<item>`, `<cli_io>` and `<repeat>` rendered and scrolled fine.
- Verify by shrinking the window (blueprintz opened at 856x404) and driving the scroll arrow's nav number through the relay; the content moved to Problems/Trace after three clicks. Not verified: a window larger than the saved size (the desktop kept the saved geometry when I tried to resize with xdotool). Button labels truncate when the window is very narrow; widen it or shorten labels.

## 4. `<cli_io>` fields (typed input) - what to know

- Give the field `class="... top"` or two fields stack at the bottom of the window, on top of each other (first build: only "add command" was visible).
- The typed value reaches your action as the last argument. `bp_req` turns it into `VERB:typed`.
- A field must be **armed** to take text: jump to its nav number, then press Enter (arms it), type, Enter again (commits). **Escape disarms.** While armed, digits are text: driving the relay with `1 7 Enter` to press Undo typed `17` into the field instead. Always send Escape before a nav-number jump if a field may be armed.
- `content=` seeds the editable buffer, `label=` is display text only (see `khtpm-house-standards`).

## 5. How to test a window (evidence, not a clean compile)

1. **Headless first.** Start the manager against a scratch target, append requests with the real `bp_req`, read `state/ui.txt` and the files the manager wrote. blueprintz: add -> delete -> undo x3 restored the original IR byte for byte (`diff` printed nothing).
2. **Then the real window through the relay**, as a person: `#.desktop/entity_menu_history/<renderer pid>.txt`, `KEY_PRESSED: <decimal>` lines, find the window by size with `xwininfo -root -tree`, screenshot with `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <window id> <png>`. Read the nav numbers off a screenshot before sending any digit; numbering is shared by all open windows.
3. **Negative tests**: a hostile value (`'; touch /tmp/x`), an unknown type, an empty undo stack. Check the artifact exists before trusting a count (AGENTS.md "never report a number you did not observe").
4. **Prove the effect on the thing edited**, not just the window text: the NODE row in `event.ir.pdl`, the `exec ... 'advance'` line in `cmd_1.sh`, and (for games) the harness tables `play/phases/*_blueprint.phases.pdl`.
5. **Put test state back.** blueprintz keeps its source list in `state/bp_sources.pdl`; the test pointed it at a scratch package and the live file was restored afterwards. Never leave a live window pointing at /tmp.

## 6. Pitfalls this build hit (so you do not)

| Symptom | Cause | Fix |
|---|---|---|
| Edit went to the wrong folder / "no-pages" flag | source rows name the event dir; `event_pkg/` is inside it | build `<event dir>/event_pkg/pages/page_1` |
| Only the last cli_io shows | missing `top` class | `class="bp-edit top"` |
| Typed digits appear in a field | field still armed | Escape first |
| Late registry commands compile to a page with no exec line | events-hq manager read only 48 of 135 registry rows | cap raised to 512, overflow message (khtpm_events_hq_manager.c) |
| Window click refused as bad-arg / item placed at 0,0 | renderer appends house and app dir | strip trailing absolute-dir args in the engine |
| Build prints huge warning output | old `-Wformat-truncation` noise in bp_ctl.c | filter `grep -E "^(OK\|FAILED)\|error"`; it is not a failure |
| A rebuilt program does not change the open window | the manager binary is loaded at launch | relaunch (`sh button.sh run`) |
| Compiled programs missing on another machine | `+x/` is not in git | `sh ops/build.sh` (the launcher builds what is missing) |

## 7. Checklist for a new HQ window

1. Copy the three-part shape (template, manager, click side). No renderer change.
2. `ui.txt` keys: a status line, a message line, `*_count` lists with `_label/_class/_action`, one `*_action` per button.
3. A target/sources redirect file for tests.
4. Every write through an op that a harness can also call.
5. Whitelist typed text; check types against their registry; refuse before writing.
6. CSS: reuse the palette above; colour = state, words say it too.
7. Wrap every list that can grow in `<scrolllist>`; test the window shrunk.
8. Headless test, relay test with screenshots, negative tests, restore state.
9. Doc the limits in the design page the same day (what is not done), and commit with explicit pathspecs.
10. A game window follows section 8. Copy the pet-window classes or the board template. Do not invent a third chrome, and do not edit the board camera.

## 8. Game look: the pet window and the pc-hq board (added 2026-10-21)

A house game is two windows over one app, not one blueprintz panel with a picture dropped in. Both stay on the shared renderer. Neither needs a new tag.

### 8.1 The small window

Reference: `44.xyz.01.00/@.apps/rpg-pvp-hq/rpg-pvp-hq.xhtpm` and `rpg-pvp-hq.css`. game-dev-hq uses the same class names.

- Window class `database-window fixed-size managed user-resizable`. Background `#14161c`, DejaVu Sans 11px. Size is in the css (`window { width: 916px; height: 560px; }` on the shooter).
- Top `<tabbar class="tab-bar">`. Each tab is `class="tab rp-tb"`. A menu tab is `onclick="ACTIVATE"` plus `target_id`. The rows that drop from it are `class="rp-menu-row dropdown-child"` with the same `target_id`. A row that only labels something uses `action="void"`.
- INT is a tab whose class includes `${interact_class}`. On is `.tab.rp-tb.interact-active` (`#2a5a2a`). The label still says on or off.
- The room is a canvas. Three chrome pieces, one of each. Left pin `canvas-overlay-left` plus `ov-pin` (`rp-side-ov`): the info the player is watching (phase, room, who is where). Right pin `canvas-overlay-right` (`rp-chat-ov`): the chat log. One bottom bar: footer `pchq-footer` / `pchq-ft`, and on a pet window that bar is the players (one button per name in `party.pdl`). A `canvas-overlay-bottom` is a second strip (the shooter's bag). Do not put the same player names on both. The menu `<sidebar>` is a 2px rail (`rp-rail`); a wide sidebar leaves a blank column and hides the pins. A left or right pin with no `<item>` child is skipped by the renderer. Quiet lines are `#8a93a8`. A number the player is watching (coins, a clock, the phase) is `rp-big` (`#ffd966`). Day and night come from the app clock into `state/daylight.txt` (night 19:00–06:00, dawn to 08:00, day to 17:00, dusk to 19:00) and tint the canvas. Doors are `rooms.pdl` rows: the trigger is the cell under the door picture.
- Vars are `state/ui.txt` plus `state/clock_ui.txt` when the LyfeDesk clock label is on the bar. The module is the game binary (`ops/+x/rpg_pvp_hq.+x`), not a second renderer.
- The cast is this app's `party.pdl` (`PET | name | sheet | index`). Sheets are the RPG Maker files under `&.widgits/palettes/sprites/rmmv/dir_characters/`. One app does not wear another app's names or cells. The island note is `16.game/BET-RPG-DESIGN.md`. A later generator is `16.game/CHAR-CREATE-DESIGN.md`.

### 8.2 The board

Reference: `44.xyz.01.00/@.apps/game-dev-hq/board/pchq-board.xhtpm`, `board/pchq-board.css`, `board/open_board.sh`. The same template shape is under each game's `board/`. The opener is `piececraft-hq/open_pchq_board.sh` with `PCHQ_BOARD_TPL` set to that game's `pchq-board.xhtpm` and `PCHQ_PKG` set to `<app>/<game>-pchq`.

- Window class `pchq-board-pal database-window managed user-resizable`. The module is `piececraft-hq/ops/+x/pchq_board_projector.+x`, not the game binary. The game binary is what the tab actions call.
- Chrome is the board's, not `rp-*`. Toolbar `pchq-toolbar`, tabs `tab pchq-tb` (idle `#232323` / `#cfe8ff`, active `#2f5f8f`). A 2px `pchq-rail` exists only so the sidebar-plus-panel layout runs. Footer `pchq-footer` / `pchq-ft`. Size on the studio board is 1074 by 556.
- The picture is the book, not a widget you draw in css. `open_board.sh` runs the game's export (studio: `game_dev_hq.+x exportbook`) which writes `maps/<book>/<room>/` : `map.png`, `events.txt`, `bar.txt`, and the frames. People are event rows: x, y, then the sheet name and the cell (`Actor3 0` is the studio programmer). Furniture is map glyphs plus `extrusion.pdl` (`EXTRUDE | glyph | height`). A height makes a cube. The word `sprite` extrudes the opaque pixels of the cell and is the wrong look for a table tile.
- Game vars are listed first on the window (`../state/ui.txt`). Board vars come after, so the canvas is the engine frame.
- The comment at the top of `game-dev-hq/board/pchq-board.xhtpm` is the key list: 2D is the default, `0` is 3D, `1`–`4` are POV, backtick is the glyph view. Do not change camera equations, `bv_menu_input.c`, or `bv_render_3d.c` to chase a look. The owner sets the camera. Cell size, when a map has it, is `view.pdl` `VIEW | cell_px | 48` (dsr-rpg). The house default grid is larger. Do not invent a size.
- A frame check is key `112` (the letter p) on `#.desktop/entity_menu_history/<pid>.txt`, which writes `/tmp/entity-menu-frame.png`. Read that file by the window's own pid.

### 8.3 Which one to copy

| You are making | Copy | Do not |
| --- | --- | --- |
| A tool (editor, h-ai, img-hai) | section 3, blueprintz.css | the board camera, a `toy.pdl` on an h-ai app |
| A walkable game the player drives from menus | section 8.1, `rpg-pvp-hq.css` | a new palette |
| The town, the office, the map you walk inside | section 8.2, that game's `board/pchq-board.xhtpm` | a second board renderer, a camera edit |

h-ai apps (img-hai, and later char-hai) are section 3 windows. They are rows in `#.desktop/livedesk_taskbar.pdl` `ai_menu_N_*`. A `toy.pdl` would put them on the toys menu. img-hai's `toy.pdl` was removed for that reason.
