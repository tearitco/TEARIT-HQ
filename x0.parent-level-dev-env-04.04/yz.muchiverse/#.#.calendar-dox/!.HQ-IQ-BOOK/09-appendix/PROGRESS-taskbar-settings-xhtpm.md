# PROGRESS - taskbar-settings / swatch-picker -> static xhtpm + projector

**Branch:** `chtpm-var-substitution` (not merged to main)
**Date:** 2026-09-03
**Status:** phase 1 done - read-only projection + full 2-phase pick chain
verified headlessly. Parallel window only; NOT wired into any menu.

---

## 1. What this ports

The swatch-picker / taskbar-settings window (one window, gated by
`g_is_swatch_picker` in `khtpm_core_render.c`). It is a flat popup: a
12-swatch bg/fg palette grid + Opacity -/+ + a close `x`. Two-phase
pick: swatch 1 = background, swatch 2 = foreground/text, then the theme
is applied and the window closes.

## 2. Files (all new, under `44.xyz.01.00/&.widgits/taskbar-settings/`)

| file | role |
|---|---|
| `taskbar-settings-pal.xhtpm` | static template. `class="taskbar-settings-pal database-window"`, `vars="#.desktop/taskbar_settings_ui.txt"`, one `<module>` = the projector. |
| `taskbar-settings-pal.css` | palette hex (copied verbatim from `ops/taskbar_settings.css`) + `.ring-bg` / `.ring-fg` border rules for the 3rd state. Merged on top of `entity_menu_default.css` (renderer auto-merges the `.css` next to the `.xhtpm`). |
| `ops/taskbar_settings_projector.c` | the `<module>` UI projector. Reads the manager's `#.desktop/taskbar_settings_state.txt`, writes `#.desktop/taskbar_settings_ui.txt` (content-gated). |
| `ops/build_taskbar_settings_projector.sh` | `gcc` one-liner (mirrors `build_evhq_projector.sh`). |
| `button-pal.sh` | parallel launcher. Starts the shared renderer on the xhtpm **and** the unmodified `swatch_picker_manager.+x` (the generic default/popup mode does not fork it; only the `g_is_swatch_picker` path did). House-global single instance, no ARG3. |

**Untouched (rollback):** `ops/taskbar_settings.chtpm`,
`ops/taskbar_settings.css`, `ops/button_taskbar_settings.sh`,
`ops/swatch_picker_manager.c`, and all `g_is_swatch_picker` C in
`khtpm_core_render.c`.

## 3. Why the class is safe

`khtpm_core_render.c` ~line 17902 sets `g_is_swatch_picker` only on an
**exact** class-token match of `"swatch-picker"`. `db-hq` / `events-hq`
/ `palettes` / `bookmarks` / `stats-hq` are matched the same way.
`"taskbar-settings-pal"` and `"database-window"` match none of them.
Verified headlessly: the frame dump landed at
`#.desktop/entity_menu_frame_<pid>.txt` (generic name), not
`taskbar_settings_frame_<pid>.txt` (the `g_is_swatch_picker` name).

## 4. Why it works without the flag

- **Grid layout**: the swatch-grid layout path
  (`khtpm_core_render.c` ~8716) keys on "any `<item class="swatch">`
  among the page children", not on `g_is_swatch_picker`. All 12
  swatches + the non-swatch opacity/close items are positioned exactly
  as the old window (byte-identical x/y/w/h and nav_index 1-15 vs the
  old `taskbar_settings_frame.txt`, except `g_win_w`: 260 here vs 420
  in the old dump - cosmetic, see §7).
- **Actions**: `dispatch()` handles `PICK:<n>`, `OPACITY_MINUS`,
  `OPACITY_PLUS`, `CLOSE` as generic recognised tokens
  (~8918-8949), not flag-gated. `PICK:<n>` writes
  `#.desktop/taskbar_settings_action.txt` with an incrementing `seq=`,
  which is exactly what `swatch_picker_manager.c` already polls for.

## 5. State-file schema

### INPUT `#.desktop/taskbar_settings_state.txt` (written by `swatch_picker_manager.c`, unchanged)

```
phase=<0|1|2>    0 = picking background, 1 = picking foreground, 2 = applied
bg=<0..11|-1>    chosen background swatch index (palette order)
fg=<0..11|-1>    chosen foreground swatch index
apply=<0|1>      1 once both chosen; manager then execs apply_theme_op.+x and exits
```

Palette order (from `swatch_picker_manager.c` `g_hex[]`):
`black white charcoal silver red orange yellow green cyan blue purple pink`.

### OUTPUT `#.desktop/taskbar_settings_ui.txt` (written by `taskbar_settings_projector.c`)

```
prompt=<"pick a background swatch" | "pick a text swatch" | "theme applied">
phase=<0|1|2>
bg_name=<palette name | ->
fg_name=<palette name | ->
sw_0_ring .. sw_11_ring = <"ring-bg" | "ring-fg" | "">
```

Content-gated: the projector keeps the last-written buffer and only
`rename(2)`s a new `ui.txt` when the bytes change.

## 6. Template mapping

| old chtpm | new xhtpm |
|---|---|
| `<window label="taskbar settings">` | `<window label="taskbar settings - ${prompt}">` - phase feedback rides the title bar (the old window drew it as an XftDraw overlay in the `g_is_swatch_picker` path; a generic `<text>` child is not positioned by the swatch-grid layout path, so the title bar is the no-new-nav-row way to surface it). |
| `<item class="swatch sw-black" .../>` x12 | `<item class="swatch sw-black ${sw_0_ring}" .../>` x12 - `${var}` substitution inside `class=` (already shipped, used by events-hq `class="${pg.active_class}"`). Empty `${sw_N_ring}` adds no stray class (verified: frame shows `swatch,sw-black`, not `swatch,sw-black,`). |
| opacity-minus / opacity-plus / close | unchanged, same `action=` tokens. |

## 7. Verified headlessly (X :0, `button-pal.sh`, key injection per HANDOFF §4)

- Launch: renderer pid + manager pid both come up; frame dump at
  `entity_menu_frame_<pid>.txt`.
- Region layout matches the old `taskbar_settings_frame.txt` (6x2 grid
  at the same coords, opacity buttons stacked below, close in chrome),
  nav_index 1..15 in order, no overlap.
- `${prompt}` resolves into the title bar ("taskbar settings - pick a
  background swatch").
- `KEY_PRESSED: 13` on swatch 1 -> generic `dispatch("PICK:0")` ->
  `taskbar_settings_action.txt` = `seq=1 / PICK:0` -> **unmodified**
  `swatch_picker_manager.c` -> `state.txt` `phase=1 bg=0` -> projector
  -> `ui.txt` `sw_0_ring=ring-bg`, `prompt=pick a text swatch` ->
  renderer content-hash reparse -> frame line becomes
  `item|sw0|swatch,sw-black,ring-bg|...`.
- `KEY_PRESSED: 112` PNG (`/tmp/entity-menu-frame.png`): swatch 0 shows
  a **green ring** (chosen bg, 3rd state) clearly distinct from the
  `[>]` focus cursor on swatch 4. All 12 palette colours correct.
- Second pick (`phase=2 bg=0 fg=4 apply=1`): `livedesk_theme.pdl`
  updated to `bg #000000 / fg #ef4444` - the manager's
  `apply_theme_op` chain ran end to end.
- `KEY_PRESSED: 27` (Esc) closes the renderer cleanly.

## 8. The 3rd-state "chosen" ring - NO renderer gap

Expressible with `class=` + CSS alone:

- projector emits `sw_<i>_ring = ring-bg | ring-fg | ""`;
- template substitutes it into `class="swatch sw-red ${sw_4_ring}"`;
- `taskbar-settings-pal.css` gives `.ring-bg { border: 3px solid #22c55e }`
  / `.ring-fg { border: 3px solid #ffd24a }`;
- `khtpm_draw_core.c` `draw_elem()` fills the bg first, then strokes
  `border-color` on top with `border-width` passes (lines ~511-519), so
  the border reads as a ring over the swatch fill. The focus ring is
  drawn separately just outside the box (~531), so the two coexist.

The old `g_is_swatch_picker` overlay drew the ring *outside* the swatch
(`x-2 .. w+4`); this version's ring is a 3px *inner* border. Visually
equivalent, no renderer change. It also distinguishes bg vs fg by
colour (green vs gold), which the old overlay did too (green box + a
text line).

## 9. Parity gaps (honest)

1. **No auto-close after a completed 2-phase pick.** The old window's
   `hq_idle_tick` (`g_is_swatch_picker` branch) polls `state.txt` and
   sets `g_quit=1` on `apply=1`. The generic path has no such poll, so
   the PAL window stays open after the theme applies; the user closes
   it with `x` / Esc. The manager still applies the theme and exits
   normally. Fixing this generically would need a "quit on a
   `vars=` key" mechanism in the renderer (out of scope; not a
   swatch-picker-specific need).
2. **Phase status is a title-bar string, not the old two-line
   green-accented overlay** ("Pick PRIMARY, then Enter" + the chosen
   colour name lines). Same information, less prominent. A generic
   `<text>` child would need the swatch-grid layout path to position
   non-`item` children (it currently `continue`s past them) - a real
   generic gap if a richer status block is wanted, but not patched.
3. **Nav badges (`[ ]1.` etc.) paint over the 34px swatches.** The old
   overlay suppressed them. Cosmetic; every generic navigable item
   gets a badge.
4. **`g_win_w` is 260 vs the old 420**, so the title string clips. The
   generic grid layout derives width from the grid extent; the old
   path widened for the overlay text. Cosmetic - a `width:` rule on the
   window in the `.css` could set it if desired.
5. **One manager per launch.** `button-pal.sh` starts one
   `swatch_picker_manager.+x`; it exits after one pick cycle (or
   CLOSE). A second pick in the same window session would need the
   manager restarted. The old path re-forked it on each window launch
   only, so this matches for the normal "open, pick once, done" flow.

## 10. Left to do

- Wire into the taskbar HQ menu (only after sign-off; deliberately not
  done - handoff rule).
- Address gap #1 (auto-close) if the window is to fully replace the old
  one - needs a generic renderer mechanism, own decision.
- Then retire the `g_is_swatch_picker` C path + `taskbar_settings.chtpm`
  + `button_taskbar_settings.sh` (own commit, like the `evhq_*` plan).
