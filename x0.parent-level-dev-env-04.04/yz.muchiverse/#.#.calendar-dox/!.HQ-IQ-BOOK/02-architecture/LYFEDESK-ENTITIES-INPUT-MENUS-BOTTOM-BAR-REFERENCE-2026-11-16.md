# LyfeDesk entities: surfaces, input map, context menu, bottom bar, hotbar (reference for agents, 2026-11-16)

Written so the next agent does not have to re-research it. Everything is **READ** (file and function named) or **SEEN** (a window photograph I took on 2026-11-16 with `dump_frame_png_op`, XO `31.Nov/nov-16/`), unless marked **NOT READ**. Written while planning cursword on the +phone (`16.game/PLUS-PHONE-CURSWORD-ENTITY-RESEARCH-2026-11-16.md`). The owner's own cursword folder is under `xyzfs/users` (private): never open or commit it; the seed template is `44.xyz.01.00/xyzfs/_seed/livedesk/pals/cursword/`.

## 1. The four surfaces an entity has
| Surface | What it is | Code / data |
|---|---|---|
| **Sprite window** | a small borderless X11 window (80x80 for the sprite, snapped to an 80px grid) on the desktop, one process per entity | `_.monads/_.livedesk-taskbar/ops/khtpm_entity.c` ("tile mode", `tp_main`) |
| **Bottom-bar entity cell** | one cell per entity of the active desk, sprite + name, nav-numbered | `khtpm_strip_bottom.xhtpm` `tab_N_label/sprite/action` in `#.desktop/strip_ui.txt` (written by the taskbar manager) |
| **Bottom-bar window cell** | one cell per open HQ window: `🪟 <name>`, dimmed when minimized | `hw_N_label/minclass/onclick` in `strip_ui.txt`, css `.hqwin-minimized` (`entity_menu_default.css`) |
| **Context menu** | the entity's menu popup | the entity's `menu.chtpm` (generated from `meta.pdl`/`objects.pdl`), popped by the entity process |
In LyfeDesk an entity is therefore BOTH a sprite on the desktop and a cell in the bottom bar (owner 2026-11-16: "in LyfeDesk it's both, you see? Right-click is the same as LyfeDesk").

## 2. The input map (READ `khtpm_entity.c`, event loop ~lines 6385 to 6620; bottom bar: `khtpm_taskbar_manager.c`)
| Input | On an ordinary entity | On cursword (`g_is_cursword`, set by `STATE \| log_mode \| 1` in the entity's `meta.pdl`, NOT by its name; CENTROID_GOLD_STD rule 7) |
|---|---|---|
| **Left press + drag + release** | moves the entity; release snaps it to the grid (`kh_drag_stack_above`, `write_pos`) | the same (a real drag still moves it) |
| **Left press + release, moved <= 5 px AND held <= 600 ms** (a plain click) | nothing special (it is a tiny drag) | **toggles ARMED**: a glowing blue halo (`cursword_update_shape`), writes the armed state file (`cursword_write_armed`, the rmmv_armed convention), appends `CURSWORD_ARMED` / `CURSWORD_DISARMED` to history, starts a fresh key log. While armed: key capture until Esc, arrow keys move it one grid cell, POV/camera keys (design `08-roadmap/design-docs/CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` sections 3a, 9, 10, 12) |
| **Right press (button 3)** | opens the context menu: reloads the menu config from DISK every time (`read_menu_config`, `load_objects` = `objects.pdl`, else `load_methods` from `meta.pdl`), so edits show without a restart | the same menu |
| **Bottom-bar entity cell: click, or its typed nav number + Enter** | writes the single word `OPEN_CONTEXT` into the entity's `interact_relay.txt` (`ktb_activate_tab`, `khtpm_taskbar_manager.c`; "same as right-click on the tile"); the entity pops the same menu | the same |
| **Bottom-bar window cell** | `FOCUSWIN:<winid>:<pid>` (raise and focus); no menu | n/a |
Code path of a cell tap: cell `action` = `strip_relay.sh <2000 + index>` (`khtpm_strip_codes.h`: `KSC_TAB_BASE 2000`) -> `#.desktop/strip_history.txt` -> manager `poll_strip_history` -> `ktb_activate_tab`. The entity reads ONE line per tick of its relay, so write exactly `OPEN_CONTEXT` (a two-line version silently did nothing until 2026-08-11). The standard click rule applies to the bar as everywhere: first click focuses the cell, second fires; typed number + Enter fires.
Open question carried from the armed design (memory note "revisit armed cursword"): the armed state is a display-wide key grab; whether that is itself a bug is undecided.

## 3. Cursword is special (READ)
- **Always open** ("cursword is an entity that should always be open, even if others aren't, it's the user's assistant, 1st entity", owner 2026-08-30): `livedesk_ensure_cursword` (`khtpm_taskbar_manager.c`, ~line 2733) runs at manager start and after every desk switch, session load, new session or reset; it spawns it if no live process has a pal folder named `cursword`. It is **outside the desk/session model**, and its position is **pinned to the top left** (`CURSWORD_HOME_X/Y = 0`) on every fresh spawn.
- A closed cursword is respawned (so a menu row "Close" is harmless on LyfeDesk; owner: "use the same as LyfeDesk").
- Its entity folder gives it an entity "phone" (`livedesk_phone_ensure`, design Q005; NOT the +phone) and a wordbank (`livedesk_wordbank_ensure`).
- Lore: cursword is also the TSOTS player's avatar/role (skill `soul-pen-lore`); not researched here.

## 4. The context menu (SEEN `4.cursword-old-context-menu-9-rows.png`; READ seed `menu.chtpm`)
Title `. cursword CURS 6368`; nav-numbered rows: **1 Dir** (xdg-open the entity folder), **2 Inventory** (a file-explorer instance on its inventory), **3 Chat** (`&.widgits/open-hai/chat_button.sh`, the assistant's chat window), **4 Bookmarks** (`&.widgits/bookmarks/bm_menu.sh`), **5 Play** (`&.widgits/events-hq/ops/play_event.sh`), **6 Act** (`&.widgits/entity-cli/open_entity_act.sh`), **7 Cli-io:** (a typed command line, `entity_cli_commit.sh`; standard in `ENTITY-MENU-CLI-IO-STANDARD.md`), **8 Close** (`CLOSE`), **9 Cancel** (`void`). Actions are `sh -c` strings with `$0` = the entity folder and `$1` = the house; most open ANOTHER desktop window.

## 5. The hotbar (READ `@.apps/hotbar-hq/hotbar-desk.xhtpm`)
The hotbar is an ordinary HQ window `Hotbar (desk)`: a fixed row of 9 slots over the HOLDER's `inventory/` (the holder is the possessed entity, cursword by default); the selected slot is what Place uses; docked above the bottom bar (`vars-positioned`, manager `ops/hotbar_manager.+x`, published `state/desk/ui.txt`); it appears as a `🪟 Hotbar (desk)` cell in the bottom bar. Design: `18.pc-hq/CURSWORD-POSSESSION-DESIGN.md` 5b and 5e (NOT READ). rpg-edit-style projects have a hotbar inside their window, and so does LyfeDesk (owner 2026-11-16); an app on the +phone may too (`PLUS-PHONE-LAYOUT-SPEC` section 9).

## 6. How to drive and photograph it safely (what worked 2026-11-16)
- **Window ids and sizes:** `xwininfo -root -tree`: the two bars are 2096x45 at +200+50 (top) and +200+1619 (bottom); entity sprite windows are 80x80 (cursword at +0+0); the open menu appears as a new ~264x342 window. Photograph with `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <winid> out.png` and OPEN the PNG.
- **The bar process:** `ps -eo pid,args | grep '[k]htpm_core_render'`: the pid whose args end in `khtpm_strip_header.xhtpm` owns BOTH bars; its relay is `#.desktop/entity_menu_history/<pid>.txt`. **A pixel click written there lands on the TOP bar** (I clicked at x=131,y=22 meaning the bottom cursword cell and instead pressed the top bar's `+` twice, which opened a phone). **Use the typed nav number for the bottom bar:** `KEY_PRESSED: 49`, `KEY_PRESSED: 55`, then `KEY_PRESSED: 13` for cell 17; read the number off the photo first (numbers are global per dock and shift when windows open).
- **The menu window** is its own `khtpm_core_render` whose args end in `.../pals/cursword/menu.chtpm`; its relay file is `entity_menu_history/<its pid>.txt`. Dismiss with its Cancel row (type `9`, then Enter); never type the Close row's number (8) on a live desk.
- **Before driving a live window, ask the owner** (the rule in `lyfedesk-taskbar-driving`); keep every step reversible (Cancel, not Close).
- **Xephyr** exists on this machine (`/usr/bin/Xephyr`), so a nested display is possible for risky experiments with a scratch house; I did NOT try it (the live desktop is Wayland with XWayland, and the house needs its own scratch tree and `xyzfs` copy). Worth trying before any experiment that moves or closes real entities.
- **Do not:** open or copy the user's `xyzfs/users/<id>/home/livedesk/pals/cursword`; `git add` anything under `xyzfs/users`; start a second cursword process.

## 7. Decisions for the +phone from the owner (2026-11-16, SAID)
Cursword on the phone is BOTH surfaces like LyfeDesk (a tile on the grid = the phone's "desktop", and a cell in the phone bottom bar); the context menu has the SAME rows with the same actions; Close is kept (as in LyfeDesk); no video. **Still open:** what a plain tap on the tile does (LyfeDesk's left click on cursword ARMS it; a phone has no right click). Proposal in the research doc.
