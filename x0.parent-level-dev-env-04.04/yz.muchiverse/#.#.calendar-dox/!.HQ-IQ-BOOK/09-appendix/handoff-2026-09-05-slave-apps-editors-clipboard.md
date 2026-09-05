# HANDOFF (slave) — 2026-09-05: toys/apps, `<grid>`, clipboard, text selection

Slave doc hanging off `handoff-2026-09-04-master.md`. Emergency-resume
snapshot of everything built in the 2026-09-05 session.

**Branch:** `chtpm-delete-per-app-c`
**All work below is COMMITTED AND PUSHED to `main`** as of commit
`143364ea` ("text selection + partial copy-out"). `git log
origin/main..HEAD` was empty at handoff time - nothing local-only.
Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN

Runtime-state files under each app dir (`*_ui.txt`, `*_action.txt`,
`module_parent.pid`, `text_area_editor.txt`, `csv_setcell_buffer.txt`,
etc.) are generated and intentionally NOT tracked - a dirty `git
status` full of those is normal.

---

## 1. New real apps / widgets (all house-standard: real manager + static template)

| thing | dir | what | toy.pdl |
|---|---|---|---|
| **File Explorer** | `44.xyz.01.00/&.widgits/file-explorer/` | shared, standalone X11 widget window - browse any dir, back/deeper nav, pick a file. `file_explorer_manager.+x` publishes `file_explorer_ui.txt`; consumers poll its `result=` / `result_action=` (no direct IPC). LOAD mode only (SAVE mode is a real deferred v2). | yes ("File Explorer") |
| **pdl-read** | `44.xyz.01.00/@.apps/pdl-read/` | paginated document reader. Live-scans `<house_root>/#.DOX` for its doc list; "file" button opens File Explorer to read any arbitrary file ad-hoc. Prev/Next below content, PDF-style page-jump sidebar. | yes |
| **text-edit-hq** | `44.xyz.01.00/@.apps/text-edit-hq/` | plain-text editor. Tabbar New/Open/Save/Save As. Open + Save As both launch File Explorer (Save As arms `SAVEAS_ARM`, next pick = save target). Editor = a `<text_area>`, single nav entry. Left sidebar is a **static** line-number gutter (`<repeat count="${n_lines}">`) - NOT wrap-synced (see §4). | yes ("text-edit-hq") |
| **csv-hq** | `44.xyz.01.00/@.apps/csv-hq/` | CSV/spreadsheet editor. Real `<grid>` element (see §2) for in-place cell editing, `CSVH_GRIDCOMMIT` reuses the existing `SETCELL` manager command. `colref` `<cli_io>` + SUM/AVG/MIN/MAX/COUNT function buttons. 26 cols (A-Z), plain-comma CSV (no quoted commas), 500-row cap. | yes |

**Toys menu**: `khtpm_taskbar_manager.c`'s `livedesk_build_toys_menu()`
now scans **three** roots (`house_root`, `@.apps/`, and NEW: `&.widgits/`)
for `toy.pdl`. Live scan on menu open - no rebuild needed to pick up a
new `toy.pdl`, but the C scan-root change DID need a taskbar rebuild +
restart (done).

**File Explorer `button.sh`** was rewritten to the real toy.pdl launch
convention (`sh button.sh run`, derives `house_root` itself). Any call
site launching it must use `... button.sh run`, NOT pass house_root as
argv[1] - `PDL_OPENFILE` had that exact regression and it's fixed.

---

## 2. `<grid>` element — real, in `khtpm_render_core.c` / `khtpm_draw_core.c`

Design: `08-roadmap/design-docs/GRID-ELEMENT-DESIGN.md` (has a
"Decided" section - read it, the badge symbols etc. are settled).

- **One nav item for the whole grid** (not one per cell). Arm it
  (Enter) → state 0 "navigating" (`#` badge): arrow keys move
  `grid_cur_row`/`grid_cur_col`; letters+digits accumulate into
  `grid_jump_buffer` (either order, `a11` or `11a`), Enter resolves the
  jump; a second Enter (nothing pending) enters the cell → state 1
  "editing" (`^` badge): `grid_cell_buffer` is a live single-line
  buffer reusing `default_cli_io_handle_key`'s own primitives; Escape
  commits (fires the grid's `action=` via `dispatch()`) → back to
  state 0; a second Escape disarms.
- New `Elem` fields: `grid_cur_row`, `grid_cur_col`, `grid_edit_mode`,
  `grid_jump_buffer[16]`, `grid_cell_buffer[256]` - all frame-round-
  tripped (`kh_serialize_frame_elem` / `kh_paint_frame_line`, tail
  fields 12-16, buffers pipe-escaped).
- `default_grid_handle_key()` (khtpm_core_render.c); draw branch in
  `draw_elem()` (khtpm_draw_core.c) - real bordered table, base-26
  column letters, row numbers, cursor highlight, a status row with the
  badge + live jump-buffer echo (+ a trailing `_` cursor glyph).
- On entering edit mode the cell buffer is **seeded** from a per-cell
  var named `<target_id><row>_<col>` (target_id defaults to `"cell_"`)
  - csv_hq_manager.c publishes `cell_R_C=...` lines; `draw_elem` reads
    the same vars to show every non-edited cell's value.
- Grid layout height MUST match the draw's own `STATUS_H_PX +
  (rows+1)*CELL_H_PX` exactly (two hard-coded copies of 18/22, kept in
  sync deliberately) or the nav-focus box floats past the real table.

**Known real-vs-relay input gap (grid, still open):** relay-driven
arrow/jump input works (verified via `#.desktop/entity_menu_history/
<pid>.txt`), a live report said physical keyboard arrows/typing didn't
register while `#`-armed. Not root-caused. Underlying state machine is
sound.

---

## 3. Clipboard + text selection — `khtpm_core_render.c` + `khtpm_draw_core.c`

Design docs:
`08-roadmap/design-docs/CLIPBOARD-COPY-PASTE-DESIGN.md`,
`08-roadmap/design-docs/TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md`
(the latter is the roadmap for what's LEFT - see §4).

**Working now:**
- Real X11 `CLIPBOARD` (+ `PRIMARY`) selection ownership -
  `kh_clipboard_copy()` / `kh_clipboard_request_paste()` +
  `SelectionRequest` / `SelectionClear` / `SelectionNotify` branches in
  `hq_dispatch_xevent()`. Interops with real apps (browser/terminal),
  not just other khtpm windows. **Confirmed live: paste-IN from a
  browser works.**
- `Ctrl+C` / `Ctrl+X` / `Ctrl+V` = ASCII 3 / 24 / 22 through
  `default_cli_io_handle_key()`. Relay: bare codes 3/22/24.
- **Text selection**: `Elem.sel_anchor` (frame tail field 17, plain
  int; `sel_anchor == cursor` = no selection). `g_key_shift` set from
  `ev->xkey.state & ShiftMask` at the KeyPress site and per-code in
  `dispatch_relay_code` (NEW relay codes **220-225** = Shift+Left/
  Right/Up/Down/Home/End). Shift+move keeps the anchor; unshifted move
  collapses. Typing / Enter / paste / Backspace / Delete over a
  selection delete it first. **`Ctrl+C`/`Ctrl+X` copy just the
  selected substring** when one exists, else the whole buffer.
- Selection highlight: a `#2f5f8f` band per visual row in
  `draw_elem()`'s `text_area` branch (text_area only for now - cli_io
  single-line highlight not drawn yet, but the logic works there too).
- "**copied**" tag top-right in the chrome strip for ~2s after any
  copy/cut (`g_clip_copied_at`, aged out by `hq_idle_tick()`).

**Not verified end-to-end** (relay tests kept colliding with a live
interactive session on the shared display): a clean "select → Ctrl+C →
paste into a terminal shows exactly the span". The `Ctrl+C` substring
path is small and reviewed; user has physical-keyboard access to
confirm.

---

## 4. What's LEFT (the text_area subsystem — one design, 7 steps)

`TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md` - steps 4/5 (selection +
partial copy) are DONE per §3. Remaining, in order:

1. **Refactor the word-wrap loop out of `draw_elem()`'s text_area
   branch into `kh_text_area_visual_lines()`** - a shared "visual
   line" model (start_off / end_off / logical_line per on-screen row).
   Everything below reads it. Pure refactor - verify byte-identical
   render first.
2. **`Elem.text_scroll`** + frame round-trip. Draw the window
   `text_scroll .. +max_lines`. Auto-scroll-to-cursor after every
   edit/move. Mouse wheel + PageUp/Down. Right scrollbar (reuse the
   `<scrolllist>` scrollbar draw).
3. **Renderer-drawn `class="numbered"` gutter** - accurate, scrolls
   with content, one number per LOGICAL line aligned to its first
   visual row. Kills text-edit-hq's static sidebar gutter. OPEN
   QUESTION: `layout_sidebar_panel()` needs both `<sidebar>` and
   `<panel>` - either give text-edit-hq a real sidebar back, or add a
   panel-only layout path (the cleaner fix).
6. **Mouse click-to-place-cursor + drag-to-select.** REAL WRINKLE:
   this Mutter/XWayland setup does NOT deliver real hardware
   `ButtonPress` to override-redirect windows (see
   `1.^V-hq/_.0.aigent-testing-k9.txt` SCOPE ADDENDUM 2026-08-12/-29) -
   must use `XQueryPointer` polling (track button state + pointer each
   idle tick), not a plain `MotionNotify` handler. Hit-test walks the
   §4.1 visual-line model.
7. `<grid>`'s `grid_cell_buffer` gets `Ctrl+C/V` for free once the
   selection primitives are wired into `default_grid_handle_key` state 1.

Explicitly NOT planned: rich text / syntax colour, undo/redo (own
design), multi-cursor, bidi.

---

## 5. Gotchas for whoever resumes

- **Frame round-trip discipline**: any new per-element runtime field
  MUST be added to BOTH `kh_serialize_frame_elem()` (write) and
  `kh_paint_frame_line()` (parse), or it never reaches `draw_elem()`.
  Label-shaped fields that can contain a literal `|` need
  `frame_field_escape_pipe`. This session hit/fixed the `label` pipe
  bug (csv-hq grid rows) - `label` IS escaped now.
- **`${bind.##}`** = NEW 1-based repeat index (line numbers, "Page N"),
  alongside the existing 0-based `${bind.#}`.
- **`<tab>` scope-confine**: a `<tab>` only confines nav into a
  container when it has an explicit `target_id=` (db-hq/events-hq
  content tabs). A plain menu-bar action `<tab>` (no target_id) must
  NOT trap nav - fixed this session (it was trapping every app whose
  main content is in `<panel>` not `<sidebar>`).
- **Nav-focus box**: inset (inside bounds) for big elements (w>120 or
  h>60), outset halo for small ones; top-padded one badge-row for a
  badged tall element so it sits below the `[^]N.` line.
- **Testing on this shared desktop**: drive khtpm_core_render.c
  windows via `#.desktop/entity_menu_history/<pid>.txt` (bare-decimal
  or `KEY_PRESSED:`/`MOUSE_EVENT:` lines), NOT xdotool - see the
  khtpm-house-standards skill's own new section and
  `1.^V-hq/_.0.aigent-testing-k9.txt`. Relay tests on this machine
  frequently collide with a live interactive session - a clean run
  often needs killing every stray instance first (`ps aux | grep
  <binary>`), and the frame file is `entity_menu_frame_<RENDER-pid>.txt`
  (render pid, not manager/shell pid).
- **Shared render code changes** (`&.widgits/_shared-lib/khtpm_render_
  core.c` / `khtpm_draw_core.c` / `*.monads/*.livedesk-taskbar/ops/
  khtpm_core_render.c`) need `sh build_core_render.sh` AND `sh
  build_khtpm_strip.sh`, then a taskbar restart (`sh run_khtpm_strip.sh
  new`) for the live desktop to pick them up. Running app windows need
  relaunch. Only `&.widgits/_shared-lib/` copies are committed; `ops/`
  copies are build-generated.

---

## 6. Also this session (context, mostly self-contained)

- A `.7z` of the whole house (`x0.parent-level-dev-env-04.04`, images
  excluded) at `/home/no/Desktop/x0.parent-level-dev-env-04.04.7z`
  (~50 MiB). Rebuild: `7z a -mx=5 -xr'!*.png' -xr'!*.jpg' ...`.
- `make_presentation_video.py` (in the crswrd media-archive) is the
  house's PNG-dump + TTS → MP4 proof-video tool - its own docstring
  is the spec, nothing was lost. Presentation gate: MEANINGFUL
  deliverable only, and ALWAYS ask first (k9 doc's own rule).
- CSV editor pointer for later: an existing external CSV ops tree at
  `/media/no/.../♍️]csv.irgo]🇬🇮️📠️]a0/` (2D-grid + "function bank"
  shape) - noted in `12.calendar/2026-09-05/2do.md`.
- New generic `<grid>`-adjacent idea logged, not built: `FLEXFOLDER`/
  `FLEXBOX` desktop container entity.
