# au-31/02 — real answers: why the browser window behaves differently, where its data comes from, real bugs found live

Direct questions, 2026-08-31, asked against the STANDALONE
`network_browser_render.+x` (the pre-correction binary, still on disk
under `&.hq-apps/network/` - NOT the real khtpm mode, which per
`xperiments/khtpm-generic-dispatch-design.md` has not been built yet).
Real, verified answers below - a live screenshot was taken
(`dump_frame_png_op.+x` against the real, running window,
`0x1c00001 "Network Browser" 760x520+200+120`) before answering, not
guessed.

## 1. "Is it using the same parser/renderer? Not using correct markup buttons?"

**No, confirmed by direct read.** The standalone binary
(`network_browser_render.c`) hand-builds its own `Elem` tree directly
in C - it never calls `parse_chtpm()`, never reads
`network-browser-hq.chtpm`'s real `<item action="CLOSE">` markup, and
its "buttons" are raw `XftDrawStringUtf8()` calls at hand-computed
x/y, not real `Elem`s with `onclick="CLOSE"` dispatched through the
shared, generic action dispatcher every other khtpm window uses. This
is exactly the violation caught and reverted the same day (see
`TPMOS-COMPLIANCE-DEBT.md` §5/§6, `CENTROID_GOLD_STD.md` §3 rules 1
and 7) - the standalone binary predates that correction and was never
rebuilt after it. The real fix in flight is `xperiments/khtpm-
generic-dispatch-design.md`: network-browser-hq becomes a real mode
INSIDE `khtpm_entity_menu_render.c`, sharing the exact same
`parse_chtpm()`/`render_tree()`/dispatch machinery db-hq and events-hq
use - not built yet.

## 2. "Why doesn't it take focus for arrow/index-jump like the others?"

**Real, confirmed cause**: db-hq has two real, hard-won focus-
robustness functions the standalone binary never got -
`dbhq_soft_focus()` (`XRaiseWindow` + `XSetInputFocus`) and
`dbhq_grab_keyboard_retry()` (retries `XGrabKeyboard` up to 5 times,
5ms apart, until it actually succeeds). These exist because a single
`XSetInputFocus` call at map time (which is ALL the standalone
binary's `main()` does) is not reliable - the WM can steal focus back,
or the grab can race the window manager's own reparenting. Without the
retry, arrow keys and digit-jumps silently go nowhere whenever real
X11 keyboard focus isn't actually held, which reads exactly as "doesn't
take focus like the others." Real fix: once network-browser is a real
khtpm mode, it inherits this same focus machinery for free (it's
already generic/shared-callable, not db-hq-specific code) - no new
code needed, just real membership in the shared dispatch.

## 3. Dynamic entries — `fo-menu-sys.md`, and "do we have an HTML parser?"

Read `#.haiku+/tpmos-re-dox/fo-menu-sys.md` in full. Real, important
distinction: that doc describes the **chtpm_parser.c (ASCII) family's**
own real "Thin Theater / Manager Projection" convention -
`${piece_methods}`-style variable substitution into a `.chtpm` layout,
sourced from a manager-published flat `gui_state.txt`, with
`<button onClick="KEY:2">` markup. This is real, documented, and is
the SAME real philosophy `CENTROID_GOLD_STD.md` already requires for
khtpm - but the exact MECHANISM differs: khtpm (X11) has no
`${variable}` substitution system - the analogous real mechanism there
is `reusable_slot()`/`elem_inject_loop()` injecting real `Elem`
children into an already-parsed panel, which is what
`dbhq_inject_list_panel()` etc. already do for db-hq. Network-
browser's real manager (`network_browser_manager.c`) already follows
the SAME real "manager owns projection, renderer only injects/
substitutes" philosophy fo-menu-sys.md describes - it publishes a flat,
already-parsed, display-ready row list (`network_browser_page.state.
txt`: `TITLE|...` / `TEXT|...` / `LINK|href|label` lines), and (once
built as a real mode) the renderer's whole job is injecting those rows
via `reusable_slot()`, never re-deriving them.

**"Do we have an HTML parser?" — No, and this is deliberate, not a
gap.** `network_browser_manager.c` does NOT use any HTML/DOM library
(no libxml2, no htmlparser, nothing). It does a real, manual, tag-
boundary text scan (`extract_and_publish()` in that file): finds
`<title>...</title>` by substring search, strips `<script>`/`<style>`
bodies entirely, walks character by character tracking whether it's
inside a tag, flushes accumulated text into a `TEXT|` row at block-tag
boundaries (`p`/`div`/`br`/`li`/headings/etc.), and separately extracts
`<a href="...">` targets into `LINK|` rows with a small manual
href-resolution helper (`resolve_url()`) for relative links. This is
real, working, tested (see the earlier session's live `example.com`/
`iana.org` fetch tests) but is explicitly NOT a real HTML/CSS renderer
- no DOM tree, no CSS cascade, no JS, no table/image layout. That
scope limit is intentional and stated in the manager's own header
comment ("just enough real parsing... deliberately NOT a real HTML/
CSS renderer").

## 4. Real, live bugs found via this session's own screenshot

Screenshotting the real running standalone window (viewing
`https://iana.org/numbers`) surfaced two real, confirmed rendering
bugs - neither was previously caught because earlier verification used
a shorter page (`example.com`) with short text lines:

- **Chrome button overlap** ("the x is a bit too far to right of
  screen"): the close (`X`) and fullscreen (`!`) glyphs render
  overlapping/garbled at the top-right corner. Real cause: each
  button's own label text width was never measured before drawing -
  the fixed 24px-wide slots (`g_win_w-24`/`g_win_w-48`) are narrower
  than the real rendered `"[ ]X"`/`"[ ]!"` cursor-prefixed label text,
  so adjacent labels visually collide. This is the exact same real bug
  class `evhq_layout_pass()`'s own header comment already documents
  fixing for events-hq's tab badges ("focus-ring border sized to e->w
  came out narrower than the actual visible content... badge was never
  counted").
- **No line-wrapping on long TEXT rows** ("rendered text on the far
  end"): the standalone renderer draws each `TEXT|`/LINK row as one
  unbroken `XftDrawStringUtf8()` call at a fixed row height, with no
  wrap - a long paragraph (IANA's own real page text) runs off the
  right edge of the window instead of wrapping to multiple lines. The
  ASCII mirror (`network_browser_render_ascii.c`) already does real
  word-wrapping (`wrap_print()`) - the X11 side never got the
  equivalent.

Both are real, honest gaps in the STANDALONE (pre-correction) binary,
not fixed here - per the current plan, this binary is being replaced
by a real khtpm mode, not patched further. Recorded so the real mode's
own layout function measures real text width (`evhq_measure_text_px()`
-style, already a real, existing, reusable helper shape) and wraps
content rows for real, from day one, instead of repeating this gap.

## 5. "Are you using `<cli_io>` for the address bar yet?"

**No - real, confirmed gap.** `<cli_io>` IS a real, already-supported
tag in `khtpm_entity_menu_render.c` (confirmed: `dbhq_cli_io_navigable()`
checks `strcmp(e->tag, "cli_io") == 0`, used by db-hq's own real
armed-text-input fields, e.g. bookmarks' "New+" path entry via
`g_input_elem`). The standalone binary's address bar is a fully
hand-rolled text buffer (`g_url_buf`/`g_url_len`, manual append/
backspace key handling) - it does not use `<cli_io>` or the shared
`g_input_elem` arm/type/commit mechanism at all. Real fix: the eventual
khtpm-mode network-browser should author its address bar as a real
`<cli_io>` element in `network-browser-hq.chtpm` and drive it through
the same generic armed-input mechanism `g_input_elem` already provides
(the same real reason `handle_key()`'s existing `'p'`-key-order
exception for `g_is_db_hq && g_input_elem` exists) - not a new,
bespoke text-editing implementation.

## Real, net conclusion

Every one of these gaps traces back to the same root cause: the
standalone binary was built BEFORE (and in violation of) this session's
own `CENTROID_GOLD_STD.md` correction - it never shares code with "the
others" because it isn't part of the shared renderer at all. All five
real fixes (parser/markup, focus-grab-retry, real `<cli_io>` input,
measured-width chrome buttons, wrapped content text) are already
"free" - they come from real, existing, shared khtpm machinery -
**once network-browser is actually built as a real mode per
`xperiments/khtpm-generic-dispatch-design.md`**, not by patching the
standalone binary piecemeal. That build has not started yet (§2a of
that design, generic `launch_module()`, is the only piece done so far,
proven on db-hq).
