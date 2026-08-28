RENDER-REFACTOR-2DO-PROGRESS.md
Started: 2026-08-28
Purpose: live-updated status tracker for the render de-mode / frame-
history refactor effort. If this session gets cut off, READ THIS FILE
FIRST — it should be enough to resume cold, without needing the full
chat history. Update it as you go, don't wait until a phase finishes.

============================================================
READ THIS FIRST IF RESUMING COLD
============================================================
1. Read `RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md` (this same directory)
   in full — it's the real design doc: what the drift is, why it's
   real (not deliberate), the wraith-alpha reference pattern, confirmed
   scope (taskbar and mutaclysm are OUT of scope, for real documented
   reasons — don't re-investigate that, it's settled).
2. Read the "CURRENT STATUS" section immediately below to see exactly
   what's done vs. not.
3. Read the "NEXT CONCRETE STEP" section — that's literally the next
   thing to do, no re-deriving needed.
4. Real commits so far (all pushed to `origin/main`, in order):
   `04cecc4` → `1932bcc` → `c220310` → `5115a60` — `git log` these for
   exact diffs if you need to see real code, don't guess from this doc
   alone.

============================================================
THE REAL GOAL (do not lose sight of this)
============================================================
`khtpm_entity_menu_render.c` (7 window modes: db-hq, events-hq,
chat-hai, palettes, bookmarks, stats-hq, taskbar-settings) should
eventually: (a) have ZERO mode-specific branches in its core engine —
all app-specific behavior lives in manager processes; (b) PAINT should
be derived from a real, written, auditable frame-description file, not
directly from a live in-memory Elem tree — matching the wraith-alpha
`chtpm_parser.c`/`renderer.c` pattern already standard everywhere else
in this house. **Two separate goals, both real, don't conflate them:**
(a) is "de-mode the engine", (b) is "make render an honest receipt".
Phase C (below) made progress on (a). Phase 2 (next) is the real start
of (b) — and (a) alone, without (b), does NOT achieve the "honest,
auditable receipts" property the user actually cares about. Don't
declare victory on (a) work and call the job done.

============================================================
CURRENT STATUS (update this section every session)
============================================================
**Phase A (research/inventory) — DONE.**
Two read-only inventories completed: every `g_is_palettes` branch
cataloged (20+ sites), and every OTHER mode's list/grid/scroll
mechanism surveyed. Key finding: palettes' scroll mechanism was the
ONLY scroll support anywhere in the file — db-hq sidebar, bookmarks,
chat-hai sidebar, events-hq command list all had ZERO scroll (silent
overflow bug, not just an architecture nit).

**Phase B (design) — DONE, informal.**
Decided: extract palettes' scroll/track/thumb/arrow mechanism into
`generic_scroll_layout_pass(Elem *container, const char *row_class,
int box_y, int box_h)`. Since each mode runs as its own SEPARATE
PROCESS of this binary (mode flags set once at startup, never change),
reusing the same `g_pal_*` globals across modes is safe — no rename
needed, only the gate condition + row-selector needed to generalize.

**Phase C (de-mode the scroll mechanism specifically) — DONE, all 7
modes, verified live, committed, pushed.**
- `generic_scroll_layout_pass()` built, zero mode-specific knowledge
  inside it (pure function of container/row_class/box).
- Palettes (tile grid, row_class="pal-grid-row") — done, verified live
  (real regression test: World_B.png 256 tiles, 13 rows, 10 visible,
  correct geometry, no crash).
- db-hq sidebar (Common Events + Terms + stats-hq, all one function,
  row_class=NULL) — done, verified live (real fix: this list had ZERO
  scroll before, now has real track/thumb/arrows).
- Bookmarks (row_class="bm-bookmark") — done (landed in the same
  commit as palettes/db-hq, `c220310`, confirmed by direct code read).
- chat-hai session sidebar (row_class="session-item") — done. Real
  finding: chat-hai has its OWN separate redraw/click/key code path
  (`chai_*` functions), never shared db-hq's — needed its own 7-site
  hookup (draw/click/key/wheel), not just a layout-pass call. Verified
  live (real 61-item feed + sidebar, no crash, no regression).
- events-hq command list (needed a NEW real class "cmd-row" added to
  `evhq_inject_commands()` since `cmd-<type>` varies per type and there
  was no shared marker) — done, same 7-site hookup as chat-hai needed.
  Verified live (3 real commands + title, no crash, no regression).
- Debug-dump (relay code 210) extended to also cover chat-hai (had
  none before, PNG-only) - and to always include real x/y/w/h geometry
  for every nav element (was tag/id/label only - this is what let a
  pure pixel-position bug hide from the debug dump earlier tonight).
- Real, live-confirmed: all 7 modes now share ONE scroll mechanism.
  Previously-silent overflow bug (4 modes) is fixed as a side effect.

**Phase 2 (the REAL fix — frame-write-then-render) — NOT STARTED YET.**
This is the part that actually delivers "honest, auditable receipts."
Scope agreed with the user just before this doc was written:
- Target: palettes' tile grid ONLY, as the first real, minimal proof.
- Mechanism: after layout computes real x/y/w/h for every visible
  element, write a real flat frame-description file (not yet named -
  pick something like `#.desktop/palettes_frame.txt`) BEFORE painting.
  Build a genuinely separate paint function that reads ONLY that file
  (zero live Elem-tree pointer access) to draw pixels.
- **Scope correction from the user, already agreed - include nav_index
  and focus-ring state in the frame file and the new paint function
  from the start.** (Original plan wrongly deferred this as "extra" -
  it's not architecturally harder, just one more column in the same
  line format + one more small draw call. Deferring it would mean
  redoing this same paint function again shortly after. Don't repeat
  that mistake.)
- **Explicitly OUT of scope for this first proof** (confirmed with
  user): click/key DISPATCH (deciding what happens when an element
  activates) stays reading the live tree for now - that's behavior,
  not the visual receipt, and wraith-alpha's own `renderer.c` doesn't
  handle input either (`chtpm_parser.c` does). This may become its own
  later phase but is not blocking Phase 2's own completion.
- Verification standard: live PNG-diff, same visual tile grid pixel-
  for-pixel before/after, but now provably sourced from a written file
  instead of live memory - not just "it compiles."

============================================================
NEXT CONCRETE STEP (do this next, no re-deriving)
============================================================
1. Design the real frame-file line format (needs at minimum: tag,
   label, x, y, w, h, sprite path, onclick string, nav_index, classes
   for focus/active-state styling - check `Elem` struct's real fields
   in khtpm_entity_menu_render.c before finalizing, don't guess field
   names).
2. Add a real serialize step right after `dbhq_layout_pass()` +
   `dbhq_assign_nav_indices()` finish for the palettes case specifically
   (scoped to `g_is_palettes`, not touching other modes yet) - walk the
   panel's visible children (tab row, tile rows, tileset chooser row)
   and write one real line per element.
3. Build a new function (e.g. `dbhq_paint_palette_frame_from_file()`)
   that reads ONLY that file and does the actual `XFillRectangle`/
   sprite-blit/text draw calls - no `Elem*` dereferencing at all in
   this function, prove it by construction (take a parsed line struct
   as input, not an `Elem*`).
4. Swap the call inside `dbhq_redraw_content()`'s palettes branch from
   the existing `render_tree(g_window, 0)` to: serialize → call the new
   frame-reading paint function, for palettes only.
5. Rebuild, live-test: launch palettes/rmmv, PNG-dump BEFORE this
   change (already have real reference PNGs from tonight's earlier
   testing) and AFTER, confirm visually identical. Also confirm the new
   frame file itself is real, readable, and genuinely matches what got
   drawn (the actual auditability property being proven).
6. Once proven for palettes, THIS DOC gets updated with the real
   result, and a decision gets made (with the user) on whether to
   extend to the other 6 modes now or pause again.

============================================================
DECISIONS LOG (append, don't rewrite history)
============================================================
- 2026-08-28: pilot mode for de-moding = palettes (user confirmed:
  "if u are comfortable doing palettes thats fine").
- 2026-08-28: taskbar and mutaclysm confirmed OUT of scope (real
  research, not assumption - see assessment doc's "SCOPE CONFIRMED"
  section).
- 2026-08-28: Phase C scope widened from "palettes only" to "fix the
  overflow bug in all 4 other modes too" per direct user choice
  ("Build the primitive AND fix the other modes' overflow bug now").
- 2026-08-28: nav_index/focus-ring included in Phase 2's first proof
  scope (user pushback, correctly identified this wasn't actually
  harder, just deferred out of my own instinct to keep the diff small).
- 2026-08-28: click/key dispatch explicitly staying on the live tree
  for Phase 2's first proof - confirmed acceptable per wraith-alpha's
  own real precedent (renderer.c doesn't handle input either).
- 2026-08-28: user recalled this render+input refactor being claimed
  "done" roughly a week prior. Searched real - NO doc anywhere claims
  khtpm_entity_menu_render.c's render/input was migrated to a file-
  derived pattern. What genuinely WAS completed and verified around
  2026-08-19 is a different, separate program: the TASKBAR's own input
  pipeline (khtpm_strip_parser.c family) was real-migrated to a relay/
  history.txt dispatch (see taskbar-keyboard-relay-and-terminal-
  render.md / taskbar-history-txt-migration-investigation.md, both
  "Phase 3 - cutover complete 2026-08-19"). Confirmed, real, not
  hallucinated - but a STRUCTURALLY DIFFERENT file (taskbar
  deliberately stays on its own LayDoc architecture, never Elem/CSS -
  see this same doc's own "SCOPE CONFIRMED" section). User's own
  conclusion, agreed: the two got conflated somewhere - this file's
  own render+input refactor was never actually started before tonight,
  regardless of how that mix-up happened. Not treating this as "was
  hallucinated then abandoned", treating it as "never began" - same
  real starting point either way.
- 2026-08-28: input-pipeline gap identified as a REAL, separate finding
  from tonight's window-geometry staleness bug (found+fixed
  independently) - real X11 input (ButtonPress/KeyPress) currently goes
  straight from XNextEvent() to handle_click()/handle_key() in-process,
  no file boundary, unlike wraith-alpha's real keyboard_input-writes-
  file / chtpm_parser-reads-file convention. db_hq_history.txt is a
  bolt-on agent-automation side channel only (keyboard-code-only, no
  mouse coordinates/hit-test outcome) - not the real human-input path.
  Scoping this as a genuinely separate refactor arm (input, alongside
  render), both real, both needed, tracked together in this same doc.

============================================================
PHASE 3 (input half — real capture-then-consume, wraith-alpha
parity for INPUT, not just render) — NOT STARTED
============================================================
Real, confirmed gap (2026-08-28): X11 input (ButtonPress/KeyPress)
goes straight from XNextEvent() to dbhq_handle_click()/dbhq_handle_key()
in the same loop tick — no file boundary, unlike wraith-alpha's real
keyboard_input.c -> pieces/keyboard/history.txt -> chtpm_parser.c
split. The existing db_hq_history.txt relay (history_path()/
poll_agent_history()/dispatch_relay_code(), ~line 6527) is real and
already does HALF of what's needed (append-only file, persistent
cursor, generic dispatch) but only for AGENT-injected keyboard codes,
not real human mouse/keyboard events, and carries no coordinates.

HONEST NOTE, do not conflate with the earlier "2 clicks" bug: that
was root-caused and fixed separately (real X11 window not shrinking
to match content, so XPutImage left a stale strip on screen). This
input refactor is a genuinely SEPARATE architectural goal — input
auditability, matching what render now has — not a fix for that
symptom. It may or may not change perceived click responsiveness;
don't claim it fixes a bug it wasn't built to fix.

REAL DESIGN (REVISED 2026-08-28 - do not reinvent, a real, already-
proven format exists and should be reused per direct instruction):
1. `pieces/keyboard/history.txt` already has a REAL, in-production,
   two-line-type convention (confirmed live in `&.widgits/_shared-lib/
   system/chtpm_parser_pal.c`'s own poll loop - this is mutaclysm's
   real format, not a proposal):
     KEY_PRESSED: <decimal code>\n
     MOUSE_EVENT: <button> <x> <y> <is_press>\n
   Reuse this EXACT format for db_hq_history.txt's new lines, not the
   earlier CLICK/KEYSYM shape this section originally proposed - same
   house, same real, working precedent, no reason to diverge. The
   taskbar's own real migration (see conflation note above) ALSO used
   this exact shape when it added a 5th field for multi-window
   disambiguation: `MOUSE_EVENT: <button> <x> <y> <is_press>
   <window_name>` - khtpm_entity_menu_render.c is one-window-per-
   process so likely doesn't need that 5th field, but note it as
   available/reusable if a future mode ever needs it, don't reinvent
   THAT either if it comes up.
2. KeyPress: `KEY_PRESSED: <decimal code>` covers printable/simple keys
   directly (bare decimal, matches the existing convention this file's
   OWN dispatch_relay_code() already parses for agent-injected codes -
   real overlap, not a new concept). Non-ASCII KeySyms (arrows, Page
   Up/Down) need their real X11 KeySym integer value carried losslessly
   - use the same `KEY_PRESSED: <code>` line with the raw KeySym int
   when `ch` isn't a plain ASCII byte, rather than inventing a THIRD
   line type - dbhq_handle_key() can tell the two apart the same way
   it already does today (checking ch vs a KeySym constant range).

2. Real capture step (write-only, replaces direct dispatch): at each
   of the 4 real `ev.type == ButtonPress` sites (db-hq ~line 7702,
   events-hq, chat-hai, taskbar-settings - grep `ev.type == ButtonPress`
   for the other 3's exact line numbers, don't assume they're
   identical shape) and 4 real `ev.type == KeyPress` sites, STOP
   calling dbhq_handle_click()/dbhq_handle_key() (or the chai_/evhq_
   equivalents) directly. Instead append the real event to that mode's
   own history file via a new small helper:
     static void dbhq_capture_click(int x, int y, int button);
     static void dbhq_capture_key(KeySym ks, char ch);
   (naming mirrors dbhq_append_frame_history()'s own real convention).
   These do ONLY a file append - zero interpretation, mirroring
   keyboard_input.c's own real discipline of capture doing nothing but
   capture.

3. Real consume step: extend dispatch_relay_code() (or add a sibling
   dispatch_relay_line() that pre-parses the "CLICK "/"KEYSYM " prefix
   before falling through to the existing bare-int path) so
   poll_agent_history()'s existing per-tick read loop is the ONLY
   place dbhq_handle_click()/dbhq_handle_key() ever get called for
   real human input too - the exact same function, same code path,
   whether the line came from a real mouse click or an agent's relay
   injection. This is what makes it a real boundary, not a renamed
   function call: the SAME poll_agent_history() that already runs
   every loop tick (line ~7606) becomes the single real consumer for
   BOTH input sources.

4. Do NOT split into two OS processes. wraith-alpha's 2-process split
   exists because IT needs the raw terminal in a mode X11 doesn't
   require here - khtpm_entity_menu_render.c already owns its own X11
   event queue natively. The real, sufficient boundary is two
   DISTINCT STEPS in the same process/loop (capture this tick, a
   read-and-dispatch pass either the same tick after capture or next
   tick via the existing 150ms poll) with the FILE as the real
   contract between them - matching this file's own existing
   interact_relay.txt precedent (per-entity, one real file, multiple
   real producers/consumers) more closely than it needs to invent a
   process split wraith-alpha's own domain (a raw terminal) required
   for different reasons.

PILOT SCOPE (mirrors render Phase 2's own "one mode first" discipline):
db-hq's ButtonPress path ONLY, not KeyPress yet, not the other 3
modes' click paths yet. Reasons: (a) db-hq's dbhq_handle_click() is
today's most mature, most-recently-fixed click path (arrows, close
button, hit_test all real and working); (b) it already coexists with
a WORKING poll_agent_history()/dispatch_relay_code() consumer loop -
less new plumbing than chat-hai/events-hq, which the render refactor
already found have their OWN separate, duplicated redraw/click/key
code (same real duplication would need its own capture/consume wiring
per mode, a later phase, not this pilot).

REAL VERIFICATION STANDARD: a real, physical mouse click (or an
XTest-simulated one, per this house's own Rule 11 in
_.0.aigent-testing-k9.txt - relay alone can't prove hit-testing
parity) on a real db-hq nav element must still activate correctly,
AND the history file must show a real, readable "CLICK <x> <y> <button>"
line was appended and consumed BEFORE dbhq_activate_elem() ran - i.e.
prove the file boundary is real, not decorative, by showing you could
audit "what was clicked" from the file alone after the fact.

OPEN QUESTION for the user before implementation starts: should
KeyPress capture (Phase 3b) happen in the SAME pass as ButtonPress
(Phase 3a), or should ButtonPress be proven alone first, same
one-thing-at-a-time discipline as the render refactor's own palettes-
first pilot?
