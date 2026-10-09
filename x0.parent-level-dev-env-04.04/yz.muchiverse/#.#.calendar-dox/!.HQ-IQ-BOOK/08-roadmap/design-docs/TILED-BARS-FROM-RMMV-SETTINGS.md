# Tiled bars from RPG Maker tiles — task — 2026-10-09

Status: researched, not wired into the renderer. The live taskbar and
the x11-hq menus stay on solid fills until this is built and the
setting is turned on. A throwaway visual is
`TILED-BARS-DEMO.html` next to this file. It is not the desk.

Branch for the work: `grok` (worktree `/tmp/grok-bars`). The live
checkout stays on `claude`.

## What was asked

Keep the taskbar and the menus that already work. Add a choice, edited
only from the HQ menu's settings row, to paint those bars with one
RPG Maker tile. Some of those tiles already look like rectangle bars.
The label text should sit inside that bar. The same screen must be
able to switch the look back.

## Where settings already is

The strip numbers that row **22**. A live menu frame records it as
`dropdown-child|settings` with relay code `5005` on `strip-cell-1`
(the HQ cell). `livedesk_taskbar.pdl` stores the same row as
`hq_menu_6_label = settings` and
`hq_menu_6_cmd = livedesk:open-settings`. The "6" is the pdl slot.
The "22" is the number the strip draws. Both are this one row.

`khtpm_taskbar_manager.c` handles `livedesk:open-settings` by launching
the settings button from `livedesk_launchers.pdl` and closing the HQ
menu. The window that opens is
`&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm`
(`class="taskbar-settings-pal database-window"`). It does not trip
`g_is_swatch_picker`. Its buttons are already the house controls:
color swatches, opacity, size, font, single-click vs two-click, close.

Those verbs are handled in `dispatch()` in
`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`
(`UI_SCALE_MINUS` / `UI_SCALE_PLUS`, `UI_FONT_FAMILY_NEXT` /
`PREV`, `CLICK_TWOSTEP_TOGGLE`, `OPACITY_MINUS` / `PLUS`).
Size, font, and click mode write `#.desktop/hq_ui.pdl` and other
windows follow through `hq_ui_pdl_reload_if_changed()`. Colors and
opacity write `#.desktop/livedesk_theme.pdl`.

The tile source is Palettes, taskbar cell 7. Category key `rmmv`,
label `RPG Maker Tiles`, in
`&.widgits/palettes/pallets.pdl`. The strip has also drawn that
category as "20. rpg maker tiles". Crops already live under
`&.widgits/palettes/sprites/rmmv/<sheet>/<nnn>/sprite.csv`
(48px RGBA rows, for example `SF_Outside_c/210`).

## The setting

One new key in `#.desktop/hq_ui.pdl`, because that file is already
read by every HQ window and already live-reloads. Empty means off.
That is the rollback.

```
bar_tile=
```

When set, the value is a house-relative path to one `sprite.csv`
under `sprites/rmmv/`. Example:

```
bar_tile=&.widgits/palettes/sprites/rmmv/SF_Outside_c/210/sprite.csv
```

Settings (the window opened from strip item 22) is the only editor:

- A row labeled `Tiled bars` whose action is `BAR_TILE_TOGGLE`.
  Off writes `bar_tile=` (empty). On writes the last picked path,
  or stays off if nothing has been picked yet.
- A row labeled `Bar tile` whose action is `BAR_TILE_PICK`. It opens
  the existing `rmmv` palette (the same picker Palettes already uses)
  and stores the clicked crop's `sprite.csv` path into `bar_tile`.
- The row's own label shows `off` or the sheet and tile number, the
  same way the font row shows the current family.

No new taskbar cell. No new HQ menu row. No edit to
`khtpm_taskbar_manager.c` menu tables. No change to palettes place
or to `tp_arm_placer_rmmv`.

## What paint does, only when the key is non-empty

In `draw_elem()` (`&.widgits/_shared-lib/khtpm_draw_core.c`), after
the solid `e->bg` fill and before the label:

- If `bar_tile` is empty, do nothing. This is the current desk.
- If the element is a bar that already carries a text label (a
  taskbar cell, an HQ menu row, a window button or list row), blit
  that one tile stretched to the element's rectangle, then draw the
  existing label on top, inset so the glyphs sit inside the art.
- Window title strips and the taskbar strip use the same tile.
- Swatches, the palettes tile grid, and sprites that are themselves
  tiles do not take the bar. A tile used as a bar must not also be
  restamped as a bar.

Stretch first. A 9-slice (keep the tile's left and right caps, repeat
the middle) is the follow-up if a stretch looks bent. The demo shows
both so that choice can be made by looking.

## Rollback

From the same settings window: `Tiled bars` off. That clears
`bar_tile`. The next `hq_ui_pdl_reload_if_changed()` returns every
open window and the taskbar to solid fills. No binary swap is
required for the rollback once the reader exists. Until the reader
exists, this document and the HTML demo are the whole change, and
deleting them removes the demo.

## Out of scope

- Replacing menu structure, relay codes, or cell order.
- Autotiling, animation frames, or desk placement of the same tile.
- Using the tile as a window background behind the whole page.
  Only the rectangle bars that hold text.

## Build order, after a look at the demo

1. Settings rows plus the `hq_ui.pdl` key. Paint stays solid.
   Proof: the key flips between empty and a path, and the taskbar
   looks unchanged.
2. `draw_elem()` stretch, gated on a non-empty key.
   Proof: one HQ window and the taskbar show the tile with the old
   labels still readable, and Off puts the solid fill back.
3. Only if the stretch looks wrong: 9-slice the same tile.
