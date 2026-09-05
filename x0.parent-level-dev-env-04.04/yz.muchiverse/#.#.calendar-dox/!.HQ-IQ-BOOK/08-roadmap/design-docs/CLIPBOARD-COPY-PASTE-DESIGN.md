# Clipboard copy/paste (window ↔ window, window ↔ OS) — design

**Status: DESIGN ONLY, not implemented.** Direct request, 2026-09-05:
"one thing we are missing from this, is the ability to copy paste from
window and to window... we can test in the text editor... we should
document a plan first."

## What "copy paste from window and to window" actually needs to mean

Two real, separate capabilities, worth naming explicitly since they
have different real difficulty:

1. **khtpm window → khtpm window** (e.g. copy a cell from csv-hq,
   paste into text-edit-hq). The easy direction if solved generically.
2. **khtpm window ↔ any other real app** (a browser, a terminal, a
   real text editor). This is what most people mean by "copy paste" -
   and it's NOT extra work over (1) if done right, because (1) falls
   out for free once a window talks to the REAL X11 clipboard instead
   of a house-private channel. Building a house-private clipboard file
   first and bolting on real OS interop later would mean solving the
   hard part (X11 selection ownership) twice - so this doc goes
   straight for the real X11 clipboard, not a shortcut.

## The real mechanism: X11 `CLIPBOARD` selection

X11 has no actual "clipboard buffer" owned by the server - copy/paste
is a live protocol between two client processes, brokered by the X
server:

- **Copying**: the app claims ownership of the `CLIPBOARD` atom via
  `XSetSelectionOwner(dpy, CLIPBOARD, win, time)`. It does NOT hand the
  text to the server at that moment - it just says "ask me for it
  later." The app must then handle a real `SelectionRequest` event
  whenever ANY other app (including another khtpm window) asks for the
  clipboard content, replying with the text via
  `XChangeProperty`+`SendEvent(SelectionNotify)`. If the app quits or
  another app claims `CLIPBOARD` first, a `SelectionClear` event tells
  it ownership was lost (nothing further to do - the new owner now
  answers requests).
- **Pasting**: the app calls `XConvertSelection(dpy, CLIPBOARD,
  UTF8_STRING, <a property atom on its own window>, win, time)` - this
  is a REQUEST, answered asynchronously by whichever app currently
  owns `CLIPBOARD` (a real event-loop round trip, not a synchronous
  call.) The requesting app then waits for a `SelectionNotify` event on
  its own window and reads the property that was filled in.

This is real, standard, and exactly how every other X11 app (a
terminal, Firefox, GIMP) already does it - a khtpm window implementing
this genuinely interoperates with the whole desktop, not just other
khtpm windows. **Confirmed this session: `xclip`/`xsel` are NOT
installed in this environment** (checked directly, `command not
found` for both) - so shelling out to an external tool is not a free
shortcut here; it would need an explicit install step, and would still
only be a testing/prototyping convenience since the real house-native
version needs this Xlib protocol implemented directly regardless (the
same "own the real protocol, don't hand-wave with an external tool"
instinct this house already applies elsewhere - e.g. this session's
own File Explorer built as a real widget rather than shelling to a
system file picker).

## Real scope decision: whole-buffer vs. a real text SELECTION

This is the one decision that most changes how much work this is, and
should be confirmed before starting:

- **v1, recommended: whole-buffer copy, paste-at-cursor.** "Copy"
  copies the ENTIRE current `cli_io`/`text_area`/grid-cell buffer to
  the X11 clipboard; "Paste" inserts the clipboard's text at the
  current cursor position (reusing `kh_text_insert_at_cursor()`,
  already real and already handles multi-character insertion one byte
  at a time - a paste is just calling it in a loop over the clipboard
  string). No new UI, no new visual concept, and it's genuinely useful
  immediately (copy a whole document out of text-edit-hq, paste a
  whole clipboard's worth of text in) - real, honest v1 scope, same
  spirit as every other "buildable now, not the maximal version"
  decision this session made (grid's own single-letter-then-multi-
  letter column decision, text_area's own "no visual-row Up/Down"
  scope note, etc).
- **v2, real, deliberately deferred: a real text SELECTION** (shift+
  arrow highlighting a range, like every normal text editor). This
  needs genuinely new state (a selection anchor + the existing cursor
  as the other end, a real highlighted-range draw in `draw_elem()`'s
  text_area/cli_io branches, Delete/Backspace/typed-char-replaces-
  selection semantics) - a real, separate feature on the same rough
  scale as `<text_area>` itself was, not a small add-on to copy/paste.
  Partial-copy ("copy just this one word") is not possible without it;
  don't promise it in v1.

## Key bindings

`Ctrl+C` / `Ctrl+V` / `Ctrl+X` arrive through `XLookupString()` as
plain **control characters** on a standard X keyboard mapping - ASCII
3 / 22 / 24 respectively (the same "printable ASCII" 32-126 range this
codebase's relay already special-cases stops just short of; these are
below it, in the same family as the already-handled `8`=Backspace/
`9`=Tab/`13`=Enter/`27`=Escape). Real, testable, but **not yet
confirmed in THIS environment** - Mutter or a GTK/desktop-wide
accelerator could theoretically intercept Ctrl+C/V before an
application ever sees them (the exact class of surprise this house's
own testing doc already caught once for a different key). Real first
implementation step: confirm live (physical keyboard AND the relay,
which can already send raw codes 3/22/24 with zero relay-format
changes) that these codes actually reach `handle_key()` before writing
any clipboard logic around them.

## Where this hooks into the existing code

- **New XSelectionEvent handling** in `hq_dispatch_xevent()`
  (`khtpm_core_render.c`) - `SelectionRequest` (someone wants our
  clipboard content - only relevant while this window owns CLIPBOARD)
  and `SelectionNotify` (a paste we requested is ready) are NEW event
  types this function doesn't handle at all today (it currently
  switches on `Expose`/`ClientMessage`/`ButtonPress`/`KeyPress`/
  `FocusIn`/`FocusOut`/`MotionNotify` only) - real, additive, same
  "never touch existing branches" discipline this whole function
  already follows.
- **`default_cli_io_handle_key()`/`default_grid_handle_key()`** (state
  3, cell-editing) both gain a `ch==3`/`ch==22` check, calling two new
  shared helpers (`kh_clipboard_copy(const char *text)`/
  `kh_clipboard_request_paste(void)` - the request; the actual paste
  text arrives later via the new `SelectionNotify` handling above,
  which needs to know WHICH armed field to insert into - `g_default_
  input_elem` is still valid at that point, same pointer copy/paste
  already needs elsewhere, so this is free).
- Grid's own cell-edit buffer (`grid_cell_buffer`) is a real, natural
  THIRD consumer beyond cli_io/text_area, once step 1-2 above exist -
  same insert-at-cursor primitive, zero new clipboard code needed
  there.

## Test ground: text-edit-hq

Confirmed as the right first target (direct instruction: "we can test
in the text editor"). Its `<text_area id="editor">` is exactly the
shape this needs (a single, real, already-armable multi-line buffer,
already wired to the same `kh_text_insert_at_cursor()` primitive
paste would reuse) - no template changes needed to test with, only the
new key handling + selection-event plumbing above. A copy FROM
text-edit-hq's editor, verified by pasting into a REAL terminal/
browser (not just another khtpm window), is the real end-to-end proof
this works - a house-internal-only test would not actually prove X11
interop.

## Suggested build order once confirmed

1. Confirm Ctrl+C/V/X actually reach `handle_key()` unintercepted (a
   throwaway debug print, same technique already used live this
   session for the grid's own real-vs-relay input investigation) -
   BEFORE writing any real clipboard code, since if the desktop
   environment eats these combos, the whole plan needs a different key
   binding (e.g. a house-standard bespoke combo, or menu items instead
   of keys).
2. `kh_clipboard_copy()` - `XSetSelectionOwner` + store the text to
   hand out; `SelectionRequest` handling in `hq_dispatch_xevent()`
   (serve `UTF8_STRING`/`STRING`/`TARGETS` - the three real targets
   every clipboard consumer expects a text owner to answer).
3. `kh_clipboard_request_paste()` - `XConvertSelection` + `Selection
   Notify` handling, inserting the received text at `g_default_input_
   elem`'s cursor via the existing insert primitive.
4. Wire `ch==3`/`ch==22` into `default_cli_io_handle_key()` for
   `text_area` first (text-edit-hq's own editor), verify real copy-out
   to a system app and real paste-in from one.
5. Extend to plain `cli_io` and (once it exists per its own design
   doc's build order) grid's `grid_cell_buffer` - same primitive, no
   new protocol code.
6. `Ctrl+X` (cut) is copy + clear-the-buffer (whole-buffer v1 scope) -
   trivial once copy exists, not its own real step.
