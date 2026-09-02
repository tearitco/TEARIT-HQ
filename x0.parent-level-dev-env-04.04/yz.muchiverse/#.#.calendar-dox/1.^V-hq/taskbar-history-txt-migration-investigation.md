---
name: taskbar-history-txt-migration-investigation
description: Investigation (not yet implementation) - should the taskbar's real X11 input path migrate from direct in-process dispatch to a shared history.txt-style relay, matching mutaclysm/TPMOS's real unified-input convention?
metadata:
  type: project
---

# Taskbar history.txt migration — investigation

Companion to `taskbar-tpmos-parallel-refactor.md` (read that first — has the full ASCII-mirror
build-out this investigation follows on from, including the real relay-forwarding fixes that exposed
this question in the first place). Direct user question, 2026-08-18: "as we go we may even deprecate
the relay in favor of same history.txt tpmos uses right? that was just a bridge, right?"

**Phase 1: DONE, verified live with genuine real user input (2026-08-18).** Real X11
`KeyPress`/`ButtonPress` handling in `khtpm_strip_parser.c` is completely unchanged — every branch
still calls `dispatch_key_code()`/hit-testing directly, in-process, exactly as before. Added two
small helpers (`mirror_key_history()`/`mirror_mouse_history()`) called alongside the existing
dispatch at every real input site (3 `ButtonPress` handlers + all `KeyPress` branches: arrows,
Enter, Escape, Backspace, printable), writing to a NEW file, `#.desktop/strip_input_history.txt`,
using the real `KEY_PRESSED: <code>` format already established by `pieces/keyboard/history.txt`,
plus a `MOUSE_EVENT: <button> <x> <y> <is_press> <window_name>` line (window name appended as a 5th
field, per this doc's own "three windows" section below, so a reader wanting only the original
4-field mutaclysm format can still parse the front of the line). Verified with real, unprompted user
interaction (not a synthetic test) — `strip_input_history.txt` correctly captured
`KEY_PRESSED: 109` / `97` / `110` (`m`/`a`/`n`) typed live. Nothing reads this file yet, matching the
design's own "zero behavior risk" goal exactly.

**Phase 2 (retiring direct dispatch in favor of this file being the sole source of truth) is NOT
started** — still gated on the open questions below, particularly #1 (mouse window-disambiguation
format, now answered by Phase 1's actual implementation: 5th trailing field) and #3 (priority vs.
bottom-tab-bar/`cli_io`).

**This is a research/design pass for Phase 2 — Phase 1 above was small and additive enough to
implement directly per direct user instruction ("lets do history phase 1").**

## The real question, precisely

`livedesk_agent_relay.txt` (the relay this session's ASCII mirror writes into) is NOT the same kind
of thing as mutaclysm's `pieces/keyboard/history.txt`. Confirmed by direct read of
`khtpm_strip_parser.c`'s own `main()` event loop: a real X11 `KeyPress`/`ButtonPress` calls
`dispatch_key_code()`/local hit-testing DIRECTLY, in-process — there is no file in the real GUI
input path at all. `livedesk_agent_relay.txt` is a deliberate, separate side-channel that only
external tools (this session's own ASCII keyboard binary) write into.

Mutaclysm's `game_dispatch.c`, by contrast, is a genuine one-shot poller: EVERY input source (real
X11 via `x11_mirror.c`, terminal via `system/keyboard_input`) writes into the SAME
`pieces/keyboard/history.txt`, and `game_dispatch.c` is the ONE consumer, invoked fresh each tick,
with no separate "direct dispatch" path at all. That's the real "unified input" shape the user is
asking whether the taskbar should adopt.

## What already exists as real precedent (don't invent a new format)

`pieces/keyboard/history.txt`'s real, established two-line convention (confirmed via direct read of
`&.widgits/_shared-lib/system/chtpm_parser_pal.c`'s own poll loop):
```
KEY_PRESSED: <decimal code>
MOUSE_EVENT: <button> <x> <y> <is_press>
```
If the taskbar migrates, this is the format to reuse — not a new one.

## Real complication mutaclysm doesn't have: THREE windows, not one

Mutaclysm's `x11_mirror.c` is a single window; its `MOUSE_EVENT` lines carry raw x/y with no
ambiguity about which surface was clicked. The taskbar's real parser owns THREE separate X11
windows simultaneously (confirmed, `khtpm_strip_parser.c` ~line 2045/2065/2080):
- `win` (the bottom strip/tab bar)
- `hq_win` (the header)
- `popup_win` (the HQ popup submenu)

Each has its OWN `ButtonPress` handler with window-relative coordinates and DIFFERENT hit-testing
logic (`hit_test`, `g_bottom_hits`, header-cell hit-testing, popup-row hit-testing — confirmed 10
real `ButtonPress`/`hit_test` call sites). A unified history.txt line would need to also carry WHICH
window was clicked (e.g. `MOUSE_EVENT: <window_id_or_name> <button> <x> <y> <is_press>`), or every
consumer would need a way to resolve "which window has focus right now" separately. This is real,
new format design — not a mechanical copy of mutaclysm's convention.

## The deeper risk: `dispatch_key_code()`'s own local state

Already documented (and already twice worked around) this session: `dispatch_key_code()` and the
real `KeyPress` handler both lean on process-local state that has no file-backed equivalent today:
- `g_nav_focus` (unified arrow-nav cursor spanning header cells + tabs)
- `header_doc`/`bottom_doc`'s own `active_index`/`focus_index` (which submenu is open, what's
  locally focused — rebuilt from `strip_state.txt` on every redraw via
  `lay_reload_preserving_scope()`, but ALSO mutated directly by local key/click handling in between
  reloads)

If ALL input moved to a file-relay model with a single poll loop as the only consumer (mutaclysm's
shape), this local state either needs to become file-backed too (a real behavior change, more
surface area) or the poll loop still needs to hold it in process memory between reads (which is
actually FINE — mutaclysm's own `game_dispatch.c` is a fresh one-shot process each tick and has NO
local state at all between invocations, a meaningfully different shape than a persistent long-running
X11 event loop that just changed its input SOURCE from "blocking on the X connection" to "polling a
file". The taskbar's parser is architecturally closer to TPMOS's own persistent daemon-style modules
than to mutaclysm's one-shot-per-tick ops — worth confirming which real precedent is the more honest
parallel before designing further.

## Recommended path: two phases, not one big rewrite

**Phase 1 (low risk, additive only):** real X11 `KeyPress`/`ButtonPress` handling keeps working
EXACTLY as it does today (direct in-process dispatch, unchanged) — but ALSO writes a mirrored
`KEY_PRESSED:`/`MOUSE_EVENT:` line into a new shared history file, purely for audit/consistency.
Zero behavior risk (nothing reads this file yet), but immediately gives:
- A single, real audit trail of ALL input (not just relay-injected), matching TPMOS's own
  "if it's not in a file, it's a lie" standard.
- A concrete, low-stakes place to validate the window-disambiguation format question above before
  anything depends on it.

**Phase 2 (real behavior change, do only after Phase 1 is live and proven):** make the shared history
file the SOLE input source — real `KeyPress`/`ButtonPress` handlers stop calling
`dispatch_key_code()`/hit-testing directly and instead only ever write to the file; a single poll
loop (which could just be the existing main loop, reading its own just-written lines) becomes the one
true dispatch path, same as `poll_agent_relay()`'s own real precedent already proven this session.
This is the point where `livedesk_agent_relay.txt` actually gets retired — external tools and real
X11 input become indistinguishable, writing the exact same format into the exact same file.

## Phase 2 design (2026-08-18, direct user request: "phase 2 planning would be good")

### What the TRUE target shape actually is — confirmed, not assumed

Checked `x11_mirror.c` directly (mutaclysm's own real GUI capture binary, the closest real reference
to what `khtpm_strip_parser.c` would need to become): it is a PURE capture-only writer to
`pieces/keyboard/history.txt` — grep for `dispatch` in that file returns nothing. It has ZERO
dispatch logic of its own. `game_dispatch.c` is the SOLE reader, and the SOLE place any key's
meaning is decided. That is the real, proven mutaclysm/TPMOS shape: **N capture-only writers, ONE
dispatcher reader, always** — not "the writer also happens to dispatch locally as a shortcut."

This matters because a SHALLOW version of Phase 2 (real X11 capture writes to
`strip_input_history.txt`, then immediately reads its own just-written line back and dispatches it
inline, same as today) would NOT be real parity — it's the current shape with extra I/O bolted on
for no benefit. The real target is structural: **real X11 KeyPress/ButtonPress handling stops
deciding anything at all** — it only ever writes. All dispatch, for every input source (real GUI,
this project's own ASCII keyboard binary, any future agent/harness), goes through exactly one place.

### Concrete restructuring plan

1. **Split capture from dispatch inside the existing event loop first** (still one process, no new
   binary yet — smallest safe step). Today's loop does, per `XPending()` iteration: read event →
   decide → dispatch → (sometimes) redraw, all inline. Restructure to match `game_dispatch.c`'s own
   proven real shape (already used successfully this session for mutaclysm — "Step 1: read ALL keys
   into memory... Step 2: dispatch each"): drain ALL pending `XPending()` events first, writing EACH
   one to `strip_input_history.txt` and nothing else; THEN, in a second pass, read back every new
   line just written (plus anything an external tool appended in the meantime) and dispatch each
   through `dispatch_key_code()`/hit-testing; redraw ONCE at the end of the tick, not per-event.
   This is a real behavior change (batches what were separate synchronous redraws into one), but
   matches the established, house-wide "dispatch all, render once" standard already proven
   elsewhere this session, not a new invention.
2. **Retire `livedesk_agent_relay.txt` in favor of `strip_input_history.txt`.** Once step 1 lands,
   the parser's read-back pass IS the dispatcher for external input too — `poll_agent_relay()` gets
   replaced by a reader for the new `KEY_PRESSED:`/`MOUSE_EVENT:` format (bare-decimal vs. this
   format is the only real parsing difference). `khtpm_strip_keyboard_ascii.c` switches from writing
   bare decimals into `livedesk_agent_relay.txt` to writing `KEY_PRESSED: N` lines into
   `strip_input_history.txt` — a small, mechanical change once the reader side exists.
3. **Only after both above are live and proven**, consider whether real X11 capture should actually
   move to a SEPARATE binary (true `x11_mirror.c`-style split) — this is a bigger, later question,
   not required for "one file, one dispatcher" parity, which steps 1-2 alone already achieve within
   a single process.

### Real risk this restructuring introduces (be honest about it)

- **Per-event redraw → batched redraw** is the one genuine behavior change for real GUI use, not
  just an implementation detail. Today, clicking one thing and seeing the popup close/reopen happens
  as N separate synchronous redraws if N events land in one `XPending()` drain (rare but possible,
  e.g. fast double-clicks); batching means only the FINAL state after all N is ever drawn. This
  matches TPMOS's own real standard and is very unlikely to be noticeable, but it's a real, not
  hypothetical, difference — flag it, don't discover it live.
- **`g_nav_focus`/`header_doc`/`bottom_doc` local state** (already documented above) still lives in
  the SAME process either way under this plan — step 1 does not require moving it to a file. Real
  risk stays contained to "does batching change any ordering assumption a handler makes," not "does
  local state need to become file-backed."
- **Rollback is cheap while this stays one process**: step 1 is a real but narrow diff (restructure
  one loop), easy to revert to today's inline-dispatch shape if batching causes any live issue —
  much lower-stakes than jumping straight to a separate capture binary.

### Implemented as a feature-flag dual path (2026-08-18, direct user instruction: "cant we split and
still do it the old way but do it new way till its confirmed working or what?")

Done, not just planned. `khtpm_strip_parser.c` now has BOTH paths, gated on
`getenv("KHTPM_NEW_DISPATCH_MODE")` (unset = old, today's exact behavior; set = new):
- `apply_captured_key()`/`apply_captured_mouse()` — faithful, verbatim extractions of the real inline
  KeyPress/ButtonPress logic (including the real, PRESERVED asymmetry: the bottom bar's right-click
  has no `!cliio_active` guard the other two windows have — not "fixed" while extracting, per this
  session's own standing discipline).
- `poll_captured_input()` — the read-back pass, real cursor/truncation-resync discipline matching
  `poll_agent_relay()`'s own proven precedent, parses `strip_input_history.txt`'s
  `KEY_PRESSED:`/`MOUSE_EVENT:` lines and dispatches each via the two functions above.
- Real X11 handlers now: always call `mirror_key_history()`/`mirror_mouse_history()` (Phase 1,
  unchanged); THEN, only if `g_new_dispatch_mode`, skip the old inline dispatch entirely (capture-
  only). Old mode's inline dispatch code is 100% untouched — same lines, same order, just gated with
  an `if (!g_new_dispatch_mode)` wrapped around it rather than rewritten.
- One batched redraw after the whole `XPending()` drain, only in new mode — old mode keeps its
  existing per-KeyPress-event redraw, unchanged.

**Verified**: compiles clean (no new warnings — all pre-existing, unrelated format-truncation noise
checked line-by-line). Rebuilt and restarted the LIVE taskbar in default (old) mode and re-ran the
same relay-driven HQ open→focus-move→close→wraparound sequence from the Phase 1 section above — all
identical, correct results (`hq_open` 0→1→0, `strip_focus_cell` wraps 0→14 correctly). Old mode is
confirmed unaffected.

**Real X11 injection tools already exist in this house** — no `xdotool` needed. Found via direct
search (`&.widgits/tile-picker/ops/tp_test_send_key.c` / `tp_test_send_click.c`, both already
compiled): real `XTest`-based key/click synthesis, written specifically because `xdotool` isn't
installed on this machine. `tp_test_send_key.c`/`tp_test_send_click.c` match target windows by
`XFetchName` (WM_NAME) — this taskbar's windows have no title set (only `WM_CLASS=
MuchiverseLivedesk`, confirmed via `XSetClassHint` in `khtpm_strip_parser.c`), so a small disposable
`WM_CLASS`-matching variant was used instead to locate the real window IDs (`XQueryTree` +
`XGetClassHint` walk) — same real `XTestFakeKeyEvent`/`XTestFakeButtonEvent` mechanism underneath,
just a different window-lookup step.

**New mode VERIFIED LIVE with genuine real user interaction (2026-08-18)** — not synthetic, not
guessed. Launched with `KHTPM_NEW_DISPATCH_MODE=1 sh run_khtpm_strip.sh new`; the user then
genuinely clicked and typed across all three real windows. `strip_input_history.txt` correctly
captured a real mixed sequence — `MOUSE_EVENT: 1 x y 1 win` / `... popup_win` / `... hq_win` and
`KEY_PRESSED: 112` (`p`) / `KEY_PRESSED: 13` (Enter) — and `strip_state.txt` showed correct,
sensible resulting state (`hq_open=0`, `hq_focus=-1`, `strip_focus_cell=2`) matching what real
interaction across those clicks/keys should produce. The new capture-then-dispatch-then-render-once
path is confirmed working end-to-end, not just compiling cleanly. Revert to old mode with a plain
`sh run_khtpm_strip.sh new` (no env var) any time.

### Recommended sequencing

Do NOT start step 1 until the bottom-tab-bar-activation and `cli_io`-typing gaps (documented in
`taskbar-tpmos-parallel-refactor.md`) are further along — those exercise the CURRENT dispatch shape
and are much easier to debug-trace against a known-stable event loop than against one mid-restructure.
Step 1 is real, valuable, well-understood work, but it touches the live GUI's core loop; sequence it
after the lower-risk items, not before.

## Questions from the original Phase 1 planning — resolved by what actually shipped

1. Mouse-event format: resolved as `MOUSE_EVENT: <button> <x> <y> <is_press> <window_name>` (window
   name as a 5th trailing field, not a prefix) — live in `strip_input_history.txt`.
2. New file vs. reusing the relay: resolved as a NEW file (`strip_input_history.txt`), per the
   reasoning already given (`livedesk_agent_relay.txt` stays untouched, still consumed by
   `poll_agent_relay()` today — no dedupe/loopback risk introduced).
3. Priority vs. bottom-tab-bar/`cli_io`: Phase 1 was small enough to do immediately (direct user
   instruction). Phase 2's real step 1 (see design above) is explicitly sequenced AFTER those two
   gaps — see "Recommended sequencing" above.

## Phase 3 — cutover complete (2026-08-19), superseding "Recommended sequencing" above

Direct instruction: "finish the cutover from old mode to the muta style update completely and
deprecate the old way." Done, confirmed by direct code read of `khtpm_strip_parser.c`:
`g_new_dispatch_mode` and the `getenv("KHTPM_NEW_DISPATCH_MODE")` check are DELETED, along with
every `if (!g_new_dispatch_mode)`-guarded old inline-dispatch block. There is no runtime toggle and
no fallback path anymore — capture (write-only, inside `XPending()`) → `poll_captured_input()`
(deferred read-back-and-dispatch-all, once per tick) is the ONLY dispatch path. "Revert to old mode
with a plain `sh run_khtpm_strip.sh new`" (line 217-218 above) no longer applies — there is nothing
to revert to short of reconstructing the deleted code by hand.

The bottom tab bar was confirmed, before cutover, to be fully covered by the new path (its own
`apply_captured_mouse()`/`apply_captured_key()` branches mirror the old inline logic exactly) — so
the cutover did NOT leave a bottom-bar gap.

Same day, a frame-unification pass (separate from this dispatch-mode work) added
`strip_frame.cells.pdl` + a terminal-mirror rewrite to consume it (see
`au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` under `44.xyz.01.00/`). That pass introduced a
real bug — its own change-signal file (`strip_frame_changed.txt`) collided with the name this
dispatch-mode work's `frame_changed_dirty()` already used for an unrelated purpose, causing the
layout docs to reload every tick and silently wipe arrow-key submenu focus (which, unlike digit/
click nav, is never persisted). Fixed same day by giving the cells.pdl signal its own file
(`strip_cells_changed.txt`) — see `tb-bugfix-au19.txt` (outer `yz.muchiverse/` root) for the full
investigation and fix.
