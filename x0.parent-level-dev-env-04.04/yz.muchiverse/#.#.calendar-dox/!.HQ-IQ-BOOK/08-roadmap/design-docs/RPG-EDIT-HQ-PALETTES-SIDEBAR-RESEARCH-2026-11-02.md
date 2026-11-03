# rpg-edit-hq: a "palettes" left sidebar instead of the "pets" one (research only, 2026-11-02; nothing built)

Owner ask: remove the pets sidebar from the editor; add a minimizable left sidebar "palettes" showing the palette list, or one palette's tiles, with a back button (href-like); tiles stacked vertically (the taskbar shows them horizontally). Keep pets as a minimized bar that SWAPS with palettes when chosen. Also: the pets bar has no scroll thumb in short windows.

## What exists today (read, not run)
- File: `@.apps/rpg-edit-hq/board/pchq-board.xhtpm`. Left bar = `<row id="side-l" class="canvas-overlay-left ov-pin ov-chrome pchq-hotbar theme" show="${side_left}">` (pets, stats, party buttons). Right bar `side-r` = chat. The footer cells `sl-toggle` ("pets") / `sr-toggle` ("chat") run `layout-studio/ops/+x/side_toggle.+x board/state/sides.txt side_left|side_right toggle`; each bar's `_` runs it with `hide`. Flags live in `board/state/sides.txt` (`side_left=1`, empty = hidden). Width from `board/side_style.pdl` (`STYLE | pin_width | 160`). A pinned bar TAKES its width out of the canvas (renderer `khtpm_core_render.c` ~6929-7140).
- **The missing scroll thumb is a one-word fix.** The renderer already has class `ov-scroll` for pinned bars (owner request 2026-10-23, ~line 7130: parks items that do not fit off screen, registers the generic scrollbar: track, thumb, arrows, wheel, page keys). rpg-pet, cdda-hq, rpg-space, gta-hq, df-hq, rpg-dust, civ-rpgce, dsr-rpg, tcg-rpgce, mcl-hq use it; rpg-edit-hq's `side-l` and `side-r` do not (grep count 0). Add `ov-scroll` to the class list. Not tried: whether it behaves with a `repeat` of image tiles.
- `href`: the renderer has an `objects.pdl` href + BACK page stack (`khtpm_core_render.c` ~19812-19823, 22371), made for `.chtpm` pages. Not verified for `.xhtpm` pinned bars. A pinned bar is a `row` inside one page, so the simpler, proven mechanism is the one this window already uses everywhere: a state key + `show="${...}"` (same as `side_left`).
- Palettes data: categories are rows in `&.widgits/palettes/pallets.pdl` (`CATEGORY | key | label | ...`: emojis, elements, rmmv, piececraft, cdda, df, doom, paint, ...). In the taskbar each opens a window `palettes-<key>.xhtpm` with a swatch grid fed by a per-category manager (`palettes_manager`/`elements_palette_manager`), and `palettes_menu.sh place|arm-rmmv|arm-pc|arm-cdda|arm-df|arm-doom ...` arms a brush. rpg-edit-hq itself has NO palette code today: its tiles come from `catalog.pdl` (shop items from RPG Maker sheets). So "place from the palette" needs a decision on which palettes feed THIS editor (see questions).
- Sprites in item labels exist (`sprite="${ent.sprite}"` + `sprite-inline sprite-big` in the footer), so a vertical list of tile thumbnails is a `repeat` of `<item sprite=...>` rows.

## Proposed shape (for approval)
1. Two pinned left bars in the same slot: `side-l` (pets, kept, default minimized) and a new `side-p` (palettes). A single state key `left_mode=pets|palettes|none` in `sides.txt`, written by one op (extend `side_toggle` or a tiny new verb); the footer cells "pets" and "palettes" set the mode, `_` sets none. `show="${left_pets}"` / `show="${left_palettes}"` so only one is drawn = they swap places.
2. `side-p` has two views driven by `pal_view=list|tiles` + `pal_key=<category>`: LIST = one button per `CATEGORY` row (from pallets.pdl, via the projector); TILES = a "< back" item (sets `pal_view=list`) then the category's tiles as a vertical `repeat`; clicking a tile arms the brush (the same `palettes_menu.sh arm-*` / placer chain the taskbar uses, or the editor's own placer).
3. Add `ov-scroll` to both bars (fixes the missing thumb; needed anyway for long tile lists).
4. Width: 160 px holds ~2 tiles of 48 px per row only if the bar can lay out a grid; a plain vertical list is 1 per row. Owner said "vertically", so 1 column, scrolling.

## Open questions (owner)
- Which palettes should the editor show: all categories from pallets.pdl, or only the ones that make sense for a pets-like room (rmmv, emojis, piececraft, cdda, df)? Does choosing a tile place a floor tile, furniture, or arm the existing placer?
- Where do the tiles come from at runtime: reuse the taskbar's palette managers (extra processes per open palette; footprint) or read the category's tile list files directly in `rpg_edit_hq` (lighter)?
- "href": OK to implement with the state-key mechanism above instead of a real href page stack?
- Should the pets bar stay minimized by default, and should the choice survive a restart (sides.txt already persists)?

## Risks
- Renderer layout is shared (see khtpm shared-layout caution): only add classes/rows in the xhtpm, no new `layout_*` branch in `khtpm_core_render.c`.
- Two pinned bars on the same side: confirm the pin-width pass handles only the visible one (a hidden `show=` row must not reserve width). Test with sides.txt both ways.
- Live test through the relay (window pid), screenshots before/after; restore the owner's `sides.txt` after.
