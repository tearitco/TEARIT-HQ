# `<text_area>` subsystem: scroll, line-number gutter, text selection

**Status: DESIGN ONLY, not implemented.** Direct instruction,
2026-09-05, after building text-edit-hq: "we will have side right
scroll for bigger pages", "the line numbers dont add up to the actual
line", "are we working on text highlighting yet?" (no), "yes write the
design doc pls".

These three features share ONE missing capability - the `<text_area>`
render/edit code has no concept of **where its content actually sits**
beyond "start at the top, draw until the box is full." Building them
piecemeal means three half-versions that fight each other; this doc
covers the lot as one subsystem.

## What exists today (the starting point)

- `Elem.text_area_buffer[4096]` - the real text, real `\n` preserved.
- `Elem.cursor` - a byte offset into it.
- `default_cli_io_handle_key()` - Left/Right/Home/End/Up/Down (logical
  line), Backspace/Delete, printable insert, Enter = insert `\n`.
  Ctrl+C/V/X (whole-buffer clipboard, v1).
- `draw_elem()`'s text_area branch - splits on real `\n`, word-wraps
  each logical line to the box width, draws top-down until `max_lines`
  (`e->h / line_h`) is reached. **Everything past that is simply not
  drawn** - no scroll, no indication there's more.
- text-edit-hq's line-number "gutter" is a static `<repeat>` of
  `<text>` in the sidebar (1..N) - it does NOT know the text_area's
  word-wrap, so a wrapped line throws every number below it off by
  one. A real, acknowledged placeholder.

## The one new concept: a scroll offset + a real "visual line" model

Add to `Elem` (frame-round-trip'd, same discipline as cursor/
text_area_buffer - serialize + parse + the label pipe-escape is not
needed for a plain int):

- `int text_scroll;` - index of the first VISUAL (post-word-wrap) row
  currently shown at the top of the box. 0 = top.

And a real, shared helper the draw code and the key code both call:

```
int kh_text_area_visual_lines(const Elem *e, int box_w,
                              VisLine *out, int max_out);
```

where `VisLine { int start_off; int end_off; int logical_line; }` -
one entry per on-screen row the content would produce at `box_w`,
walking the exact same "split on \n, then greedy width-wrap" the draw
branch already does (factor that loop out of `draw_elem()` into this
helper so there is ONE wrap implementation, not two). This is the
single source of truth all three features read.

## Feature 1: right-side scrollbar + real scrolling

- `draw_elem()`'s text_area branch: instead of drawing visual rows
  `0 .. max_lines`, draw `text_scroll .. text_scroll + max_lines`.
- A real scrollbar in the right ~8px of the box when
  `total_visual_lines > max_lines`: a track + a thumb sized/positioned
  from `text_scroll / total_visual_lines`. Reuse the existing generic
  scrollbar draw (`g_pal_track_*` / the `<scrolllist>` scrollbar in
  this same file) rather than a second implementation - it already
  handles thumb geometry and the tiny-content edge case.
- Mouse wheel over the box (`MOUSE_EVENT: <btn> ... ` buttons 4/5,
  already parsed in `poll_agent_history()` for scrolllists) adjusts
  `text_scroll`. Page_Up/Page_Down while armed = `± max_lines`.
- **Auto-scroll-to-cursor**: after any cursor move / insert / delete /
  paste, if the cursor's own visual row is `< text_scroll` or
  `>= text_scroll + max_lines`, snap `text_scroll` so it's visible
  (one line of margin). This is what makes typing at the bottom of a
  long doc actually work - non-negotiable, do it in the same key
  handler right after the cursor changes.

## Feature 2: accurate line-number gutter (renderer-drawn)

- Kill the static sidebar `<repeat>` gutter. A gutter this precise
  can only be drawn where the real visual-row Y positions are known -
  inside `draw_elem()`'s text_area branch.
- New opt-in: `<text_area class="numbered">` (or a `gutter="1"`
  attribute). When set, reserve a left strip (~46px, scale-aware) and,
  for each drawn visual row, if it's the FIRST visual row of its
  logical line, draw that logical line's 1-based number in the strip,
  right-aligned; continuation (wrapped) rows get a blank gutter cell.
  This is exactly how real editors (VS Code, vim `set number`) show
  wrapped lines - one number per logical line, aligned to its first
  screen row.
- The gutter scrolls with the content for free (it's drawn per
  visual row, from the same `text_scroll` window).
- text-edit-hq then drops its sidebar entirely. **Open question**:
  `layout_sidebar_panel()` needs BOTH a `<sidebar>` and a `<panel>`.
  Either (a) give text-edit-hq a real (now genuinely useful) sidebar
  back - a file-info / outline panel - or (b) add a real panel-only
  layout path (`<panel>` with no `<sidebar>` lays out full-width).
  (b) is the cleaner generic fix and probably wanted regardless;
  decide before starting.

## Feature 3: text selection / highlighting (the clipboard doc's v2)

Add to `Elem`:

- `int sel_anchor;` - byte offset of the other end of the selection
  (`cursor` is the moving end). `sel_anchor == cursor` (or
  `sel_anchor < 0`) means "no selection".

Key handling (`default_cli_io_handle_key()`):

- **Shift+Left/Right/Up/Down/Home/End**: if no selection, set
  `sel_anchor = cursor` first; then move `cursor` as the unshifted
  key already does. The selection is always `[min(anchor,cursor),
  max(anchor,cursor))`.
- **Any unshifted cursor move**: clear the selection (`sel_anchor =
  cursor`), same as every real editor.
- **A printable key / Enter / paste WITH an active selection**:
  delete the selected range first (one `memmove`, cursor → range
  start), THEN insert - "typing replaces the selection".
- **Backspace/Delete WITH an active selection**: delete the range,
  no extra char.
- **Ctrl+C / Ctrl+X WITH an active selection**: copy/cut just the
  selected substring (this is what "copy out" needs - `kh_clipboard_
  copy()` already takes a `const char *`, so pass the substring). No
  selection → fall back to today's whole-buffer behavior (keep it -
  it's a genuinely useful "copy the whole doc" shortcut).
- Shift is a real modifier: `XLookupString` reports it in
  `ev->xkey.state & ShiftMask`. The relay has no shift concept today -
  add codes for shifted arrows (e.g. 210-215, in the same reserved
  band as the 200-205 plain arrows) so headless tests can drive
  selection.

Drawing (`draw_elem()` text_area branch):

- For each drawn visual row, if any of `[sel_lo, sel_hi)` intersects
  `[row.start_off, row.end_off]`, `XFillRectangle` a highlight (a
  muted blue, `#2f5f8f` at low alpha-feel, or just a solid mid-tone)
  behind that row's text span BEFORE drawing the glyphs. Multi-row
  selections fill full-width for the middle rows, partial for the
  first/last. Same per-visual-row loop the gutter and scroll use.
- Mouse: `MOUSE_EVENT` press inside the box sets `cursor` + `sel_
  anchor` to the hit offset (hit-test = which visual row by Y, which
  byte by X via `XftTextExtentsUtf8` growth, same technique the
  single-line cli_io cursor bar already uses); drag (`MOUSE_EVENT`
  with `is_press=0` and a button still down - needs the loop to
  track button state, small addition) extends `cursor`. Mouse
  selection is real but secondary to keyboard for this house's
  relay-driven testing - keyboard shift-select is the primary target
  and the real proof.

## Build order

1. Factor the wrap loop out of `draw_elem()` into
   `kh_text_area_visual_lines()` - pure refactor, verify text-edit-hq
   renders byte-identical before/after.
2. `Elem.text_scroll` + frame round-trip. Draw the scroll WINDOW
   (`text_scroll .. +max_lines`) instead of `0 .. max_lines`.
   Auto-scroll-to-cursor in the key handler. Wheel + PageUp/Down.
   Right scrollbar draw (reuse the scrolllist one).
3. Renderer-drawn `class="numbered"` gutter. Decide + do the
   panel-only layout path (open question above). text-edit-hq drops
   the sidebar `<repeat>` gutter, gains `class="numbered"`.
4. `Elem.sel_anchor` + frame round-trip. Shift+move sets/extends
   selection; unshifted move clears it. Highlight draw per visual
   row. Typing/Backspace/paste replace-selection semantics.
5. Ctrl+C/Ctrl+X copy the selection when there is one (fall back to
   whole-buffer otherwise). Relay codes for shifted arrows.
6. Mouse click-to-place-cursor and drag-to-select (secondary).
7. `<grid>`'s own cell-edit buffer (`grid_cell_buffer`) is a single-
   line reuse of the same selection primitives once they exist - no
   new code, just wire the same `ch==3/22` checks in
   `default_grid_handle_key()` state 1.

## Explicitly NOT in this pass

- Rich text / styling / syntax highlighting (colour-per-token). This
  doc is plain-text selection + a monochrome highlight only.
- Undo/redo. Real, wanted, its own separate design (a ring buffer of
  buffer+cursor snapshots on each edit boundary) - not bundled here.
- Multi-cursor. No.
- Bidi / complex-script shaping. The whole text_area path is ASCII-
  offset-based today (documented in cursor's own field comment);
  keeping that scope.
