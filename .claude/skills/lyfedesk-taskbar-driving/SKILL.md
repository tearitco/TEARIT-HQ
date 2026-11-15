---
name: lyfedesk-taskbar-driving
description: How to find, click, photograph and rebuild the LyfeDesk top bar (book:page, tools, menu cells and their dropdowns) and the editor's Tools tab like a person would, and the traps that cost time (stale manager, loading splash, wrong book source, a closed dropdown still existing as an unmapped window). Use BEFORE changing or testing a top-bar cell, a header dropdown, or the Tools list.
---

# Driving the LyfeDesk top bar (first written 2026-11-14; items marked UNSOLVED are not solved)

Paths are from `44.xyz.01.00/` (the house). Quote every path (`&`, `@`, `#`). Companion skills: `khtpm-house-standards` (relay input, renderer rules), `rpg-space-board-driving` (the board/editor window). Ask the owner before driving a live window; they may be using it.

## 1. Find the bar and its relay
- The top bar is an X window about 2079x45 at +200+50 (`xwininfo -root -tree`, filter small heights); the bottom bar is the same width at the bottom. A header dropdown is its OWN window (about 198x675 at x of the cell, y 95).
- The process that draws the top bar: `ps -eo pid,etime,args | grep '[k]htpm_core_render'` and pick the one whose argv ends in `khtpm_strip_header.xhtpm` (the hotbar one is `hotbar-desk.xhtpm`, the editor is another pid). The pid CHANGES when the desktop relaunches: look it up every time.
- Photograph: `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <window id> out.png`, then OPEN the PNG. `--root` did not work (no file written); dump the window id instead.
- Cells are numbered by the nav badges drawn on the bar (`[ ]11. tools`): read the picture, do not assume numbers.

## 2. Click a cell
Relay file `#.desktop/entity_menu_history/<pid>.txt`, append only: move the pointer onto the cell first (`xdotool mousemove <winX+x> <winY+y>`, focus follows the mouse), then append `MOUSE_EVENT: 1 <x> <y> 1` and `MOUSE_EVENT: 1 <x> <y> 0`, x and y relative to the window. The first click is often dropped: repeat once, and check that exactly one dropdown window exists afterwards (more or fewer = look again). Wait about 3 s before the dump.

## 3. What the Tools list is made of
- ONE op, `&.widgits/_shared-lib/ops/harness_menu_op.c`, builds the list for the editor's Tools tab AND the LyfeDesk Tools cell (`harness_menu_op list <house> <book> <page> <out>`; `-` = none). Groups: `-- this page --` (the book's own ACTION rows), `-- plugs --` (`[x]`/`[ ]`), `-- tests --` (CASE rows). Empty groups are left out. Global rows live in `#.desktop/harness_menu.pdl`, a book's own rows in `@.apps/<book>/harness.pdl`, its plugs in `@.apps/<book>/plugs/*.plug.pdl`.
- Owner rule (2026-11-14): a BOOK WITH NO TOOLS OF ITS OWN (no `harness.pdl` rows, no plugs, for example `pre-design:office`) lists NO global tests, only the generic rows the bar adds (notes-..., cancel). With no book at all (`- -`) the global rows still show. An own row that repeats a global one (same kind and target) is listed once.
- The bar's book and page are the ones it SHOWS: `book:<session name>` and `page:<desk name>` (`livedesk_current_session_name` / `livedesk_current_desk_name` in `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c`; the session folder is under the user's home, read only). Do NOT take them from the editor board state (`rpg-edit-hq-pchq/pieces/world_01/state.txt`, `last_pchq_book_page.txt`): that is the EDITOR's book (centroid:live), a different thing. I made exactly that mistake on 2026-11-14 and the dropdown listed the wrong project.
- Check the op's answer directly first (`harness_menu_op list <house> pre-design office /tmp/x.txt; cut -f1 /tmp/x.txt`): if the op is right and the dropdown is wrong, the running manager is stale.

## 4. Rebuild and relaunch (the traps)
- The manager is a separate compiled process (`+x/khtpm_taskbar_manager_main.+x`). A running desktop keeps the OLD binary: a rebuild changes nothing on screen until the desktop is relaunched. `ps -o lstart` of the manager against the binary's mtime tells you which build you are looking at. A dropdown you photograph may come from an intermediate build.
- Build: `cpu_gate <house> run <label> --wait 120 -- env LIVEDESK_NO_SPLASH=1 sh -c 'cd _.monads/_.livedesk-taskbar/ops && sh build_khtpm_strip.sh'`. WITHOUT `LIVEDESK_NO_SPLASH=1` the script starts the "loading LyfeDesk" strip (`livedesk_splash.+x --boot`, pid in `#.desktop/livedesk_boot_splash.pid`) and nothing dismisses it when you only compile. If it is stuck, check `+x/.build_failed.txt` and `#.desktop/boot_build_failed.txt` do not exist (a failed build must stay visible), then kill that one pid.
- Never wrap `button.sh reset` in cpu_gate; relaunching changes the owner's screen: ask first.

## 5. Closing a dropdown (corrected 2026-11-14)
Type the CANCEL row's nav number then Enter through the bar's relay (`KEY_PRESSED: 51`, `KEY_PRESSED: 49`, then `KEY_PRESSED: 13` for row 31; read the number off the photo). That closed it. I first reported it as NOT closed because I counted windows of the dropdown's size: a closed dropdown stays as an UNMAPPED window of the same geometry. Check `xwininfo -id <win> | grep "Map State"` (IsViewable = open, IsUnMapped = closed), never the existence of the window. Esc and a second click on the cell did not close it.

## 6. Evidence rules
A window that exists is not a window that is open: check its Map State.
Open the PNG and say what it shows; a dropdown that exists in a window list is not proof of its rows. Write the manager build time next to any claim about what the bar does. A source-text check (`taskbar_harness_menu` case) proves wiring, not behaviour: say so.

## 7. The plugs / harnz / tests windows (plugz-hq, built 2026-11-14)
- The bar's Tools dropdown is a launcher: `plugs`, `harnz`, `tests`, notes, cancel. A pick runs `harness_menu_op pick <house> <book> <page> open:<kind>` (the bar passes the book and page it shows) which starts `&.hq-apps/plugz-hq/plugz-hq.xhtpm` with the instance dir `instances/<kind>` as argv[3] (the renderer's generic hook). To open one without the bar: run that op yourself; it prints `WINDOW-OPENED <kind> <pid>` or `WINDOW-REFRESHED` or `WINDOW-MISSING <what>`.
- Drive it with the window's own relay (`#.desktop/entity_menu_history/<pid>.txt`, the pid in the instance's `window.pid`): move the pointer onto the control button, then `MOUSE_EVENT: 1 <x> <y> 1` and `0`. The window is a plain 784x504 at +80+96; find it by size (no X name); all three kinds open at the SAME spot, so look at the newest window id.
- What each shows is in the instance's `.hq_manager/ui.txt` (read that first, it is what the window displays); the last result is `last.txt`. A plug click flips the real `ENABLED` row and appends a `TOGGLE` row to the book's `state/plug_log.txt`: restore the plug and tell the owner about the log rows.
- Close a window by its exact pid (`kill <pid>`); its projector module ends with it (check no `plugz_projector` is left).
- Read `X11-HQ-APP-DESIGN-WISDOMS.md` section 12b before changing it; the design is `PLUGS-AND-HARNZ-SPEC-2026-11-14.md` section 14.
- **dir and GLOBAL | no (2026-11-14):** the launcher has a 4th row `dir` (and the editor's full list a `-- folder --` group with `dir`): `pick open:dir` opens `@.apps/<book>/` with xdg-open, so do NOT click it while driving unless you want a real file manager window (a case swaps the opener through `HARNESS_MENU_OPENER`). A book can hide the global tests with `GLOBAL | no` in its own `harness.pdl`. The copyable example of all three kinds is `@.apps/pre-design/` (plug `office-example`, harnz `stamp this page`, test `check office tools`).
- **Click loops:** stop a click retry loop on a condition the click itself makes true (a `running` or `(exit` result line, a changed plug file), never on "the file is not empty": a stale or newline-only file ends the loop before the click lands (I reported a run that was my own earlier result).
