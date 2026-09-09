# LIVEDESK-UI-SCALE — a settings size +/- (font + button/bar scale)

Status: **design** (2026-09-09). Not implemented.

Direct request: "settings should have size + & - button that will
increase font size and button/bar sizes."

---

## 1. Goal

One user-facing scale factor, adjusted by `+` / `−` in taskbar-settings,
that grows/shrinks **everything** a khtpm window draws — font size,
row/button height, padding, chrome, canvas-less layout metrics — live,
with no rebuild and no restart (same delivery path colours now use,
`70a5c1a5`).

Non-goals: per-window scale; DPI autodetection; scaling the raw
`<canvas>` pixel blit (a board framebuffer has its own size — separate
concern); reflowing text content.

---

## 2. What exists today

- **`scaled(int base_px)`** in `khtpm_core_render.c` (~line 2170) is an
  **identity stub**: `return base_px;`. It was mode-aware once
  (`g_dbhq_font_scale`, read from `#.desktop/hq_ui.pdl`), but that was
  removed in the 2026-09-04 `g_is_*` cleanup. The hook is still called
  in **21 places**, including the important one:
  `font_for()` (~2218/2227) already does
  `int size = scaled(st->has_font_size ? st->font_size : 12);`
  → **font-size is already routed through `scaled()`**. Make `scaled()`
  non-identity and CSS-driven text scales for free.
- Fixed `#define`s that do **not** go through `scaled()`:
  `ROW_H 24`, `CHROME_H 24`, `DOCK_BAR_H 36`, `POPUP_ROW_H 28`,
  `GRID_CELL_PX` (tile mode), `WIN_PX` (tile mode), `KH_WIN_FRAME 2`.
- `font_ui` (line 1940) — a single shared `XftFont` loaded **once** at
  startup for chrome/title/dock/tab text (drawn directly, not via
  `font_for()`); it would not scale without an explicit reload.
- Live-reload infra is already in place: `load_theme_colors()` runs at
  startup and on every `#.desktop/livedesk_theme_changed.txt` marker
  bump (HQ: `hq_idle_tick` → `pchq_theme_changed_dirty`; tile:
  `tp_main`'s `theme_changed_dirty`). Colours ride this; scale will too.
- `OPACITY_MINUS` / `OPACITY_PLUS` (`dispatch()` ~5513) is the exact
  template for the new buttons: read current value → clamp step →
  write file → apply live → repaint.

---

## 3. Storage

Reuse `#.desktop/livedesk_theme.pdl` — it is already the live-reloaded,
marker-backed settings file. Add one row:

```
SCALE        | ui                   | 100
```

Integer percent. Absent row → `100`. `apply_theme_op.c`'s
"preserve other COLOR rows" loop already carries unknown rows through;
extend it to also carry a `SCALE` row (or generalise to "carry every
non-bg/fg line").

Bounds **75–200**, step **25** (`75 100 125 150 175 200` — six stops,
matches the coarse feel of Opacity ±0.05). Clamp in the handler.

---

## 4. Implementation

### 4.1 `scaled()` + a global

```c
static int g_ui_scale_pct = 100;   /* 75..200, from livedesk_theme.pdl SCALE|ui */

static int scaled(int base_px) {
    if (g_ui_scale_pct == 100) return base_px;
    return (base_px * g_ui_scale_pct + 50) / 100;   /* round to nearest */
}
```

`scaled()` must stay pure and O(1) — it runs many times per
`assign_nav_and_layout()` pass (see the `khtpm-shared-layout-caution`
memory). Reading a plain int global is fine; never fopen inside it.

### 4.2 Load / live-reload

Add `load_ui_scale(void)` (reads the `SCALE|ui` row, clamps to
75..200, sets `g_ui_scale_pct`) and call it from **exactly the same
sites** `load_theme_colors()` is called:
- `main()` startup + `tp_main()` startup (next to the `bac09d24`
  `load_theme_colors()` call),
- `hq_idle_tick()`'s `pchq_theme_changed_dirty` block,
- `tp_main()`'s `theme_changed_dirty` block.

Simplest: call it *inside* `load_theme_colors()` so there is one
function and one call list. On a change, the existing
`hq_request_redraw()` / `need_redraw = 1` in those blocks already
forces a repaint; add an `assign_nav_and_layout()` there too (layout
metrics changed, not just colours) — the fullscreen-toggle handler
already shows the safe `assign_nav_and_layout(); redraw();` pair.

### 4.3 The fixed `#define`s

Wrap the layout-relevant ones at their use sites (not the macro — a
scaled macro would recompute per reference and some arithmetic mixes
them):

- `ROW_H` → `scaled(ROW_H)` in the flat-list / canvas-toolbar layout
  paths and row-height math.
- `POPUP_ROW_H` → `scaled(POPUP_ROW_H)` in the context-menu and Show
  Text popup layout + draw (row pitch and window height).
- `CHROME_H` → leave fixed for now (a taller titlebar buys little and
  every chrome-button y is hardcoded to it). Revisit if the +200%
  titlebar text clips.
- `DOCK_BAR_H` → `scaled(DOCK_BAR_H)` so the taskbar strip grows with
  the setting (its whole point). Audit the strip's own y math.
- Tile mode `GRID_CELL_PX` / `WIN_PX` → **phase 2** (entity windows are
  grid-snapped to the desktop; changing the cell size moves every
  entity — needs its own think, see §6).

### 4.4 `font_ui` reload

`font_ui` is opened once. On a scale change, close + reopen it at the
scaled size. Factor the open into `load_font_ui(void)` (currently
inline in `main()`/`tp_main()`), call it from the reload block:
`if (font_ui) XftFontClose(dpy, font_ui); font_ui = load_font_ui();`
Guard for the tile-mode local-`dpy` gotcha (see
`khtpm-tp_main-globals-footgun` memory) — pass the right Display.

### 4.5 Settings UI

`&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm`, next to the
opacity buttons:

```xml
<item id="ui-scale-minus" action="UI_SCALE_MINUS" class="opacity-btn" label="Size -"/>
<item id="ui-scale-plus"  action="UI_SCALE_PLUS"  class="opacity-btn" label="Size +"/>
```

`dispatch()` in `khtpm_core_render.c`, mirroring `OPACITY_MINUS`:

```c
if (strcmp(action, "UI_SCALE_MINUS") == 0 || strcmp(action, "UI_SCALE_PLUS") == 0) {
    int s = load_ui_scale_pct_from_file();          /* fresh read */
    s += (action[8] == 'P') ? 25 : -25;             /* PLUS vs MINUS */
    if (s < 75) s = 75; if (s > 200) s = 200;
    write_ui_scale_pct(s);                          /* rewrites SCALE|ui row in livedesk_theme.pdl */
    bump_theme_changed_marker();                    /* the shared append */
    load_theme_colors();                            /* picks up scale too (§4.2) */
    if (font_ui) { XftFontClose(dpy, font_ui); font_ui = load_font_ui(); }
    assign_nav_and_layout(); redraw();
    return;
}
```

`write_ui_scale_pct()` = same rewrite-one-row shape as
`write_theme_opacity()`. The marker bump means *other* windows also
re-read and relayout on their next tick.

Optionally show the current value: projector
(`taskbar_settings_projector.c`) publishes `${ui_scale}` and the xhtpm
shows `Size: 125%` between the buttons (like the opacity readout, if
one exists).

---

## 5. Risks / watch-items

- **Idempotent layout.** `assign_nav_and_layout()` runs repeatedly;
  every `scaled()` call must yield the same value for the same input
  within a frame. A plain int global satisfies this. Do **not** read
  the file inside `scaled()`.
- **Window growth.** A larger scale grows `g_win_w/g_win_h`; the
  Pixmap-grow + `XResizeWindow` path (used by the fullscreen toggle and
  live `.chtpm` reparse) already handles this — reuse it, do not add a
  second resize path.
- **Nav numbering** unaffected (counts, not sizes).
- **`font_for()` cache.** If `font_for()` memoises `XftFont*` by spec
  string, the spec already contains `pixelsize=<scaled>`, so a new
  scale = a new spec = a new font; no cache invalidation needed. Verify
  it keys on the full spec.
- **Tile mode** (`tp_main`) shares `scaled()` and `font_for()`, so
  entity-window *text* scales in phase 1; entity-window *geometry*
  (`GRID_CELL_PX`) is phase 2.
- **Dock strip** math is the most hardcoded; budget real time for the
  `DOCK_BAR_H` audit or ship phase 1 without strip scaling and note it.

---

## 6. Phases

1. **P1 — text + HQ/menu geometry.** `g_ui_scale_pct`, real `scaled()`,
   `load_ui_scale()` folded into `load_theme_colors()`, `font_ui`
   reload, `ROW_H`/`POPUP_ROW_H` wrapped, `UI_SCALE_±` verbs + two
   xhtpm buttons. Every HQ window, context menu, and Show Text popup
   scales. Verify at 75 / 125 / 200 %.
2. **P2 — taskbar strip.** `DOCK_BAR_H` + the strip's own y math.
3. **P3 — tile/entity geometry.** `GRID_CELL_PX` / `WIN_PX` + the
   desktop grid-snap implications (every entity's saved
   `desktop_pos.txt` is in cell units — decide: reinterpret, or store
   px and re-snap).

---

## 7. Files

| File | Change |
|---|---|
| `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` | `g_ui_scale_pct`, real `scaled()`, `load_ui_scale()` (in `load_theme_colors()`), `load_font_ui()` + reload, wrap `ROW_H`/`POPUP_ROW_H`, `UI_SCALE_±` in `dispatch()` |
| `*.monads/*.livedesk-taskbar/ops/apply_theme_op.c` | carry a `SCALE` row through the rewrite |
| `&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm` | `Size -` / `Size +` items |
| `&.widgits/taskbar-settings/ops/taskbar_settings_projector.c` | (optional) publish `${ui_scale}` |
| `#.desktop/livedesk_theme.pdl` | new `SCALE | ui | 100` row (runtime data, not committed) |

Related: `[[khtpm-tp_main-globals-footgun]]`,
`khtpm-shared-layout-caution` memory, `70a5c1a5` (colour = marker, no
rebuild — same delivery path this uses).
