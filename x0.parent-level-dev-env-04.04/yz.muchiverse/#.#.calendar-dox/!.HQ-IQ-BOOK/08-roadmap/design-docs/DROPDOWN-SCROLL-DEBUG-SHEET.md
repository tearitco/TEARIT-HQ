# Dropdown scroll - debug sheet (what works, how to prove it, where it breaks)

2026-10-08. Long dropdowns (98 map pages in a Desk menu) scroll: a window of rows, the LAST row ("- cancel -") pinned, a thumb, numbered
^/v arrows. Owner confirmed pc-hq works well; livedesk top-bar menus work with the wheel, arrows and thumb drag.

## The one rule (shared logic)
`&.widgits/_shared-lib/khtpm_menu_window.c` (prefix `mw_`, pure, no X11): `mw_plan(total, cap, &scroll)`, `mw_slot(idx, scroll)`
(-1 = hidden), `mw_thumb`, `mw_scroll_from_y`. Cases + a mutant (no pinned row must FAIL) were run when it was added; copy that
pattern if you change it.

## The setting (no env var any more)
`#.desktop/hq_ui.pdl`: `dropdown_max_rows=N` (N >= 3). A dropdown with more rows than N scrolls even when it fits on screen.
Missing / 0 = scroll only when the list would run off the screen. It is read by the same loader as the other UI keys, so
change the number and append one byte to `#.desktop/hq_ui_pdl_changed.txt` (`printf K >>`) and open windows pick it up without a
restart. Applies to: livedesk dock menus (book:, page:, pals ...) and in-window dropdown-child menus (pc-hq tabs). Short lists
never change.

## Two code paths (both use mw_*)
1. Livedesk dock menu: its own X window (`g_dock_menu_win`), laid out in the dock layout block (look for `dd_rows_only`,
   `g_dock_dd_*`, `DD_STRIP_W`). Extras: ^/v are synthetic nav items (`DDSCROLL:-1/1`, handled in `activate_focused`; they bypass the
   dock's first-click-focus relay in `click_focus_then_activate`, otherwise the strip manager resets focus to row 1), thumb track
   painted in `dock_paint_menu`, thumb drag = press on the right strip + motion on the menu window (mask needs ButtonMotionMask).
2. In-window dropdown (pc-hq): `layout_fixed_rows_and_scrolllist` overlay pass + the generic scrollbar (`generic_sbar_register`,
   `kh_sbar_track_at`, `generic_sbar_scroll_from_y`). Menu shape: rows capped to 320 px, bar right of the rows.

## Prove it (no human needed)
- Frame dump of the real window: `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x <window-id> <out.png>` (override-redirect menus
  sometimes give BadMatch; then read the menu frame file `#.desktop/entity_menu_frame_<pid>_menu.txt`, column 4 = labels,
  first label = first visible row).
- Real mouse events with `xdotool` (XTEST = same path as a hand): wheel = `click 5`, drag = `mousedown 1`, `mousemove`..., `mouseup 1`.
  Relay mouse: `printf 'MOUSE_EVENT: 5 x y 1\n' >> #.desktop/entity_menu_history/<pid>.txt` (wheel only; clicks need two for the
  focus-then-activate step in most windows).
- Open the livedesk page menu: relay `KEY_PRESSED: 52` then `13` (cell 4). pc-hq tabs: relay mouse `MOUSE_EVENT: 1 400 44 1` twice
  (digits go to the game when interact is on).
- A long list for pc-hq without touching real data: temp project `@.apps/piececraft-hq/pieces/system/maps/zz_ddtest/game.pdl` with
  `n_desks 99` + `desk_N_id/label`, set `map_id=zz_ddtest` in `@.apps/piececraft-hq/pieces/world_01/state.txt`, relaunch pc-hq
  (`sh @.apps/piececraft-hq/open_pchq_board.sh <house>`). UNDO: restore map_id, delete the folder.
- Scratch window: a 99-row `.xhtpm` with sidebar + panel and `dropdown-child` rows (kept in `XO/14.oct8/dropdown_test_page_99_rows.xhtpm`)
  launched with `khtpm_core_render.+x <house> <file>`.

## Known traps (each cost time)
- A running window never gets a rebuilt renderer until its process restarts. Check start time vs binary time before believing a screenshot.
- `pgrep -f`/`pkill -f` match your own shell; kill by exact `ps` awk on the binary path.
- The deferred dropdown paint list held 32 rows INCLUDING hidden ones: row 33+ and a pinned cancel never painted (now 128, open rows only).
- Open dropdown rows serialize last; scrollbar elements must serialize after them or they sit under the rows.
- A projector cap of 8 desks hid every page past 8 (`pchq_board_projector.c`, now 128; UI buffer 64 KB).
- pc-hq's canvas also takes button-1 presses: a press on a scrollbar must not be a canvas press (`kh_sbar_track_at`).
- The thumb itself has NO nav number (owner); only ^ and v are numbered. Swipe-on-rows is NOT built (owner: skip).
- A pdl edit to `hq_ui.pdl` is the owner's settings file: add one line, never commit the whole file.

## Not done / open
- Dragging with the pointer outside the menu window (pc-hq keeps following across the screen; livedesk was not investigated).
- Entity context menus (`khtpm_entity.c`) still have their own scroll-less path (step 3 of the refactor).
- Narrow in-window menu wraps long labels onto two lines.
