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

## WIP — file-explorer grid (do not "finish" in one pass)

User: grid does not show image or filename; left-click dir currently
opens the dir. Transition: **right-click → context → Move/Drag**
arms placement-grid image. Left click stays browse/select.

See GROK plan section "File-explorer GRID — WIP". Code only when
asked, one gap at a time.

## Scheduled — file-explorer extra X (done 2026-09-18)

User: dual X never intended; auto chrome only. Cause: template
`<item id="chrome-close">` plus swatch/flat `g_default_close_elem`.
Removed the template item. Relaunch File Explorer to see one X.

## TODO today — chrome `_` / `!` / `X` on non-trivial windows

User 2026-09-18: besides X, chrome should get **minimize** (`_`) and
**expand/fullscreen** (`!`), especially File Explorer and other
non-trivial HQ windows.

Already true on the **sidebar+panel** path (`g_default_minimize_elem` /
`g_default_fullscreen_elem` / `g_default_close_elem` in
`khtpm_core_render.c`). **Swatch-grid and flat-page** (File Explorer
list/grid) only synthesize **X**. Do not add template buttons — extend
the same auto-chrome trio those two layout paths already use for X.

Not started this burst. Ask before coding.

## Active today

- Cursword `inventory/` dir + slow migrate of `inventory.txt` /
  `qolq` (see `13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md`)
- Experimental File METHOD hook only; full 📁 icon mode waits
- New 🤖️ pal later, not this burst
