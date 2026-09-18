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

## Active today

- Cursword `inventory/` dir + slow migrate of `inventory.txt` /
  `qolq` (see `13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md`)
- Experimental File METHOD hook only; full 📁 icon mode waits
- New 🤖️ pal later, not this burst
