# Cursword File / Inventory / robot-chat — planning burst (2026-09-17)

**Status:** PLAN ONLY. No code this burst. Waiting on user check-in.
**Why:** faster bootstrap than a full LLM or event-chat on Cursword main.

## What we are NOT doing this pass

- Not a full transformer / backprop / famous-LLM training loop.
- Not "event-chat dropped into Cursword main" (previous naive theory).
- Not patching `nav.sh` parser-layer commands (still dead; use
  `hqcell`/`mgrcode` + per-PID `entity_menu_history/<pid>.txt`).
- Not rewriting kilo §4 WSR-CIV.

## Direction (user, 2026-09-17, after Claude weekly-limit disconnect)

1. **Cursword gets two new context-menu buttons: File, Inventory.**
   Existing rows stay: Dir, Chat, Bookmarks, Play, Close, Cancel
   (`pals/cursword/meta.pdl` → generated `menu.chtpm`).
2. **File mode:** Cursword itself can become a 📁 icon with its PNG
   drawn on top. That icon represents `cursword/inventory/` (not
   cursword main). Opened → grid file-explorer of that dir.
3. **Inventory button:** opens an x11-hq window, same grid of inventory
   items. Drag entities in/out the same way as File mode.
   Linux-FS equivalent: `mv <entity-dir> cursword/inventory/`.
4. **Robot chat is a separate entity** (🤖️ emoji), events modified on
   *that* pal — not Cursword main. Drag the robot into the Cursword
   folder (or out to desktop / another inventory). Chat happens from
   Inventory, not Cursword main.
5. **Gemma later, not first:** when the user types, gemma DESCRIBEs
   synonym/antonym banks per word; a second pass sets category
   "attention" by hand/cosine (no BP/FF); then a late famous-style
   word-prediction stage. Architecture-only until File+Inventory exist.
6. Palette drag-drop (RPG Maker tiles, one-way) is the existing
   precedent to reuse, not invent.

## Already on disk (do not re-derive)

- Cursword `inventory.txt` exists (`qolq=105`) — not a directory of
  entities yet; do not assume it is `inventory/`.
- `METHOD | Dir` already `xdg-open`s the pal dir — File/Inventory are
  **not** that. They target a nested inventory, in-house grid, not
  the host file manager.
- kilo §11 still says add "AI Chat (events)" next to Chat. **Superseded
  as the first new button.** Chat stays. File + Inventory go first.
- Relay: parser layer dead (live-probed). Window drive =
  `entity_menu_history/<pid>.txt`. PID targeting design =
  `08-roadmap/design-docs/RELAY-WINDOW-TARGETING-DESIGN.md` (design
  only).

## Proposed next burst (after you confirm)

**Only:** add File + Inventory METHOD rows on Cursword `meta.pdl`,
regenerate `menu.chtpm`, make Inventory open *something* visible
(even a stub grid of `inventory/` after mkdir). Stop. Relay-click
those two rows. Ask before robot pal or gemma.

## Open questions (please answer)

1. File vs Inventory: same window/code, two entry points — or File =
   icon-transform of the Cursword sprite, Inventory = always a separate
   x11-hq window?
2. `inventory/` vs current `inventory.txt` — new dir beside the txt,
   or replace?
3. Robot: new pal cloned from an existing 🤖️, or retarget an already-
   on-desktop pal?
4. First visible grid: reuse palettes swatch-grid (`class="swatch"`)
   / file-hq, or a new x11-hq layout?
5. Keep kilo WSR-CIV/DSR running in parallel, or pause those until
   File+Inventory exist?
