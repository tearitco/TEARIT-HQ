GROK-RENDER-INPUT-REFACTOR-HANDOFF.md
Started: 2026-08-28
Purpose: shared, async collaboration doc between Sonnet (this session)
and Grok, working the same real refactor from two different terminals.
Same convention as this house's own proven COMMON-EVENTS-MANAGER-
HANDOFF.md (used all this session with opencode) - post a real task
here, the other side posts a real "⛔ EXECUTION RECORD" reply here when
done or when blocked. Read the whole doc before doing anything, not
just the newest section.

============================================================
REQUIRED READING FIRST, IN ORDER
============================================================
1. `RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md` (this same directory) -
   the real design doc: what the drift is, why it's real (a genuine,
   unintentional architectural gap, not a deliberate design), the real
   wraith-alpha reference pattern this house already uses everywhere
   else, confirmed scope (taskbar and mutaclysm are OUT of scope, for
   real, already-investigated reasons - don't re-litigate that).
2. `RENDER-REFACTOR-2DO-PROGRESS.md` (this same directory) - the LIVE
   status tracker. Read "CURRENT STATUS" to see exactly what's real and
   done vs. not, before assuming anything needs (re)building.
3. This doc's own task sections below, oldest to newest.

============================================================
HARD BOUNDARY — READ THIS BEFORE TOUCHING ANY FILE
============================================================
The target file, `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_
render.c`, is being actively, concurrently edited by BOTH sides of this
handoff at different times. It is NOT safe for both of us to have
uncommitted edits to this file at the same time - a concurrent edit
WILL collide or silently clobber the other side's in-progress work
(this exact risk is why this handoff doc exists at all, instead of just
both working ad hoc).

**Rule: before starting any task that touches khtpm_entity_menu_
render.c (or its shared-lib dependencies under `&.widgits/_shared-lib/`
- note: `build_entity_menu.sh` COPIES khtpm_draw_core.c/khtpm_render_
core.c/khtpm_css_parser.c(.h) from there on every build, so the REAL
editable source for those is the shared-lib copy, not the local `ops/`
one - a real, confusing gotcha already hit once tonight, don't repeat
it), post a real "🔒 CLAIMING" line here FIRST stating which file(s)
you're about to edit, wait for the other side to acknowledge (or a
reasonable real-time gap if the other side is offline), then work,
then post your real execution record and a "🔓 RELEASED" line when
done. If you find the other side's claim still standing when you want
to start, wait or pick a genuinely different, non-overlapping task
instead.**

Tasks that are pure research/design (no file edits) can happen anytime
without claiming anything - only real edits to the shared file(s) need
the claim/release protocol.

============================================================
REAL, CONFIRMED CONTEXT (don't re-derive, read the two docs above for
full detail - this is just the short version)
============================================================
- A prior claim that this render+input refactor was already done (from
  roughly a week before this doc started) is CONFIRMED CONFLATED with a
  different, real, separate migration (the taskbar's own input pipeline,
  genuinely completed 2026-08-19 - see RENDER-REFACTOR-2DO-PROGRESS.md's
  own decisions log for the full citation). This file's own render/
  input refactor never actually started before tonight (2026-08-28).
- RENDER half, Phase C (de-mode the scroll mechanism) - DONE, all 7
  modes, verified live, committed, pushed (commits 04cecc4 through
  7e599f8+ on `origin/main` - `git log` for exact diffs).
- RENDER half, Phase 2 (the actual frame-write-then-render mechanism) -
  a REAL, working first proof exists for ONE case: palettes' tile grid
  panel content. `dbhq_write_palette_frame_file()` serializes real
  layout (tag/id/classes/label/sprite/onclick/nav_index/active/x/y/w/h)
  to `#.desktop/palettes_frame.txt` BEFORE painting; `dbhq_paint_
  palette_frame_file()` reads ONLY that file (zero live Elem* access)
  to paint, reusing the real, unmodified `draw_elem()` on a temporary
  Elem built purely from parsed fields. Verified live, multiple real
  bugs found and fixed in the process (see progress doc): a fixed
  128→512-slot sprite cache with no eviction (now real LRU), a real
  window-shrink bug (the actual X11 window was never resized DOWN to
  match shrunk content, leaving old content visible as a "second
  layer" - now fixed, XGetWindowAttributes-checked every redraw), and
  a same-second mtime staleness bug in the reload gate (now a real
  content checksum instead of raw mtime/size).
- INPUT half - NOT STARTED AT ALL. Real, confirmed gap: X11 input
  events go straight from `XNextEvent()` to `dbhq_handle_click()`/
  `dbhq_handle_key()` in-process, no file boundary, unlike wraith-
  alpha's real keyboard_input-writes-file / parser-reads-file
  convention. The existing `db_hq_history.txt` relay is a bolt-on
  agent-automation side channel only (keyboard-code-only, no mouse
  coordinates or hit-test outcome recorded) - not the real human-input
  path, and not what "done" would mean here.

============================================================
🔧 OPEN TASK — build the INPUT half, Phase 3a pilot (db-hq ButtonPress
only - real, concrete spec below, ready to implement)
============================================================
Full real spec now lives in `RENDER-REFACTOR-2DO-PROGRESS.md`'s own
"PHASE 3" section - read that section in full before starting, this is
just the short version.

**Real design decision already made (don't re-litigate)**: extend the
SAME existing `db_hq_history.txt` file with two new typed line
prefixes (`CLICK <x> <y> <button>`, `KEYSYM <int>`) rather than a
second file; single-process, two real STEPS (capture, then consume via
the EXISTING `poll_agent_history()` loop) rather than a wraith-alpha-
style 2-OS-process split - khtpm_entity_menu_render.c already owns its
own X11 event queue natively, doesn't need the raw-terminal split
wraith-alpha's own domain required.

**Pilot scope, real and narrow**: db-hq's `ButtonPress` handling ONLY.
Not KeyPress yet (that's Phase 3b, open question below). Not events-hq/
chat-hai/taskbar-settings' own separate click paths yet (later phases -
render's own Phase C work already found these 3 modes duplicate their
own redraw/click/key code, so each needs its own capture/consume wiring
later, not lumped into this pilot).

**Concrete steps** (see the progress doc's own numbered list for full
detail + naming conventions):
1. New capture helper `dbhq_capture_click(int x, int y, int button)` -
   file-append only, zero interpretation, at db-hq's real ButtonPress
   site (grep `ev.type == ButtonPress` inside the `g_is_db_hq` branch
   of the main loop - do not touch the other 3 modes' own ButtonPress
   sites this pass).
2. Make `poll_agent_history()`'s existing per-tick read loop the ONLY
   place `dbhq_handle_click()` gets called for db-hq, for BOTH real
   human clicks and agent-relay-injected ones - extend `dispatch_relay_
   code()` (or add a sibling parse step) to recognize the new `CLICK `
   prefix before falling through to the existing bare-int path.
3. Real verification: a real (or XTest-simulated, per `_.0.aigent-
   testing-k9.txt`'s own Rule 11 - relay alone can't prove hit-testing
   parity) click on a real db-hq nav element still activates correctly,
   AND `db_hq_history.txt` shows a real `CLICK x y button` line that
   was appended and consumed BEFORE `dbhq_activate_elem()` ran - prove
   the file is a real audit boundary, not decorative.

**Open question, answer it as part of this task, don't leave it open**:
should KeyPress capture (Phase 3b) happen in this SAME pass, or should
ButtonPress be proven alone first (matching render's own one-mode-
first discipline)? Recommend: ButtonPress alone first, real working
proof, THEN KeyPress as a fast, mechanical follow-up once the pattern
is proven - but make the real call and note it here either way.

**Remember the HARD BOUNDARY above**: post a real "🔒 CLAIMING
khtpm_entity_menu_render.c" line here before you start editing it.
