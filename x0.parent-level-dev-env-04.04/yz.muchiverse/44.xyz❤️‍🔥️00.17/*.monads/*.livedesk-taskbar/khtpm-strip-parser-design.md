# khtpm_strip_parser — CHTPM-style design for the taskbar strip

This document proposes an analog of `chtpm_parser.c` for the taskbar
("strip") UI, treating the existing, done, live-verified
`khtpm_taskbar_manager.c`/`.h` as the business-logic "manager" that a new
`khtpm_strip_parser.c` would drive — matching real CHTPM's two-process,
file-relay architecture. No manager code is touched or second-guessed here;
this is planning only.

## 1. Process topology

Real CHTPM's fork model lives in `chtpm_parser.c`: a `<module>` tag (with
`src=` attribute, or inner text `<module>${module_path}</module>`, both
handled around lines 1851–1888) triggers `launch_module(const char*
launch_str)` (line 1358), which `fork()`s (line 1385) and `execv(args[0],
args)`s (line 1388) a **separate OS process**, tracked via
`current_module_pid`. The parser's main loop reaps it with
`waitpid(current_module_pid, &status, WNOHANG)` (line 2933) and sets
`current_module_pid = -1` when it exits, marking the frame dirty so a
relaunch/re-render can happen. `cleanup_module()` (line 546) explicitly
`kill(current_module_pid, SIGTERM)`s the child on parser shutdown.

**The taskbar today does not use this pattern at all.** Reading
`khtpm_taskbar_main.c` in full (29 lines) shows a single-process,
in-process design: `main()` calls `ktb_init()`, `ktb_write_pidfile()`,
`ktb_reload()`, then hands control to `ktb_plat_run(&st)` (the Xlib event
loop in `khtpm_taskbar_plat_x11.c`) — all direct C function calls, no
`fork`/`execv`/`system()`-launch-of-itself anywhere in
`khtpm_taskbar_manager.c`. The only `system()` calls in the manager (lines
853, 1026, 1134, 1330) launch unrelated external apps/shortcuts, not a
second copy of the taskbar logic. `ktb_pid_alive()` (line 57) similarly
checks whether a **tab's target app** is still running (used at lines 192
and 802 to prune dead tabs on reload) — it is not a self-health-check for
a manager process.

To honor the user's decision (full separate-process CHTPM architecture,
not in-process calls), `khtpm_strip_parser.c` should be the new outer
process that plays `chtpm_parser.c`'s role, and it should `fork()`/`exec()`
the existing taskbar manager logic (compiled as its own small binary,
e.g. wrapping the current `khtpm_taskbar_manager.c` + a thin driver loop,
analogous to how `chtpm_parser.c` execs an arbitrary `module_path` binary)
as its child — mirroring `launch_module()`/`current_module_pid` exactly.

**Restart-on-death**: CHTPM's own reference gives a real pattern to copy —
`waitpid(..., WNOHANG)` polled once per frame, `current_module_pid` reset
to `-1` on exit, dirty-flag set, and re-`launch_module()` on the next
tag-driven need. That is directly reusable: `khtpm_strip_parser.c`'s loop
should `waitpid(WNOHANG)` the manager child each tick and re-`fork/exec`
it if it disappears. This is a genuinely new decision only in the sense
that CHTPM's `launch_module()` restarts lazily (next time the `<module>`
tag is evaluated), not proactively — whether the strip parser should
restart the manager *immediately* on death (since a taskbar, unlike a
game module, must always be present) is not answered by the reference
files and is flagged in Open Questions.

## 2. File-based message contract

**Keys in.** Neither `khtpm_taskbar_manager.c`/`.h` nor
`khtpm_taskbar_plat_x11.c` nor `khtpm_taskbar_main.c` contains any
existing history.txt-style or bare-decimal-ASCII-per-line relay for the
taskbar — greps for `history`, `relay`, `KEY_PRESSED`, and `agent_relay`
across all three files return nothing except the manager's own
`interact_relay.txt` writes (output, not input; see below) and comments
about `_NET_WM_WINDOW_OPACITY`. Today, `khtpm_taskbar_plat_x11.c` reads
keys directly off X11 `KeyPress` events via `XLookupString(&ev.xkey, buf,
...)` (line 184) inside its own Xlib event loop — genuinely in-process,
no relay file at all. So there is **no existing taskbar-specific
agent-relay keycode format to reuse**; this design must adopt one, and the
most defensible choice is to copy CHTPM's own real, working convention
rather than invent something new: `chtpm_parser.c`'s `inject_raw_key(int
code)` (line 1262) appends either a bare `"%d\n"` decimal code or, when
`history_target_needs_key_pressed_prefix()` says so, a `"KEY_PRESSED:
%d\n"`-prefixed line, to a `history.txt`-family file (see
`WRAITH_ALPHA_SHARED_HISTORY "projects/wraith-alpha/session/history.txt"`,
line 1256, and the various `pieces/apps/player_app/history.txt` writers).
`khtpm_strip_parser.c` should write the bare-decimal-per-line form to a
new `strip_history.txt` (or similar) that the manager-side driver polls,
matching this exact real format rather than a bespoke one.

**State out.** The manager already has a real, working output-relay
convention worth reusing as-is: `write_relay()` in
`khtpm_taskbar_manager.c` (line 261) writes single-word commands
(`"ACTIVATE"`, `"OPEN_CONTEXT"`, `"CLOSE"`) to
`<tab_path>/interact_relay.txt`, one relay file per tab/package, with
backslash-normalized paths (line 265) — confirmed at call sites
`ktb_activate_tab()` (lines 274–275), `ktb_jump_nav()` (line 277ff), and
`ktb_quit_and_save()` (line 393). That mechanism is between the manager
and each tab's own app process and should be left exactly as-is (it's the
manager's read-only business logic).

For the **new** manager→parser channel (state changes the strip needs to
re-render, e.g. tab list changed, focus moved), model directly on
`chtpm_parser.c`'s `frame_changed.txt` touch signal: the parser's render
loop only calls `compose_frame()` when `pieces/display/frame_changed.txt`
grows (see the four `fopen("pieces/display/frame_changed.txt", "a")`
appenders at lines 2521, 2534, 2824, 2887, and the size-comparison logic
around line 2905–2919). `khtpm_strip_parser.c` should watch an analogous
`strip_frame_changed.txt`, appended-to by the manager driver whenever
`ktb_reload()`/`ktb_activate_tab()`/`ktb_focus_delta()` etc. mutate
`KtbState`, alongside a `strip_state.txt` dump of the fields the layout
needs (tab labels/active index, shortcut glyphs, digit buffer, theme
colors) — matching CHTPM's `state.txt`/`gui_state.txt` pattern (e.g.
`projects/%s/manager/state.txt`, line 334).

## 3. Layout format for the strip

CHTPM's real tag vocabulary, confirmed in `chtpm_parser.c`: `<button
label="..." onClick="...">` (used literally at line 931/933),
`visibility="..."` parsed into `el->visibility_expr` (line 1737) and
evaluated by `evaluate_visibility()`/`substitute_vars()` (line 1702-1703),
`<module src=... >` / inner-text `<module>` for launching processes
(1851–1888), and `cli_io` elements with `target_id`/`input_mode`
attributes for text entry. A `.chtpm` strip layout should reuse these
verbatim: each tab/shortcut is a `<button>` with `onClick="KEY:n"` (the
same `KEY:` prefix `send_command()` already parses at line 1550-1556) or
`onClick="ACTIVATE"` for popups.

**Variable-length tab list.** CHTPM has no `<foreach>`/loop tag; instead
the manager side pre-renders a markup fragment into a single string
variable and the layout substitutes it with `${var}` — exactly what
happens with `piece_methods`/`methods_raw`: `set_var("piece_methods",
methods_buf)` (line 946) builds a `<button .../><br/>`-concatenated string
in a C loop (`asprintf(&btn, "<button label=\"%s\" onClick=\"KEY:%d\"
/><br/>", ...)`, line 931), and the layout just references
`${piece_methods}` wherever it wants the list to appear. The strip driver
should do the same: iterate `KtbState.tabs[0..n_tabs)` and
`shortcuts[0..n_shortcuts)`, `asprintf` one `<button>` per tab/shortcut
into a `strip_tabs` / `strip_shortcuts` var, and write those into
`strip_state.txt` for the layout to interpolate via `${strip_tabs}` /
`${strip_shortcuts}`.

**Popups.** CHTPM has no dedicated popup/submenu tag — it reuses
`onClick="ACTIVATE"` as a toggle/scope marker: an element with
`onClick="ACTIVATE"` becomes a togglable "menu" whose children are hidden
unless it is the `active_index` (visibility logic at lines 2199-2210,
2324-2326), and `INTERACT`/click-handling walks up to "Find nearest
ACTIVATE ancestor" (lines 1671-1672, 2748-2749) to resolve which menu a
click belongs to. The strip's session/desk/pals popups (the `HQMenuItem`
rows already defined in the manager's `.h`) map directly onto this: a
`<button onClick="ACTIVATE">` wrapping a `${strip_popup_rows}`-substituted
block of child buttons, with `HQMenuItem.command`/`.nav` driving each
child's `onClick`.

## 4. Drawing: direct Xlib vs. rasterize-then-mirror

`khtpm_taskbar_plat_x11.c` is a **working, live** direct-Xlib renderer:
it opens a `Display`, creates a `Window` with
`ExposureMask|ButtonPressMask|KeyPressMask`, and (confirmed by grep)
calls `set_window_opacity()` (line 23), which sets the real
`_NET_WM_WINDOW_OPACITY` atom (line 26, `XChangeProperty(...,
opacity_atom, XA_CARDINAL, 32, ...)`) directly on that plain Xlib window,
driven by `load_theme_opacity()` reading the same
`#.desktop/livedesk_theme.pdl "COLOR | opacity | N"` convention the
legacy `tp_taskbar.c` used. This is proven working translucency on a bare
Xlib window today — no compositing helper library, no GL involved.

The alternative two-stage pattern — `chtpm_rgb_render.c` rasterizing
into an in-memory `unsigned char fb[FRAME_H][FRAME_W][4]` RGBA buffer
(built up via `blit_char`/`blit_text`/`blit_solid_block`/`blit_emoji_tile`,
then written out via `write_file_atomic()` + a `pulse_rgb_ready()`
signal file) and `gl_mirror.c` passively displaying it — is built for a
completely different scenario: `gl_mirror.c` is explicitly "the ONLY file
... allowed to call GL/GLUT", reads the finished pixel buffer with
`glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_frame_w, g_frame_h, ...,
frame_buffer)` and draws one textured quad, with **zero** real input
handling of its own (its GLUT keyboard callbacks exist only to prove/log
what GLUT's raw key codes are, per its own comments around line 128).
That pattern exists to mirror a *headless-rendered* frame (built by a
process with no windowing system access at all) into a GL window — it is
solving "how do I show pixels I computed without X" rather than "how do I
draw a taskbar."

**Recommendation: keep direct Xlib**, i.e. extend
`khtpm_taskbar_plat_x11.c`'s pattern for `khtpm_strip_parser.c`'s
rendering rather than adopting the rgb_render+gl_mirror two-stage
approach. Reasons: (a) the opacity mechanism is proven and file-cited as
working *specifically* on a plain Xlib window (`XChangeProperty` on the
window's own `_NET_WM_WINDOW_OPACITY` atom) — routing through a GL
texture quad would mean either re-deriving equivalent EWMH compositor
integration for a GLUT-created window (untested here, `gl_mirror.c` has
no opacity code at all) or layering a second compositing mechanism on
top, both strictly riskier than "don't touch what already works"; (b)
`gl_mirror.c`'s two-stage design pays for headlessness the strip doesn't
need — the strip already has direct X11 access exactly like
`khtpm_taskbar_plat_x11.c` does today; (c) input: `gl_mirror.c` explicitly
does none, so click/drag hit-testing (`ktb_tab_index_at_x`,
`ktb_shortcut_index_at_x`, `ktb_close_x0` etc., all already implemented
against raw pixel coordinates in the manager's `.h`) would need a second
input path anyway, duplicating what Xlib's `KeyPress`/`ButtonPress`
events already deliver for free in the current code.

## 5. Decisions (resolved 2026-08-11 — see khtpm-refactor-plan.md for the direct instructions these came from)

- **Manager restart: lazy, matching CHTPM exactly.** Direct decision: "lazy seems fine if chtpm does it. weve never had a problem with chtpm." `khtpm_strip_parser.c` follows `launch_module()`'s own pattern verbatim — `waitpid(WNOHANG)` per tick, `current_module_pid = -1` + dirty flag on death, relaunch next time the manager is needed (not proactively). No divergence from the reference file.
- **Binary split: manager keeps `KtbState` + the file-relay loop only.** The new standalone manager-driver binary owns `KtbState`, polls `strip_history.txt`, writes `strip_state.txt`/`strip_frame_changed.txt` — the same shape `khtpm_taskbar_manager.c` already has today (pure logic, zero platform calls), just moved behind a relay loop instead of direct function calls from `khtpm_taskbar_main.c`. `khtpm_strip_parser.c` (new binary) owns the Xlib window, all drawing, hit-testing (`ktb_tab_index_at_x`/`ktb_shortcut_index_at_x`/`ktb_close_x0` etc. move WITH the parser, not the manager — clicks are resolved locally against the last-known `strip_state.txt`, then a resolved action, not raw coordinates, goes out over `strip_history.txt`), and forks the manager child. Chosen over routing raw click coordinates through the relay for every click (extra round-trip latency for zero real benefit, since the layout math doesn't change per-click).
- **Poll interval: fixed-interval size-check polling, matching CHTPM exactly.** Direct decision: "use polling when chtpm uses it and frame changed file size check when chtpm does as well (uses a marker file after appending to it an 'x' mark)." No inotify — `khtpm_strip_parser.c` appends a single marker byte to `strip_frame_changed.txt` (mirroring the four `fopen(..., "a")` appenders in `chtpm_parser.c`) and the render loop does the same cheap `stat()`-size-comparison short-circuit `poll_agent_relay()` already uses elsewhere in this taskbar, at a fixed tick interval (exact ms not fixed here — should match whatever interval `poll_agent_relay()` already runs at in the legacy taskbar for consistency, since that's the taskbar's own precedent, not CHTPM's).
- **`strip_state.txt` schema: pipe-delimited rows, matching the taskbar's own `.pdl` convention.** Direct decision, explicitly rejecting CHTPM's own `key=value` pattern in favor of house consistency: rows shaped like the existing `DESK | entity | path | x | y | gx | gy | glyph | idx` convention already used in `desk_01.pdl` and the taskbar's own `livedesk_theme.pdl`/shortcuts files. Concretely (not yet finalized field-by-field, but the shape is fixed): one `TAB | pid | nav | entity | path` row per tab, one `SHORTCUT | glyph | command` row per shortcut, plus scalar rows for theme colors, digit buffer, and focus index using the same `KEY | value` two-column shape the theme file already uses. This keeps `strip_state.txt` readable with the same eyeball/debug habits as every other `.pdl` file in the house, rather than introducing a second serialization convention.
