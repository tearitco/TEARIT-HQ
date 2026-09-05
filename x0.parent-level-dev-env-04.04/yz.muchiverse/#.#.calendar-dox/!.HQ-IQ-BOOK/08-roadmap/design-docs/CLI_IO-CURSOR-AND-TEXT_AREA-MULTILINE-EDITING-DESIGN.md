# Plan: a real cursor for `cli_io`, and a new `<text_area>` element for multi-line editing

**Status: IMPLEMENTED and live-verified, 2026-09-05.** Grew out of
`11.brainstorm/2026-09-05/PDL-READER-AND-FILE-EXPLORER-WIDGET.md` §5's
open question ("does `cli_io` get extended, or does the editor need
its own element?") — answered directly this session, see Decisions
below, then built the same day.

**What actually shipped** (commits `f3ba7629` cli_io cursor,
`c6eaa24c` text_area + an armed-state bug fix found while verifying
it): everything in this doc's own "Decisions" and "What cli_io gains"
sections below, real and working. Verified live via real X11 key
events (not synthetic relay) against a standalone test window AND
open-hai's own real, unmodified composer, with `dump_frame_png_op`
invoked directly by window ID (bypassing the app's own keyboard input)
so an ARMED field's true on-screen state could be checked without
disarming it first — this is how a real bug got caught: the cursor-bar
draw gate was checking nav-focus equality, not the same real armed
check the `^` badge already used correctly, so a merely-tabbed-to
field could show a misleading cursor. Fixed before commit.

**Left for later, exactly as scoped below**: real VISUAL (word-wrapped)
row Up/Down within one long logical line — text_area's Up/Down today
is real but LOGICAL-line-only (crosses actual `\n` boundaries, not
wrap breaks). The File Explorer widget and the actual `toys` text
editor app are still not built — this doc only covers the underlying
`cli_io`/`text_area` element capability they'll be built on top of.

## Why this exists

The planned `toys` text-editor refactor (and any future khtpm app
needing real multi-line text) has nowhere to go today. `<cli_io>` —
the house's one generic text-input element — was extended
(2026-09-01/02) to reserve `rows="N"` of vertical space and
word-wrap its buffer across them, but it is NOT a real multi-line
editor:

1. **No cursor.** `input_buffer` is append/backspace-at-the-end only —
   no left/right/up/down movement, no clicking into the middle of
   text. (**Update, 2026-09-05**: this is now being fixed for `cli_io`
   itself too, not just worked around inside a new element — see
   Decisions below.)
2. **Enter submits, not inserts.** `default_cli_io_handle_key()`:
   `XK_Return` runs the field's `action=` and clears the buffer —
   correct for a one-line chat/search composer, wrong for an editor.
3. **Tiny buffer** (`input_buffer[256]`) — fine for a chat line, far
   too small for a document.
4. **Word-wrap is display-only, not real line breaks.** The existing
   wrap breaks a single flat string purely by width; there is no
   concept of a real `\n` the user actually typed, distinct from where
   the renderer happened to wrap.

## Decisions (made 2026-09-05, direct instruction)

- **New element, not an extension of `cli_io`.** `cli_io`'s own real
  reference behavior (the tpmos `agy-text-editor` capture in the
  sibling brainstorm doc, §2.1) shows Interact Mode as its own
  logically separate thing from a one-line composer/search field —
  a genuinely different kind of control, not a bigger version of the
  same one. New tag: **`<text_area>`**.
- **Share code with `cli_io` wherever it keeps things legible**, not
  as a hard requirement. Concretely: the existing word-wrap-for-width
  calculation `cli_io` already has is real, working, and tag-agnostic
  logic — factor it into a shared helper both tags call, rather than
  duplicating it in `text_area`. Don't force a shared abstraction
  where the two controls' needs genuinely diverge (that's how `cli_io`
  itself would end up half-broken for its own existing single-line
  consumers).
- **`cli_io` itself gets a real cursor too** (direct instruction,
  2026-09-05, added after the rest of this doc was first written):
  `cli_io` staying append/backspace-at-the-end-only was never actually
  desirable on its own merits — a one-line chat/search composer
  benefits from Left/Right/Home/End and click-to-position exactly as
  much as a multi-line editor does; it just never got done because
  nothing needed it badly enough yet. This is now real, in-scope work
  for `cli_io` itself, independent of `text_area` existing — NOT
  gated on `text_area` being built first. Practical effect: the
  cursor-position + insert-at-cursor/delete-at-cursor primitive
  becomes the ACTUAL shared code between the two elements (more so
  than just the word-wrap calc originally called out above) —
  `text_area`'s own multi-line cursor logic (§ below) is this same
  single-line primitive generalized to move across embedded `\n`
  characters and visual (post-wrap) rows, not a separate
  implementation. `cli_io` itself stays single-line — Enter still
  submits there, exactly as today; only the CURSOR gets real, not the
  multi-line/newline behavior, which stays `text_area`-exclusive.
- **Activation matches the house's existing armed-field convention,
  not a new one.** `activate_focused()` already special-cases
  `cli_io` (arm it, `kh_grab_keyboard_retry()`, real X keyboard grab)
  — add `text_area` to that same branch, verbatim mechanism. The `^`
  indicator is the SAME glyph this house already uses everywhere for
  "this is the active/armed thing" (scope confine, focused-window
  title mark, nav badges) — not a new visual language, matching the
  tpmos reference's own `[>] 1. [EDIT TEXT (INTERACT)]` → armed field
  shape closely, but through khtpm's own existing generic nav-row +
  `^` convention rather than a bespoke "(INTERACT)" label state.
  **Escape disarms**, exactly like `cli_io`'s existing
  `XK_Escape → g_default_input_elem = NULL; XUngrabKeyboard(...)` path
  — reused, not reinvented.

## What `cli_io` gains (shared with `text_area`) vs. what stays `text_area`-only

**Shared (built once, used by both)**: a real cursor position + real
insert-at-cursor/delete-at-cursor editing, replacing today's
append/backspace-at-the-end-only behavior. For `cli_io` (single-line):
Left/Right move by one character, Home/End jump to start/end,
click-to-position if/when this renderer gets real click-to-cursor
support for text (not yet confirmed to exist anywhere in the house —
worth checking before assuming it's free). For `text_area`
(multi-line): the SAME primitive, generalized so Up/Down move by one
*visual* line (post-wrap) and the cursor correctly crosses embedded
`\n` boundaries — real, non-trivial logic, the biggest single chunk of
new work in this whole plan, but now framed as extending one shared
cursor model rather than building a second one from nothing.

**`text_area`-only**:
- **Enter inserts `\n`** into the buffer at the cursor position while
  armed. Saving/submitting is a SEPARATE action — matching the tpmos
  reference's own FILE MENU shape (`[ ] 2. [SAVE FILE]` as its own
  distinct nav row, not tied to any keystroke inside the text field
  itself).
- **A real newline vs. a display-wrap line are different things** and
  must stay different: the buffer stores the user's actual typed
  content including real `\n` characters; the renderer computes
  word-wrap fresh at DRAW time for display/cursor-row math only, and
  never mutates the underlying buffer to "bake in" a wrap break. This
  is the rule that makes Save write back exactly what the user typed,
  not a width-mangled version of it.
- **A much bigger buffer.** `cli_io`'s 256 bytes is nowhere near
  enough for a document; needs either a large fixed cap (simplest,
  matches this codebase's general style of fixed-size buffers) sized
  for realistic document lengths, or a real dynamically-grown buffer
  if fixed-size turns out too limiting once this is actually built —
  not decided yet, flagged as an implementation-time call.

## Where the real content lives

Per `CENTROID_GOLD_STD.md`'s standing rule (real manager owns state,
shared renderer never does), the actual document buffer/file content
should NOT live only inside the renderer's own `Elem.text_area_buffer`
in memory — that's what every other stateful khtpm widget already
avoids. Follows the same shape `cli_io` itself already uses
(`default_cli_io_save()` writes the live buffer out to
`cli_io_state.txt` on every edit): a real editor manager process reads
the actual file from disk, publishes it as the live buffer, and
reconciles what the renderer's own live-edited projection reports back
via the same plain-text relay convention — exact mechanism (write-every-
keystroke vs. write-on-arm/disarm/save) is an implementation-time
decision, not settled here.

## Frame round-trip (a real, already-bitten trap — do not skip this)

Every generic-mode window draws through the serialize-to-file /
reparse round trip (`kh_serialize_frame_elem()` → file →
`kh_paint_frame_line()` → fresh `Elem` → `draw_elem()`) — this
session's own `relay` and `bg` fields both needed adding to BOTH
functions or they would have silently never reached the draw path, no
matter how correct everything else was. `text_area`'s own buffer field
needs the exact same treatment on day one of implementation, not
discovered as a bug afterward a second time.

## Explicitly not decided yet (implementation-time calls)

- Whether `cli_io`'s own cursor fix ships as its own independent,
  earlier piece of work (it's fully decoupled from `text_area`
  existing at all, and is real value on its own for every existing
  chat/search composer) or lands together with `text_area` since
  they'd share the same new primitive — an efficiency-vs-shippable-
  increments call to make when this is actually picked up, not here.
- Exact buffer size / fixed-vs-dynamic (above).
- Exact relay/save-state file format and write cadence.
- Whether Up/Down at the very first/last visual line should do
  anything special (move focus out of the field, like a real form) or
  stay a no-op — no reference behavior captured for this edge case.
- Whether `text_area` should support the `rows="N"` layout attribute
  identically to `cli_io` (very likely yes, since the box still needs
  a real height reserved in the fixed-rows layout pass) or needs its
  own sizing model (e.g. "fill remaining space" for a real full-screen
  editor, closer to how `<scrolllist>` already claims remaining
  vertical space) — this actually matters a lot for what a real
  editor window looks like and should get a real answer before coding
  starts, not be discovered mid-implementation.

## Sequencing

Per direct instruction: this doc is written now; nothing is scheduled
or implemented yet. When picked up, log the decision to start real
work in `12.calendar/<date>/`, same as any other feature graduating
out of the brainstorm/plan stage.
