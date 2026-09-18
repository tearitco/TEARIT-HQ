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

## TODO later — file-explorer breadcrumbs wrap (do not start now)

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
