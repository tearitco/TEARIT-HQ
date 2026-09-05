# `<grid>` element design — real in-place spreadsheet cell editing

**Status: DESIGN ONLY, not implemented.** Direct request, 2026-09-05,
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
