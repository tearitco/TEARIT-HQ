# Cursword on the +phone: research of the OLD behaviour, and what to confirm before building (2026-11-16)

> Owner 2026-11-16 (SAID): "we can put a cursword entity on a grid on the phone if you can look into that and see how we would address it from the bottom tb, and give it its expected context menu, but you should definitely research, doc, show the old functionality and confirm with me first." Also: the stack of three in the phone comes before entities (built, see `PLUS-PHONE-LAYOUT-SPEC` section 8).
> Labels: **SAID** = the owner told an agent; **READ** = I opened it (file named); **SEEN** = I looked at a photograph I took today; **DOC** = my reasoning; **NOT READ** = I did not open it. Nothing is built. The user's own cursword folder lives under `xyzfs/users` (private user data): I did NOT open it; everything below is from code, docs and the seed template.
> Pictures (XO `31.Nov/nov-16/`): `2.cursword-old-bottom-bar-cell17.png`, `3.cursword-old-entity-sword-icon.png`, `4.cursword-old-context-menu-9-rows.png`.

## 1. What cursword is (old behaviour)
- **SAID 2026-08-30** (quoted in `khtpm_taskbar_manager.c`, `livedesk_ensure_cursword`): "cursword is an entity that should always be open, even if others aren't. It's the user's assistant. 1st entity." and "it should always start in the upper top left".
- **READ** (`_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c`, `livedesk_ensure_cursword`, `CURSWORD_HOME_X/Y = 0`): cursword is the one entity OUTSIDE the desk/session model. It is not "on" any desk. Every taskbar-manager start, and every desk switch/session load/reset, checks that a live process exists for the pal folder named `cursword` under the user's `pals/`, and spawns one if not; its position is forced to the top left on every fresh spawn. `livedesk_phone_ensure` gives it a phone item (the entity "phone" of design Q005, a different thing from the +phone).
- **SEEN** (photo 3): on screen it is an 80x80 round window at the top left with a sword icon. **SEEN** (photo 2): in the bottom bar it is the first entity cell, `[ ]17. 🗡 cursword` (sprite + name), followed by the other entities of the desk (`m1_ninjadragon`, `asa`, `ava`, `book-stack`, `door_civ`) and then the window cells (`🪟 Hotbar (desk)`, `🪟 +2 :41001` = the phone). Lore (soul-pen-lore skill): "cursword" is also the player's avatar/role in TSOTS; I did not research that link here (NOT READ).
- **READ** seed template `xyzfs/_seed/livedesk/pals/cursword/`: `menu.chtpm` (the context menu), `meta.pdl`, `stats.pdl`, `glyph.txt`, `instance_id.txt`, `bookmarks.pdl/.chtpm/.css`, `harnesses/`, `presentations/`. (Contents of meta/stats NOT READ.)

## 2. What a tap on its bottom-bar cell did (READ + SEEN)
1. The bottom bar is one row of cells; each ENTITY cell has `label` (the entity name), `sprite` (the entity's folder) and `action = strip_relay.sh <2000 + index>` (`strip_ui.txt` `tab_N_*`; codes in `khtpm_strip_codes.h`: `KSC_TAB_BASE 2000`).
2. The taskbar manager reads the code and calls `ktb_activate_tab` (`khtpm_taskbar_manager.c`), which writes the single word **`OPEN_CONTEXT`** into the ENTITY's own `interact_relay.txt` (comment in the code: "same as right-click on the tile"; a 2026-08-11 bug fix made it one line, because the entity reads one line per tick).
3. The entity process pops its context menu from its `menu.chtpm`. Window cells (🪟) are different: their click is `FOCUSWIN:<winid>:<pid>` (raise and focus), no menu.
4. Same standard two-step as every button: type the cell's number (`17`) then Enter (what I did), or click once to focus and again to fire.

## 3. The expected context menu (SEEN photo 4, READ `menu.chtpm`)
Title `. cursword CURS 6368`; nine rows, each a nav-numbered row:
| # | Row | What it runs (READ, from `menu.chtpm`; `$0` = the entity folder, `$1` = the house) |
|---|---|---|
| 1 | **Dir** | `xdg-open` the entity folder in the file manager (writes an audit log) |
| 2 | **Inventory** | opens a file-explorer instance on the entity's inventory (`&.widgits/file-explorer/instances/inv-<entity>-...`) |
| 3 | **Chat** | `&.widgits/open-hai/chat_button.sh <house> <entity>/chat cursword-chat`: the assistant's chat window |
| 4 | **Bookmarks** | `&.widgits/bookmarks/bm_menu.sh` |
| 5 | **Play** | `&.widgits/events-hq/ops/play_event.sh <entity>`: plays the entity's event/common event |
| 6 | **Act** | `&.widgits/entity-cli/open_entity_act.sh <entity> <house> <win x> <win y>` |
| 7 | **Cli-io:** | a typed command line (`<cli_io>`; `entity_cli_commit.sh`; standard in `02-architecture/ENTITY-MENU-CLI-IO-STANDARD.md`) |
| 8 | **Close** | `CLOSE` (closes the entity; cursword is re-ensured on the next desk switch or manager start) |
| 9 | **Cancel** | `void` (dismiss) |
Every row except Cli-io, Close and Cancel opens ANOTHER desktop window (file manager, file explorer, chat, bookmarks, an event, the act panel). I dismissed the menu with Cancel; the cursword entity was untouched afterwards (checked).

## 4. How cursword could appear on the phone (DOC; nothing decided)
**A. A tile in the app grid.** The picker (`plus-phone.xhtpm`) shows apps as square swatch tiles from `apps/<id>/manifest.pdl`. An ENTITY tile would use the entity's sprite and name. Open question: is a tile an app (opens a screen) or the entity's context menu (like its bottom-bar cell)?
**B. A cell in the phone bottom bar.** Owner: the bottom bar holds only NAMES of apps and entities (entities "none now, later a hotbar"). An entity cell after the app cells: sprite + name, the same shape as the LyfeDesk entity cell (SEEN photo 2). A tap = the old behaviour: open its context menu.
**C. The context menu on the phone** = the phone's own dropdown shape (`dropdown-child` rows under a tab, as the HQ menu), built from the entity's `menu.chtpm` rows so the phone shows the same nine rows with the same numbers. Two ways: (C1) rows run the SAME actions unchanged (Dir/Inventory/Chat... open normal desktop windows beside the phone); (C2) phone-native screens (a Chat screen inside the phone window, a Cli-io screen), added one row at a time, the way the owner's "phone = LyfeDesk logic" rule suggests.
**One authority per fact (DOC):** the phone must NOT start a second cursword process or copy its folder; it is another VIEW of the one entity (`pals/cursword`), reading its name, sprite and `menu.chtpm`, and sending it `OPEN_CONTEXT`-style requests, exactly as the bottom bar does. The entity stays always-open on the desktop.
**Order (DOC):** (1) entity cell in the phone bottom bar from a data list (no process), (2) its context menu as a phone dropdown with C1 rows, (3) the grid tile, (4) native Chat/Cli-io screens.

## 5. What I did NOT read, and what could break (honest list)
- **NOT READ:** the entity process that pops the menu (`tp_desktop_window.c` / `khtpm_entity`), the chat widget (`open-hai`), `meta.pdl`/`stats.pdl`, the real cursword folder of the user (private), how the hotbar window (`@.apps/hotbar-hq`) already lists entities (it may be the right model for "later a hotbar").
- The `menu.chtpm` actions are `sh -c` strings with `$0`/`$1`; reusing them from the phone means a safe, no-shell way to run them (fork/exec like `phone_op` does), not copying the strings.
- The always-open rule (`livedesk_ensure_cursword`) respawns cursword; a phone "Close" row must not fight it.

## 6. Questions to confirm with the owner BEFORE building
1. Entity on the phone: a bottom-bar cell, a grid tile, or both, and in which order?
2. A tap on the cell: open the context menu (old behaviour), or focus an app-like screen for the entity?
3. Menu rows: the same nine, unchanged actions first (they open desktop windows), then phone-native Chat and Cli-io later? Or only Chat and Cli-io on the phone?
4. Is cursword the only entity for now, or the whole desk entity list (the "hotbar" later)?
5. Should "Close" be offered on the phone (the desktop re-spawns cursword), or hidden?
6. Do you want a short narrated video of the old behaviour (the three photos plus the nine rows) before we decide? (`video_op`, needs the TTS host; I have not started one.)
