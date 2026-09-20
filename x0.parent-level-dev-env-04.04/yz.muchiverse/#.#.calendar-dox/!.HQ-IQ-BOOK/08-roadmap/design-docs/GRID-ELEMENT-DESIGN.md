# `<grid>` element design — real in-place spreadsheet cell editing

**Status: IMPLEMENTED 2026-09-05 (csv-hq's `<grid>`; the "DESIGN ONLY"
header this doc used to carry was stale). 2026-09-20: fixed the armed
state being dropped on every full reparse (`kh_find_input_by_key()` never
matched `<grid>`) plus a persistent keyboard-grab retry - see
`04-bugs/bug_bounty.md` "FIXED 2026-09-20: csv-hq `<grid>`". The state
machine below is accurate to the code.** Direct request, 2026-09-05,
after csv-hq's own real grid rendering (formatted `<scrolllist>` rows,
see `csv_hq_manager.c`'s own header) shipped but doesn't allow editing
a cell in place — today's csv-hq requires typing a cell ref into a
separate `<cli_io id="cellref">` + a separate `<cli_io id="cellval">`
+ clicking "Set Cell", which is real and working but not how a human
actually uses a spreadsheet.

Direct instruction, verbatim intent: *"a special kind of interact,
with the grid mode, where entering '^' grid allows traversing using
arrow keys or by index + letter (or letter+index, example: a11 [jumps
to a11] or 11a [will show in debug var, accumulated digits] [jumps to
that part of grid and it allows cli-io like input or backspacing; by
activating that cell '^' till esc is pressed]. this way we dont need
nav for every column or row... we can make a new chtpm element if we
need to."*

## Why a new element, not a bigger `<scrolllist>`

`<scrolllist>`'s existing nav model is one nav-index per row (this
session's own csv-hq uses this for the read-only grid display). Giving
every CELL its own nav index (rows × cols) would work mechanically but
defeats the actual ask - a 500-row × 26-col sheet would need up to
13,000 real nav items, and every `[ ]N.` bracket badge next to a cell
value clutters the exact spreadsheet look the grid rendering fix just
achieved. The real ask is a SINGLE nav item (the whole grid, same as
one `<cli_io>` or one `<text_area>` today) with its OWN internal 2D
cursor and its own key-handling once armed - structurally the same
"armed sub-mode" shape `cli_io`/`text_area` already have, just with a
richer internal state machine than a 1D text cursor.

## State machine

Three real states, all scoped to ONE `<grid>` element (its own nav
index, like any other focusable element):

1. **Unarmed** - grid is just a focusable nav item like any other; `[ ]N.`
   badge, no visible cell cursor. Enter (the existing generic
   `activate_focused()` path) arms it into state 2.
2. **Armed, navigating** (`#` badge on the grid itself - see Decided
   §1 below for why this is a NEW symbol, not `cli_io`/`text_area`'s
   existing `^`) - a real 2D cell cursor
   (`grid_cur_row`/`grid_cur_col`) highlights one cell. Real keys in
   this state:
   - Arrow keys (existing relay codes 200-203, `XK_Up/Down/Left/Right`)
     move the cursor by one cell, clamped to the grid's real
     `rows=`/`cols=` bounds.
   - A-Z / 0-9 keystrokes append to a real **jump buffer**
     (`grid_jump_buffer`, e.g. up to 8 chars) instead of moving the
     cursor immediately - shown live (a debug/status var the app's own
     template can render, e.g. `${grid_jump_buffer}`, NOT baked into
     the grid element's own draw - keeps the element generic). Letters
     and digits can arrive in EITHER order (`a11` or `11a`) per the
     direct instruction - the buffer is parsed as a whole once
     resolved, not position-sensitively.
   - **Enter** resolves the jump buffer: split into its letter-run and
     digit-run (order-independent), map letters → column (A=0, AA=26,
     same base-26 scheme `col_to_letter`/`letter_to_col` in the
     external CSV reference tree already used, per this session's own
     earlier note that shape was worth reusing), digits → row (1-based
     → 0-based internally, same convention `csv_hq_manager.c`'s own
     `parse_cell_ref()` already uses). Cursor jumps there, buffer
     clears. An empty or unparseable buffer on Enter is a no-op (clears
     the buffer, doesn't crash/move).
   - **A second Enter** (buffer already empty, i.e. Enter pressed with
     nothing pending) instead ENTERS the cell at the current cursor -
     transitions to state 3. (Real, deliberate: this is why the jump-
     buffer's own Enter doesn't also enter edit mode - two different
     "Enter does something" cases, disambiguated by whether a buffer
     was pending, not two different keys, matching the direct
     instruction's own single-key spirit.)
   - **Escape** disarms back to state 1 (unarmed), same as
     `cli_io`/`text_area` today.
3. **Armed, editing one cell** (`^` badge - the element's existing
   house-wide meaning, "real text input is live here right now," see
   Decided §1) - the current cell's text becomes
   a real, live-typed buffer (`grid_cell_buffer`, reuses
   `text_area_buffer`'s own real cursor/backspace/typing machinery
   verbatim - this is NOT a new text-input implementation, just
   `cli_io`'s existing single-line armed behavior pointed at whatever
   cell the grid's own 2D cursor is on). Real keys: everything
   `default_cli_io_handle_key()` already handles for a single-line
   field (printable insert at cursor, Backspace, Home/End, Left/Right
   move the TEXT cursor within the cell - NOT the grid cursor while in
   this state, a real, necessary shadowing the implementation must get
   right). **Escape** commits the cell edit (see Commit below) and
   returns to state 2 (armed, navigating) - matches the direct
   instruction ("activating that cell '^' till esc is pressed").

## Commit: how an edited cell reaches the manager

Same real, already-proven pattern this session's own `CSVH_SETCELL`
dispatch handler uses (`khtpm_core_render.c`) - a live element's typed
value isn't something a manager can see until the RENDERER hands it
over on some explicit trigger, since the manager only ever reads its
own `csv_hq_ui.txt` (its last-published state, always one tick stale
relative to a human's live typing):

- On the Escape that ends state 3, dispatch a new generic verb (e.g.
  the app's own `action="CSVH_GRIDCOMMIT"`, mirroring the existing
  `CSVH_SETCELL`) - `khtpm_core_render.c` reads the grid element's own
  live `grid_cur_row`/`grid_cur_col`/`grid_cell_buffer`, dumps the
  buffer to the same kind of scratch file `csv_setcell_buffer.txt`
  already uses, and writes `SETCELL:<computed A1-style ref>` into
  `csv_hq_action.txt` - **reusing the exact existing SETCELL command**,
  not a new manager-side command. The grid element is purely a richer
  INPUT surface; the manager-side contract doesn't need to change at
  all.
- This means `<grid>` could ship as "just" a new interact/input
  primitive in the shared renderer, with ZERO changes needed to
  `csv_hq_manager.c` beyond swapping the template's cell-ref/cell-val/
  Set-Cell trio for one `<grid>` element once it exists.

## New `Elem` fields needed (frame round-trip discipline)

Per this session's OWN repeatedly-hit lesson (cursor/text_area_buffer
both needed this, twice forgotten mid-session): any new per-element
runtime field MUST be added to BOTH `kh_serialize_frame_elem()` and
`kh_paint_frame_line()`, and label-shaped fields need the SAME pipe-
escape just added for `label` (2026-09-05 fix) if they can ever contain
a literal `|` (a jump buffer or cell buffer plausibly could, if a CSV
cell's own content has one):
- `int grid_cur_row, grid_cur_col;`
- `int grid_edit_mode;` (0 = navigating, 1 = editing current cell -
  state 2 vs. state 3 above; state 1 unarmed is just "not the
  `g_default_input_elem`", same as `cli_io`/`text_area` today)
- `char grid_jump_buffer[16];` (short - a ref like "AA1234" is already
  generous for a 26-col/500-row real cap)
- `char grid_cell_buffer[256];` (reuses `MAX_CELL_LEN`-shaped sizing
  from `csv_hq_manager.c` - the same real cap a cell value already has)

## New tag: `<grid>`

```xml
<grid id="sheet" rows="${n_rows}" cols="${n_cols}"
      cell_prefix="cell_" commit_action="CSVH_GRIDCOMMIT"/>
```
- `rows=`/`cols=` - real bounds, `${var}`-resolvable like `<repeat
  count=>` already is.
- `cell_prefix=` - the manager publishes each cell as its own var,
  `<prefix><row>_<col>` (e.g. `cell_0_0`, `cell_0_1`, ...) - a real
  departure from csv-hq's current `row_N_text` (one joined string per
  row) since the grid element needs to know REAL cell boundaries
  itself to draw a bordered table and to know what to show while
  editing one cell - `csv_hq_manager.c`'s `write_ui_file()` would need
  a real rewrite to publish per-cell vars instead of joined row
  strings (a bigger publish - up to `rows×cols` vars - real, bounded
  by the same display cap `DISPLAY_MIN_ROWS`/`DISPLAY_MIN_COLS`
  already uses).
- `commit_action=` - which dispatch verb fires on cell-edit-commit
  (Escape from state 3) - generic, so `<grid>` itself has zero
  csv-hq-specific knowledge, same "the app's own action= wiring, not a
  new per-project dispatch branch" rule every other element already
  follows.

## Drawing (`khtpm_draw_core.c`, new `draw_elem()` branch)

A real bordered table: `rows+1` (header) × `cols+1` (row-number
column) grid of fixed-width cells (same fixed-width-with-`|`-separator
LOOK the current formatted-scrolllist fix already achieves, but real
cell boundaries instead of one big label string) - column-letter
header row, row-number left column, current cursor cell gets a real
highlight (background swap plus the `#`-navigating/`^`-editing badge
per Decided §1), and
while in state 3 the current cell shows its OWN live text-cursor bar
(reusing `text_area`'s existing cursor-bar draw code against
`grid_cell_buffer` instead of `text_area_buffer`).

## Decided (direct answers, 2026-09-05)

1. **Two distinct badges, one per armed sub-state** (direct: "we may
   need a secondary symbol, how about '^' 1st # 2nd (viceversa?)" -
   resolved here rather than left ambiguous): **`#` = state 2 (armed,
   navigating the grid - arrow keys / jump buffer, no text input yet)**;
   **`^` = state 3 (armed, editing one cell - real text input)**. `^`
   keeps its existing house-wide meaning exactly ("a real text buffer
   is live here right now," same as `cli_io`/`text_area` today) instead
   of gaining a second, different meaning for this one element; `#` is
   the genuinely NEW concept (2D positional navigation, not text entry)
   and gets the new symbol. State 1 (unarmed) shows neither, same as
   every other element.
2. **`<grid>` supports real multi-letter (AA/AB/...) column parsing
   generically** - the base-26 `col_to_letter`/`letter_to_col` scheme
   (A=0..Z=25, AA=26..AZ=51, ...) is the element's own real jump-buffer
   parser, independent of any one consumer's column cap. csv-hq's own
   manager can still cap at 26 real columns for its v1 data model
   without that limiting what the generic element itself can address.
3. **`<grid>` replaces ONLY csv-hq's cellref/cellval/Set-Cell trio.**
   The separate `colref` `<cli_io>` field stays for the SUM/AVG/MIN/
   MAX/COUNT function buttons (those need a column selection
   independent of wherever the grid's own cursor happens to be sitting,
   not a cell-edit concern).

## Open questions still real, not yet decided - confirm before building

1. **Scroll behavior for a grid larger than the visible area** - does
   `<grid>` get its own real scroll region (like `<scrolllist>`'s
   existing `layout_scroll_region()`), following the cursor
   (auto-scroll when the cursor moves off-screen)? Real and needed for
   csv-hq's actual 500-row cap, not a hypothetical.
2. **`<grid>` belongs in `khtpm_render_core.c`/`khtpm_draw_core.c`**
   (the shared, text-included core files copied into every consuming
   binary), same as `text_area` - near-certain per this house's "zero
   new per-project C" rule, flagged only so file placement isn't
   guessed at implementation time.

## Suggested build order once confirmed

1. Answer the Open Questions above.
2. Add the four new `Elem` fields + frame round-trip (serialize +
   parse + pipe-escape for the two buffer fields) - do this FIRST and
   verify a trivial round-trip before any drawing/key-handling code,
   matching this session's own repeated lesson about this exact class
   of bug.
3. Key handling: extend `activate_focused()` (arm), add a new
   `default_grid_handle_key()` (arrow/jump-buffer/Enter/Escape state
   machine for state 2, delegate to a thin wrapper around
   `default_cli_io_handle_key()`'s own logic for state 3 against
   `grid_cell_buffer`).
4. Drawing: new `draw_elem()` branch in `khtpm_draw_core.c`.
5. `csv_hq_manager.c`: rewrite `write_ui_file()` to publish per-cell
   `cell_R_C` vars instead of joined `row_N_text`; add the
   `CSVH_GRIDCOMMIT` dispatch handler (mirrors `CSVH_SETCELL` almost
   exactly).
6. `csv-hq-pal.xhtpm`: swap the panel's `<scrolllist>` of formatted
   rows for one `<grid>`; decide per Open Question 4 whether
   cellref/cellval/Set-Cell get removed or kept.
7. Verify via the relay (`#.desktop/entity_menu_history/<pid>.txt`,
   per khtpm-house-standards skill's own new section) - arrow-key
   cursor movement, a jump-buffer resolve (both `a11` and `11a` order),
   a real cell edit commit, Escape-out-of-cell, Escape-out-of-grid, all
   independently confirmed via a text-state read (the published
   `cell_R_C` vars) before trusting a PNG dump alone.

## Reuse by overlay pickers (2026-09-20)

The pure part of the jump behaviour now lives in
`44.xyz.01.00/&.widgits/_shared-lib/khtpm_grid_jump.c` (text-included canonical
helper; plain C + `<string.h>/<ctype.h>/<stdlib.h>`, no Elem/X11/khtpm
dependency; standalone test: `_shared-lib/tests/test_grid_jump.c`).
`default_grid_handle_key()` (khtpm_core_render.c) already drives its
navigating state through it, so behaviour is defined in exactly one place.

API: `GjState {jump[16], row, col, rows, cols}` (rows/cols 0 = unbounded),
`gj_step(&st, key, ch)` with `GjKey` = CHAR/UP/DOWN/LEFT/RIGHT/ENTER/ESC/
BACKSPACE returning `GjAction` = NONE / MOVED / BUFFER / JUMPED / ENTER_CELL /
DISARM; plus `gj_parse()` (a11 / 11a / AA5 -> 0-based row,col, bounds-checked),
`gj_col_to_letters()` / `gj_letters_to_col()` (bijective base-26, A=0, AA=26),
`gj_buf_append()` (only `[A-Za-z0-9]`, capped) and `gj_clamp()`.

How the Place overlay (`tp_arm_placer_rmmv.c`, a standalone X process with a
labelled wire grid) uses it - WIRED 2026-09-20, see "IMPLEMENTED in the Place overlay" below (this list was the plan):
1. `#include "khtpm_grid_jump.c"` (add `-I ../../_shared-lib` to its build
   line, same as build_core_render.sh does) and keep one `GjState` with
   `rows`/`cols` = the number of grid cells on screen (pane size / cell px).
2. Draw column letters (`gj_col_to_letters`) along the top and 1-based row
   numbers down the side of the wire grid, and a highlight box on
   `(st.col, st.row)`; show `st.jump` in a small status label so the user sees
   what they typed.
3. In the existing keyboard loop map `XK_Up/Down/Left/Right`, `XK_Return`,
   `XK_Escape`, `XK_BackSpace` and printable characters to `GjKey`, call
   `gj_step`, and react: `MOVED/BUFFER/JUMPED` -> redraw the highlight;
   `ENTER_CELL` (Enter with an empty buffer = the second Enter) -> place at the
   cell centre, i.e. the same result a click there produces (write the same
   `RMMV_CLICK` ledger line with that pixel); `DISARM` -> cancel exactly like
   today's Esc.
4. Type `c7` (or `7c`) + Enter to jump, Enter again to place. Bounds come from
   `rows/cols`, so an off-grid ref is a no-op instead of a stray placement.
   Keep the existing click-to-place, Esc handling (`XQueryKeymap` poll) and
   drop-zone hover untouched.

### IMPLEMENTED in the Place overlay (2026-09-20) - `tp_arm_placer_rmmv.c`

The palettes RPG-Maker placer and File Explorer's right-click Place (same binary) now draw a **labelled grid** and take the same keyboard behaviour as csv-hq via `gj_step()`:

- Columns lettered A, B, ... along the top, rows numbered 1, 2, ... down the left, on a dark band so they read over any desktop. Labels and lines use the **real desk cell** (`desk_grid.pdl cell_px`, 80 reference px, scaled per screen by `khtpm_ui_scale.c`; edges = `kps_ref_to_screen(k*cell)`), so a labelled cell is exactly where placement snaps. (Before, the overlay drew fixed 64px screen cells.) Cells cut by the screen edge are labelled on their visible part.
- Type a ref in either order (`c7` / `7c`), shown as `jump: c7_` in a status pill (moved off the picker hole if needed). **Enter** jumps a bright green frame to the cell, arrows move it one cell, **Enter again** (empty buffer) places at the cell centre, Backspace edits, **Esc** cancels, a mouse click places as before. Moving the mouse hands control back to the mouse.
- The **first key of any kind only arms** the highlight (at the pointer's cell), and keys already held at launch (the Enter that opened Place) are ignored until released, so launching Place can never place by itself.
- A cell whose centre is under the picker hole, or a ref that doesn't exist (`z99`), is **refused with a red cue** in the pill and places nothing. After a refused ref the cursor stays where it was, so Enter would place *that* cell.
- Placement writes the same `RMMV_CLICK` ledger line / `FE_PLACE_CLICK` file / drop-zone result a mouse click at that cell centre produces (reference px). The green drop-zone hover follows the keyboard cursor too.
- Keys arrive as `KeyPress` under the keyboard grab and as `XQueryKeymap` edges when another client holds it; a per-keycode "already acted on" table stops a press being handled twice, and autorepeat is filtered (held arrows/Backspace still repeat).
- Env: `TP_PLACE_CELL_REF` (cell override), `TP_PLACE_NO_ARGB=1` (plain visual), `TP_PLACE_DEBUG=1` (stderr key trace).
- **Differs from csv-hq:** no cell *editing* state (`^`) - placing replaces it; rows/cols are bounded by the screen; the jump helper is the only shared code (a documented TRANSITIONAL text include, `INMEM-DB-STATE-LAYER-PLAN.md` section 5).
- **Not yet done:** the non-rmmv `tp_arm_placer.c` (emoji brush, board-viewer branch) is a different overlay and was left unchanged; File Explorer's script still snaps the click to 64 ref px after placement (`fe_place_on_desk.sh`), which is not the 80px labelled cell.
- **Real hardware:** the private-Xephyr tests inject keys through the X server (xdotool/XTest), which bypasses the Wayland/Mutter key routing and any stale grab (HOUSE_CODE_PITFALLS #24). Only the user's session can prove typed keys arrive there.

