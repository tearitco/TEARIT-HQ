# LIVEDESK-UI-SCALE — a settings size +/- (font + button/bar scale)

Status: **design** (2026-09-09). Not implemented.

Direct request: "settings should have size + & - button that will
increase font size and button/bar sizes."

Precursor: `11.brainstorm/2026-09-05/FONT-SIZE-AND-UI-SCALE-BRAINSTORM.md`
(current-state findings + options — read it; this doc promotes it to a
plan and picks Option 1 + a fonts-first phasing).

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
  **identity stub**: `return base_px;`. Its own comment still describes a
  `g_dbhq_font_scale` "read from `#.desktop/hq_ui.pdl`" that **does not
  exist** (grep-confirmed — stale comment, same class as the
  `click_two_step` / `dbhq_load_font_scale()` stale comments). The hook
  is still called in **~21 places**, including the important one:
  `font_for()` (~2218/2227) already does
  `int size = scaled(st->has_font_size ? st->font_size : 12);`
  → **CSS font-size is already routed through `scaled()`**. Make
  `scaled()` non-identity and CSS-driven text scales for free.
- **`#.desktop/hq_ui.pdl` already ships `font_scale=1.25`** — documented
  in that file ("multiplier applied to both font sizes and layout box
  sizes … Range 0.5-3.0") and **read by nothing**. `hq_ui.pdl` is the
  house-wide UI-knobs file (`click_two_step`, `focus_grab`,
  `emoji_sprite_view`) and already has a live-reload path,
  `hq_ui_pdl_reload_if_changed()`, called each `hq_idle_tick()`. **Use
  this key and this path** — do not add a new file or a
  `livedesk_theme.pdl` row.
- **Font sizes that are NOT CSS-driven**: several hardcoded specs
  (`"DejaVu Sans:pixelsize=12"`, `=10`, `=9`, the `font_ui` load, the
  popup fontset, the splash) bypass `font_for()` → they will not scale
  unless each is made scale-aware or switched to `font_for()`.
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

**`#.desktop/hq_ui.pdl`, key `font_scale`** — the key that already
exists and ships `1.25` (see §2). It is the house-wide UI-knobs file
and already has `hq_ui_pdl_reload_if_changed()` wired into
`hq_idle_tick()`. No new file, no `livedesk_theme.pdl` row, no
`apply_theme_op` change.

Value is a **decimal multiplier** (the existing convention: `1.00`,
`1.25`, …). Absent / unparseable → `1.00`. Store internally as an int
percent (`g_ui_scale_pct`, 100 = 1.0) to keep `scaled()` integer-only.

Bounds: the file comment says **0.5–3.0**; for the +/- stepper use a
sane sub-range **0.75–2.0** in steps of **0.25**
(`0.75 1.0 1.25 1.5 1.75 2.0` — six stops, matches the coarse feel of
Opacity ±0.05). A hand-edit of `hq_ui.pdl` can still go to the full
0.5–3.0; clamp only in the stepper handler, not on load.

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

`hq_ui.pdl` already has a reader (`hq_ui_pdl_reload_if_changed()`,
mtime/marker-gated, called every `hq_idle_tick()`) that parses
`click_two_step` etc. **Extend that same parser** to also read
`font_scale` → `g_ui_scale_pct` (round `atof(v) * 100`). Then:

- On the reload path, after the value changes: `assign_nav_and_layout();
  redraw();` **and** the `font_ui` reload (§4.4). The fullscreen-toggle
  handler already models the safe `assign_nav_and_layout(); redraw();`
  pair. Layout metrics changed, not just a colour, so a plain
  `need_redraw` is not enough — a relayout is required.
- Startup: `hq_ui.pdl` is already read once at launch for
  `click_two_step`; `font_scale` rides that same initial read in
  `main()`. For `tp_main()` (tile/entity mode — which has its own event
  loop and does **not** call `hq_idle_tick()`; see
  `khtpm-tp_main-globals-footgun` memory), add a `font_scale` read next
  to `bac09d24`'s `load_theme_colors()` call, and re-read it in
  `tp_main()`'s `theme_changed_dirty` block (or give `tp_main` its own
  cheap `hq_ui.pdl` mtime check).

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
    int s = ui_scale_pct_from_hq_ui_pdl();          /* fresh read of font_scale */
    s += (action[9] == 'P') ? 25 : -25;             /* MINU[S] vs PLU[S] */
    if (s < 75) s = 75;
    if (s > 200) s = 200;
    write_hq_ui_pdl_key("font_scale", s / 100.0);   /* rewrite one row, keep the rest */
    g_ui_scale_pct = s;
    if (font_ui) { XftFontClose(dpy, font_ui); font_ui = load_font_ui(); }
    assign_nav_and_layout(); redraw();
    return;
}
```

`write_hq_ui_pdl_key()` = the rewrite-one-row shape
`write_theme_opacity()` already uses, pointed at `hq_ui.pdl`. Its mtime
change is what makes *other* windows' `hq_ui_pdl_reload_if_changed()`
pick up the new scale and relayout on their next tick — no extra
marker needed (`hq_ui.pdl` reload is mtime-gated, not marker-gated;
acceptable here since the writer touches the file exactly once per
click).

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
| `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` | `g_ui_scale_pct`, real `scaled()`, read `font_scale` in `hq_ui_pdl_reload_if_changed()` + tp_main, `load_font_ui()` + reload, wrap `ROW_H`/`POPUP_ROW_H`(/`DOCK_BAR_H`), `write_hq_ui_pdl_key()`, `UI_SCALE_±` in `dispatch()` |
| `&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm` | `Size -` / `Size +` items |
| `&.widgits/taskbar-settings/ops/taskbar_settings_projector.c` | (optional) publish `${ui_scale}` readout |
| `#.desktop/hq_ui.pdl` | the `font_scale` key already exists (ships `1.25`); the stepper rewrites its value (runtime data, not committed) |

Related: `[[khtpm-tp_main-globals-footgun]]`,
`khtpm-shared-layout-caution` memory, `70a5c1a5` (colour = marker, no
rebuild — same delivery path this uses).
