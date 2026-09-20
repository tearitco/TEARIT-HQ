# 2026-09-18 — 2do

## PAUSED (momentarily) — resume after Cursword Inventory hook

**WSR-CIV + DSR parallel tracks** from
`13.agent-coms/KILO/claude-2-kilo-9.17.md` (§1, §4, §10).

User 2026-09-18: pause those while we bootstrap Cursword
File-hook + Inventory dir + later robot-chat-as-entity. Not cancelled.
Not handed off to another agent this morning.

When unpausing, kilo (or whoever) still starts at:

- events-creation screen via **live** relay (`hqcell`/`mgrcode` +
  `entity_menu_history/<pid>.txt` — not dead `nav.sh nav`)
- piececraft-hq ".main tab only" board bug still open in `04-bugs/BUG-LOG.md`
- DSR menu class (New/Load/Save/Save As) still a parallel track

Resume trigger: Inventory window shows `cursword/inventory/` as a
grid and a human has clicked it once via relay.

User-facing writeup (do not lose the reason):
`10-user-docs/2026-09-18/WHY-DSR-WSR-PAUSED.md`.

## Hover UX — USER SAW IT (slow; not 60fps)

Pal poll 300ms, FE loop 150ms. Tighten later if wanted. Un-factor next.

## Hover UX (2026-09-18) — file-area fill + banner

On drop-hover the explorer **body** paints `drop_highlight` and a
dashed frame + `[ drop into inventory: name ]` on top of the list.
Reopen Inventory. Un-factor pal `+x` is NEXT, not this burst.

## A landed (stat-poll list refresh) — reopen File Explorer once

Manager `stat`s `current_dir` each 50ms; mtime/nlink change → relist.
Place-grid (B) still waiting. Architecture: `08-roadmap/design-docs/XHTPM-RE.md`.

## Slice 1 — window highlight + desktop pal → Inventory (landed, check)

Highlight color = `drop_highlight=` (FE default `#88ff66`). Drag a
desk pal over File Explorer / Inventory: thick green frame. Release:
`mv` pal dir into `dir=` (current explorer folder). Spec slices 2–5
still later.

See `08-roadmap/design-docs/INVENTORY-DROP-AND-WINDOW-HIGHLIGHT-2026-09-18.md`.
Palette Place ≠ Xdnd. Highlight the **window** on hover; reuse
`drop_action=`. Do not start until the next sprint.

## Overlay (palette placer) — check 2026-09-18 (user: looks good)

`tp_arm_placer_rmmv.c`: solid amber 12% wash replaced with **64px
tic-tac-toe lines** (ARGB if the display has a 32-bit visual). Hole
around the picker unchanged. Arm a palette RMMV tile and look: grid,
not yellow screen. Esc still cancels.

## WIP — file-explorer grid / ctx menu

Browse grid names: landed. Right-click: **Cut/Copy/Paste/Delete/Place**
via the same CTXMENU entity-menu as HQ (meta.pdl +
kh_open_cli_io_context_menu). Place writes `fe_place_armed.txt` only
(wireframe overlay later). Delete: files unlink, dirs rmdir if empty.
Left click still opens dirs.

## Scheduled — file-explorer extra X (done 2026-09-18)

User: dual X never intended; auto chrome only. Cause: template
`<item id="chrome-close">` plus swatch/flat `g_default_close_elem`.
Removed the template item. Relaunch File Explorer to see one X.

## TODO today — chrome `_` / `!` / `X` on non-trivial windows (done)

User 2026-09-18: besides X, chrome should get **minimize** (`_`) and
**expand/fullscreen** (`!`), especially File Explorer.

Swatch-grid + flat-page now call `kh_place_chrome_btn` for `_` `!` `X`
(same `g_default_*_elem` as sidebar+panel). Chrome hit-zone uses
minimize x even without sidebar. Relaunch File Explorer.

## TODO later — file-explorer breadcrumbs wrap (landed 2026-09-18, check)

User 2026-09-18: crumb path is extremely long; want wrap + grow
vertical until the window is resized. File Explorer is already
`user-resizable`.

Difficulty (not doing it this pass): **medium, localized**. Crumbs are
a `<tabbar>` laid out as **one** horizontal row
(`assign_nav_and_layout` flat-page tabbar loop, ~tx += tw). Palettes
already wrap chips (`cx + w > g_win_w` → next `cy`). Same cursor on
`tab` children, add row height into `canvas_tabbar_h` so list/grid
start lower. Grid/swatch path currently has **no tabbar** — wrap must
also apply there or crumbs stay broken in Grid View. Not a new
`layout_*`. Pitfall: nav numbers + click hitboxes per wrapped row.

## Active today

- Cursword **Inventory** METHOD landed (opens File Explorer at
  `cursword/inventory/`). Slow migrate of `inventory.txt` / `qolq` still
  open. File METHOD stub + 📁 icon mode still wait. New 🤖️ pal later.

## 2026-09-19 — inventory follow-ups (user, deferred)

- **Cli-io experimental option on ALL entity context menus** (mv <nav#> <nav#> and further verbs) — later. Only `mv` exists (manager `CLIIO_MV:`); the auto-added "Cli-io" row in `load_methods()` has no pal-side handler yet; File Explorer's `meta.pdl` Cli-io row stays out until it does.
- ~~**File Explorer "Search" cli-io** as its own toolbar button above Back~~ ✅ DONE 2026-09-19: `<cli_io id="search">` row above Back (list + grid); `file_explorer_manager.c` polls `cli_io_state.txt` `search=` and filters the current dir live (case-insensitive name substring); empty = all; cleared on directory change. Grid mode needed a small generic allowance for `cli_io` in the swatch-grid chip row (`khtpm_core_render.c`).
- Real sprite in Inventory tiles — **done** (0eda3dc5, transparency a53ddb80).
- Inventory row on every entity — **done** (e8a5baa4); named explorer instances (`button.sh run-instance <dir>`) so several Inventory windows can be open.
- Cross-window Cut/Paste (global `#.desktop/fe_clipboard.txt`) — **done** (711a3c46). Place into an open Inventory (pointer-over-window highlight + click moves item in) — **done**, see commit for Task 4.
- ✅ **Keyboard/relay context menu (2026-09-20):** Space on the focused nav item opens the right-click menu in HQ windows; on the dock/strip Space = Enter (pal cell → its menu, header cell → dropdown). Armed text fields keep Space literal. Not covered: desk pal windows themselves (no nav focus; they open from the dock cell).
- Not yet: right-click methods for a PAL that has no `sprite.csv` (menu keys off the tile's sprite= dir); Place from the desk (only from an explorer); dragging between explorer windows (cut/paste and Place cover it).
